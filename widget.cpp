#include "widget.h"
#include "ui_widget.h"
#include <QFileDialog>
#include <QDebug>
#include <QMessageBox>

#define APP_VERSION "1.0.6"

Widget::Widget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Widget)
    , mPlayer(new AudioPlayer(this))
    , mCharacterTestPlayer(new AudioPlayer(this))
{
    ui->setupUi(this);
    this->setWindowFlags(Qt::Widget | Qt::MSWindowsFixedSizeDialogHint);
    QString title = QString("VoiceTestTools - Version %1, build:%2-%3").arg(APP_VERSION).arg(__DATE__).arg(__TIME__);
    this->setWindowTitle(title);
    ui->VolumeSlider->setRange(0,100);
    ui->VolumeSlider->setValue(10);
    ui->VolumeSlider->setTickPosition(QSlider::TicksBelow);
    ui->VolumeSlider->setSingleStep(1);
    ui->VolumeSlider->setTickInterval(1);

    connect(ui->VolumeSlider,&QSlider::valueChanged,this, &Widget::on_VolumeSlider_ValueChanged);

    QStringList audioOutputDevices = AudioPlayer::getAvailableAudioDevices();
    if (!audioOutputDevices.isEmpty()){
        ui->AudioDevComboBox->addItems(audioOutputDevices);
        ui->AudioDevComboBox->setCurrentIndex(0);
        strAudioOutputDevice = ui->AudioDevComboBox->currentText();
    }

    QString str = QString::asprintf("%llu",mWakeTestIntervalTime);
    ui->WakeIntervalTimeEdit->setText(str);
    str = QString::asprintf("%llu",mCharTestInterValTIme);
    ui->CharTestIntervalTimeEdit->setText(str);

    currentIndex = 0; //audio file index init
    connect(ui->AudioFileBtn,&QPushButton::clicked,this, &Widget::on_AudioFileBtn_Clicked);
    connect(ui->WakeTestBtn,&QPushButton::clicked,this, &Widget::on_WakeTestBtn_Clicked);
    connect(ui->WakeTestPauseBtn,&QPushButton::clicked,this, &Widget::on_WakeTestPauseBtn_Clicked);
    connect(ui->WakeTestStopBtn,&QPushButton::clicked,this, &Widget::on_WakeTestStopBtn_Clicked);
    connect(ui->GetWakeTestResultBtn,&QPushButton::clicked,this,&Widget::on_GetWakeTestResultBtn_Clicked);
    connect(ui->AudioDevComboBox,&QComboBox::currentTextChanged, this , &Widget::on_AudioDevComboBox_CurrentTextChanged);
  //  connect(ui->PlayModeComboBox,&QComboBox::currentIndexChanged, this , &Widget::on_PlayModeComboBox_CurrentIndexChanged);
    connect(ui->PlayModeComboBox,SIGNAL(currentIndexChanged(int)),
            this, SLOT(on_PlayModeComboBox_CurrentIndexChanged(int)));
    connect(ui->TestModeComboBox,SIGNAL(currentIndexChanged(int)),
            this, SLOT(on_TestModeComboBox_CurrentIndexChanged(int)));
    connect(ui->WakeIntervalTimeEdit,&QLineEdit::editingFinished,this,&Widget::on_WakeIntervalTimeEdit_EditingFinished);
    connect(ui->CharTestIntervalTimeEdit,&QLineEdit::editingFinished,this,&Widget::on_CharTestIntervalTimeEdit_EditingFinished);

    // 唤醒测试播放器信号连接
     connect(mPlayer, &AudioPlayer::playbackFinished, this, &Widget::AudioPlayFinished);
     connect(mPlayer, &AudioPlayer::positionChanged, this, &Widget::AudioPlayPositionChanged);
     connect(mPlayer, &AudioPlayer::errorOccurred, this, &Widget::AudioPlayErrorOccurred);
   //字准测试播放器信号连接
     connect(mCharacterTestPlayer, &AudioPlayer::playbackFinished, this, &Widget::AudioPlayFinished);
     connect(mCharacterTestPlayer, &AudioPlayer::positionChanged, this, &Widget::AudioPlayPositionChanged);
     connect(mCharacterTestPlayer, &AudioPlayer::errorOccurred, this, &Widget::AudioPlayErrorOccurred);

    connect(ui->listWidget, &QListWidget::itemClicked, this, &Widget::onItemClicked);
    connect(ui->listWidget, &QListWidget::itemDoubleClicked, this, &Widget::onItemDoubleClicked);

    mAdbProcess = new QProcess(this);
    // 处理进程的标准输出
     connect(mAdbProcess, &QProcess::readyReadStandardOutput, this, &Widget::adbCmdOutput);
    // 处理进程的标准错误
     connect(mAdbProcess, &QProcess::readyReadStandardError, this, &Widget::adbCmdErrorOutput);
    // 进程完成后自动清理
    connect(mAdbProcess, SIGNAL(finished(int, QProcess::ExitStatus)),
            this, SLOT(adbCmdFinished(int, QProcess::ExitStatus)));

    mAdbPath = "adb";
    mWakeArgsList << "shell" << "cat " << "/sdcard/test.txt";
    mCharacterArgsList << "shell" << "cat " << "/sdcard/test.txt";
   // adbProcessExecude();
    mTimer = new QTimer(this);
    mClickTimer = new QTimer(this);
    connect(mTimer,&QTimer::timeout, this, &Widget::on_TimerTimeout_do);
    connect(mClickTimer,&QTimer::timeout, this, &Widget::on_ClickTimerTimeout_do);
    ui->stackedWidget->setCurrentIndex(0);
    ptrPlayer = mPlayer; //默认设置为唤醒测试播放器
}

