#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include <QSignalSpy>

#include "backend.h"
#include "theme.h"
#include "translator.h"

class VitalsTests : public QObject {
    Q_OBJECT

private slots:
    // A fake /proc with two cores, then a second sample one second later.
    void samplesCountersFromProc()
    {
        QTemporaryDir dir;
        const QString proc = dir.filePath("proc"), block = dir.filePath("block");
        QDir().mkpath(proc + "/net");
        QDir().mkpath(block + "/nvme0n1");
        QDir().mkpath(block + "/loop0");
        writeProc(proc, 0, 0);

        Backend backend(proc, block, dir.path(), false);
        backend.sample(1);
        QVERIFY2(backend.error().isEmpty(), qPrintable(backend.error()));
        QCOMPARE(backend.cpuHistory().size(), 0);
        QCOMPARE(backend.memTotal(), 16.0 * 1024 * 1024 * 1024);
        QCOMPARE(backend.memUsed(), 4.0 * 1024 * 1024 * 1024);
        QCOMPARE(backend.swapUsed(), 1.0 * 1024 * 1024 * 1024);
        QCOMPARE(backend.load().size(), 3);
        QCOMPARE(backend.uptime(), 3725.5);
        QVERIFY(backend.diskTotal() > 0);

        writeProc(proc, 1, 1);
        backend.sample(2);
        QCOMPARE(backend.cpuHistory().size(), 1);
        // Total: 300 busy of 400 jiffies; core 0 fully busy, core 1 half.
        QCOMPARE(backend.cpu(), 75.0);
        QCOMPARE(backend.cores().size(), 2);
        QCOMPARE(backend.cores().at(0).toDouble(), 100.0);
        QCOMPARE(backend.cores().at(1).toDouble(), 50.0);
        // 2000 sectors read and 1000 written on the real disk, over 2 s; the
        // loop device and the partition are ignored.
        QCOMPARE(backend.diskRead(), 2000.0 * 512 / 2);
        QCOMPARE(backend.diskWrite(), 1000.0 * 512 / 2);
        // 10 000 bytes in and 4 000 out on eth0 over 2 s; lo doesn't count.
        QCOMPARE(backend.netRx(), 5000.0);
        QCOMPARE(backend.netTx(), 2000.0);
    }

    // Without an elapsed time the backend measures one itself, as it does
    // from its timer in the app.
    void measuresElapsedTimeItself()
    {
        QTemporaryDir dir;
        const QString proc = dir.filePath("proc");
        QDir().mkpath(proc + "/net");
        writeProc(proc, 0, 0);
        Backend backend(proc, dir.filePath("block"), dir.path(), false);
        backend.sample();
        QTest::qWait(50);
        writeProc(proc, 1, 1);
        backend.sample();
        QCOMPARE(backend.cpuHistory().size(), 1);
        QCOMPARE(backend.cpu(), 75.0);
        QVERIFY(backend.netRx() > 0);
    }

    void reportsMissingProc()
    {
        QTemporaryDir dir;
        Backend backend(dir.filePath("nowhere"), dir.filePath("block"), dir.path(), false);
        backend.sample(1);
        QVERIFY(backend.error().contains("stat"));
    }

    void historyKeepsAMinute()
    {
        QTemporaryDir dir;
        const QString proc = dir.filePath("proc");
        QDir().mkpath(proc + "/net");
        Backend backend(proc, dir.filePath("block"), dir.path(), false);
        for (int i = 0; i < 100; ++i) {
            writeProc(proc, i, i);
            backend.sample(1);
        }
        QCOMPARE(backend.memHistory().size(), Backend::kHistory);
        QCOMPARE(backend.cpuHistory().size(), Backend::kHistory);
    }

    void pauseStopsAndResumes()
    {
        QTemporaryDir dir;
        Backend backend(dir.path(), dir.path(), dir.path(), false);
        QSignalSpy spy(&backend, &Backend::pausedChanged);
        backend.togglePause();
        QVERIFY(backend.paused());
        backend.togglePause();
        QVERIFY(!backend.paused());
        QCOMPARE(spy.count(), 2);
    }

    void themeFallsBack()
    {
        QTemporaryDir dir;
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#FFD60A"));
        QCOMPARE(theme.accentForeground(), QString("black"));
    }

