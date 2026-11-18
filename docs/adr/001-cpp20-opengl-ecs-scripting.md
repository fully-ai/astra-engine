# ADR 001 — C++20, OpenGL 3.3 Core, Binary glTF/GLB, and AstraScript VM

- **Status:** Accepted
- **Date:** 2026-11-18

## Decision
1. **Core Language & Build**: C++20 + CMake 3.20+ with `SDL2`, `GLEW`, and `OpenGL 3.3 Core` for cross-platform Linux and Windows support without heavyweight third-party framework lock-in.
2. **Rendering & Testability**: Implement real OpenGL 3.3 directional shadow mapping + Cook-Torrance PBR lighting plus headless EGL offscreen testing so CI verifies actual GPU shader compilation and frame output.
3. **Asset Pipeline**: Implement a zero-dependency glTF 2.0 / `.glb` binary parser, `.tga`/`.ppm` texture loader, `.wav` loader, and asset cache with mtime reload.
4. **Scripting**: Embed **AstraScript** (`.as`), a deterministic sandboxed scripting interpreter exposing entity movement, physics impulses, triggers, score state, and hot reload.