Widget::~Widget()
{
    if (mPlayer != nullptr){
    //   mPlayer->stop();
    //   mPlayer->wait();
    //   mPlayer->quit();
        delete mPlayer;
    }
    if (mCharacterTestPlayer != nullptr){
       delete mCharacterTestPlayer;
    }
    delete mAdbProcess;
    delete mTimer;
    delete ui;
}

void Widget::on_VolumeSlider_ValueChanged(int value)
{
     qreal volumeVal = (value / 1000.00 * 2);
     qDebug() << volumeVal;
     if (ptrPlayer != nullptr){
        ptrPlayer->setVolume(volumeVal);
     }
}

void Widget::adbProcessExecude(QString &adbPath, QStringList &arguments)
{
 //   QString adbPath = "adb";
 //   QStringList arguments;
 //   arguments << "shell" << "cat " << "/sdcard/test.txt";
 //   qDebug() << "adbProcessExecude";
    if (mAdbProcess->state() == QProcess::NotRunning) {
       // mAdbProcess->();  // 清除之前的数据
        mAdbProcess->close();
        qDebug() << "mAdbProcess->start";
        mAdbProcess->start(adbPath, arguments);
    }
}

void Widget::analysisWakeTestResult()
{
    QFile file("./log/wake.log");
    // 检查文件是否可以打开
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "Failed to open file:" << "./log/wake.log";
        return;
    }
    mWakeDevCount = 0;
    QTextStream in(&file);

    // 逐行读取文件内容
    while (!in.atEnd()) {
        in.readLine();  // 读取一行
        ++mWakeDevCount;    // 行数计数
    }

    file.close();  // 关闭文件
    QString str = QString::asprintf("%llu",mWakeDevCount);
    ui->WakeDevCountEdit->setText(str);
    if (mWakePlayCycleCount == 0){
        mWakeupRate = 0;
    }else{
        mWakeupRate = mWakeDevCount *100.0 / mWakePlayCycleCount;
    }
    str = QString::asprintf("%f",mWakeupRate);
    ui->WakeupRateEdit->setText(str);
}

void Widget::analysisCharTestResult()
{

}

void Widget::adbCmdOutput()
{
    QByteArray output = mAdbProcess->readAllStandardOutput();
  //  mWakeDevCount++;
  //  QString str = QString::asprintf("%llu",mWakeDevCount);
  //  ui->WakeDevCountEdit->setText(str);
    qDebug() << "Output:" << output;
}

void Widget::adbCmdErrorOutput()
{
    QByteArray error = mAdbProcess->readAllStandardError();
    qDebug() << "Error:" << error;
}
void Widget::adbCmdFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (exitStatus == QProcess::NormalExit && exitCode == 0) {
        qDebug() << "adb cmd completed successfully";
        if (mCmdType == WakeTest){  //解析waketest获取的数据
           analysisWakeTestResult();
        }else{ //解析字准测试数据

        }
    } else {
        qDebug() << "adb cmd failed with exit code:" << exitCode;
    }
  //  mAdbProcess->deleteLater();  // 释放资源
}

