#include "widget.h"
#include "ui_widget.h"
#include <QFileDialog>
#include <QDebug>
#include <QMessageBox>
#include <QDateTime>
#include <QBuffer>
#include <QFile>
#include <QStringList>
#pragma execution_character_set("utf-8")


#define APP_VERSION "1.0.30"

Widget::Widget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Widget)
    , mPlayer(new AudioPlayer(this))
    , mCharacterTestPlayer(new AudioPlayer(this))
    , settings("sunnyverse", "VoiceTestTools")
{
    ui->setupUi(this);
    QString eqJsonPath = QCoreApplication::applicationDirPath() + "/json/EQ.json";
    loadEQPresets(ui->EQcomboBox, eqJsonPath); // 你可以换成实际路径
    loadAudioPathFromJson();
    this->setWindowFlags(Qt::Widget | Qt::MSWindowsFixedSizeDialogHint);
    QString title = QString("VoiceTestTools - Version %1, build:%2-%3").arg(APP_VERSION).arg(__DATE__).arg(__TIME__);
    this->setWindowTitle(title);
    QFont font("Microsoft YaHei", 12);
    ui->textEdit->setFont(font);
    ui->SetPlaySumEdit->setText(QString("%1").arg(mWakePlaySum));

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
    // widget.cpp
    connect(ui->SaveBox, &QCheckBox::toggled, this, [=](bool checked) {
        flag = checked;
        emit saveFlagChanged(flag);
        qDebug() << "widget, flag = " << flag;
    });
    connect(ui->EQcomboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &Widget::on_EQcomboBox_currentTextChanged);
    connect(this, &Widget::saveFlagChanged, mPlayer, &AudioPlayer::onSaveFlagChanged);
    connect(ui->LoadJSON, &QAbstractButton::clicked, this, [=]() {
        this->on_LoadJSON_clicked(ui->EQcomboBox, eqJsonPath);
    });
    connect(ui->WakeCount,&QPushButton::clicked,this, &Widget::on_pushButton_3_clicked);
    connect(ui->AudioFileBtn,&QPushButton::clicked,this, &Widget::on_AudioFileBtn_Clicked);
    connect(ui->WakeTestBtn,&QPushButton::clicked,this, &Widget::on_WakeTestBtn_Clicked);
    connect(ui->WakeTestPauseBtn,&QPushButton::clicked,this, &Widget::on_WakeTestPauseBtn_Clicked);
    connect(ui->WakeTestStopBtn,&QPushButton::clicked,this, &Widget::on_WakeTestStopBtn_Clicked);
    connect(ui->GetWakeTestResultBtn,&QPushButton::clicked,this,&Widget::on_GetWakeTestResultBtn_Clicked);
    connect(ui->GetCharTestDataBtn,&QPushButton::clicked,this,&Widget::on_GetCharTestDataBtn_Clicked);
    connect(ui->SetPlaySumBtn,&QPushButton::clicked,this,&Widget::on_SetPlaySumBtn_Clicked);
    connect(ui->ClearTextEditBtn,&QPushButton::clicked,this,&Widget::on_ClearTextEditBtn_Clicked);
    connect(ui->CalcDelayTimeBtn,&QPushButton::clicked,this,&Widget::on_CalcDelayTimeBtn_Clicked);
    connect(ui->SaveLogFileBtn,&QPushButton::clicked,this,&Widget::on_SaveLogFileBtn_Clicked);
    connect(ui->ClearDevWakeRecordBtn,&QPushButton::clicked,this,&Widget::on_ClearDevWakeRecordBtn_Clicked);
    connect(ui->DisplayDateTimeBtn,&QPushButton::clicked,this,&Widget::on_DisplayDateTimeBtn_Clicked);

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
    mRealTimer = new QTimer(this);
    connect(mRealTimer,&QTimer::timeout, this, &Widget::on_UpdateTime_do);
    //mRealTimer->start(17);
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
   // mAdbProcess->deleteLater();  // 释放资源
    delete mAdbProcess;
    delete mTimer;
    delete mClickTimer;
    delete mRealTimer;
    delete ui;
}

