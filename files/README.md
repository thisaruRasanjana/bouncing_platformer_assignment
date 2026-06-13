# 3D Bouncing Ball Platformer — CG Demo

A single-file OpenGL/FreeGLUT C++ demo built around 5 Computer Graphics
concepts, matching the 5-member role plan. Game logic (collisions, score,
menus) is intentionally omitted — this is a graphics techniques showcase.

## Files
- `main.cpp` — all source code, organized into clearly labeled sections per role
- `CMakeLists.txt` — CMake build (cross-platform)
- `Makefile` — quick build for Linux/macOS

## Build & Run

### Linux (Ubuntu/Debian)
```bash
sudo apt-get install freeglut3-dev
make
./platformer
```

### macOS
```bash
make
./platformer
```

### Windows (MSYS2/MinGW with freeglut)
```bash
g++ main.cpp -o platformer.exe -lfreeglut -lopengl32 -lglu32
platformer.exe
```

### CMake (any platform)
```bash
mkdir build && cd build
cmake ..
cmake --build .
./platformer
```

## Controls
- `ESC` — quit
- `SPACE` — pause / resume animation

## Code Map (per role)

| Role | Concepts | Functions |
|---|---|---|
| Member 1 — Camera & Viewing | Perspective projection, gluLookAt, viewport | `setupProjection()`, `setupCamera()`, `reshape()` |
| Member 2 — Geometric Transformer | Translation/rotation matrices, matrix stack | `drawPlatforms()`, `drawGround()`, `drawUnitCube()` |
| Member 3 — Animator | Time-based motion, periodic bounce, squash & stretch | `updateBall()`, `drawBall()` |
| Member 4 — Illumination & Materials | Phong (ambient/diffuse/specular) | `setupLighting()`, `applyGlossyMaterial()`, `applyMatteMaterial()` |
| Member 5 — Rendering & Depth | Z-buffer, exponential fog | `initGL()` (depth test), `setupFog()` |

Each section in `main.cpp` is clearly commented with the corresponding
member/role so it can be split for individual work and then merged on Day 2.
