#include <QtCore/QCoreApplication>
#include <QtCore/QTimer>
#include <QtCore/QTextStream>
#include <QtCore/QStringList>
#include <QtCore/QPair>
#include <QtCore/QVector>
#include <QProcess>
#include <QFile>
#include <QDir>
#include <QRegularExpression>
#include <QUuid>
#include <iostream>

typedef QPair<QString, float> parmset;

bool waitForReadyRead(QProcess & process) {
    while (!process.canReadLine() && process.state() == QProcess::Running) {
        process.waitForReadyRead(-1);
    }

    // somebody crashed
    if (process.state() != QProcess::Running) {
        return false;
    }

    return true;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    // Map streams
    QTextStream cin(stdin, QIODevice::ReadOnly);
    QTextStream cout(stdout, QIODevice::WriteOnly);
    QTextStream cerr(stderr, QIODevice::WriteOnly);

    // (2) Set up something that will trigger events
    QTimer::singleShot(0, &app, SLOT(quit()));

    cerr << "clopgtp v0.1" << endl;

    /*Arguments are:\n\
     #0: program name
     #1: processor id (symbolic name, typically a machine name to ssh to)\n\
     #2: seed (integer)\n\
     #3: parameter id of first parameter (symbolic name)\n\
     #4: value of first parameter (float)\n\
     #5: parameter id of second parameter (optional)\n\
     #6: value of second parameter (optional)\n\*/
    QStringList slargs = app.arguments();

    if (slargs.size() < 5 || ((slargs.size() % 2) != 1)) {
        cerr << "Invalid number of arguments (" << slargs.size() << ")" << endl;
        exit(1);
    }
    slargs.pop_front();
    QString procid = slargs.takeFirst();
    unsigned int seed = slargs.takeFirst().toInt();
    int parms = slargs.size() / 2;
    cerr << "Processor ID: " << procid << endl;
    cerr << "Seed: " << seed << endl;
    cerr << "Parameters read: " << parms << endl;
    QVector<parmset> vps;

    for (int i = 0; i < parms; i++) {
        QString parid = slargs.takeFirst();
        float parval = slargs.takeFirst().toFloat();
        vps.append(qMakePair(parid, parval));
        cerr << parid << " = " << parval << endl;
    }

    qsrand(seed);

    QString orig_cmdline("./leela_ref --gtp --noponder --threads 1");
    QString tune_cmdline("./leela --gtp --noponder --threads 1");

    parmset ps;
    foreach (ps, vps) {
        tune_cmdline += " --";
        tune_cmdline += ps.first;
        tune_cmdline += " ";
        tune_cmdline += QString::number(ps.second);
    }

    cerr << orig_cmdline << endl;
    cerr << tune_cmdline << endl;

    QProcess orig_process;
    orig_process.start(orig_cmdline);

    QProcess tune_process;
    tune_process.start(tune_cmdline);

    orig_process.waitForStarted();
    tune_process.waitForStarted();

    char readbuff[256];
    int read_cnt;

    QString winner;
    bool stop = false;
    bool orig_to_move = (qrand() % 2 == 0);
    bool black_to_move = true;
    bool black_resigned;
    bool orig_is_black = orig_to_move;
    int passes = 0;

    cerr << "Original is black: " << orig_is_black << endl;

#if 1
    orig_process.write(qPrintable("boardsize 9\n"));
    tune_process.write(qPrintable("boardsize 9\n"));
    orig_process.waitForBytesWritten(-1);
    tune_process.waitForBytesWritten(-1);
    if (!waitForReadyRead(orig_process)) {
        goto orig_crash;
    }
    read_cnt = orig_process.readLine(readbuff, 256);
    Q_ASSERT(read_cnt > 0);
    Q_ASSERT(readbuff[0] == '=');
    if (!waitForReadyRead(tune_process)) {
        goto tune_crash;
    }
    read_cnt = tune_process.readLine(readbuff, 256);
    Q_ASSERT(read_cnt > 0);
    Q_ASSERT(readbuff[0] == '=');
    // Eat double newline from GTP protocol
    if (!waitForReadyRead(orig_process)) {
        goto orig_crash;
    }
    read_cnt = orig_process.readLine(readbuff, 256);
    Q_ASSERT(read_cnt > 0);
    if (!waitForReadyRead(tune_process)) {
        goto tune_crash;
    }
    read_cnt = tune_process.readLine(readbuff, 256);
    Q_ASSERT(read_cnt > 0);
    cerr << "Started, board successfully set." << endl;
#endif

    orig_process.write(qPrintable("time_settings 120 0 0\n"));
    tune_process.write(qPrintable("time_settings 120 0 0\n"));
    orig_process.waitForBytesWritten(-1);
    tune_process.waitForBytesWritten(-1);
    if (!waitForReadyRead(orig_process)) {
        goto orig_crash;
    }
    read_cnt = orig_process.readLine(readbuff, 256);
    Q_ASSERT(read_cnt > 0);
    Q_ASSERT(readbuff[0] == '=');
    if (!waitForReadyRead(tune_process)) {
        goto tune_crash;
    }
    read_cnt = tune_process.readLine(readbuff, 256);
    Q_ASSERT(read_cnt > 0);
    Q_ASSERT(readbuff[0] == '=');
    // Eat double newline from GTP protocol
    if (!waitForReadyRead(orig_process)) {
        goto orig_crash;
    }
    read_cnt = orig_process.readLine(readbuff, 256);
    Q_ASSERT(read_cnt > 0);
    if (!waitForReadyRead(tune_process)) {
        goto tune_crash;
    }
    read_cnt = tune_process.readLine(readbuff, 256);
    Q_ASSERT(read_cnt > 0);
    cerr << "Time successfully set." << endl;

    do {
        QString move_cmd;
        if (black_to_move) {
            move_cmd = "genmove b\n";
        } else {
            move_cmd = "genmove w\n";
        }
        if (orig_to_move) {
            orig_process.write(qPrintable(move_cmd));
            orig_process.waitForBytesWritten(-1);
            if (!waitForReadyRead(orig_process)) {
                goto orig_crash;
            }
            read_cnt = orig_process.readLine(readbuff, 256);
        } else {
            tune_process.write(qPrintable(move_cmd));
            tune_process.waitForBytesWritten(-1);
            if (!waitForReadyRead(tune_process)) {
                goto tune_crash;
            }
            read_cnt = tune_process.readLine(readbuff, 256);
        }
        if (read_cnt <= 3 || readbuff[0] != '=') {
            cerr << "Error read " << read_cnt
                 << " '" << readbuff << "'" << endl;
            tune_process.terminate();
            orig_process.terminate();
            return 1;
        }
        // Skip "= "
        QString resp_move(&readbuff[2]);
        resp_move = resp_move.simplified();

        // Eat double newline from GTP protocol
        if (orig_to_move) {
            if (!waitForReadyRead(orig_process)) {
                goto orig_crash;
            }
            read_cnt = orig_process.readLine(readbuff, 256);
            Q_ASSERT(read_cnt > 0);
        } else {
            if (!waitForReadyRead(tune_process)) {
                goto tune_crash;
            }
            read_cnt = tune_process.readLine(readbuff, 256);
            Q_ASSERT(read_cnt > 0);
        }

        cerr << "Move received: " << resp_move << endl;

        QString move_side(QStringLiteral("play "));
        QString side_prefix;

        if (black_to_move) {
            side_prefix = QStringLiteral("b ");
        } else {
            side_prefix = QStringLiteral("w ");
        }

        move_side += side_prefix + resp_move + "\n";

        if (resp_move.compare(QStringLiteral("pass"),
                              Qt::CaseInsensitive) == 0) {
            passes++;
        } else if (resp_move.compare(QStringLiteral("resign"),
                                     Qt::CaseInsensitive) == 0) {
            passes++;
            stop = true;
            black_resigned = black_to_move;
        }

        // Got move, swap sides now
        orig_to_move = !orig_to_move;
        black_to_move = !black_to_move;

        if (!stop) {
            if (orig_to_move) {
                orig_process.write(qPrintable(move_side));
                orig_process.waitForBytesWritten(-1);
                if (!waitForReadyRead(orig_process)) {
                    goto orig_crash;
                }
                read_cnt = orig_process.readLine(readbuff, 256);
                Q_ASSERT(read_cnt > 0);
                Q_ASSERT(readbuff[0] == '=');
                if (!waitForReadyRead(orig_process)) {
                    goto orig_crash;
                }
                read_cnt = orig_process.readLine(readbuff, 256);
                Q_ASSERT(read_cnt > 0);
            } else {
                tune_process.write(qPrintable(move_side));
                tune_process.waitForBytesWritten(-1);
                if (!waitForReadyRead(tune_process)) {
                    goto tune_crash;
                }
                read_cnt = tune_process.readLine(readbuff, 256);
                Q_ASSERT(read_cnt > 0);
                Q_ASSERT(readbuff[0] == '=');
                if (!waitForReadyRead(tune_process)) {
                    goto tune_crash;
                }
                read_cnt = tune_process.readLine(readbuff, 256);
                Q_ASSERT(read_cnt > 0);
            }
        }
    } while (!stop && passes < 2);

    if (!stop) {
        QString sgf_name(QDir::tempPath() + "/");
        sgf_name += QUuid::createUuid().toRfc4122().toHex();
        sgf_name += ".sgf";

        cerr << "Writing " << sgf_name << endl;

        orig_process.write(qPrintable("printsgf " + sgf_name + "\n"));

        orig_process.write(qPrintable("quit\n"));
        tune_process.write(qPrintable("quit\n"));

        orig_process.waitForFinished(-1);
        tune_process.waitForFinished(-1);

        QProcess gnugo_process;
        QString gnugo_cmdline("gnugo --score aftermath ");
        gnugo_cmdline += " --positional-superko --chinese-rules -l ";
        gnugo_cmdline += sgf_name;
        gnugo_process.start(gnugo_cmdline);
        gnugo_process.waitForFinished(-1);

        QByteArray output = gnugo_process.readAllStandardOutput();
        QString outstr(output);
        QStringList outlst = outstr.split("\n");

        QRegularExpression re("^(Black|White)\\swins\\sby\\s((\\d+)\\.(\\d+))\\spoints?");
        QListIterator<QString> i(outlst);
        QString score;
        i.toBack();
        while (i.hasPrevious()) {
            QString str = i.previous();
            cerr << str << endl;
            auto match = re.match(str);
            if (match.hasMatch()) {
                winner = match.captured(1);
                score = match.captured(2);
                cerr << winner << " by " << score << " points." << endl;
            }
        }

        //QFile::remove(sgf_name);
    } else {
        orig_process.write(qPrintable("quit\n"));
        tune_process.write(qPrintable("quit\n"));

        orig_process.waitForFinished(-1);
        tune_process.waitForFinished(-1);

        if (black_resigned) {
            winner = QString(QStringLiteral("White"));
        } else {
            winner = QString(QStringLiteral("Black"));
        }
    }

    if (winner.isNull()) {
        cerr << "No winner found" << endl;
        exit(EXIT_FAILURE);
    }

    cerr.flush();

    if (winner.compare(QStringLiteral("Black"),
                       Qt::CaseInsensitive) == 0) {
        if (orig_is_black) {
            cout << "L" << endl;
        } else {
            cout << "W" << endl;
        }
    } else {
        Q_ASSERT(winner.compare(QStringLiteral("White"),
                                Qt::CaseInsensitive) == 0);
        if (orig_is_black) {
            cout << "W" << endl;
        } else {
            cout << "L" << endl;
        }
    }

    cout.flush();

    return app.exec();

orig_crash:
    cout << "W" << endl;
    return app.exec();
tune_crash:
    cout << "L" << endl;
    return app.exec();
}
