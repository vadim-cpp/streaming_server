cmake_minimum_required(VERSION 3.15)
project(ascii_streamer)

# Настройки для Windows
if(WIN32)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
    add_compile_definitions(_WIN32_WINNT=0x0601)
    add_compile_options("/Zc:__cplusplus")
endif()

# Поиск Boost
set(BOOST_ROOT "C:/boost")  # Укажите ваш путь к Boost
set(Boost_USE_STATIC_LIBS OFF)
find_package(Boost 1.70 REQUIRED COMPONENTS system thread)

# Список исходных файлов
set(SOURCES
    src/main.cpp
    src/server.cpp
    src/http_session.cpp
    src/websocket_session.cpp
)

# Создание исполняемого файла
add_executable(server ${SOURCES})

# Подключение заголовочных файлов
target_include_directories(server PRIVATE include)

# Линковка с Boost
target_link_libraries(server PRIVATE 
    Boost::system 
    Boost::thread
)

# Для Windows дополнительно линкуем с ws2_32
if(WIN32)
    target_link_libraries(server PRIVATE ws2_32)
endif()

# Копирование веб-ресурсов
file(GLOB WEB_FILES "${CMAKE_CURRENT_SOURCE_DIR}/web/*")
add_custom_command(TARGET server POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory 
        "${CMAKE_CURRENT_BINARY_DIR}/web"
    COMMAND ${CMAKE_COMMAND} -E copy
        ${WEB_FILES}
        "${CMAKE_CURRENT_BINARY_DIR}/web"
    COMMENT "Copying web resources"
)