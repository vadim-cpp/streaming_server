Сборка проекта:
mkdir build
cd build
cmake ..

Компиляция:
Пуск → Visual Studio 2022 → x64 Native Tools Command Prompt
cd C:\Users\vadim_25\Desktop\web_cpp\build
cmake --build . --config Release

Запуск:
cd Release
copy /Y ..\..\web\* .
server.exe