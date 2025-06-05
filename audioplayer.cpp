#include "audioplayer.h"
#include <QDateTime>

AudioPlayer::AudioPlayer(const QString &filePath, const QString &outputDeviceName)
    : audioFilePath(filePath), outputDeviceName(outputDeviceName) {

}

AudioPlayer::~AudioPlayer() {
    stop();
    wait();  // 等待线程完全退出
    releaseResources();
}

// 用来存储原始频率和增益
struct EqPoint {
    double frequency;
    double gain;
};

// audioplayer.cpp
void AudioPlayer::onSaveFlagChanged(bool flag) {
    mSaveFlag = flag;
    qDebug() << "AudioPlayer mSaveFlag:" << mSaveFlag;
}

bool AudioPlayer::initEQFilters(const std::vector<double> &frequencies, const std::vector<double> &gains, double bandwidth) {
    int ret;

    filterGraph = avfilter_graph_alloc();
    if (!filterGraph) {
        return false;
    }

    const AVFilter *srcFilter = avfilter_get_by_name("abuffer");
    const AVFilter *sinkFilter = avfilter_get_by_name("abuffersink");

    char args[512];
    snprintf(args, sizeof(args),
             "time_base=1/%d:sample_rate=%d:sample_fmt=%s:channel_layout=0x%" PRIx64,
             codecContext->time_base.den,
             codecContext->sample_rate,
             av_get_sample_fmt_name(codecContext->sample_fmt),
             codecContext->channel_layout);

    ret = avfilter_graph_create_filter(&srcFilterCtx, srcFilter, "in", args, nullptr, filterGraph);
    if (ret < 0) {
        return false;
    }

    ret = avfilter_graph_create_filter(&sinkFilterCtx, sinkFilter, "out", nullptr, nullptr, filterGraph);
    if (ret < 0) {
        return false;
    }

    // 准备插值
    std::vector<EqPoint> eqPoints;
    for (size_t i = 0; i < frequencies.size(); ++i) {
        eqPoints.push_back({ frequencies[i], gains[i] });
    }

    // 生成filter graph描述字符串
    std::string filterDesc;
    filterDesc += "[in]";  // 起点
    filterDesc += "firequalizer=gain_entry='";
    // ================ 关键修改：直接构建参数 ================
    // 生成EQ点字符串（格式：f1:g1:w1|f2:g2:w2|...）
    float Bandwidths1 = 5.0;
    for (size_t i = 0; i < frequencies.size(); ++i) {
        // 确保带宽值在合法范围内
        char point[64];
        snprintf(point, sizeof(point), "entry(%.1f,%.1f)",
                 frequencies[i], gains[i]);

        filterDesc += point;
        if (i != frequencies.size() - 1) {
            filterDesc += ";";  // 用竖杠分隔不同EQ点
        }
    }
    filterDesc += "'";  // 结束gain参数
    filterDesc += "[out]";  // 终点
    qDebug() << "Final filterDesc: " << filterDesc.c_str();

    // 分配输入输出
    AVFilterInOut *outputs = avfilter_inout_alloc();
    AVFilterInOut *inputs = avfilter_inout_alloc();

    outputs->name = av_strdup("in");
    outputs->filter_ctx = srcFilterCtx;
    outputs->pad_idx = 0;
    outputs->next = nullptr;

    inputs->name = av_strdup("out");
    inputs->filter_ctx = sinkFilterCtx;
    inputs->pad_idx = 0;
    inputs->next = nullptr;

    // 解析整个图
    ret = avfilter_graph_parse_ptr(filterGraph, filterDesc.c_str(), &inputs, &outputs, nullptr);
    if (ret < 0) {
        return false;
    }

    // 配置图
    ret = avfilter_graph_config(filterGraph, nullptr);
    if (ret < 0) {
        return false;
    }
    return true;
}


