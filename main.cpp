#include "widget.h"

#include <QApplication>
#include <QTimer>
#include <QDebug>

#include "audioplayer.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    Widget w;
    w.show();
#if 0
       AudioPlayer player;
       QStringList devices = player.getAudioDevices();
       qDebug() << "Available audio devices:" << devices;

       // 设置音频文件和输出设备
       player.setAudioFile("D:/develop/qtApp/build-VoiceTestTools-Desktop_Qt_5_12_12_MinGW_64_bit-Debug/debug/test.mp3");
       player.setOutputDevice(devices.at(0));  // 选择第一个可用的设备

       QObject::connect(&player, &AudioPlayer::playbackFinished, []() {
           qDebug() << "Playback finished!";
           QCoreApplication::quit();
       });

       player.play();

       QTimer::singleShot(50000, [&player]() {
           qDebug() << "Pausing playback...";
           player.pause();
       });

       QTimer::singleShot(70000, [&player]() {
           qDebug() << "Resuming playback...";
           player.play();
       });

       QTimer::singleShot(100000, [&player]() {
           qDebug() << "Stopping playback...";
           player.stop();
       });


       // 创建音频播放器对象
       AudioPlayer *player = new AudioPlayer("D:/develop/qtApp/build-VoiceTestTools-Desktop_Qt_5_12_12_MinGW_64_bit-Debug/debug/test.mp3");

       // 连接信号与槽
       QObject::connect(player, &AudioPlayer::playbackFinished, []() {
           qDebug() << "Playback finished!";
           QCoreApplication::quit();  // 播放完成后退出应用
       });

       QObject::connect(player, &AudioPlayer::positionChanged, [](int position) {
           qDebug() << "Current Position: " << position << " seconds";
       });

       QObject::connect(player, &AudioPlayer::errorOccurred, [](const QString &errorMessage) {
           qDebug() << "Error: " << errorMessage;
       });

       // 启动播放线程
       player->play();
       QTimer::singleShot(50000, [&player]() {
           qDebug() << "Pausing playback...";
           player->pause();
       });

       QTimer::singleShot(70000, [&player]() {
           qDebug() << "Resuming playback...";
           player->play();
       });

       QTimer::singleShot(100000, [&player]() {
           qDebug() << "Stopping playback...";
           player->stop();
       });
#endif
    return a.exec();
}