void Widget::on_VolumeSlider_ValueChanged(int value)
{
     qreal volumeVal = (value / 100.0);
     qDebug() << volumeVal;
     if (ptrPlayer != nullptr){
        ptrPlayer->setVolume(volumeVal);
     }
}

void Widget::adbProcessExecude(QString &adbPath, QStringList &arguments)
{
    if (mAdbProcess->state() == QProcess::NotRunning) {
        mAdbProcess->close();
        qDebug() << "mAdbProcess->start";
        mAdbProcess->start(adbPath, arguments);
    }
}

void Widget::analysisWakeTestResult()
{
    QFile file("./log/wake.log");
    //qDebug() << "Current working dir:" << QDir::currentPath();
    // 检查文件是否可以打开
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "Failed to open file:" << "./log/wake.log";
        return;
    }
    mWakeDevCount = 0;

    QTextStream in(&file);
    QString line = "";
    // 逐行读取文件内容
    while (!in.atEnd()) {
     line = in.readLine();  // 读取一行
     ++mWakeDevCount;    // 行数计数
    }
    ui->WakeFinishTimeEdit->setText(line.mid(5));
    file.close();  // 关闭文件

    QString str = QString::asprintf("%llu",mWakeDevCount);
    ui->WakeDevCountEdit->setText(str);
    qint64 wakePlayCycleCount = ui->WakePlayCycleCountEdit->text().toInt();
    if (wakePlayCycleCount == 0){
        mWakeupRate = 0;
    }else{
        mWakeupRate = mWakeDevCount *100.0 / wakePlayCycleCount;
    }
    str = QString::asprintf("%f",mWakeupRate);
    ui->WakeupRateEdit->setText(str);
}

void Widget::analysisCharTestResult()
{
     qDebug() << "analysisCharTestResult";
}

void Widget::adbCmdOutput()
{
    QByteArray output = mAdbProcess->readAllStandardOutput();
  //  mWakeDevCount++;
  //  QString str = QString::asprintf("%llu",mWakeDevCount);
  //  ui->WakeDevCountEdit->setText(str);
    qDebug() << "Output:" << output;
    QString str = "Output:" + output;
    ui->textEdit->append(str);
}

void Widget::adbCmdErrorOutput()
{
    QByteArray error = mAdbProcess->readAllStandardError();
    qDebug() << "Error:" << error;
    QString str = "Error:" + error;
    //ui->textEdit->append(str);
    // 判断是否是 adb 的正常提示（不是错误）
    if (str.contains("file pulled") || str.contains("skipped") || str.contains("bytes")) {
        ui->textEdit->append("<font color='blue'>提示信息：" + str + "</font>");
    } else {
        ui->textEdit->append("<font color='red'>错误信息：" + str + "</font>");
    }
}
void Widget::adbCmdFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (exitStatus == QProcess::NormalExit && exitCode == 0) {
        qDebug() << "adb cmd completed successfully";
        ui->textEdit->append("adb cmd completed successfully");
        if (mCmdType == WakeTest){  //解析waketest获取的数据
           analysisWakeTestResult();
        }else{ //解析字准测试数据
           analysisCharTestResult();
        }
    } else {
        qDebug() << "adb cmd failed with exit code:" << exitCode;
    }
   // mAdbProcess->deleteLater();  // 释放资源
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
                qDebug("music is existed, cannot add the item");
                return false;
            }
}

void Widget::on_UpdateTime_do()
{
     QString dateTime = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
     ui->UpdateTimeLabel->setText(dateTime);
}

void Widget::on_ClickTimerTimeout_do()
{
    mClickTimer->stop();
    int index = ui->listWidget->currentRow();
    currentIndex = index;
    strAudioFileName = audioFiles.at(currentIndex);
    ptrPlayer->setAudioFile(strAudioFileName);
    qDebug() << "row:" << index  <<",path:"<< audioFiles.at(index);
}

