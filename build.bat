@echo off
setlocal

set "BUILD_DIR=build"

:: Установка значения флага по умолчанию
set "BUILD_TESTS=OFF"
set "CONFIGURATION=Release"

:: Проверка аргументов командной строки для изменения значения флага
for %%A in (%*) do (
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
    echo.
    echo Deleting a directory %BUILD_DIR%...
    rmdir /s /q "%BUILD_DIR%"
)

echo.
echo Creating a directory %BUILD_DIR%...
mkdir "%BUILD_DIR%"

echo.
echo Going to the directory %BUILD_DIR%...
cd "%BUILD_DIR%"

echo.
echo CMake execution with BUILD_TESTS=%BUILD_TESTS%...
cmake -DBUILD_TESTS=%BUILD_TESTS% ..

echo.
echo Project assembly in %CONFIGURATION% configuration...
cmake --build . --config %CONFIGURATION%

:: Проверка кода возврата сборки
echo.
if %ERRORLEVEL% equ 0 (
    echo The project has been successfully assembled.
) else (
    echo The project assembly failed with error code %ERRORLEVEL%.
)

:: Ожидание нажатия клавиши перед закрытием консоли
pause

endlocal
