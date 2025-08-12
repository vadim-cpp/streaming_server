cmake_minimum_required(VERSION 3.15)
set(CMAKE_TOOLCHAIN_FILE "C:/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")
project(ascii_streamer)

# Установка стандарта C++
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Настройка для Windows
if(WIN32)
    add_compile_definitions(_WIN32_WINNT=0x0601)
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

find_package(spdlog REQUIRED)

# Список исходных файлов для сервера
set(SERVER_SOURCES
    src/main.cpp
    src/server.cpp
    src/http_session.cpp
    src/websocket_session.cpp
    src/video_source.cpp
    src/ascii_converter.cpp
    src/logger.cpp
)

# Список исходных файлов для тестов
set(TEST_SOURCES
    tests/tests_main.cpp
    tests/test_ascii_converter.cpp
    tests/test_video_source.cpp
    src/ascii_converter.cpp
    src/video_source.cpp
    src/logger.cpp
)

# Создание исполняемого файла для сервера
add_executable(server ${SERVER_SOURCES})

# Подключение заголовочных файлов
target_include_directories(server PRIVATE 
    include
    ${OpenCV_INCLUDE_DIRS}
)

# Линковка с библиотеками
target_link_libraries(server PRIVATE 
    Boost::system 
    Boost::thread
    ${OpenCV_LIBS}
    nlohmann_json::nlohmann_json
    spdlog::spdlog
)

# Для Windows дополнительно линкуем с системными библиотеками
if(WIN32)
    target_link_libraries(server PRIVATE 
        ws2_32
        comctl32
        gdi32
        ole32
        setupapi
        winmm
    )
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

# Включение тестов
option(BUILD_TESTS "Build tests" OFF)

if(BUILD_TESTS)
    # Поиск GoogleTest
    find_package(GTest REQUIRED)
    
    # Создание тестовой цели
    add_executable(tests ${TEST_SOURCES})

    target_include_directories(tests PRIVATE 
        include
        ${OpenCV_INCLUDE_DIRS}
        ${Boost_INCLUDE_DIRS}
    )
    
    target_link_libraries(tests PRIVATE
        Boost::system
        GTest::gtest_main
        GTest::gmock
        GTest::gmock_main
        ${OpenCV_LIBS}
        nlohmann_json::nlohmann_json
        spdlog::spdlog
    )
    
    # Добавление в CTest
    include(GoogleTest)
    gtest_discover_tests(tests)
endif()