void Widget::onItemClicked(QListWidgetItem * item)
{
    //实现单击与双击区别开
    mClickTimer->start(300);
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
   QString lastDir = settings.value("lastAudioDir", QCoreApplication::applicationDirPath()).toString();
   strAudioFileName = QFileDialog::getOpenFileName(
       nullptr,
       "选择音频文件",              // 对话框标题
       lastDir,                     // 初始目录 (根目录)
       "音频文件 (*.mp3 *.wav);;所有文件 (*)"  // 文件过滤器
   );
   // 如果选择了文件，输出文件路径
   if (!strAudioFileName.isEmpty()) {
       saveAudioPathToJson(strAudioFileName);
       QFileInfo fileInfo(strAudioFileName);
       settings.setValue("lastAudioDir", fileInfo.absolutePath());
       qDebug() << "settings dir:" << fileInfo.absolutePath();
       ui->AudioFilelNameEdit->setText(strAudioFileName);
       QString fileName = strAudioFileName.split("/").last();
      // ui->listWidget->addItem(fileName);
       if (addAudiofileToList(fileName)){
            audioFiles.push_back(strAudioFileName);
            if (currentIndex < 0){
                currentIndex = 0;
            }else {
                currentIndex = qMin(currentIndex, ui->listWidget->count() - 1);
            }
            ui->listWidget->setCurrentRow(currentIndex);
            strAudioFileName = audioFiles.at(currentIndex);
       }
   } else {
       qDebug() << "no select";
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
    QString str = QString::asprintf("%llu",mWakePlayCycleCount);
    ui->WakePlayCycleCountEdit->setText(str);
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
        if (mAudioPlayerStop != true){
            ptrPlayer->play();
        }
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
    //analysisWakeTestResult();
    //获取唤醒测试的结果统计
    QString adbPath = "adb";
    QStringList cmdList;
    cmdList << "pull" << "/sdcard/wake.log" << "./log/";
    mCmdType = WakeTest;
    adbProcessExecude(adbPath,cmdList);
}

void Widget::on_GetCharTestDataBtn_Clicked()
{   //获取字准测试数据文档
    QString adbPath = "adb";
    QStringList cmdList;
    cmdList << "pull" << "/sdcard/wake.log" << "./log/";
    mCmdType = CharTest;
    adbProcessExecude(adbPath,cmdList);
}

void Widget::on_SetPlaySumBtn_Clicked()
{
    QString str = ui->SetPlaySumEdit->text();
    mWakePlaySum = str.toInt(); //设置播放次数
    ui->textEdit->append("<font color='green'>[完成] 已设置播放总次数为:" + str + "次。</font>");
}

void Widget::on_ClearTextEditBtn_Clicked()
{
     ui->textEdit->clear();
}

void Widget::on_CalcDelayTimeBtn_Clicked()
{
    QDateTime palyDateTime = QDateTime::fromString(ui->WakePlayFinishTimeEdit->text().trimmed(), "yyyy-MM-dd HH:mm:ss.zzz");
    QDateTime wakeDevDateTime = QDateTime::fromString(ui->WakeFinishTimeEdit->text().trimmed(),"yyyy-MM-dd HH:mm:ss:zzz");
    if (!palyDateTime.isValid() || !wakeDevDateTime.isValid()) {
        ui->textEdit->append("Invalid QDateTime!");
        return;
    }

    qint64 timeCompensation = ui->TimeCompensationEdit->text().toInt();
    qint64 millisDiff = palyDateTime.msecsTo(wakeDevDateTime);
    millisDiff += timeCompensation;
    ui->WakeDelayTimeEdit->setText(QString("%1").arg(millisDiff));
}

void Widget::on_ClearDevWakeRecordBtn_Clicked()
{
    QString adbPath = "adb";
    QStringList cmdList;
    cmdList << "shell" << "rm" << "/sdcard/wake.log";
    mCmdType = CharTest;
    adbProcessExecude(adbPath,cmdList);
}

void Widget::on_DisplayDateTimeBtn_Clicked()
{
     if (ui->DisplayDateTimeBtn->text() == "显示时间"){
         mRealTimer->start(40);
         ui->DisplayDateTimeBtn->setText("停止显示");
     }else {
         mRealTimer->stop();
         ui->DisplayDateTimeBtn->setText("显示时间");
     }
}

void Widget::saveTextEditContentToFile(QTextEdit *textEdit, const QString &filePath)
{
    // 获取 QTextEdit 的内容
       QString content = textEdit->toPlainText(); // 获取纯文本内容
       // QString content = textEdit->toHtml(); // 如果需要保存 HTML 格式

       // 打开文件
       QFile file(filePath);
       if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
           qDebug() << "无法打开文件:" << filePath;
           return;
       }

       // 写入文件
       QTextStream out(&file);
       out << content;

       // 关闭文件
       file.close();
       qDebug() << "文本已保存到:" << filePath;
}

