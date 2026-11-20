#include "backend.h"

#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <sys/statvfs.h>

Backend::Backend(const QString &procRoot, const QString &blockRoot, const QString &diskPath, bool autostart,
                 QObject *parent)
    : QObject(parent), m_procRoot(procRoot), m_blockRoot(blockRoot), m_diskPath(diskPath) {
    m_timer.setInterval(1000);
    connect(&m_timer, &QTimer::timeout, this, [this] { sample(); });
    if (autostart) {
        sample();
        m_timer.start();
    }
}

QVariantList Backend::list(const QVector<double> &values) {
    QVariantList out;
    out.reserve(values.size());
    for (double v : values)
        out << v;
    return out;
}

void Backend::push(QVector<double> &history, double value) {
    history << value;
    if (history.size() > kHistory)
        history.remove(0, history.size() - kHistory);
}

QByteArray Backend::readProc(const QString &name) {
    QFile file(m_procRoot + "/" + name);
    if (!file.open(QIODevice::ReadOnly)) {
        m_errors << tr("Can't read %1").arg(file.fileName());
        return {};
    }
    return file.readAll();
}

void Backend::togglePause() {
    m_paused = !m_paused;
    if (m_paused) {
        m_timer.stop();
    } else {
        // A rate averaged over the whole pause would put a misleading point on
        // the graph, so start counting again from now.
        m_havePrevious = false;
        sample();
        m_timer.start();
    }
    emit pausedChanged();
}

void Backend::sample(double elapsedSeconds) {
    double elapsed = elapsedSeconds;
    if (!m_clock.isValid()) {
        m_clock.start();
        elapsed = qMax(0.0, elapsed);
    } else {
        const double measured = m_clock.restart() / 1000.0;
        if (elapsed < 0)
            elapsed = measured;
    }
    const bool rates = m_havePrevious && elapsed > 0;

    m_errors.clear();
    sampleCpu(rates);
    sampleMemory();
    sampleDisk(rates, elapsed);
    sampleNetwork(rates, elapsed);
    m_havePrevious = true;
    emit sampled();
}

void Backend::sampleCpu(bool rates) {
    // Each "cpu" line counts jiffies by kind; idle and iowait are the idle ones.
    QVector<CpuTimes> current;
    const auto lines = readProc("stat").split('\n');
    for (const auto &line : lines) {
        if (!line.startsWith("cpu"))
            continue;
        const auto fields = line.simplified().split(' ');
        CpuTimes t;
        for (int i = 1; i < fields.size() && i <= 8; ++i)
            t.total += fields[i].toULongLong();
        const quint64 idle = (fields.size() > 4 ? fields[4].toULongLong() : 0)
                           + (fields.size() > 5 ? fields[5].toULongLong() : 0);
        t.busy = t.total - idle;
        current << t;
    }

    if (rates && current.size() == m_previousCpu.size() && !current.isEmpty()) {
        auto percent = [](const CpuTimes &now, const CpuTimes &before) {
            const double total = double(now.total - before.total);
            return total > 0 ? qBound(0.0, 100.0 * double(now.busy - before.busy) / total, 100.0) : 0.0;
        };
        m_cpu = percent(current[0], m_previousCpu[0]);
        m_cores.clear();
        for (int i = 1; i < current.size(); ++i)
            m_cores << percent(current[i], m_previousCpu[i]);
        push(m_cpuHistory, m_cpu);
    }
    m_previousCpu = current;

    const auto load = QString::fromUtf8(readProc("loadavg")).split(' ');
    m_load.clear();
    for (int i = 0; i < 3 && i < load.size(); ++i)
        m_load << load[i].toDouble();
    m_uptime = QString::fromUtf8(readProc("uptime")).section(' ', 0, 0).toDouble();
}

void Backend::sampleMemory() {
    QHash<QString, double> info;
    for (const auto &line : QString::fromUtf8(readProc("meminfo")).split('\n')) {
        const auto key = line.section(':', 0, 0);
        const auto kib = line.section(':', 1).simplified().section(' ', 0, 0).toDouble();
        info.insert(key, kib * 1024);
    }
    m_memTotal = info.value("MemTotal");
    m_memUsed = qMax(0.0, m_memTotal - info.value("MemAvailable"));
    m_swapTotal = info.value("SwapTotal");
    m_swapUsed = qMax(0.0, m_swapTotal - info.value("SwapFree"));
    if (m_memTotal > 0)
        push(m_memHistory, 100.0 * m_memUsed / m_memTotal);
}

void Backend::sampleDisk(bool rates, double elapsed) {
    struct statvfs fs;
    if (statvfs(QFile::encodeName(m_diskPath).constData(), &fs) == 0) {
        m_diskTotal = double(fs.f_blocks) * fs.f_frsize;
        m_diskUsed = double(fs.f_blocks - fs.f_bfree) * fs.f_frsize;
    } else {
        m_errors << tr("Can't read %1").arg(m_diskPath);
    }

    // Count only physical disks: partitions would count the same bytes twice,
    // and so would device-mapper and RAID devices stacked on top of them.
    const QStringList disks = QDir(m_blockRoot).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    static const QRegularExpression virtualDisk("^(loop|ram|zram|dm-|md|sr|fd)");
    quint64 read = 0, written = 0;
    for (const auto &line : QString::fromUtf8(readProc("diskstats")).split('\n')) {
        const auto fields = line.simplified().split(' ');
        if (fields.size() < 10 || !disks.contains(fields[2]) || virtualDisk.match(fields[2]).hasMatch())
            continue;
        // diskstats counts 512-byte sectors whatever the device's block size.
        read += fields[5].toULongLong() * 512;
        written += fields[9].toULongLong() * 512;
    }
    if (rates) {
        m_diskRead = read >= m_previousRead ? (read - m_previousRead) / elapsed : 0;
        m_diskWrite = written >= m_previousWritten ? (written - m_previousWritten) / elapsed : 0;
        push(m_diskReadHistory, m_diskRead);
        push(m_diskWriteHistory, m_diskWrite);
    }
    m_previousRead = read;
    m_previousWritten = written;
}

void Backend::sampleNetwork(bool rates, double elapsed) {
    // The loopback interface is traffic the machine sends itself.
    quint64 rx = 0, tx = 0;
    const auto lines = QString::fromUtf8(readProc("net/dev")).split('\n');
    for (const auto &line : lines) {
        if (!line.contains(':'))
            continue;
        const auto name = line.section(':', 0, 0).trimmed();
        if (name == "lo")
            continue;
        const auto fields = line.section(':', 1).simplified().split(' ');
        if (fields.size() < 9)
            continue;
        rx += fields[0].toULongLong();
        tx += fields[8].toULongLong();
    }
    if (rates) {
        m_netRx = rx >= m_previousRx ? (rx - m_previousRx) / elapsed : 0;
        m_netTx = tx >= m_previousTx ? (tx - m_previousTx) / elapsed : 0;
        push(m_rxHistory, m_netRx);
        push(m_txHistory, m_netTx);
    }
    m_previousRx = rx;
    m_previousTx = tx;
}
