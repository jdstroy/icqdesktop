#pragma once

#include "local_peer.h"

namespace Ui
{
    class MainWindow;
}

namespace launch
{
    class CommandLineParser;
}

namespace Utils
{
#ifdef _WIN32
    class AppGuard
    {
    public:
        AppGuard();
        ~AppGuard();

        bool succeeded() const;

    private:
        HANDLE Mutex_;
        bool Exist_;
    };
#endif //_WIN32

    class Application : public QObject
    {
        Q_OBJECT

    public:

        Application(int argc, char *argv[]);
        ~Application();

        int exec();

        bool init();

        bool isMainInstance();
        void switchInstance(launch::CommandLineParser& _cmd_parser);

        bool updating();
        void parseUrlCommand(const QString& _urlCommand);

   public Q_SLOTS:

            void initMainWindow();
            void receiveUrlCommand(QString _urlCommand);
            void applicationStateChanged(Qt::ApplicationState state);

    private:
        void init_win7_features();

        std::unique_ptr<Ui::MainWindow> main_window_;
        std::unique_ptr<LocalPeer> peer_;
        std::unique_ptr<QApplication> app_;

#ifdef _WIN32
        AppGuard guard_;
#endif //_WIN32
    };
}