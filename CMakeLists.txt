cmake_minimum_required(VERSION 3.16)
project(QtCalendarApp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt использует moc (meta-object compiler) для обработки Q_OBJECT.
# Эти три флага включают автоматический запуск moc/uic/rcc при сборке,
# чтобы не прописывать их вручную.
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Sql - добавлен в Этапе 2 для хранения событий в SQLite.
find_package(Qt6 REQUIRED COMPONENTS Widgets Sql)

add_executable(QtCalendarApp
    src/main.cpp
    src/models/Event.h
    src/models/Event.cpp
    src/models/Category.h
    src/models/Execution.h
    src/models/Execution.cpp
    src/services/EventManager.h
    src/services/EventManager.cpp
    src/services/PomodoroTimer.h
    src/services/PomodoroTimer.cpp
    src/services/CsvImporter.h
    src/services/CsvImporter.cpp
    src/services/NotificationService.h
    src/services/NotificationService.cpp
    src/services/EventReminder.h
    src/services/EventReminder.cpp
    src/services/AppSettings.h
    src/services/AppSettings.cpp
    src/services/Theme.h
    src/services/ThemeManager.h
    src/services/ThemeManager.cpp
    src/ui/MainWindow.h
    src/ui/MainWindow.cpp
    src/ui/CalendarView.h
    src/ui/CalendarView.cpp
    src/ui/ICalendarPage.h
    src/ui/MonthView.h
    src/ui/MonthView.cpp
    src/ui/WeekView.h
    src/ui/WeekView.cpp
    src/ui/DayView.h
    src/ui/DayView.cpp
    src/ui/MonthDayCell.h
    src/ui/MonthDayCell.cpp
    src/ui/EventLayout.h
    src/ui/EventLayout.cpp
    src/ui/TimeGridView.h
    src/ui/TimeGridView.cpp
    src/ui/EventDialog.h
    src/ui/EventDialog.cpp
    src/ui/PomodoroWidget.h
    src/ui/PomodoroWidget.cpp
    src/ui/SettingsDialog.h
    src/ui/SettingsDialog.cpp
    src/ui/CategoryManagerDialog.h
    src/ui/CategoryManagerDialog.cpp
)

target_include_directories(QtCalendarApp PRIVATE src)

target_link_libraries(QtCalendarApp PRIVATE Qt6::Widgets Qt6::Sql)

# Тесты (MVP1, раздел 11 Product Context) - подключаются, только если
# у установленного Qt есть модуль Test. Модуль не обязателен для сборки
# самого приложения, поэтому его отсутствие не должно ломать QtCalendarApp -
# просто тесты в этом случае не собираются.
find_package(Qt6 QUIET COMPONENTS Test)
if(TARGET Qt6::Test)
    enable_testing()
    add_subdirectory(tests)
else()
    message(STATUS "Qt6::Test not found - skipping tests (QtCalendarApp target is unaffected)")
endif()
