@echo off
rem Compiles VOID RUNNER for the Arduino Mega into build\ (used by Wokwi for VS Code).
set ACLI=%LOCALAPPDATA%\Programs\arduino-cli\arduino-cli.exe
"%ACLI%" compile --fqbn arduino:avr:mega --warnings all --output-dir "%~dp0build" "%~dp0void_runner"
