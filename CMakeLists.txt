cmake_minimum_required(VERSION 3.15)

# Настройка для Windows
if(WIN32)
    add_compile_definitions(_WIN32_WINNT=0x0601)
    add_compile_options(/bigobj)
    
    # Использование vcpkg
    if(EXISTS "C:/vcpkg/scripts/buildsystems/vcpkg.cmake")
        set(CMAKE_TOOLCHAIN_FILE "C:/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")
    endif()
endif()

project(ascii_streamer)

# Установка стандарта C++
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Настройка для Linux
if(UNIX)
    add_compile_options(-pthread)
    add_link_options(-pthread)
endif()

# Поиск OpenCV
find_package(OpenCV REQUIRED)

if(OpenCV_FOUND)
    message(STATUS "OpenCV found: ${OpenCV_VERSION}")
else()
    message(FATAL_ERROR "OpenCV not found")
endif()

# Поиск Boost
find_package(Boost 1.70 REQUIRED COMPONENTS system thread)

# Поиск nlohmann-json
find_package(nlohmann_json REQUIRED)

# Поиск spdlog
find_package(spdlog REQUIRED)

# Поиск curl
find_package(CURL REQUIRED)

# Поиск OpenSSL
find_package(OpenSSL REQUIRED)

# Поиск CURL
find_package(CURL REQUIRED)

# Список исходных файлов для сервера
set(SERVER_SOURCES
    src/main.cpp
    src/server.cpp
    src/http_session.cpp
    src/websocket_session.cpp
    src/video_source.cpp
    src/ascii_converter.cpp
    src/logger.cpp
    src/api_key_manager.cpp
    src/stream_controller.cpp
    src/network_utils.cpp 
)

# Создание исполняемого файла для сервера
add_executable(server ${SERVER_SOURCES})

# Подключение заголовочных файлов
target_include_directories(server PRIVATE 
    include
    ${OpenCV_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
)

# Линковка с библиотеками
target_link_libraries(server PRIVATE 
    Boost::system 
    Boost::thread
    ${OpenCV_LIBS}
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    CURL::libcurl
    OpenSSL::SSL
    OpenSSL::Crypto
)

# Платформозависимые настройки линковки
if(WIN32)
    target_link_libraries(server PRIVATE 
        ws2_32
        comctl32
        gdi32
        ole32
        setupapi
        winmm
        iphlpapi
        crypt32
    )
elseif(UNIX)
    target_link_libraries(server PRIVATE pthread)
endif()

# Копирование веб-ресурсов
add_custom_command(TARGET server POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/web"
        "${CMAKE_CURRENT_BINARY_DIR}/web"
    COMMENT "Copying web resources"
)

# Включение тестов
option(BUILD_TESTS "Build tests" OFF)

if(BUILD_TESTS)
    add_subdirectory(tests)
endif()