#include "backend.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSettings>
#include <QTcpSocket>

#include <obs-module.h>

#ifdef __linux__
#include <csignal>
#include <sys/prctl.h>
#endif

#ifndef BACKEND_INSTALL_DIR
#define BACKEND_INSTALL_DIR ""
#endif
#ifndef BACKEND_DEV_DIR
#define BACKEND_DEV_DIR ""
#endif

static QProcess *g_backendProc = nullptr;

static QSettings Settings()
{
    return QSettings("omniversify", "obs-omniversify-multichat-plugin");
}

int BackendPort()
{
    int port = Settings().value("port", 6973).toInt();
    if (port < 1 || port > 65535)
        port = 6973;
    return port;
}

void SetBackendPort(int port)
{
    QSettings s = Settings();
    s.setValue("port", port);
    s.sync();
}

QString BackendBaseUrl()
{
    return QString("http://127.0.0.1:%1").arg(BackendPort());
}

bool BackendIsRunning(int port)
{
    if (port < 1)
        port = BackendPort();

    QTcpSocket sock;
    sock.connectToHost("127.0.0.1", static_cast<quint16>(port));
    if (!sock.waitForConnected(300))
        return false;
    sock.disconnectFromHost();
    return true;
}

// Locate backend/main.py: installed location first (AUR package), then the
// dev checkout this plugin was compiled from (both baked in by CMake).
static QString BackendScriptPath()
{
    const QString envDir = qEnvironmentVariable("OMNIVERSIFY_BACKEND_DIR");
    QStringList candidates;
    if (!envDir.isEmpty())
        candidates << envDir + "/main.py";
    if (QString(BACKEND_INSTALL_DIR).length() > 1)
        candidates << QStringLiteral(BACKEND_INSTALL_DIR) + "/main.py";
    if (QString(BACKEND_DEV_DIR).length() > 1)
        candidates << QStringLiteral(BACKEND_DEV_DIR) + "/main.py";

    for (const QString &path : candidates) {
        if (QFileInfo::exists(path))
            return path;
    }
    return QString();
}

void StartBackend()
{
    if (BackendIsRunning()) {
        blog(LOG_INFO, "[omniversify] backend already listening on port %d — not spawning another",
             BackendPort());
        return;
    }

    const QString script = BackendScriptPath();
    if (script.isEmpty()) {
        blog(LOG_ERROR,
             "[omniversify] backend script not found (checked OMNIVERSIFY_BACKEND_DIR, %s, %s)",
             BACKEND_INSTALL_DIR, BACKEND_DEV_DIR);
        return;
    }

    // Absolute interpreter path: OBS often inherits a shell PATH where
    // python3 is a uv/pyenv shim without the system site-packages.
    QString python = "/usr/bin/python3";
    if (!QFileInfo::exists(python))
        python = "python3";

    g_backendProc = new QProcess(QCoreApplication::instance());
    g_backendProc->setProgram(python);
    g_backendProc->setArguments({script, "--port", QString::number(BackendPort())});
    g_backendProc->setWorkingDirectory(QFileInfo(script).absolutePath());

    // Give ROCm libraries a home if present (system onnxruntime-rocm needs them).
    QStringList env = QProcess::systemEnvironment();
    if (QDir("/opt/rocm/lib").exists())
        env << "LD_LIBRARY_PATH=/opt/rocm/lib";
    g_backendProc->setEnvironment(env);

#ifdef __linux__
    // If OBS dies (even SIGKILL), the kernel sends SIGTERM to the backend,
    // so we never leave an orphaned server holding the port.
    g_backendProc->setChildProcessModifier([] {
        prctl(PR_SET_PDEATHSIG, SIGTERM);
    });
#endif

    // Pipe backend output into the OBS log — otherwise crashes are invisible.
    auto forward = [](QProcess::ProcessChannel ch, int obsSeverity) {
        const QByteArray data = ch == QProcess::StandardError
                                    ? g_backendProc->readAllStandardError()
                                    : g_backendProc->readAllStandardOutput();
        for (const QByteArray &line : data.split('\n')) {
            if (!line.trimmed().isEmpty())
                blog(obsSeverity, "[omniversify-backend] %s", line.constData());
        }
    };
    QObject::connect(g_backendProc, &QProcess::readyReadStandardOutput,
                     [forward] { forward(QProcess::StandardOutput, LOG_INFO); });
    QObject::connect(g_backendProc, &QProcess::readyReadStandardError,
                     [forward] { forward(QProcess::StandardError, LOG_WARNING); });

    QObject::connect(g_backendProc,
                     QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), [](int code,
                                                                                         QProcess::ExitStatus) {
                         blog(LOG_ERROR, "[omniversify] backend exited (code %d)", code);
                     });

    g_backendProc->start();
    if (!g_backendProc->waitForStarted(5000)) {
        blog(LOG_ERROR, "[omniversify] failed to start backend: %s",
             qPrintable(g_backendProc->errorString()));
        delete g_backendProc;
        g_backendProc = nullptr;
        return;
    }
    blog(LOG_INFO, "[omniversify] backend started: python3 %s --port %d (pid %d)",
         qPrintable(script), BackendPort(), static_cast<int>(g_backendProc->processId()));
}

void StopBackend()
{
    if (!g_backendProc)
        return;
    if (g_backendProc->state() != QProcess::NotRunning) {
        g_backendProc->terminate();
        if (!g_backendProc->waitForFinished(3000)) {
            g_backendProc->kill();
            g_backendProc->waitForFinished(2000);
        }
    }
    delete g_backendProc;
    g_backendProc = nullptr;
    blog(LOG_INFO, "[omniversify] backend stopped");
}

void RestartBackend()
{
    StopBackend();
    StartBackend();
}
