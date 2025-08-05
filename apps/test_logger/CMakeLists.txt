# Тестовое приложение для проверки библиотеки логирования
add_executable(test_logger main.cpp)

# Связываем со статической библиотекой
target_link_libraries(test_logger PRIVATE logging::static)

# Требования к стандарту C++
target_compile_features(test_logger PRIVATE cxx_std_17)

# Компиляторные флаги
target_compile_options(test_logger PRIVATE 
    -Wall -Wextra -Wpedantic
    $<$<CONFIG:Debug>:-g -O0>
    $<$<CONFIG:Release>:-O3>
)

# Связываем с pthread для многопоточности
find_package(Threads REQUIRED)
target_link_libraries(test_logger PRIVATE Threads::Threads)