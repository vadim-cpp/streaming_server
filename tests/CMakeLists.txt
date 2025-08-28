cmake_minimum_required(VERSION 3.15)
set(CMAKE_TOOLCHAIN_FILE "C:/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")
project(tests)

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
    ../src/ascii_converter.cpp
    ../src/video_source.cpp
    ../src/logger.cpp
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

# Добавление в CTest
include(GoogleTest)
gtest_discover_tests(tests)
