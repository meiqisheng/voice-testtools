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

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
}

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

public slots:
    void play();
    void pause();
    void stop();

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

    QString audioFilePath;
    QString outputDeviceName;

    AVFormatContext *formatContext = nullptr;
    AVCodecContext *codecContext = nullptr;
    AVPacket *packet = nullptr;
    AVFrame *frame = nullptr;
    SwrContext *swrContext = nullptr;

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

public:
    bool m_thread_start;
};

#endif // AUDIOPLAYER_H

