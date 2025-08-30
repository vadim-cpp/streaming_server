cmake_minimum_required(VERSION 3.15)

# Настройка для Windows
if(WIN32)
    add_compile_definitions(_WIN32_WINNT=0x0601)
    
    if(EXISTS "C:/vcpkg/scripts/buildsystems/vcpkg.cmake")
        set(CMAKE_TOOLCHAIN_FILE "C:/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")
    endif()
endif()

project(tests)

# Установка стандарта C++
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Настройка для Linux
if(UNIX)
    add_compile_options(-pthread)
    add_link_options(-pthread)
endif()

# Поиск необходимых библиотек
find_package(OpenCV REQUIRED)
find_package(Boost 1.70 REQUIRED COMPONENTS system thread)
find_package(nlohmann_json REQUIRED)
find_package(spdlog REQUIRED)
find_package(GTest REQUIRED)

# Список исходных файлов для тестов
set(TEST_SOURCES
    src/tests_main.cpp
    src/test_ascii_converter.cpp
    src/test_video_source.cpp
    src/test_stream_controller.cpp
    ../src/ascii_converter.cpp
    ../src/video_source.cpp
    ../src/logger.cpp
    ../src/stream_controller.cpp
    ../src/websocket_session.cpp
)

# Создание тестовой цели
add_executable(tests ${TEST_SOURCES})

target_include_directories(tests PRIVATE 
    ../include
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

# Платформозависимые настройки линковки
if(WIN32)
    target_link_libraries(tests PRIVATE 
        ws2_32
        comctl32
        gdi32
        ole32
        setupapi
        winmm
    )
elseif(UNIX)
    target_link_libraries(tests PRIVATE pthread)
endif()

# Добавление в CTest
include(GoogleTest)
gtest_discover_tests(tests)
