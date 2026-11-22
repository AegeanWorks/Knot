@echo off
rem Canonical regression gate for Knot: compile, run unit tests, smoke the CLI.
setlocal
set BUILD=build
if not exist "%BUILD%" mkdir "%BUILD%"
set CORE=src\core\lexer.cpp src\core\graph.cpp src\core\path.cpp src\core\json.cpp src\core\diff.cpp src\core\impact.cpp src\core\export.cpp src\core\ignore.cpp src\core\policy.cpp src\core\missing.cpp src\core\scaffold.cpp src\core\stats.cpp src\core\paths.cpp src\core\csv.cpp src\core\html.cpp src\core\report.cpp
g++ -std=c++17 -Wall -Wextra -O2 -Isrc %CORE% src\shell\cli.cpp tests\test_all.cpp -o "%BUILD%\knot_tests.exe"
if errorlevel 1 exit /b 1
"%BUILD%\knot_tests.exe"
if errorlevel 1 exit /b 1
g++ -std=c++17 -Wall -Wextra -O2 -Isrc %CORE% src\shell\cli.cpp src\shell\files.cpp src\shell\main.cpp -o "%BUILD%\knot.exe"
if errorlevel 1 exit /b 1
"%BUILD%\knot.exe" demo > "%BUILD%\demo.out"
if errorlevel 1 exit /b 1
echo OK: knot tests and demo passed
exit /b 0
