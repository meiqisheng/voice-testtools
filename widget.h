#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include <QListWidgetItem>
#include <QProcess>
#include <QTimer>
#include <QTextEdit>
#include <QComboBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonArray>
#include <QJsonParseError>
#include <QLayout>
#include <QSettings>


#include "audioplayer.h"

QT_BEGIN_NAMESPACE
namespace Ui { class Widget; }
QT_END_NAMESPACE

class Widget : public QWidget
{
    Q_OBJECT

public:
    Widget(QWidget *parent = nullptr);
    ~Widget();
    void adbProcessExecude(QString &adbPath, QStringList &arguments); //执行adb 命令
    void loadEQPresets(QComboBox* EQcomboBox, const QString& filePath);
    void loadAudioPathFromJson();
    void saveAudioPathToJson(const QString& filePath);
public slots:
    void on_AudioFileBtn_Clicked();
    void on_WakeTestBtn_Clicked();
    void on_WakeTestPauseBtn_Clicked();
    void on_WakeTestStopBtn_Clicked();
    void on_GetWakeTestResultBtn_Clicked();
    void on_GetCharTestDataBtn_Clicked();
    void on_SetPlaySumBtn_Clicked();
    void on_ClearTextEditBtn_Clicked();
    void on_CalcDelayTimeBtn_Clicked();
    void on_ClearDevWakeRecordBtn_Clicked();
    void on_SaveLogFileBtn_Clicked();
    void on_DisplayDateTimeBtn_Clicked();
    void on_AudioDevComboBox_CurrentTextChanged(QString deviceName);
    void on_PlayModeComboBox_CurrentIndexChanged(int indx);
    void on_TestModeComboBox_CurrentIndexChanged(int indx);
    void AudioPlayFinished();
    void AudioPlayPositionChanged(qint64 position);       // 播放位置改变信号
    void AudioPlayErrorOccurred(const QString &error); // 错误状态信号
    void onItemClicked(QListWidgetItem * item);
    void onItemDoubleClicked(QListWidgetItem * item);
    void adbCmdOutput();
    void adbCmdErrorOutput();
    void adbCmdFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void on_TimerTimeout_do();
    void on_ClickTimerTimeout_do();
    void on_UpdateTime_do();
    void on_WakeIntervalTimeEdit_EditingFinished();
    void on_CharTestIntervalTimeEdit_EditingFinished();
    void on_VolumeSlider_ValueChanged(int value);
signals:
    void saveFlagChanged(bool flag);
private slots:
    void on_AudioFileBulkBtn_clicked();
    void on_EQcomboBox_currentTextChanged(int index);
    void on_LoadJSON_clicked(QComboBox* EQcomboBox, const QString filePath);
    void on_ClearList_clicked();

    void on_pushButton_clicked();

    void on_pushButton_3_clicked();

private:
    bool addAudiofileToList(QString fileName);  //成功返回true,失败返回false，防止名字重复
    void analysisWakeTestResult();
    void analysisCharTestResult();
    void saveTextEditContentToFile(QTextEdit* textEdit, const QString& filePath);
private:
    Ui::Widget *ui;
    AudioPlayer * mPlayer;
    AudioPlayer * mCharacterTestPlayer;
    AudioPlayer * ptrPlayer; //做播放切换使用
    QString strAudioOutputDevice;
    QString strAudioFileName;
    qint64 mWakeTestIntervalTime = 1000; //每次播放完成的等待下一次的时间间隔
    qint64 mWakePlayCycleCount = 0;   //循环播放唤醒词次数
    qint64 mWakePlaySum = 100;        //要求播放的次数
    qint64 mWakeDevCount = 0;         //唤醒设备的次数
    qint64 mHWakeDevCount = 0;         //唤醒设备的次数
    qint64 mLWakeDevCount = 0;         //唤醒设备的次数
    float mWakeupRate = 0.0;          //唤醒率计算
    qint64 mCharTestInterValTIme = 1000;
    qint64 mCharPlayCycleCount = 0;
    float mCharSimilarity = 0.0;  //字符相似度

    bool mAudioPlayerStop = true; //音频播放器默认为停止状态。

    std::vector<QString> audioFiles;  // 存储音频文件路径
    int currentIndex;                 // 当前播放的音频文件索引

    enum PlaybackMode {
        Single=0,         // 单曲播放，播放完一首歌后停止
        SingleLoop,     // 单曲循环，重复播放当前的单曲
        Sequential,     // 顺序播放，按播放列表的顺序播放歌曲，播放完后停止
        SequentialLoop  // 顺序循环，按播放列表顺序循环播放
    };
    PlaybackMode mPlayMode = Single;
    //bool mLoopMode;                    // 是否启用循环播放
    QProcess * mAdbProcess;
    QString mAdbPath;
    enum CmdType {
        WakeTest=0,
        CharTest
    };
    CmdType mCmdType = WakeTest;
    QStringList mWakeArgsList;       //唤醒结果读取命令
    QStringList mCharacterArgsList;  // 字准结果读取命令
    QTimer * mTimer;
    QTimer * mClickTimer;
    QTimer * mRealTimer;
    bool flag;
    QJsonObject rootObj;
    std::vector<double> mFrequencies;
    std::vector<double> mGains;
    QSettings settings;

};
#endif // WIDGET_H
