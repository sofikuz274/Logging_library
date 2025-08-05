# Приложение для сбора статистики логов
add_executable(log_stats main.cpp)

# Связываем со статической библиотекой
target_link_libraries(log_stats PRIVATE logging::static)

# Требования к стандарту C++
target_compile_features(log_stats PRIVATE cxx_std_17)

# Компиляторные флаги
target_compile_options(log_stats PRIVATE 
    -Wall -Wextra -Wpedantic
    $<$<CONFIG:Debug>:-g -O0>
    $<$<CONFIG:Release>:-O3>
)

# Связываем с pthread для многопоточности
find_package(Threads REQUIRED)
target_link_libraries(log_stats PRIVATE Threads::Threads)