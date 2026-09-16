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

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(QtCalendarApp
    src/main.cpp
    src/models/Event.h
    src/services/EventManager.h
    src/services/EventManager.cpp
    src/ui/MainWindow.h
    src/ui/MainWindow.cpp
    src/ui/CalendarWidget.h
    src/ui/CalendarWidget.cpp
    src/ui/EventDialog.h
    src/ui/EventDialog.cpp
)

target_include_directories(QtCalendarApp PRIVATE src)

target_link_libraries(QtCalendarApp PRIVATE Qt6::Widgets)
