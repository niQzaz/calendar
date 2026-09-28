# Тестовые исполняемые файлы на Qt Test.
#
# Сюда намеренно НЕ подключается основной target QtCalendarApp - вместо
# этого нужные .cpp (Event.cpp, EventManager.cpp) просто компилируются
# заново в каждый тестовый executable. Да, это небольшое дублирование
# компиляции, зато конфигурация основного приложения в корневом
# CMakeLists.txt (add_executable(QtCalendarApp ...)) остаётся нетронутой -
# минимальное изменение, ноль риска для существующей сборки.

set(CALENDAR_SRC_DIR ${CMAKE_SOURCE_DIR}/src)

# --- test_recurrence: models/Event.h, без БД ---
add_executable(test_recurrence
    test_recurrence.cpp
    ${CALENDAR_SRC_DIR}/models/Event.cpp
)
target_include_directories(test_recurrence PRIVATE ${CALENDAR_SRC_DIR})
target_link_libraries(test_recurrence PRIVATE Qt6::Test Qt6::Core)
add_test(NAME test_recurrence COMMAND test_recurrence)

# --- test_execution: models/Execution.h, без БД (MVP 3.0) ---
add_executable(test_execution
    test_execution.cpp
    ${CALENDAR_SRC_DIR}/models/Execution.cpp
)
target_include_directories(test_execution PRIVATE ${CALENDAR_SRC_DIR})
target_link_libraries(test_execution PRIVATE Qt6::Test Qt6::Core)
add_test(NAME test_execution COMMAND test_execution)

# --- test_event_manager: services/EventManager.h, in-memory SQLite ---
# Execution.cpp нужен здесь же (не только в test_execution) - EventManager.cpp
# сам вызывает executionStatusToString()/resolveExecutionStatus() и т.п.,
# без него получим ошибку линковки (undefined reference), а не ошибку компиляции.
add_executable(test_event_manager
    test_event_manager.cpp
    ${CALENDAR_SRC_DIR}/models/Event.cpp
    ${CALENDAR_SRC_DIR}/models/Execution.cpp
    ${CALENDAR_SRC_DIR}/services/EventManager.cpp
)
target_include_directories(test_event_manager PRIVATE ${CALENDAR_SRC_DIR})
target_link_libraries(test_event_manager PRIVATE Qt6::Test Qt6::Sql Qt6::Core)
add_test(NAME test_event_manager COMMAND test_event_manager)
