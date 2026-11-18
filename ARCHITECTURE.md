# Astra Engine Architecture

Astra Engine is a modular C++20 3D game engine and visual editor built on SDL2 and OpenGL 3.3 Core (with optional headless EGL context support for automated rendering tests).

```
+-----------------------------------------------------------------------+
| Applications: astra-editor | astra-sample (Game) | astra-build (CLI)  |
+-----------------------------------------------------------------------+
| Visual Editor (Scene Viewport, Hierarchy, Inspector, Gizmos, Play/Stop) |
+--------------------------+-----------------------+--------------------+
| ECS & Scene Graph        | AstraScript VM        | Audio (SDL2/WAV)   |
|  - Transform Hierarchy   |  - Lifecycle Callbacks|  - Null-safe Mixer |
|  - JSON Scene/Prefabs    |  - Hot Reload (.as)   |                    |
+--------------------------+-----------------------+--------------------+
| Physics & Collision      | Asset Pipeline        | OpenGL 3.3 PBR     |
|  - AABB/Sphere Impulses  |  - Binary GLB / glTF  |  - Depth ShadowMap |
|  - Raycasts & Triggers   |  - TGA/PPM/WAV Cache  |  - Dir/Point/Spot  |
+-----------------------------------------------------------------------+
| Core Math (Vec2/3/4, Quat, Mat4, Ray, AABB) & RFC-8259 JSON           |
+-----------------------------------------------------------------------+
```
