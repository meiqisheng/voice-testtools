// LogSystemThread.cpp
#include "logsystemthread.h"
#include <QDateTime>
#include <QDir>
#include <QDebug>

LogSystemThread::LogSystemThread(QObject *parent)
    : QThread(parent), isRunning(true)
{
    QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    // 定义 audio_test 文件夹路径
    QString audioTestFolder = QDir(desktopPath).filePath("audio_test");
    QDir dir;
    if (!dir.exists(audioTestFolder)) {
        dir.mkpath(audioTestFolder); // 创建多级目录
    }
    QString logFolder = QDir(audioTestFolder).filePath("log");

    if (!dir.exists(logFolder)) {
        dir.mkpath(logFolder);
    }
    // 构建 log.txt 的完整路径
    QString logFilePath = QDir(logFolder).filePath("log.txt");
    logFile.setFileName(logFilePath);

    if (logFile.exists() && logFile.size() > 1024 * 100) {
        qDebug() << "Log file is too large. Resetting log file.";
        // 重命名旧日志文件，给它加个时间戳，避免覆盖
        QString backupLogFilePath = QDir(audioTestFolder).filePath(
                    QString("log_%1.txt").arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"))
                    );
        if (!logFile.rename(backupLogFilePath)) {
            qWarning() << "Failed to rename old log file.";
        } else {
            qDebug() << "Old log file renamed to: " << backupLogFilePath;
        }

        logFile.setFileName(logFilePath);
    }

    if (!logFile.open(QIODevice::Append | QIODevice::Text)) {
        qWarning() << "Failed to open log file.";
    } else {
        logStream.setDevice(&logFile);
    }

}

LogSystemThread::~LogSystemThread()
{
    isRunning = false;
    wait();  // 等待线程结束
    if (logFile.isOpen()) {
        logFile.close();
    }
}

void LogSystemThread::run()
{
    // 在此线程中处理日志的读取与写入
    while (isRunning) {
        if (!logQueue.isEmpty()) {
            QMutexLocker locker(&logMutex);  // 确保线程安全
            QString logMessage = logQueue.dequeue();  // 获取队列中的日志信息
            logStream << logMessage << endl;
            logStream.flush();
        }
        msleep(10);  // 可以让线程每次循环稍作休眠，减少CPU负载
    }
    // 退出前清理剩余的日志
    while (!logQueue.isEmpty()) {
        QMutexLocker locker(&logMutex);
        logStream << logQueue.dequeue() << endl;
        logStream.flush();
    }
}

void LogSystemThread::log(LogLevel level, const QString &message)
{
    QMutexLocker locker(&logMutex);  // 确保线程安全
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    QString logMessage = QString("[%1] [%2] %3")
            .arg(timestamp)
            .arg(getLogLevelString(level))
            .arg(message);

    logQueue.enqueue(logMessage);  // 将日志信息放入队列中
    cond.wakeOne();  // 唤醒日志处理线程
}


QString LogSystemThread::getLogLevelString(LogLevel level) const
{
    switch (level) {
    case Info: return "INFO";
    case Warning: return "WARNING";
    case Error: return "ERROR";
    default: return "UNKNOWN";
    }
}

void LogSystemThread::stopLogging()
{
    isRunning = false;
}
