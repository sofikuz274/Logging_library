# Файлы исходного кода библиотеки
set(LIBRARY_SOURCES
    Logger.cpp
)

# Создание статической библиотеки
add_library(logging_static STATIC ${LIBRARY_SOURCES})
target_include_directories(logging_static PUBLIC 
    ${CMAKE_SOURCE_DIR}/include
)
target_compile_features(logging_static PRIVATE cxx_std_17)

# Создание динамической библиотеки
add_library(logging_shared SHARED ${LIBRARY_SOURCES})
target_include_directories(logging_shared PUBLIC 
    ${CMAKE_SOURCE_DIR}/include
)
target_compile_features(logging_shared PRIVATE cxx_std_17)

# Установка свойств для динамической библиотеки
set_target_properties(logging_shared PROPERTIES
    OUTPUT_NAME logging
    VERSION 1.0.0
    SOVERSION 1
)

# Установка свойств для статической библиотеки
set_target_properties(logging_static PROPERTIES
    OUTPUT_NAME logging
)

# Создание алиаса для удобства использования
add_library(logging::static ALIAS logging_static)
add_library(logging::shared ALIAS logging_shared)