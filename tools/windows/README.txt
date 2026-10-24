Proteus boat study - Windows x64

Extract the entire ZIP, then double-click launch.bat (or boat_study.exe) to open
the Wake Velocity title. Pick NAME and BOAT, then SINGLE PLAYER for the river, or
MULTIPLAYER to type a room's HOST:PORT address and connect. Esc in a session
returns to the title. Keep assets beside boat_study.exe.
Requires a GPU/driver supporting OpenGL 3.3. The build uses static raylib
and the static MSVC runtime.

A/D or Left/Right: adjust held wheel setting
W/S or Up/Down: adjust held thrust, including reverse
R: reset boat
+/-: camera distance
PageUp/PageDown: camera height
C: reset camera
F3: toggle profiler overlay
Esc: back to the title (Esc again on the title quits)

Explicit options skip the title. Eriksberg with 4x4 supersampling, from PowerShell:
  .\boat_study.exe --location eriksberg --gpu --gpu-aa

If GPU startup fails, launch from PowerShell to read the diagnostic output.
CPU rendering is also available:
  .\boat_study.exe --renderer cpu --location eriksberg

For the expanded river and bridge, use --location river. Add --waves for gentle
waves in any location. Run .\boat_study.exe --help for course and replay options.

When reporting issues, include build.txt, Windows version, GPU model, and
whether the problem also happens without --gpu-aa.

Map data (c) OpenStreetMap contributors, ODbL 1.0:
https://www.openstreetmap.org/copyright
Source details: assets/geometry/eriksberg/manifest.json
raylib and GLFW license notices are included alongside this file.