bool AudioPlayer::initializeFFmpeg() {
    // 设置 FFmpeg 日志级别和回调，用于调试 filter 报错
    av_log_set_level(AV_LOG_DEBUG);
    av_log_set_callback([](void*, int level, const char* fmt, va_list args) {
        if (level <= AV_LOG_ERROR) {
            char buf[1024];
            vsnprintf(buf, sizeof(buf), fmt, args);
            qDebug() << "[FFmpeg]" << buf;
        }
    });
    if (audioFilePath.isEmpty()){
        return false;
    }
    // 打开输入文件并获取格式信息
    if (avformat_open_input(&formatContext, audioFilePath.toStdString().c_str(), nullptr, nullptr) < 0) {
        emit errorOccurred("Failed to open audio file");
        return false;
    }
    if (avformat_find_stream_info(formatContext, nullptr) < 0) {
        emit errorOccurred("Failed to find stream info");
        return false;
    }

    AVCodec *codec = nullptr;
    int streamIndex = av_find_best_stream(formatContext, AVMEDIA_TYPE_AUDIO, -1, -1, &codec, 0);
    if (streamIndex < 0) {
        emit errorOccurred("Failed to find audio stream");
        return false;
    }


    for (int i = 0; i < formatContext->nb_streams; i++) {
        if (formatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audio_stream_index = i;
            break;
        }
    }

    if (audio_stream_index == -1) {
        fprintf(stderr, "No video stream found\n");
        return -1;
    }

    codecContext = avcodec_alloc_context3(codec);
    if (!codecContext) {
        emit errorOccurred("Failed to allocate codec context");
        return false;
    }

    if (avcodec_parameters_to_context(codecContext, formatContext->streams[audio_stream_index]->codecpar) < 0) {
        emit errorOccurred("Failed to copy codec parameters to context");
        return false;
    }
    // 如果 channel_layout 为 0，手动推断
    if (codecContext->channel_layout == 0 && codecContext->channels > 0) {
        codecContext->channel_layout = av_get_default_channel_layout(codecContext->channels);
    }
    if (avcodec_open2(codecContext, codec, nullptr) < 0) {
        emit errorOccurred("Failed to open codec");
        return false;
    }
    // 初始化 EQ 滤波器
    if (!initEQFilters(mFrequencies, mGains, mBandwidth)) {
        emit errorOccurred("Failed to initialize EQ filters");
        return false;
    }

    // 设置音频总时长
    totalDuration = static_cast<int>(formatContext->duration / AV_TIME_BASE); // 总时长（秒）

    //防止音频中出现异常参数无法播放问题
    if (codecContext->channels <= 0 || codecContext->channel_layout == 0) {
        AVStream* stream = formatContext->streams[audio_stream_index];
        if (stream && stream->codecpar) {
            codecContext->channels = stream->codecpar->channels; // 从流参数获取通道数
            codecContext->channel_layout = stream->codecpar->channel_layout; // 获取通道布局
        }

        // 如果仍然无效，设置默认值
        if (codecContext->channels <= 0) {
            codecContext->channels = 2; // 默认设置为立体声
        }
        if (codecContext->channel_layout == 0) {
            if (codecContext->channels == 1) {
                codecContext->channel_layout = AV_CH_LAYOUT_MONO; // 单声道
            } else if (codecContext->channels == 2) {
                codecContext->channel_layout = AV_CH_LAYOUT_STEREO; // 立体声
            } else {
                emit errorOccurred("Unsupported channel configuration");
                return false;
            }
        }
    }
    // 初始化重采样器
    swrContext = swr_alloc_set_opts(nullptr,
                                    av_get_default_channel_layout(codecContext->channels),
                                    AV_SAMPLE_FMT_S16,
                                    codecContext->sample_rate,
                                    codecContext->channel_layout,
                                    AV_SAMPLE_FMT_FLTP,
                                    codecContext->sample_rate,
                                    0, nullptr);
    if (!swrContext){
        emit errorOccurred("Failed to initialize resampler swrContext=NULL");
        return false;
    }
    int ret = swr_init(swrContext);
    if ( ret < 0) {

        char errbuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errbuf, sizeof(errbuf));
        QString str = QString::asprintf("Failed to initialize ret:%d\n,"
                                        "sample_rate:%d\n"
                                        "channel_layout:%llu\n"
                                        "sample_fmt:%d\n"
                                        "SwrContext: %s",
                                        ret,
                                        codecContext->sample_rate,
                                        codecContext->channel_layout,
                                        codecContext->sample_fmt,
                                        errbuf);
        emit errorOccurred(str);
        swr_free(&swrContext);
        return false;
     }

    packet = av_packet_alloc();
    frame = av_frame_alloc();
    if (!packet || !frame) {
        emit errorOccurred("Failed to allocate packet or frame");
        return false;
    }

    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    QString saveFileName = QString("audio_%1.pcm").arg(timestamp);
    if(mSaveFlag){
        saveAudioPCMFile.setFileName(saveFileName);
        if (!saveAudioPCMFile.open(QIODevice::WriteOnly)) {
            qDebug() << "open file failed";
        }
    }

    return true;
}


