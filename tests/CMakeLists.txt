# Юнит-тесты для библиотеки логирования
add_executable(unit_tests unit_tests.cpp)

# Связываем со статической библиотекой
target_link_libraries(unit_tests PRIVATE logging::static)

# Требования к стандарту C++
target_compile_features(unit_tests PRIVATE cxx_std_17)

# Компиляторные флаги
target_compile_options(unit_tests PRIVATE 
    -Wall -Wextra -Wpedantic
    $<$<CONFIG:Debug>:-g -O0>
    $<$<CONFIG:Release>:-O3>
)

# Связываем с pthread для многопоточности
find_package(Threads REQUIRED)
target_link_libraries(unit_tests PRIVATE Threads::Threads)

# Включаем CTest для автоматического тестирования
enable_testing()
add_test(NAME LoggerUnitTests COMMAND unit_tests)