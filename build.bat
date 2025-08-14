@echo off
setlocal

set "BUILD_DIR=build"

:: Установка значения флага по умолчанию
set "BUILD_TESTS=OFF"
set "CONFIGURATION=Release"

:: Проверка аргументов командной строки для изменения значения флага
for %%A in (%*) do (
    echo A = %%A
    if "%%~A"=="tests--OFF" (
        set "BUILD_TESTS=OFF"
    ) else if "%%~A"=="tests--ON" (
        set "BUILD_TESTS=ON"
    ) else if "%%~A"=="config--Release" (
        set "CONFIGURATION=Release"
    ) else if "%%~A"=="config--Debug" (
        set "CONFIGURATION=Debug"
    )
)

if exist "%BUILD_DIR%" (
    echo Deleting a directory %BUILD_DIR%...
    rmdir /s /q "%BUILD_DIR%"
)

echo Creating a directory %BUILD_DIR%...
mkdir "%BUILD_DIR%"

echo Going to the directory %BUILD_DIR%...
cd "%BUILD_DIR%"

echo CMake execution with BUILD_TESTS=%BUILD_TESTS%...
cmake -DBUILD_TESTS=%BUILD_TESTS% .. -G "Visual Studio 17 2022"

echo Project assembly in %CONFIGURATION% configuration...
cmake --build . --config %CONFIGURATION%

echo The project has been successfully assembled.
endlocal