bool Widget::addAudiofileToList(QString fileName)
{
    // 检查是否存在相同名称的项目
            bool exists = false;
            for (int i = 0; i < ui->listWidget->count(); ++i) {
                QListWidgetItem *item = ui->listWidget->item(i);
                if (item->text() == fileName) {
                    exists = true;
                    break;
                }
            }

            // 如果不存在相同名称的项目，则添加新项目
            if (!exists) {
                ui->listWidget->addItem(fileName);
                return true;
            } else {
                qDebug("项目已存在，不重复添加");
                return false;
            }
}

void Widget::on_ClickTimerTimeout_do()
{
    mClickTimer->stop();
    int index = ui->listWidget->currentRow();
    currentIndex = index;
    strAudioFileName = audioFiles.at(currentIndex);
    ptrPlayer->setAudioFile(strAudioFileName);
    //mPlayer->switchFile(strAudioFileName);
    qDebug() << "row:" << index  <<",path:"<< audioFiles.at(index);
}

void Widget::onItemClicked(QListWidgetItem * item)
{
    mClickTimer->start(300);
 //   int index = ui->listWidget->row(item);
 //   currentIndex = index;
//    strAudioFileName = audioFiles.at(currentIndex);
//    ptrPlayer->setAudioFile(strAudioFileName);
    //mPlayer->switchFile(strAudioFileName);
//    qDebug() << "row:" << index  <<",path:"<< audioFiles.at(index);
}

void Widget::onItemDoubleClicked(QListWidgetItem * item)
{
    mClickTimer->stop();

    int index = ui->listWidget->row(item);
    qDebug() << "row:" << index  <<",path:"<< audioFiles.at(index);
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "确认是否删除", "确定要删除吗？",
                                  QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        qDebug() << "用户选择了是";
        delete ui->listWidget->takeItem(index);
        audioFiles.erase(audioFiles.begin() + index);

        if (index < currentIndex) {
            currentIndex--;
        } else if (index == currentIndex) {
            on_WakeTestStopBtn_Clicked(); //删除的是正在播放的文件，则先停止播放
            if (ui->listWidget->count() > 0) {
                currentIndex = qMin(currentIndex, ui->listWidget->count() - 1);
                ui->listWidget->setCurrentRow(currentIndex); // 设置新的播放索引
                // 播放
                strAudioFileName = audioFiles.at(currentIndex);
                ptrPlayer->setAudioFile(strAudioFileName);
                QThread::sleep(1);//等待接收到播放完成信号在去播放
                on_WakeTestBtn_Clicked();
            } else {
                currentIndex = -1;
                on_WakeTestStopBtn_Clicked();
            }
        }else {
          //currentIndex = qMin(currentIndex, ui->listWidget->count() - 1);
        }

    } else {
        qDebug() << "用户选择了否";
    }
}

void Widget::on_AudioFileBtn_Clicked()
{
   qDebug() << "into on_AudioFileBtn_Clicked";
   // 打开文件选择对话框
   strAudioFileName = QFileDialog::getOpenFileName(
       nullptr,
       "选择音频文件",              // 对话框标题
       "/",                     // 初始目录 (根目录)
       "音频文件 (*.mp3);;所有文件 (*)"  // 文件过滤器
   );
   // 如果选择了文件，输出文件路径
   if (!strAudioFileName.isEmpty()) {
     //  qDebug() << "选择的文件:" << strAudioFileName;
       ui->AudioFilelNameEdit->setText(strAudioFileName);
       QString fileName = strAudioFileName.split("/").last();
      // ui->listWidget->addItem(fileName);
       if (addAudiofileToList(fileName)){
            audioFiles.push_back(strAudioFileName);
            currentIndex = qMin(currentIndex, ui->listWidget->count() - 1);
            ui->listWidget->setCurrentRow(currentIndex);
            strAudioFileName = audioFiles.at(currentIndex);
       }
   } else {
       qDebug() << "未选择任何文件";
   }
}


void Widget::on_WakeTestBtn_Clicked()
{
    if (strAudioFileName.isEmpty() || strAudioOutputDevice.isEmpty()){
        qDebug() << "请确认是否选择音频输出设备和音频文件"; //后续增加Qmessagebox提示
        return;
    }

    if (ptrPlayer == nullptr){
        return;
    }
    ptrPlayer->setAudioDevice(strAudioOutputDevice);
    ptrPlayer->setAudioFile(strAudioFileName);

    ptrPlayer->play();
    mAudioPlayerStop = false;

}

