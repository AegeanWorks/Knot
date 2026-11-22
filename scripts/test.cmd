@echo off
rem Canonical regression gate for Knot: compile, run unit tests, smoke the CLI.
setlocal
set BUILD=build
if not exist "%BUILD%" mkdir "%BUILD%"
g++ -std=c++17 -Wall -Wextra -O2 -Isrc src\core\lexer.cpp src\core\graph.cpp src\core\report.cpp src\shell\cli.cpp tests\test_all.cpp -o "%BUILD%\knot_tests.exe"
if errorlevel 1 exit /b 1
"%BUILD%\knot_tests.exe"
if errorlevel 1 exit /b 1
g++ -std=c++17 -Wall -Wextra -O2 -Isrc src\core\lexer.cpp src\core\graph.cpp src\core\report.cpp src\shell\cli.cpp src\shell\files.cpp src\shell\main.cpp -o "%BUILD%\knot.exe"
if errorlevel 1 exit /b 1
"%BUILD%\knot.exe" demo
if errorlevel 1 exit /b 1
echo OK: knot tests and demo passed
exit /b 0
