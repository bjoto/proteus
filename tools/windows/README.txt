Proteus boat study - Windows x64

Extract the entire ZIP, then double-click launch.bat to explore Eriksberg
with GPU rendering and 4x4 supersampling. Keep assets beside boat_study.exe.
Requires a GPU/driver supporting OpenGL 3.3. The build uses static raylib
and the static MSVC runtime.

A/D or Left/Right: adjust held wheel setting
W/S or Up/Down: adjust held thrust, including reverse
R: reset boat
+/-: camera distance
PageUp/PageDown: camera height
C: reset camera
Esc: quit

To compare performance without supersampling, run from PowerShell:
  .\boat_study.exe --location eriksberg --gpu

If GPU startup fails, launch from PowerShell to read the diagnostic output.
CPU rendering is also available:
  .\boat_study.exe --location eriksberg

When reporting issues, include build.txt, Windows version, GPU model, and
whether the problem also happens without --gpu-aa.

Map data (c) OpenStreetMap contributors, ODbL 1.0:
https://www.openstreetmap.org/copyright
Source details: assets/geometry/eriksberg/manifest.json
raylib and GLFW license notices are included alongside this file.
