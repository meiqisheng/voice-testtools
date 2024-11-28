#ifndef LOGSYSTEMTHREAD_H
#define LOGSYSTEMTHREAD_H

#include <QThread>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QMutex>
#include <QStandardPaths>
#include <QDir>
#include <QQueue>
#include <QWaitCondition>

class LogSystemThread : public QThread
{
    Q_OBJECT

public:
    enum LogLevel {
        Info,
        Warning,
        Error
    };

    explicit LogSystemThread(QObject *parent = nullptr);
    ~LogSystemThread();

    void log(LogLevel level, const QString &message);
    void stopLogging();
    QString getLogLevelString(LogLevel level) const;

protected:
    void run() override;  // 线程执行的代码

private:
    QFile logFile;
    QTextStream logStream;
    QQueue<QString> logQueue;
    QMutex logMutex;
    bool isRunning;
    QWaitCondition cond;  // 用于线程同步

};

#endif