void Widget::on_WakeTestPauseBtn_Clicked()
{
    if (ptrPlayer == nullptr){
        return;
    }

    if (ui->WakeTestPauseBtn->text() == "暂停"){
      ptrPlayer->pause();
      ui->WakeTestPauseBtn->setText("继续");
    }else {
        ptrPlayer->play();
        ui->WakeTestPauseBtn->setText("暂停");
    }
}

void Widget::on_WakeTestStopBtn_Clicked()
{
    if (ptrPlayer != nullptr){
       ptrPlayer->stop();
    }
    mAudioPlayerStop = true;
}

void Widget::on_GetWakeTestResultBtn_Clicked()
{
    //获取唤醒测试的结果统计
    QString adbPath = "adb";
    QStringList cmdList;
    cmdList << "pull" << "/sdcard/wake.log" << "./log/";
    mCmdType = WakeTest;
    adbProcessExecude(adbPath,cmdList);
}

void Widget::on_AudioDevComboBox_CurrentTextChanged(QString deviceName)
{
    strAudioOutputDevice = deviceName;
}


void Widget::on_PlayModeComboBox_CurrentIndexChanged(int index)
{
  //  qDebug() << "index: " << index;
    mPlayMode = (PlaybackMode)index;
}

void Widget::on_TestModeComboBox_CurrentIndexChanged(int index)
{
     ui->stackedWidget->setCurrentIndex(index);
     if (index == 0){
         ptrPlayer = mCharacterTestPlayer;
     }else {
        ptrPlayer = mPlayer;
     }
}

void Widget::on_TimerTimeout_do()
{
    mTimer->stop();
    if (mPlayMode == SingleLoop) { //
        on_WakeTestBtn_Clicked();
    }else if (mPlayMode == Sequential){  //顺序播放
        currentIndex++;
        if (currentIndex >= ui->listWidget->count()){
            currentIndex--;
            return;
        }
        strAudioFileName = audioFiles.at(currentIndex);
        ui->listWidget->setCurrentRow(currentIndex);
        on_WakeTestBtn_Clicked();
    }else if (mPlayMode == SequentialLoop){ //顺序循环播放
        currentIndex = (currentIndex+1) % ui->listWidget->count();
        strAudioFileName = audioFiles.at(currentIndex);
        ui->listWidget->setCurrentRow(currentIndex);
        on_WakeTestBtn_Clicked();
    }else {}
}
void Widget::on_WakeIntervalTimeEdit_EditingFinished()
{
    QString data = ui->WakeIntervalTimeEdit->text();
    mWakeTestIntervalTime = data.toInt();
    qDebug() << "intervalTime: " << mWakeTestIntervalTime;
}

void Widget::on_CharTestIntervalTimeEdit_EditingFinished()
{
    QString data = ui->CharTestIntervalTimeEdit->text();
    mCharTestInterValTIme = data.toInt();
    qDebug() << "intervalTime: " << mCharTestInterValTIme;
}

void Widget::AudioPlayFinished()
{
    qDebug() << "Playback finished!";
    if (ptrPlayer != nullptr){
       ptrPlayer->stop();
    }
    //此处可以优化
    if (ui->TestModeComboBox->currentIndex() == 0){
       mWakePlayCycleCount++;
       QString str = QString::asprintf("%llu",mWakePlayCycleCount);
       ui->WakePlayCycleCountEdit->setText(str);
    }else{
       mCharPlayCycleCount++;
       QString str = QString::asprintf("%llu",mCharPlayCycleCount);
       ui->CharTestPalyCountEdit->setText(str);
    }
    //adbProcessExecude(mAdbPath,mWakeArgsList);  //采用异步方式处理
    if (mAudioPlayerStop == true){
        return;
    }
    //此处可以优化
    if (ui->TestModeComboBox->currentIndex() == 0){
        mTimer->start(mWakeTestIntervalTime);
    }else {
        mTimer->start(mCharTestInterValTIme);
    }
}

void Widget::AudioPlayPositionChanged(qint64 position)
{
  //  qDebug() << "Current Position: " << position << " seconds";
}

void Widget::AudioPlayErrorOccurred(const QString &error)
{
    qDebug() << "Error: " << error;
}



