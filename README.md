# 3D Fractal Screensaver

A lightweight, high-performance Windows screensaver (`.scr`) and interactive real-time tuning tool (`fractals_debug.exe`) written in native C++ using Win32 API and GPU-accelerated OpenGL raymarching.

## Features
- **5 Beautiful 3D Fractals:** Renders complex 3D fractals (Mandelbulb, Julia Quaternion, Jerusalem Cube, Sierpinski Pyramid, Menger Sponge) in real-time at 60 FPS directly on the GPU using GLSL shaders.
- **Adaptive Lightweight Antialiasing:** Applies a fast sub-pixel supersampling filter specifically in high-frequency/noisy zones (where ray steps are high), eliminating noise without a performance penalty on flat regions.
- **Synchronized Palette & Independent Views:** Multi-display setup shares a global color palette and fade-in/fade-out transitions, but renders independent scenes and camera fly-through angles per monitor.
- **Multi-Monitor Support:** Automatically detects all connected displays, launching a fullscreen borderless window with its own hardware-accelerated OpenGL context on each screen.
- **Dynamic Camera Fly-Throughs:** Features smooth, slow camera paths animated in C++ to travel through the heart of the fractals.
- **Cosine Gradient Palettes:** Cycles through 50 beautifully calculated color gradients, transitioning smoothly using a fade-out/fade-in animation every 30 seconds.
- **Robust Input Handling:** Gracefully handles cursor wiggle room (customizable `#define` threshold, default 10px) and key presses to prevent accidental exits.
- **Zero External Dependencies:** Built purely on standard Windows systems libraries (`opengl32.dll`, `gdi32.dll`, `user32.dll`), compiling into a single standalone executable (<190 KB).

---

## Directory Structure

```text
.\Fractals
|- LICENSE              # MIT license documentation
|- README.md            # Repository documentation (this file)
|- docs/
|  L README.md          # Additional documentation
|- libs/                # Dependencies (unused, built native)
|- logs/
|  |- debug_vars.cfg    # Persisted debug parameter coordinates
|  L runtime.log        # Diagnostic output from screensaver runtime
|- src/
|  |- main.cpp          # Screensaver engine, WinMain entry point
|  |- shaders.h         # GLSL vertex & raymarching fragment shaders
|  L palettes.h         # Procedural cosine gradient palettes
|- target/
|  |- fractals_debug.exe # Debug window control utility
|  L fractals.scr       # Production screensaver binary
```

---

## Compilation

The project uses MinGW `g++` (version 14.2.0+) and standard system dynamic link libraries.

### Compile the Screensaver
Run this command from the repository root directory:
```powershell
g++ -O3 -mwindows src/main.cpp -o target/fractals.scr -lopengl32 -lgdi32 -luser32 -lcomctl32
```
*Note: The `-mwindows` flag compiles it as a GUI subsystem application, preventing a blank command window from launching behind the screensaver.*

---

## Command-Line Arguments (`fractals.scr`)

The screensaver supports standard Windows screensaver arguments and custom configuration parameter overrides:
* `target\fractals.scr /s`: Run the screensaver in **fullscreen screensaver mode** across all monitors (does not write logs).
* `target\fractals.scr` (or with `/c`): Show the interactive **configuration / settings control panel** window. This opens the GUI panel with real-time sliders and inputs (writes debug logs to `logs/runtime.log`).
* `target\fractals.scr /p <HWND>`: Run the screensaver in **preview mode** inside the child window of the specified parent window handle.

### Custom Configuration Overrides (Case-Insensitive "/Letter" flags):
You can override initial settings on startup when executing the screensaver:
* `/f <index>`: Select starting fractal scene (`0`: Mandelbulb, `1`: Julia, `2`: Jerusalem Cube, `3`: Sierpinski, `4`: Menger).
* `/t <index>`: Select starting gradient palette index (`0` - `49`).
* `/a <0|1>`: Enable (`1`) or disable (`0`) adaptive antialiasing.
* `/r <scale>`: Set initial resolution scale (`0.1` - `1.0`).
* `/st <steps>`: Set maximum raymarch step limit (`10` - `150`).
* `/sp <speed>`: Set camera animation speed scale (`0.0` - `5.0`).
* `/l` or `/log`: Force writing diagnostic logs to `logs/runtime.log` (enabled by default in `/c` mode).
* `/hl` or `/hide-legend`: Hide the visual on-screen stats legend overlay at the bottom-left of the display.

---

## Installation & Explorer Context Menu

To install and use this screensaver on your system:
1. Copy `target/fractals.scr` and place it in your Windows System32 folder (usually `C:\Windows\System32`).
2. Alternatively, right-click `fractals.scr` in File Explorer:
   - Select **Install** to open the Windows Screen Saver Settings control panel with this screensaver selected.
   - Select **Run** (renamed from "Test") to run the screensaver fullscreen.
   - Select **Configurar** (Configure) to open the interactive configuration panel.

---

## Configuration & Tuning Panel Hotkeys (`/c` mode)

When running in configuration mode (`target\fractals.scr /c`), you can manipulate the simulation parameters interactively using standard control panel sliders/inputs, or with these hotkeys:
* **`P`**: Pause / Resume the camera fly-through tracks.
* **`F`** / **`B`**: Cycle the fractal type forward (`F`) or backward (`B`).
* **`C`**: Cycle through the 50 gradient palettes.
* **`A`**: Toggle adaptive lightweight antialiasing.
* **`H`**: Toggle the variables control panel visibility.
* **`Up Arrow`** / **`Down Arrow`**: Increase or decrease the camera speed (0.0x to 5.0x scale).
* **`Escape`**: Exit configuration mode cleanly.

