#include <QApplication>

#include "ui/MainWindow.h"

// main.cpp больше не занимается темами оформления (Phase C) - MainWindow
// сам создаёт ThemeManager, и тот применяет сохранённую тему при старте.
// До Phase C здесь жили applyDarkTheme()/applyLightTheme() - вся эта
// логика теперь в services/ThemeManager.*, одном месте на всё приложение.
int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    MainWindow window;
    window.show();

    return app.exec();
}