void Widget::on_SaveLogFileBtn_Clicked()
{
     QString filename = QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss");
     filename = "./log/" + filename + ".log";
     saveTextEditContentToFile(ui->textEdit,filename);
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
    if (mAudioPlayerStop == true){
        mWakePlayCycleCount = 0;
        return;
    }
    if (mPlayMode == SingleLoop && mWakePlayCycleCount < mWakePlaySum ) { //
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
    }else if (mPlayMode == SequentialLoop && mWakePlayCycleCount < mWakePlaySum){ //顺序循环播放
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
    if (ui->TestModeComboBox->currentIndex() == 0){ //唤醒测试播放完成

       mWakePlayCycleCount++;
       QDateTime currentDateTime = QDateTime::currentDateTime();
       QString userTest = "当前播放次数:" + QString::number(mWakePlayCycleCount)
                          +" 当前日期时间:" + currentDateTime.toString("yyyy-MM-dd HH:mm:ss.zzz");
       ui->textEdit->append(userTest);
       QString str = QString::asprintf("%llu",mWakePlayCycleCount);
       ui->WakePlayCycleCountEdit->setText(str);
       ui->WakePlayFinishTimeEdit->setText(currentDateTime.toString("yyyy-MM-dd HH:mm:ss.zzz"));
    }else{ //字准测试播放完成
       mCharPlayCycleCount++;
       QString str = QString::asprintf("%llu",mCharPlayCycleCount);
       ui->CharTestPalyCountEdit->setText(str);
    }
    //adbProcessExecude(mAdbPath,mWakeArgsList);  //采用异步方式处理
    if (mAudioPlayerStop == true || mWakePlayCycleCount >= mWakePlaySum ){
        mWakePlayCycleCount = 0;
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
    qDebug() << "Current Position: " << position << " seconds";
}

void Widget::AudioPlayErrorOccurred(const QString &error)
{
    qDebug() << "Error: " << error;
}

void Widget::on_AudioFileBulkBtn_clicked()
{
    qDebug().noquote() << QStringLiteral("into on_AudioFileBulkBtn_clicked");
    QString lastDir = settings.value("lastAudioDir", QCoreApplication::applicationDirPath()).toString();
    QStringList files = QFileDialog::getOpenFileNames(
        this,
        QStringLiteral("选择多个音频文件"),
        lastDir,
        "音频文件 (*.mp3 *.wav);;所有文件 (*)"
    );
    if (files.isEmpty()) {
        qDebug().noquote() << QStringLiteral("no select");
        return;
    }

    // 保存第一个音频文件所在目录
    QFileInfo firstFileInfo(files.first());
    QString dirPath = firstFileInfo.absolutePath();

    // 写入 JSON 和 settings
    saveAudioPathToJson(files.first());
    settings.setValue("lastAudioDir", dirPath);

    for (const QString &filePath : files) {
        QString fileName = QFileInfo(filePath).fileName();

        // 加入 listWidget 并避免重复
        if (addAudiofileToList(fileName)) {
            audioFiles.push_back(filePath);
        }
    }

    // 自动选中第一个添加的文件
    if (currentIndex < 0 && !audioFiles.empty()) {
        currentIndex = 0;
        ui->listWidget->setCurrentRow(currentIndex);
        strAudioFileName = audioFiles.at(currentIndex);
    } else {
        // 保持当前索引有效
        currentIndex = qMin(currentIndex, ui->listWidget->count() - 1);
        ui->listWidget->setCurrentRow(currentIndex);
        strAudioFileName = audioFiles.at(currentIndex);
    }
    ui->AudioFilelNameEdit->setText(strAudioFileName);
}

void Widget::loadEQPresets(QComboBox* EQcomboBox, const QString& filePath)
{
    QFile file(filePath);
    qDebug() << "current folder:" << QDir::currentPath();
    QString currentfolder = "current folder:" + QDir::currentPath();
    ui->textEdit->append(currentfolder);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Cannot open EQ json file:" << filePath;
        ui->textEdit->append("<font color='red'>[错误] 未找到EQ预设文件，请确认文件路径是否正确。</font>");
        mFrequencies.clear();
        mGains.clear();
        return;
    }

    QByteArray jsonData = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(jsonData, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "解析 EQ 预设文件出错:" << parseError.errorString();
        ui->textEdit->append("<font color='red'>[错误] EQ预设文件格式错误，无法解析。</font>");
        mFrequencies.clear();
        mGains.clear();
        return;
    }

    if (!doc.isObject()) {
        qWarning() << "EQ 预设 JSON 文件格式错误，根不是对象";
        ui->textEdit->append("<font color='red'>[错误] EQ预设文件格式错误，根元素应为对象。</font>");
        mFrequencies.clear();
        mGains.clear();
        return;
    }

    rootObj = doc.object();
    for (const QString& key : rootObj.keys()) {
        EQcomboBox->addItem(key);
    }

    // 默认选中第一个预设
    if (EQcomboBox->count() > 0) {
        QString defaultPreset = EQcomboBox->itemText(0);
        EQcomboBox->setCurrentIndex(0);

        QJsonObject presetObj = rootObj.value(defaultPreset).toObject();
        QJsonArray freqArray = presetObj["frequencies"].toArray();
        QJsonArray gainArray = presetObj["gains"].toArray();

        mFrequencies.clear();
        mGains.clear();

        QString html = QString("默认预设: <span style='color:red;'>%1</span><br>").arg(defaultPreset);
        html += "<table border='1' cellspacing='0' cellpadding='2'><tr><th>频率 (Hz)</th><th>增益 (dB)</th></tr>";

        for (int i = 0; i < freqArray.size(); ++i) {
            double freq = freqArray[i].toDouble();
            double gain = gainArray[i].toDouble();
            mFrequencies.push_back(freq);
            mGains.push_back(gain);
            html += QString("<tr><td>%1</td><td>%2</td></tr>").arg(freq).arg(gain, 0, 'f', 2);
        }

        html += "</table>";
        ui->textEdit->append(html);

        if (mPlayer)
            mPlayer->setEQData(mFrequencies, mGains);
    }
}

void Widget::on_EQcomboBox_currentTextChanged(int index)
{
    if (index < 0 || index >= ui->EQcomboBox->count())
        return;

    QString presetName = ui->EQcomboBox->itemText(index);
    if (!rootObj.contains(presetName))
        return;

    QJsonObject presetObj = rootObj[presetName].toObject();
    QJsonArray freqArray = presetObj["frequencies"].toArray();
    QJsonArray gainArray = presetObj["gains"].toArray();

    mFrequencies.clear();
    mGains.clear();

    for (const auto& v : freqArray)
        mFrequencies.push_back(v.toDouble());

    for (const auto& v : gainArray)
        mGains.push_back(v.toDouble());

    QString html = QString("当前预设: <span style='color:red;'>%1</span><br>").arg(presetName);
    html += "<table border='1' cellspacing='0' cellpadding='2'><tr><th>频率 (Hz)</th><th>增益 (dB)</th></tr>";

    for (int i = 0; i < freqArray.size(); ++i) {
        double freq = freqArray[i].toDouble();
        double gain = gainArray[i].toDouble();
        html += QString("<tr><td>%1</td><td>%2</td></tr>").arg(freq).arg(gain, 0, 'f', 2);
    }

    html += "</table>";
    ui->textEdit->append(html);
    // ✅ 将 EQ 数据传给 AudioPlayer
    if (mPlayer)
        mPlayer->setEQData(mFrequencies, mGains);
}


void Widget::on_LoadJSON_clicked(QComboBox* EQcomboBox, const QString filePath)
{
    QFile file(filePath);
    qDebug() << "current folder:" << QDir::currentPath();
    QString currentfolder = "current folder:" + QDir::currentPath();
    ui->textEdit->append(currentfolder);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Cannot open EQ json file:" << filePath;
        ui->textEdit->append("<font color='red'>[错误] 未找到EQ预设文件，请确认文件路径是否正确。</font>");
        mFrequencies.clear();
        mGains.clear();
        return;
    }

    QByteArray jsonData = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(jsonData, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "解析 EQ 预设文件出错:" << parseError.errorString();
        ui->textEdit->append("<font color='red'>[错误] EQ预设文件格式错误，无法解析。</font>");
        mFrequencies.clear();
        mGains.clear();
        return;
    }

    if (!doc.isObject()) {
        qWarning() << "EQ 预设 JSON 文件格式错误，根不是对象";
        ui->textEdit->append("<font color='red'>[错误] EQ预设文件格式错误，根元素应为对象。</font>");
        mFrequencies.clear();
        mGains.clear();
        return;
    }

    rootObj = doc.object();
    EQcomboBox->clear();
    for (const QString& key : rootObj.keys()) {
        EQcomboBox->addItem(key);
    }

    // 默认选中第一个预设
    if (EQcomboBox->count() > 0) {
        QString defaultPreset = EQcomboBox->itemText(0);
        EQcomboBox->setCurrentIndex(0);

        QJsonObject presetObj = rootObj.value(defaultPreset).toObject();
        QJsonArray freqArray = presetObj["frequencies"].toArray();
        QJsonArray gainArray = presetObj["gains"].toArray();

        mFrequencies.clear();
        mGains.clear();

        QString html = QString("默认预设: <span style='color:red;'>%1</span><br>").arg(defaultPreset);
        html += "<table border='1' cellspacing='0' cellpadding='2'><tr><th>频率 (Hz)</th><th>增益 (dB)</th></tr>";

        for (int i = 0; i < freqArray.size(); ++i) {
            double freq = freqArray[i].toDouble();
            double gain = gainArray[i].toDouble();
            mFrequencies.push_back(freq);
            mGains.push_back(gain);
            html += QString("<tr><td>%1</td><td>%2</td></tr>").arg(freq).arg(gain, 0, 'f', 2);
        }

        html += "</table>";
        ui->textEdit->append(html);

        if (mPlayer)
            mPlayer->setEQData(mFrequencies, mGains);
    }
}

