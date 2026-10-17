@echo off
cd /d "%~dp0"
boat_study.exe --location eriksberg --gpu --gpu-aa
if errorlevel 1 pause
