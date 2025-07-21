#include "mainwindow.h"

#include <fcntl.h>
#include <io.h>

#include <windows.h>

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QTextStream>

#include "settings.h"
#include "windowhider.h"

enum class Operation {
    None,
    Hide,
    Unhide
};

class Target {
   public:
    static Target fromProcessId(DWORD processId) {
        Target target;
        target.m_type = Type::ProcessId;
        target.m_processId = processId;
        return target;
    }

    static Target fromWindowHandle(HWND windowHandle) {
        Target target;
        target.m_type = Type::WindowHandle;
        target.m_windowHandle = windowHandle;
        return target;
    }

    bool isValid() const { return m_type != Type::Invalid; }

    bool performOperation(Operation operation, QString* errorMessage) const {
        switch (m_type) {
            case Type::ProcessId:
                return operation == Operation::Hide ? WindowHider::HideProcessWindows(m_processId, errorMessage)
                                                    : WindowHider::UnhideProcessWindows(m_processId, errorMessage);
            case Type::WindowHandle:
                return operation == Operation::Hide ? WindowHider::HideWindow(m_windowHandle, errorMessage)
                                                    : WindowHider::UnhideWindow(m_windowHandle, errorMessage);
            default:
                if (errorMessage) {
                    *errorMessage = "Invalid target";
                }
                return false;
        }
    }

    QString description() const {
        switch (m_type) {
            case Type::ProcessId:
                return QString("process ID: %1").arg(m_processId);
            case Type::WindowHandle:
                return QString("window handle: 0x%1").arg(reinterpret_cast<quintptr>(m_windowHandle), 0, 16);
            default:
                return "invalid target";
        }
    }

    Target() = default;

   private:
    enum class Type {
        Invalid,
        ProcessId,
        WindowHandle
    };
    Type m_type = Type::Invalid;
    union {
        DWORD m_processId;
        HWND m_windowHandle;
    };
};

bool executeOperation(const Target& target, Operation operation) {
    QTextStream out(stdout);
    QTextStream err(stderr);

    if (!target.isValid()) {
        err << "Error: Invalid target specified" << Qt::endl;
        return false;
    }

    const char* operationName = (operation == Operation::Hide) ? "hide" : "unhide";
    out << "Attempting to " << operationName << " window for " << target.description() << Qt::endl;

    QString errorMessage;
    bool result = target.performOperation(operation, &errorMessage);
    if (!result) {
        err << "Operation failed: " << errorMessage << Qt::endl;
    } else {
        out << "Successfully " << (operation == Operation::Hide ? "hidden" : "unhidden") << " windows for "
            << target.description() << Qt::endl;
    }

    return result;
}

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    // app.setApplicationName("Evanesco");
    // app.setApplicationDisplayName("Evanesco");
    app.setApplicationVersion(kVersion);

    QCommandLineParser parser;
    parser.setApplicationDescription(
        "Hide/unhide windows from screen capture and recording.\n\n"
        "Usage:\n"
        "    evanesco [hide|unhide] --process <processId>\n"
        "    evanesco [hide|unhide] --window <windowHandle>\n"
        "    evanesco (Without arguments to launch the GUI)\n\n"
        "Examples:\n"
        "    evanesco hide --process 1234\n"
        "    evanesco unhide --window 12AB34"
    );

    parser.addHelpOption();
    parser.addVersionOption();

    // Define options
    QCommandLineOption processOption(QStringList() << "p" << "process", "Target process by process ID", "processId");
    QCommandLineOption windowOption(
        QStringList() << "w" << "window", "Target window by handle (hexadecimal)", "windowHandle"
    );

    parser.addOption(processOption);
    parser.addOption(windowOption);
    parser.process(app);

    QTextStream out(stdout);
    QTextStream err(stderr);

    // Parse positional arguments
    QStringList positionalArgs = parser.positionalArguments();
    Operation operation = Operation::None;

    if (!positionalArgs.isEmpty()) {
        QString operationStr = positionalArgs.first().toLower();
        if (operationStr == "hide") {
            operation = Operation::Hide;
        } else if (operationStr == "unhide") {
            operation = Operation::Unhide;
        } else {
            err << "Error: Invalid operation '" << operationStr << "'. Use 'hide' or 'unhide'." << Qt::endl;
            return 1;
        }
    }

    // Check target options
    bool hasProcess = parser.isSet(processOption);
    bool hasWindow = parser.isSet(windowOption);

    // Check for target options without operation
    if (operation == Operation::None && (hasProcess || hasWindow)) {
        err << "Warning: --process or --window options specified without operation (hide or unhide)" << Qt::endl;
        err << "Use 'evanesco --help' for usage information" << Qt::endl;
        return 1;
    }

    if (operation != Operation::None) {
        // CLI mode
        if (hasProcess && hasWindow) {
            err << "Error: Cannot specify both --process and --window options" << Qt::endl;
            return 1;
        }

        if (!hasProcess && !hasWindow) {
            err << "Error: Must specify either --process or --window option" << Qt::endl;
            return 1;
        }

        // Initialize as invalid
        Target target;
        if (hasProcess) {
            QString processIdStr = parser.value(processOption);
            bool ok;
            DWORD processId = processIdStr.toULong(&ok);
            if (!ok || processId == 0) {
                err << "Error: Invalid process ID '" << processIdStr << "'" << Qt::endl;
                return 1;
            }
            target = Target::fromProcessId(processId);
        } else if (hasWindow) {
            QString windowHandleStr = parser.value(windowOption);
            bool ok;
            quintptr handleValue = windowHandleStr.toULongLong(&ok, 16);
            if (!ok) {
                err << "Error: Invalid window handle '" << windowHandleStr << "'" << Qt::endl;
                return 1;
            }
            target = Target::fromWindowHandle(reinterpret_cast<HWND>(handleValue));
        }

        bool success = executeOperation(target, operation);

        if (success) {
            out << Qt::endl << "Operation completed successfully!" << Qt::endl;
            return 0;
        } else {
            out << Qt::endl << "Operation failed!" << Qt::endl;
            return 1;
        }
    } else {
        // Launch GUI when no operation is specified
        MainWindow window;

        // Apply taskbar icon setting
        Settings* settings = Settings::instance();
        if (settings->hideTaskbarIcon()) {
            window.setWindowFlags(window.windowFlags() | Qt::Tool);
        }

        window.show();
        return app.exec();
    }
}