void Widget::on_ClearList_clicked()
{
    qDebug().noquote() << QStringLiteral("Clear the music playlist");

    // 清空 listWidget
    ui->listWidget->clear();

    // 清空保存路径的容器
    audioFiles.clear();

    // 重置当前索引和文件名
    currentIndex = -1;
    strAudioFileName.clear();

    // 清空文件名显示
    ui->AudioFilelNameEdit->clear();
}

void Widget::loadAudioPathFromJson()
{
    QString configPath = QCoreApplication::applicationDirPath() + "/json/audio_config.json";
    QFile configFile(configPath);

    if (!configFile.exists()) {
        qDebug() << "JSON config not found.";
        ui->textEdit->append("<font color='red'>[错误] 未找到JSON config，请确认文件路径是否正确。</font>");
        settings.setValue("lastAudioDir", QCoreApplication::applicationDirPath());
        return;
    }

    if (!configFile.open(QIODevice::ReadOnly)) {
        qWarning() << "Unable to open JSON config file.";
        ui->textEdit->append("<font color='red'>[错误] 无法打开JSON config，请确认文件是否可读。</font>");
        settings.setValue("lastAudioDir", QCoreApplication::applicationDirPath());
        return;
    }

    QByteArray data = configFile.readAll();
    configFile.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        qWarning() << "JSON parse error:" << parseError.errorString();
        ui->textEdit->append("<font color='red'>[错误] 无法解析JSON config，请确认文件格式是否正确。</font>");
        settings.setValue("lastAudioDir", QCoreApplication::applicationDirPath());
        return;
    }

    QString path = doc.object().value("last_audio_dir").toString();
    if (!QDir(path).exists()) {
        path = QCoreApplication::applicationDirPath();
    }

    settings.setValue("lastAudioDir", path);
}

