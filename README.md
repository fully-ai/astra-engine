# Astra Engine

**Astra Engine** is an independent cross-platform C++20 3D game engine and visual editor built on SDL2 and OpenGL 3.3 Core (with optional headless EGL context verification).

> **Maturity:** Alpha (`v0.1.0-alpha.1`) — Core math/JSON, sparse ECS & hierarchy, binary glTF 2.0 / `.glb` & `.wav` asset pipeline, OpenGL 3.3 PBR + shadow mapping renderer, AABB/sphere physics & raycasts, AstraScript hot-reloadable scripting VM, interactive visual editor (`astra-editor`), packaging CLI (`astra-build`), and playable 3D sample game (`astra-sample`).

## Modules & Applications

- **`astra-editor`**: Interactive 3D visual editor with orbit/zoom viewport, raycast object selection, hierarchy & inspector overlays, profiler metrics, Play/Stop live simulation, primitive spawning, and `.astra.json` scene serialization.
- **`astra-sample` ("Starlight Collector")**: Playable 3D obstacle & orb collector game featuring WASD + Jump rigid-body physics, orbiting `.glb` collectibles with trigger callbacks, chime `.wav` audio, patrolling scripted obstacles, directional shadows, and victory HUD.
- **`astra-build`**: Packaging CLI copying scene manifests, `.glb`/`.gltf`, `.wav`, `.tga`, and `.as` scripts into a clean distributable directory.

## Building & Testing (Linux)

Requires `cmake` (3.20+), C++20 compiler (`g++`/`clang++`), `SDL2`, `GLEW`, `OpenGL`, and optionally `GTest` + `EGL`.

```bash
git clone https://github.com/fully-ai/astra-engine
cd astra-engine
./scripts/build.sh
./scripts/test.sh
./build/astra-sample
```

## Building on Windows

Install `SDL2`, `GLEW`, and `CMake` (via `vcpkg` or MSYS2) and run `./scripts/build.ps1`. Note: Windows compilation is configured in CI (`windows-latest` with vcpkg) and has not been directly executed on a Windows host in this Linux environment.

## Documentation & License
- [ARCHITECTURE.md](ARCHITECTURE.md), [docs/THREAT_MODEL.md](docs/THREAT_MODEL.md), [SECURITY.md](SECURITY.md)
- Apache License 2.0 ([LICENSE](LICENSE), [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md))