void AudioPlayer::cleanupFFmpeg() {
    av_packet_free(&packet);
    av_frame_free(&frame);
    swr_free(&swrContext);
    avcodec_free_context(&codecContext);
    avformat_close_input(&formatContext);
    if(mSaveFlag){
        saveAudioPCMFile.close();
    }
}


void AudioPlayer::releaseResources() {
    delete audioOutput;
    audioOutput = nullptr;
    audioDevice = nullptr;
}

void AudioPlayer::switchFile(const QString &newFilePath) {
    QMutexLocker locker(&mutex);

    if (state != Stopped){
        // 停止当前播放
        stop();
        wait();  // 等待线程安全退出

        // 清理旧的文件资源
        releaseResources();

    }
    // 设置新的文件路径
    audioFilePath = newFilePath;

    // 重新初始化并播放新文件
    if (!initializeFFmpeg()) {
        emit errorOccurred("Failed to initialize new file");
        return;
    }
    play();  // 开始播放新文件
}

void AudioPlayer::setAudioDevice(const QString &deviceName)
{
     QMutexLocker locker(&mutex);  // 使用互斥锁保护共享数据
     outputDeviceName =  deviceName;

    return;
}

void AudioPlayer::setAudioFile(const QString &filePath)
{
    QMutexLocker locker(&mutex);  // 使用互斥锁保护共享数据
    audioFilePath = filePath;
    return;
}

void AudioPlayer::setVolume(qreal vol)
{
     volValue = vol;
     if (audioOutput == nullptr){
         return;
     }
     audioOutput->setVolume(volValue);
}

bool AudioPlayer::applyEQ(AVFrame *inputFrame, AVFrame *outputFrame) {
    int ret;
//    qDebug() << "inputFrame->format:" << inputFrame->format;
//    qDebug() << "inputFrame->sample_rate:" << inputFrame->sample_rate;
//    qDebug() << "inputFrame->channel_layout:" << inputFrame->channel_layout;
//    qDebug() << "codecContext->sample_fmt:" << codecContext->sample_fmt;
//    qDebug() << "codecContext->sample_rate:" << codecContext->sample_rate;
//    qDebug() << "codecContext->channel_layout:" << codecContext->channel_layout;
    inputFrame->channel_layout = codecContext->channel_layout;
    inputFrame->sample_rate = codecContext->sample_rate;
    inputFrame->format = codecContext->sample_fmt;
    // 推送音频帧到滤波器图
    ret = av_buffersrc_add_frame(srcFilterCtx, inputFrame);
    if (ret < 0) {
      //  qWarning() << "Failed to push frame to filter graph:" << av_err2str(ret);
        return false;
    }

    // 从滤波器图拉取处理后的音频帧
    ret = av_buffersink_get_frame(sinkFilterCtx, outputFrame);
    if (ret < 0) {
       // qWarning() << "Failed to pull frame from filter graph:" << av_err2str(ret);
        return false;
    }

    return true;
}