    void themeReadsAccent()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("theme");
        QFile colors(dir.path() + "/theme/colors.toml");
        QVERIFY(colors.open(QIODevice::WriteOnly));
        colors.write("accent = \"#112233\"\n");
        colors.close();

        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#112233"));
        QCOMPARE(theme.accentForeground(), QString("white"));
    }

    void themeFollowsHyprlandRounding()
    {
        QTemporaryDir dir;
        const QString response = dir.filePath("response.json"), command = dir.filePath("hyprctl");
        auto write = [](const QString &path, const QByteArray &data) {
            QFile file(path);
            return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
        };
        QVERIFY(write(command, "#!/bin/sh\ncat '" + response.toUtf8() + "'\n"));
        QVERIFY(QFile::setPermissions(command, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        QVERIFY(write(response, "{\"int\":0}"));

        Theme theme(dir.path(), nullptr, command);
        QCOMPARE(theme.radius(), 0);
        QVERIFY(write(response, "{\"int\":6}"));
        QTRY_COMPARE(theme.radius(), 6);
    }

    void translatorFallsBackToEnglish()
    {
        QTemporaryDir dir;
        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QString("en"));
        QVERIFY(translator.translate("Main", "Save").isNull());
    }

    void translatorFollowsTheLanguageFile()
    {
        QTemporaryDir dir;
        write(dir.path() + "/language", "pt-BR\n");
        write(dir.path() + "/pt.json", R"({"Save": "Guardar", "Open": "Abrir"})");
        write(dir.path() + "/pt-BR.json", R"({"Save": "Salvar"})");

        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QString("pt-BR"));
        QCOMPARE(translator.translate("Main", "Save"), QString("Salvar"));
        QCOMPARE(translator.translate("Main", "Open"), QString("Abrir"));
        QVERIFY(translator.translate("Main", "Quit").isNull());
    }

    void translatorPicksPluralForms()
    {
        QTemporaryDir dir;
        write(dir.path() + "/language", "ru\n");
        write(dir.path() + "/en.json", R"({"%n clips": {"one": "%n clip", "other": "%n clips"}})");
        write(dir.path() + "/ru.json", R"({"%n clips": {"one": "%n клип", "few": "%n клипа", "many": "%n клипов", "other": "%n клипа"}})");

        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.translate("Main", "%n clips", nullptr, 21), QString("%n клип"));
        QCOMPARE(translator.translate("Main", "%n clips", nullptr, 22), QString("%n клипа"));
        QCOMPARE(translator.translate("Main", "%n clips", nullptr, 25), QString("%n клипов"));
        QCOMPARE(Translator::pluralCategory("ar", 11), QString("many"));
        QCOMPARE(Translator::pluralCategory("ja", 1), QString("other"));
    }

private:
    // Step n advances every counter: each core and the total gain jiffies,
    // the disk sectors, and the network bytes.
    static void writeProc(const QString &proc, int n, int)
    {
        const quint64 u0 = 1000 + n * 200, i0 = 1000;          // core 0: all busy
        const quint64 u1 = 1000 + n * 100, i1 = 1000 + n * 100; // core 1: half busy
        write(proc + "/stat", QString("cpu  %1 0 0 %2 0 0 0 0 0 0\ncpu0 %3 0 0 %4 0 0 0 0 0 0\ncpu1 %5 0 0 %6 0 0 0 0 0 0\nintr 1 2 3\n")
                                  .arg(u0 + u1).arg(i0 + i1).arg(u0).arg(i0).arg(u1).arg(i1).toUtf8());
        write(proc + "/meminfo", "MemTotal:       16777216 kB\nMemFree:  1000 kB\nMemAvailable:   12582912 kB\n"
                                 "SwapTotal:       8388608 kB\nSwapFree:        7340032 kB\n");
        write(proc + "/loadavg", "0.52 0.48 0.40 1/900 12345\n");
        write(proc + "/uptime", "3725.50 12000.00\n");
        write(proc + "/diskstats", QString("   7       0 loop0 9 0 %1 0 9 0 %1 0 0 0 0\n"
                                           " 259       0 nvme0n1 9 0 %2 0 9 0 %3 0 0 0 0\n"
                                           " 259       1 nvme0n1p1 9 0 %2 0 9 0 %3 0 0 0 0\n")
                                       .arg(n * 99999).arg(n * 2000).arg(n * 1000).toUtf8());
        write(proc + "/net/dev", QString("Inter-|   Receive  |  Transmit\n face |bytes packets errs drop fifo frame compressed multicast|bytes\n"
                                         "    lo: %1 0 0 0 0 0 0 0 %1 0 0 0 0 0 0 0\n"
                                         "  eth0: %2 0 0 0 0 0 0 0 %3 0 0 0 0 0 0 0\n")
                                     .arg(n * 777777).arg(n * 10000).arg(n * 4000).toUtf8());
    }

    static void write(const QString &path, const QByteArray &contents)
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(contents);
    }
};

QTEST_MAIN(VitalsTests)
#include "vitals_tests.moc"
