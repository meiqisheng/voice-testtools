#ifndef AUDIOPLAYER_H
#define AUDIOPLAYER_H

#include <QAudioOutput>
#include <QElapsedTimer>
#include <QFile>
#include <QThread>
#include <QDebug>
#include <QAudioDeviceInfo>
#include <QString>
#include <QObject>
#include <QMutex>
#include <QWaitCondition>
#include <complex>
#include <QBuffer>
#include <QByteArray>
#include <QIODevice>
#include <QAudioFormat>
#include <QAudioDeviceInfo>

//#include "fftw3.h"
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersrc.h>
#include <libavfilter/buffersink.h>
#include <libavutil/opt.h>
#include <libavutil/error.h>
}

#if defined(_MSC_VER)
    #define PRIx64 "I64x"  // MSVC 的 64 位整数格式化宏
#else
    #define PRIx64 "llx"
#endif

class AudioPlayer : public QThread {
    Q_OBJECT

public:
  //  explicit AudioPlayer();
   explicit AudioPlayer(QObject *parent = nullptr) : QThread(parent) {}
   explicit AudioPlayer( const QString & filePath, const QString &outputDeviceName = QString());
    ~AudioPlayer();

    int getTotalDuration() const;       // 获取音频总时长（秒）
    int getCurrentPosition() const;     // 获取当前播放位置（秒）
    static QStringList getAvailableAudioDevices(); // 获取可用的声卡设备列表
    void switchFile(const QString &newFilePath);   // 切换音频文件
    void setAudioDevice(const QString &deviceName);
    void setAudioFile(const QString &filePath);
    void setVolume(qreal vol);
    void setEQData(const std::vector<double>& frequencies, const std::vector<double>& gains);

public slots:
    void play();
    void pause();
    void stop();
    void onSaveFlagChanged(bool flag);  // 槽函数

signals:
    void playbackFinished();             // 播放完成信号
    void positionChanged(int pos);       // 播放位置改变信号
    void errorOccurred(const QString &errorMessage); // 错误状态信号

protected:
    void run() override;

private:
    bool initializeFFmpeg();
    void cleanupFFmpeg();
    void releaseResources();
    //音频时域频域转化接口
    bool initEQFilters(const std::vector<double> &frequencies,
                       const std::vector<double> &gains,
                       double bandwidth);
    bool applyEQ(AVFrame *inputFrame, AVFrame *outputFrame);
    QString audioFilePath;
    QString outputDeviceName;

    AVFormatContext *formatContext = nullptr;
    AVCodecContext *codecContext = nullptr;
    AVPacket *packet = nullptr;
    AVFrame *frame = nullptr;
    SwrContext *swrContext = nullptr;
    AVFilterGraph *filterGraph = nullptr;
    AVFilterContext *srcFilterCtx = nullptr;
    AVFilterContext *sinkFilterCtx = nullptr;

    QAudioOutput *audioOutput = nullptr;
    QIODevice *audioDevice = nullptr;

    QElapsedTimer timer;
    int64_t startPts = 0;
    int totalDuration = 0;   // 音频总时长（秒）
    int lastPosition = -1;   // 记录上一次的播放位置
    int audio_stream_index = -1;
    qreal volValue = 0.1;

    enum PlayState { Stopped, Playing, Paused };
    PlayState state = Stopped;
    QMutex mutex;
    QWaitCondition pauseCondition;

    std::vector<double> mFrequencies;

    std::vector<double> mGains;

    double mBandwidth = 20.0; // 1 倍频程
    QFile saveAudioPCMFile;
    bool mSaveFlag = false;

public:
    bool m_thread_start;
};

#endif // AUDIOPLAYER_H

