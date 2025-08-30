#!/bin/bash

BUILD_DIR="build"
BUILD_TESTS="OFF"
CONFIGURATION="Release"

# Парсинг аргументов командной строки
for arg in "$@"
do
    case $arg in
        tests--OFF)
        BUILD_TESTS="OFF"
        shift
        ;;
        tests--ON)
        BUILD_TESTS="ON"
        shift
        ;;
        config--Release)
        CONFIGURATION="Release"
        shift
        ;;
        config--Debug)
        CONFIGURATION="Debug"
        shift
        ;;
    esac
done

# Удаление существующей директории сборки
if [ -d "$BUILD_DIR" ]; then
    echo
    echo "Deleting directory $BUILD_DIR..."
    rm -rf "$BUILD_DIR"
fi

# Создание директории сборки
echo
echo "Creating directory $BUILD_DIR..."
mkdir "$BUILD_DIR"

# Переход в директорию сборки
echo
echo "Entering directory $BUILD_DIR..."
cd "$BUILD_DIR"

# Запуск CMake
echo
echo "Running CMake with BUILD_TESTS=$BUILD_TESTS..."
cmake -DBUILD_TESTS=$BUILD_TESTS ..

# Сборка проекта
echo
echo "Building project in $CONFIGURATION configuration..."
if [ "$CONFIGURATION" = "Release" ]; then
    cmake --build . --config Release
else
    cmake --build . --config Debug
fi

# Проверка результата сборки
echo
if [ $? -eq 0 ]; then
    echo "Project built successfully."
else
    echo "Project build failed with error code $?."
    exit 1
fi

# Ожидание нажатия клавиши (опционально)
read -p "Press any key to continue..." -n1 -s
echo

exit 0