void Widget::saveAudioPathToJson(const QString& filePath)
{
    QFileInfo fileInfo(filePath);
    QString dirPath = fileInfo.absolutePath();
    if (!QDir(dirPath).exists()) {
        dirPath = QCoreApplication::applicationDirPath();
    }

    QJsonObject jsonObj;
    jsonObj["last_audio_dir"] = dirPath;

    QJsonDocument doc(jsonObj);

    QString jsonDirPath = QCoreApplication::applicationDirPath() + "/json";
    QDir jsonDir(jsonDirPath);
    if (!jsonDir.exists()) {
        if (!jsonDir.mkpath(".")) {
            qCritical() << "can not create JSON dir:" << jsonDirPath;
            return;
        }
    }

    QString configPath = jsonDirPath + "/audio_config.json";
    QFile configFile(configPath);
    if (!configFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qCritical() << "error open JSON file write:" << configPath << "error:" << configFile.errorString();
        return;
    }

    qint64 written = configFile.write(doc.toJson(QJsonDocument::Indented));
    configFile.close();

    if (written <= 0) {
        qCritical() << "fail save JSON:" << configPath;
    } else {
        qDebug() << "successful save JSON:" << dirPath;
    }
}

void Widget::on_pushButton_clicked()
{
    ui->AudioDevComboBox->clear();
    QStringList audioOutputDevices = AudioPlayer::getAvailableAudioDevices();
    if (!audioOutputDevices.isEmpty()){
        ui->AudioDevComboBox->addItems(audioOutputDevices);
        ui->AudioDevComboBox->setCurrentIndex(0);
        strAudioOutputDevice = ui->AudioDevComboBox->currentText();
    }
}


