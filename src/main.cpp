#include "ui/MainWindow.h"
#include "ui/Branding.h"
#include "ui/ThemeManager.h"

#include <QApplication>
#include <QColor>
#include <QFont>
#include <QSettings>
#include <QSplashScreen>
#include <QStyleFactory>
#include <QTimer>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("ChromaDesk"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("chromadesk.app"));
    QCoreApplication::setApplicationName(QStringLiteral("ChromaDesk"));
    QCoreApplication::setApplicationVersion(QString::fromLatin1(CHROMADESK_VERSION));
    application.setWindowIcon(chroma::branding::applicationIcon());

    application.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    const QSettings settings;
    chroma::ThemeManager::apply(application, chroma::ThemeManager::fromKey(
        settings.value(QStringLiteral("ui/theme"), QStringLiteral("gray")).toString()));

    QSplashScreen splash(chroma::branding::splashPixmap());
    splash.showMessage(QStringLiteral("Starting local editing engine…"),
                       Qt::AlignBottom | Qt::AlignHCenter,
                       QColor(QStringLiteral("#aeb5c5")));
    splash.show();
    application.processEvents();

    chroma::MainWindow window;
    QTimer::singleShot(2200, &application, [&window, &splash] {
        window.show();
        splash.finish(&window);
    });
    return application.exec();
}
