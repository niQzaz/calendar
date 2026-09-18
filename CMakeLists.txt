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
    src/ui/MainWindow.h
    src/ui/MainWindow.cpp
    src/ui/CalendarWidget.h
    src/ui/CalendarWidget.cpp
    src/ui/EventDialog.h
    src/ui/EventDialog.cpp
    src/ui/PomodoroWidget.h
    src/ui/PomodoroWidget.cpp
)

target_include_directories(QtCalendarApp PRIVATE src)

target_link_libraries(QtCalendarApp PRIVATE Qt6::Widgets Qt6::Sql)