void Widget::on_pushButton_3_clicked()
{
    QString logPath = QCoreApplication::applicationDirPath() + "/log/wake.log";
    QFile file(logPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "Failed to open file:" << logPath;
        return;
    }

    QTextStream in(&file);
    in.setCodec("UTF-8");  // 强制使用 UTF-8 编码
    QString line;
    mWakeDevCount = 0;

    // 使用 <唤醒词, 通道> 作为 key，统计次数
    QMap<QString, int> wakeWordCountMap;

    while (!in.atEnd()) {
        line = in.readLine().trimmed();
        if (line.isEmpty()) continue;

        QStringList parts = line.split(' ', QString::SkipEmptyParts);
        if (parts.size() >= 5 && parts[0] == "wake") {
            QString channel = parts[3];     // 通道，如 N
            QString word = parts[4];        // 唤醒词，如 小溪小溪

            QString key = QString("%1-%2").arg(word).arg(channel);
            wakeWordCountMap[key]++;
            mWakeDevCount++;
        }
    }

    file.close();

    // 颜色列表（支持 10 个不同颜色，超出会重复使用）
    QStringList colorList = {
        "red", "blue", "green", "orange", "purple",
        "teal", "brown", "magenta", "navy", "darkcyan"
    };

    QStringList summaryList;
    int colorIndex = 0;
    for (auto it = wakeWordCountMap.constBegin(); it != wakeWordCountMap.constEnd(); ++it, ++colorIndex) {
        QString color = colorList[colorIndex % colorList.size()];
        QString coloredItem = QString("<span style='color:%1'>%2-%3</span>")
                              .arg(color)
                              .arg(it.key())
                              .arg(it.value());
        summaryList << coloredItem;
    }
    ui->textEdit->setText(summaryList.join("；"));
}

