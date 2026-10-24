@echo off
rem Opens a multiplayer room on every network interface (UDP port 24680 unless --port is given).
rem Extra options are passed through, for example: host.bat --waves
cd /d "%~dp0"
echo Friends join with MULTIPLAYER and this machine's address, for example 192.168.1.20:24680.
echo Close this window or press Ctrl+C to end the room.
boat_server.exe --bind 0.0.0.0 %*
if errorlevel 1 pause
