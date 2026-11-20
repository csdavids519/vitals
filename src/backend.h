#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QVector>

// Samples the kernel's counters once a second and keeps a minute of history
// for each graph. The roots are arguments so tests can point it at fake
// /proc and /sys/block trees.
class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool paused READ paused NOTIFY pausedChanged)
    Q_PROPERTY(QString error READ error NOTIFY sampled)
    Q_PROPERTY(int historyLength READ historyLength CONSTANT)

    Q_PROPERTY(double cpu READ cpu NOTIFY sampled)
    Q_PROPERTY(QVariantList cpuHistory READ cpuHistory NOTIFY sampled)
    Q_PROPERTY(QVariantList cores READ cores NOTIFY sampled)
    Q_PROPERTY(QVariantList load READ load NOTIFY sampled)
    Q_PROPERTY(double uptime READ uptime NOTIFY sampled)

    Q_PROPERTY(double memUsed READ memUsed NOTIFY sampled)
    Q_PROPERTY(double memTotal READ memTotal NOTIFY sampled)
    Q_PROPERTY(double swapUsed READ swapUsed NOTIFY sampled)
    Q_PROPERTY(double swapTotal READ swapTotal NOTIFY sampled)
    Q_PROPERTY(QVariantList memHistory READ memHistory NOTIFY sampled)

    Q_PROPERTY(double diskUsed READ diskUsed NOTIFY sampled)
    Q_PROPERTY(double diskTotal READ diskTotal NOTIFY sampled)
    Q_PROPERTY(double diskRead READ diskRead NOTIFY sampled)
    Q_PROPERTY(double diskWrite READ diskWrite NOTIFY sampled)
    Q_PROPERTY(QVariantList diskReadHistory READ diskReadHistory NOTIFY sampled)
    Q_PROPERTY(QVariantList diskWriteHistory READ diskWriteHistory NOTIFY sampled)

    Q_PROPERTY(double netRx READ netRx NOTIFY sampled)
    Q_PROPERTY(double netTx READ netTx NOTIFY sampled)
    Q_PROPERTY(QVariantList rxHistory READ rxHistory NOTIFY sampled)
    Q_PROPERTY(QVariantList txHistory READ txHistory NOTIFY sampled)

public:
    explicit Backend(const QString &procRoot = "/proc", const QString &blockRoot = "/sys/block",
                     const QString &diskPath = "/", bool autostart = true, QObject *parent = nullptr);

    static constexpr int kHistory = 60;

    bool paused() const { return m_paused; }
    QString error() const { return m_errors.join('\n'); }
    int historyLength() const { return kHistory; }

    double cpu() const { return m_cpu; }
    QVariantList cpuHistory() const { return list(m_cpuHistory); }
    QVariantList cores() const { return list(m_cores); }
    QVariantList load() const { return list(m_load); }
    double uptime() const { return m_uptime; }

    double memUsed() const { return m_memUsed; }
    double memTotal() const { return m_memTotal; }
    double swapUsed() const { return m_swapUsed; }
    double swapTotal() const { return m_swapTotal; }
    QVariantList memHistory() const { return list(m_memHistory); }

    double diskUsed() const { return m_diskUsed; }
    double diskTotal() const { return m_diskTotal; }
    double diskRead() const { return m_diskRead; }
    double diskWrite() const { return m_diskWrite; }
    QVariantList diskReadHistory() const { return list(m_diskReadHistory); }
    QVariantList diskWriteHistory() const { return list(m_diskWriteHistory); }

    double netRx() const { return m_netRx; }
    double netTx() const { return m_netTx; }
    QVariantList rxHistory() const { return list(m_rxHistory); }
    QVariantList txHistory() const { return list(m_txHistory); }

    // Space: freeze the graphs to read them, then pick up live again.
    Q_INVOKABLE void togglePause();

    // Reads every counter once. Rates divide by the time since the previous
    // sample, which tests pass in so they don't depend on the clock.
    void sample(double elapsedSeconds = -1);

signals:
    void sampled();
    void pausedChanged();

private:
    struct CpuTimes { quint64 busy = 0, total = 0; };

    static QVariantList list(const QVector<double> &values);
    static void push(QVector<double> &history, double value);
    QByteArray readProc(const QString &name);

    void sampleCpu(bool rates);
    void sampleMemory();
    void sampleDisk(bool rates, double elapsed);
    void sampleNetwork(bool rates, double elapsed);

    QString m_procRoot, m_blockRoot, m_diskPath;
    QTimer m_timer;
    QElapsedTimer m_clock;
    bool m_paused = false;
    bool m_havePrevious = false;
    QStringList m_errors;

    QVector<CpuTimes> m_previousCpu;  // index 0 is the total, then each core
    double m_cpu = 0, m_uptime = 0;
    QVector<double> m_cpuHistory, m_cores, m_load;

    double m_memUsed = 0, m_memTotal = 0, m_swapUsed = 0, m_swapTotal = 0;
    QVector<double> m_memHistory;

    quint64 m_previousRead = 0, m_previousWritten = 0;
    double m_diskUsed = 0, m_diskTotal = 0, m_diskRead = 0, m_diskWrite = 0;
    QVector<double> m_diskReadHistory, m_diskWriteHistory;

    quint64 m_previousRx = 0, m_previousTx = 0;
    double m_netRx = 0, m_netTx = 0;
    QVector<double> m_rxHistory, m_txHistory;
};
