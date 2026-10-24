@echo off
cd /d "%~dp0"
wake_velocity.exe
if errorlevel 1 pause
