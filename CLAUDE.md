# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**OrbitalSim** — Real-time 3D simulation of low Earth orbit, deorbit burn, and atmospheric reentry using C++17 and OpenGL 3.3. Currently at v0.0.1 (early prototype): the visual layer is functional (interactive wireframe sphere), but the physics engine is not yet implemented.

## Build

```bash
# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Compile
cmake --build build --parallel

# Run
./build/OrbitalSim
```

On Windows, you can also open `ISAR simu.vcxproj` directly in Visual Studio 2022 (x64 platform). All dependencies are pre-bundled — no package installation needed.

## Architecture

### Current state

All logic lives in `main.cpp` (monolithic):
- GLFW window + OpenGL 3.3 Core Profile initialization
- GLAD loader (`glad.c` compiled alongside)
- Embedded GLSL shaders (no separate shader files)
- Arcball camera (LMB drag = rotate, scroll = zoom)
- UV-sphere wireframe (24 stacks × 36 slices, `GL_LINES`)

### Planned modular structure (from `files/README.md` + `files/CMakeLists.txt`)

```
include/simulation/   Physics.h, Atmosphere.h, Spacecraft.h, OrbitalElements.h
include/rendering/    Renderer.h, Shader.h, Camera.h, Mesh.h
include/core/         Window.h, InputHandler.h, Timer.h
src/                  Corresponding .cpp implementations
```

As modules are extracted from `main.cpp` into `src/`, update `CMakeLists.txt` accordingly — the reference config in `files/CMakeLists.txt` already accounts for this layout.

### Physics (to be implemented)

| Module | Details |
|--------|---------|
| Gravity | Two-body, μ_Earth = 3.986 × 10¹⁴ m³/s² |
| Integration | RK4 — 10 s timestep (orbit), 0.5 s (reentry) |
| Atmosphere | Exponential: ρ = ρ₀·exp(−h/H), H = 8 500 m |
| Aerodynamics | Drag + lift (Cd, Cl, A_ref configurable) |
| Heat flux | Detra–Kemp–Riddell simplified: q ~ √(ρ/R_n)·v³ |
| Orbital elements | Bidirectional ECI ↔ Keplerian (a, e, i, Ω, ω, ν) |

Default scenario: 200 km circular orbit (28.5° inclination), −120 m/s retrograde burn, Crew Dragon-like capsule (m = 9 500 kg, Cd = 1.28, A = 12 m²).

## Dependencies (all bundled)

| Library | Location | Notes |
|---------|----------|-------|
| GLFW 3.3 | `libraries/` | Pre-built x64 `.lib` |
| GLM 0.9.9.8 | `include/glm/` | Header-only |
| GLAD (OpenGL 3.3 Core) | `glad.c` + `libraries/include/glad/` | Compiled as part of build |

`include/glm/` and `libraries/` are `.gitignore`d — don't commit them.

## Controls (target)

| Input | Action |
|-------|--------|
| LMB drag | Rotate camera |
| RMB drag | Pan camera |
| Scroll | Zoom |
| Space | Pause / Resume |
| N | Step one frame (while paused) |
| +/− | Speed ×2 / ÷2 |
| R | Reset camera |
| ESC / Q | Quit |

## Roadmap (next steps)

- Physics engine: `Spacecraft`, `Physics`, `Atmosphere`, `OrbitalElements` classes
- Trajectory visualization (polyline of state vectors)
- Telemetry HUD (ImGui: altitude, velocity, Mach, G-load, heat flux)
- Earth texture (NASA Blue Marble)
- NRLMSISE-00 atmosphere model
- J2 perturbation (Earth oblateness)
- CSV telemetry export
- JSON scenario configuration
