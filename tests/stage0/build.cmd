@echo off
rem Compiles the Stage 0 sketch for the Arduino Mega into build\ (used by Wokwi for VS Code).
set ACLI=%LOCALAPPDATA%\Programs\arduino-cli\arduino-cli.exe
"%ACLI%" compile --fqbn arduino:avr:mega --output-dir "%~dp0build" "%~dp0stage0"
