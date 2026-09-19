#include "ThemeManager.h"

#include <QApplication>
#include <QPalette>
#include <QStyle>

ThemeManager::ThemeManager(AppSettings *settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_currentThemeId(settings->theme())
    , m_currentTheme(themeFor(m_currentThemeId))
{
    applyToApplication();
}

void ThemeManager::setTheme(AppTheme themeId)
{
    if (themeId == m_currentThemeId)
        return;

    m_currentThemeId = themeId;
    m_currentTheme = themeFor(themeId);
    m_settings->setTheme(themeId);

    applyToApplication();
    emit themeChanged(m_currentTheme);
}

QVector<AppTheme> ThemeManager::allThemeIds()
{
    return {AppTheme::Dark, AppTheme::Light, AppTheme::Purple,
            AppTheme::Ocean, AppTheme::Forest, AppTheme::Rose};
}

Theme ThemeManager::themeFor(AppTheme themeId)
{
    // Таблица цветов всех тем. Каждая тема описывает один и тот же набор
    // ролей (см. Theme.h) - именно поэтому добавление новой темы позже
    // будет означать "дописать ещё один case сюда", а не искать цвета
    // по десятку .cpp файлов.
    switch (themeId) {
    case AppTheme::Dark:
        return Theme{
            "Dark",
            QColor("#1e1e22"), QColor("#2a2a30"), QColor("#35353c"),
            QColor("#e8e8ea"), QColor("#9a9aa0"), QColor("#3a3a42"),
            QColor("#5e81f4"), QColor("#7b9bf7"), QColor("#f2b155"),
            QColor("#5e81f4"), QColor("#35353c"), QColor("#dcdce2"),
            QColor("#313138")
        };

    case AppTheme::Light:
        return Theme{
            "Light",
            QColor("#f5f5f7"), QColor("#ffffff"), QColor("#eceef2"),
            QColor("#1c1c1e"), QColor("#6b6b70"), QColor("#dcdce2"),
            QColor("#4a6cf7"), QColor("#6685f9"), QColor("#e0932a"),
            QColor("#4a6cf7"), QColor("#eef1fb"), QColor("#24325c"),
            QColor("#e4e4e9")
        };

    case AppTheme::Purple:
        // Тёмная фиолетовая: deep purple/indigo фон, лавандовый акцент -
        // не кислотный фиолетовый, а приглушённый и контрастный.
        return Theme{
            "Purple",
            QColor("#1c1826"), QColor("#241f34"), QColor("#302a44"),
            QColor("#ece9f7"), QColor("#a79bc4"), QColor("#372f4d"),
            QColor("#9b7bf0"), QColor("#b199f5"), QColor("#f0b45e"),
            QColor("#9b7bf0"), QColor("#302a44"), QColor("#e3d9fb"),
            QColor("#2c2640")
        };

    case AppTheme::Ocean:
        return Theme{
            "Ocean",
            QColor("#0f1f2b"), QColor("#16293a"), QColor("#1e364a"),
            QColor("#e4eef4"), QColor("#8fa8b8"), QColor("#21445a"),
            QColor("#35b4c9"), QColor("#55c6d9"), QColor("#f0b155"),
            QColor("#35b4c9"), QColor("#1e364a"), QColor("#cdeef4"),
            QColor("#1c3244")
        };

    case AppTheme::Forest:
        return Theme{
            "Forest",
            QColor("#131d16"), QColor("#1b271e"), QColor("#243527"),
            QColor("#e6ece7"), QColor("#93a894"), QColor("#2b3b2e"),
            QColor("#52b86a"), QColor("#6fca85"), QColor("#e8b256"),
            QColor("#52b86a"), QColor("#243527"), QColor("#d3ecd8"),
            QColor("#202f23")
        };

    case AppTheme::Rose:
        return Theme{
            "Rose",
            QColor("#221419"), QColor("#2c1a20"), QColor("#3a232a"),
            QColor("#f2e4e8"), QColor("#b98c98"), QColor("#3f2731"),
            QColor("#e26a8a"), QColor("#ea86a1"), QColor("#f0b45e"),
            QColor("#e26a8a"), QColor("#3a232a"), QColor("#f6d3dd"),
            QColor("#331f27")
        };
    }

    return themeFor(AppTheme::Dark);
}

void ThemeManager::applyToApplication() const
{
    auto *app = qApp;
    if (!app)
        return;

    app->setStyle("Fusion");

    QPalette palette;
    palette.setColor(QPalette::Window, m_currentTheme.background);
    palette.setColor(QPalette::WindowText, m_currentTheme.text);
    palette.setColor(QPalette::Base, m_currentTheme.surface);
    palette.setColor(QPalette::AlternateBase, m_currentTheme.surfaceElevated);
    palette.setColor(QPalette::Text, m_currentTheme.text);
    palette.setColor(QPalette::Button, m_currentTheme.surface);
    palette.setColor(QPalette::ButtonText, m_currentTheme.text);
    palette.setColor(QPalette::Highlight, m_currentTheme.accent);
    palette.setColor(QPalette::HighlightedText, Qt::white);
    app->setPalette(palette);

    app->setStyleSheet(buildStyleSheet(m_currentTheme));
}

QString ThemeManager::buildStyleSheet(const Theme &theme)
{
    // Единый QSS-шаблон для всех тем - меняются только цвета, не структура
    // правил. Подстановка через @токен@, а не позиционные %1/%2/...,
    // специально: цветов много, и с именованными токенами исключён риск
    // "перепутать местами" при добавлении/удалении одного из них.
    QString qss = R"(
        QLabel#monthLabel { font-size: 16px; font-weight: bold; }
        QLabel#weekDayLabel { color: @textSecondary@; }
        QLabel#selectedDateLabel { font-size: 14px; font-weight: bold; padding-bottom: 4px; }
        QLabel#hintLabel { color: @textSecondary@; font-size: 11px; }
        QLabel#currentTaskLabel { font-size: 13px; font-weight: bold; color: @text@; }
        QLabel#pomodoroModeLabel { font-size: 14px; color: @textSecondary@; }
        QLabel#pomodoroTimeLabel { font-size: 48px; font-weight: bold; }
        QLabel#pomodoroCompletedLabel { color: @textSecondary@; }

        QPushButton#navButton {
            border: none;
            border-radius: 6px;
            background-color: @surface@;
        }
        QPushButton#navButton:hover {
            background-color: @surfaceElevated@;
        }

        QPushButton#todayButton {
            border: 1px solid @accent@;
            border-radius: 6px;
            padding: 4px 12px;
            color: @accent@;
        }
        QPushButton#todayButton:hover {
            background-color: @accentHoverTranslucent@;
        }

        QPushButton#panelButton {
            border: none;
            border-radius: 6px;
            background-color: @surface@;
            padding: 7px 10px;
            text-align: left;
        }
        QPushButton#panelButton:hover {
            background-color: @surfaceElevated@;
        }
        QPushButton#panelButton:disabled {
            color: @textSecondary@;
        }

        QListWidget#eventsList {
            border: none;
            border-radius: 8px;
            background-color: @surface@;
            padding: 4px;
        }
        QListWidget#eventsList::item {
            border-radius: 6px;
            padding: 6px 8px;
            margin: 1px 0px;
        }
        QListWidget#eventsList::item:selected {
            background-color: @selectionTranslucent@;
            color: @text@;
        }
        QListWidget#eventsList::item:hover {
            background-color: @surfaceElevated@;
        }

        QSplitter::handle {
            background-color: @border@;
        }

        QGroupBox {
            border: 1px solid @border@;
            border-radius: 8px;
            margin-top: 10px;
            padding-top: 10px;
            font-weight: bold;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 4px;
        }
    )";

    auto rgba = [](const QColor &color, int alpha) {
        return QString("rgba(%1, %2, %3, %4)").arg(color.red()).arg(color.green()).arg(color.blue()).arg(alpha);
    };

    qss.replace("@background@", theme.background.name());
    qss.replace("@surface@", theme.surface.name());
    qss.replace("@surfaceElevated@", theme.surfaceElevated.name());
    qss.replace("@text@", theme.text.name());
    qss.replace("@textSecondary@", theme.textSecondary.name());
    qss.replace("@border@", theme.border.name());
    qss.replace("@accent@", theme.accent.name());
    qss.replace("@accentHoverTranslucent@", rgba(theme.accent, 40));
    qss.replace("@selectionTranslucent@", rgba(theme.accent, 55));

    return qss;
}
