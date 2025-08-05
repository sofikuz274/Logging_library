cmake_minimum_required(VERSION 3.12)
project(LoggingLibrary VERSION 1.0.0 LANGUAGES CXX)

# Установка стандарта C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Настройки сборки
set(CMAKE_BUILD_TYPE_INIT "Release")

# Компиляторные флаги для gcc
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
endif()

# Включаем тестирование
enable_testing()

# Подключение подпроектов
add_subdirectory(src)      # Библиотека логирования
add_subdirectory(apps)     # Приложения
add_subdirectory(tests)    # Юнит-тесты

# Информация о сборке
message(STATUS "")
message(STATUS "=== Конфигурация сборки ===")
message(STATUS "Проект: ${PROJECT_NAME}")
message(STATUS "Версия: ${PROJECT_VERSION}")
message(STATUS "Тип сборки: ${CMAKE_BUILD_TYPE}")
message(STATUS "Компилятор: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Стандарт C++: ${CMAKE_CXX_STANDARD}")
message(STATUS "============================")
message(STATUS "")

# Цели для удобства
add_custom_target(run_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --verbose
    DEPENDS unit_tests
    COMMENT "Запуск всех тестов"
)

# Демонстрационная цель
add_custom_target(demo
    COMMAND echo "Демонстрация работы библиотеки логирования:"
    COMMAND echo "1. Запуск тестов:"
    COMMAND ${CMAKE_CTEST_COMMAND} --verbose
    COMMAND echo ""
    COMMAND echo "2. Пример использования:"
    COMMAND echo "   ./apps/test_logger/test_logger demo.log INFO"
    COMMAND echo ""
    COMMAND echo "3. Сбор статистики (требует подключение через сокет):"
    COMMAND echo "   ./apps/log_stats/log_stats 12345 5 10"
    DEPENDS unit_tests test_logger log_stats
    COMMENT "Демонстрация возможностей"
)