void AudioPlayer::run() {
    if (!initializeFFmpeg()) {
        qWarning() << "Failed to initialize FFmpeg";
        return;
    }
    AVFrame *filteredFrame = av_frame_alloc();
    if (!filteredFrame){
        return;
    }

    QAudioFormat format;
    format.setSampleRate(codecContext->sample_rate);
    format.setChannelCount(codecContext->channels);
    format.setSampleSize(16);  // 假设为16位PCM输出
    format.setCodec("audio/pcm");
    format.setByteOrder(QAudioFormat::LittleEndian);
    format.setSampleType(QAudioFormat::SignedInt);

    QAudioDeviceInfo deviceInfo;
    if (!outputDeviceName.isEmpty()) {
        for (const QAudioDeviceInfo &device : QAudioDeviceInfo::availableDevices(QAudio::AudioOutput)) {
            if (device.deviceName() == outputDeviceName) {
                deviceInfo = device;
                break;
            }
        }
    }
    audioOutput = new QAudioOutput(deviceInfo.isNull() ? QAudioDeviceInfo::defaultOutputDevice() : deviceInfo, format, this);
    audioOutput->setBufferSize(192000); // 加大缓冲区
    audioOutput->setVolume(volValue);
    audioDevice = audioOutput->start();
    timer.start();
    startPts = 0;

    state = Playing;

    while (state != Stopped && av_read_frame(formatContext, packet) >= 0) {
        {
            QMutexLocker locker(&mutex);
            if (state == Paused) {
                pauseCondition.wait(&mutex); // 等待暂停解除
            }
        }

        if (packet->stream_index != audio_stream_index){
            continue;
        }

        if (state == Stopped) break;

        int ret = avcodec_send_packet(codecContext, packet);
        if ( ret >= 0) {
            while (avcodec_receive_frame(codecContext, frame) >= 0) {
                int64_t pts = frame->pts;
                if (startPts == 0) {
                    startPts = pts;
                }
                int64_t elapsed = timer.elapsed();
                int64_t frameTimeMs = (pts - startPts) * av_q2d(formatContext->streams[0]->time_base) * 1000;

                // 等待时间以保持同步
                if (elapsed < frameTimeMs) {
                    QThread::msleep(frameTimeMs - elapsed);
                }
                if (!applyEQ(frame,filteredFrame)){
                    continue;
                }
                // 转换数据并写入音频输出设备
                uint8_t *outputData = nullptr;
                int linesize = 0;
                av_samples_alloc(&outputData, &linesize, codecContext->channels,
                                 filteredFrame->nb_samples, AV_SAMPLE_FMT_S16, 0);
                int convertedSamples = swr_convert(swrContext, &outputData, filteredFrame->nb_samples,
                                                   (const uint8_t **)filteredFrame->data, filteredFrame->nb_samples);
                int outputSize = convertedSamples * codecContext->channels * 2;
//                int outputSize = av_samples_alloc(&outputData, nullptr, codecContext->channels,
//                                                  filteredFrame->nb_samples, AV_SAMPLE_FMT_S16, 0);
//                swr_convert(swrContext, &outputData, filteredFrame->nb_samples,
//                            (const uint8_t **)filteredFrame->data, filteredFrame->nb_samples);

                //音频时域转化为频域

//                audioDevice->write((const char *)outputData, outputSize);
//                if(mSaveFlag){
//                    saveAudioPCMFile.write((const char *)outputData,outputSize);
//                }
//                av_freep(&outputData);  // 释放转换后的数据
                // 等待缓冲区有足够空间再写入，防止杂音
                while (audioOutput->bytesFree() < outputSize && state != Stopped) {
                    QThread::msleep(2);
                }

                // 写入音频数据
                qint64 written = audioDevice->write((const char *)outputData, outputSize);
                if (written != outputSize) {
                    qWarning() << "Audio write mismatch:" << written << "/" << outputSize;
                }

                // 可选：写入PCM文件
                if (mSaveFlag) {
                    saveAudioPCMFile.write((const char *)outputData, outputSize);
                }

                av_freep(&outputData);

                // 检查并发射位置改变信号
                int currentPosition = getCurrentPosition();
                if (currentPosition != lastPosition) {
                    emit positionChanged(currentPosition);
                    lastPosition = currentPosition;
                }
            }
        } else {
            char errbuf[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, errbuf, sizeof(errbuf));
           // fprintf(stderr, "Error sending packet: %s\n", errbuf);
            QString str = QString::asprintf("Error sending packet: %s\n", errbuf);
            emit errorOccurred(str);
            break;
        }

        av_packet_unref(packet);  // 释放packet数据
    }
    av_frame_unref(filteredFrame);
    cleanupFFmpeg();
    emit playbackFinished();  // 发射播放完成信号
}

// 播放方法
void AudioPlayer::play() {
    QMutexLocker locker(&mutex);
    if (state == Paused) {
        state = Playing;
        pauseCondition.wakeAll();
    } else if (state == Stopped) {
        start();
    }
}

// 暂停方法
void AudioPlayer::pause() {
    QMutexLocker locker(&mutex);
    if (state == Playing) {  //
        state = Paused;
    }
}

// 停止方法
void AudioPlayer::stop() {
    QMutexLocker locker(&mutex);
    if (state != Stopped) {
        state = Stopped;
        pauseCondition.wakeAll(); // 解除暂停状态
    }
}

// 获取音频总时长（秒）
int AudioPlayer::getTotalDuration() const {
    return totalDuration;
}

// 获取当前播放位置（秒）
int AudioPlayer::getCurrentPosition() const {
    return static_cast<int>(timer.elapsed() / 1000);
}

// 获取可用的音频设备列表
QStringList AudioPlayer::getAvailableAudioDevices() {
    QStringList deviceNames;
    for (const QAudioDeviceInfo &deviceInfo : QAudioDeviceInfo::availableDevices(QAudio::AudioOutput)) {
        deviceNames << deviceInfo.deviceName();
    }
    return deviceNames;
}

void AudioPlayer::setEQData(const std::vector<double>& frequencies, const std::vector<double>& gains)
{
    mFrequencies = frequencies;
    mGains = gains;
}
