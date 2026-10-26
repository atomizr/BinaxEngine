# Binax Engine

<p align="center">
  <b>A custom game engine and editor built with C++20 / OpenGL 4.6</b><br>
  Deferred rendering · PBR · ECS · GPU-driven pipeline · Built-in scene editor
</p>

<p align="center">
  <a href="https://github.com/atomizr/BinaxEngine/blob/main/LICENSE"><img alt="License" src="https://img.shields.io/badge/license-MIT-blue.svg"></a>
  <img alt="Language" src="https://img.shields.io/badge/language-C%2B%2B20-orange.svg">
  <img alt="OpenGL" src="https://img.shields.io/badge/OpenGL-4.6-5586a4.svg">
  <img alt="Status" src="https://img.shields.io/badge/status-work%20in%20progress-yellow.svg">
</p>

---

## 📖 About

**Binax Engine** is a game engine and scene editor being developed from scratch in **C++20** using **OpenGL 4.6 Core**. The project serves as a hands-on learning and experimentation platform for studying the internals of modern graphics engines — from deferred rendering and PBR to GPU-driven rendering and ECS architecture.

The engine includes a full-featured editor with a scene hierarchy, component inspector, gizmos, material editor, and JSON serialization.

> ⚠️ The project is under active development. APIs and file formats may change without backward compatibility.

---

## ✨ Current Features

### Graphics & Rendering
- **Deferred Shading** with a G-Buffer (positions, normals, albedo, material parameters, velocity).
- **PBR lighting** with support for up to **32 light sources**: directional, point, and spot lights.
- **Bindless textures** (`GL_ARB_bindless_texture`) for efficient material handling.
- **GPU-driven rendering**:
  - Compute-shader frustum culling.
  - Indirect draws (`glMultiDrawElementsIndirectCount`).
  - Instancing with SSBOs for instances, materials, and meshes.
- **Skybox** based on a cubemap with rotation and adjustable brightness.
- **Post-processing**:
  - Tonemapping: ACES, Reinhard, Filmic, AgX, Linear.
  - TAA (Temporal Anti-Aliasing) with projection jitter and frame history.
  - FXAA (as an alternative to TAA).
  - Bloom (multi-mip down/up-sampling).
  - Motion Blur based on the velocity buffer.
  - Vignette, film grain, chromatic aberration, lens distortion.
  - Fog: linear, exponential, exponential squared.
  - Color grading: exposure, contrast, saturation, temperature, tint, lift/gamma/gain.

### Architecture
- **ECS based on [EnTT](https://github.com/skypjack/entt)** — components: `Transform`, `MeshComponent`, `MaterialComponent`, `CameraComponent`, `LightComponent`, `SkyboxComponent`, `PostProcessComponent`, `ParentComponent`, `WorldTransform`, `PrevTransform`, etc.
- **Transform hierarchy** with cached world matrices and cycle protection.
- **Reflection system** for automatic inspector widget generation and serialization.
- **JSON serialization** with versioning:
  - Scene (`.bxlvl`)
  - Materials (`.bxmat`)
  - Post-processing (`.bxdata`)

### Editor
- **Scene hierarchy** with drag-and-drop, search, entity creation, deletion, and duplication.
- **Inspector** powered by reflection — automatic widgets for all registered components.
- **Viewport with gizmos** ([ImGuizmo](https://github.com/CedricGuillemet/ImGuizmo)): translate, rotate, scale with grid snapping.
- **Material editor** — edit PBR parameters and assign textures via the built-in file browser.
- **Stats panel** — FPS, frame time, draw calls, vertices, triangles, instances, GPU culling and post-processing state.
- **Hot shader reloading**.
- One-click scene save/load.

---

## 🛠️ Tech Stack

| Category | Technology |
|----------|-----------|
| Language | C++20 |
| Graphics | OpenGL 4.6 Core |
| Windowing / Input | [GLFW](https://www.glfw.org/) |
| OpenGL Loader | [GLAD](https://github.com/Dav1dde/glad) |
| Math | [glm](https://github.com/g-truc/glm) |
| ECS | [EnTT](https://github.com/skypjack/entt) |
| Editor UI | [Dear ImGui](https://github.com/ocornut/imgui) + [ImGuizmo](https://github.com/CedricGuillemet/ImGuizmo) |
| Logging | [spdlog](https://github.com/gabime/spdlog) |
| Image Loading | [stb_image](https://github.com/nothings/stb) |
| JSON | [nlohmann/json](https://github.com/nlohmann/json) |
| Build System | CMake |

---

## 🚧 Roadmap / In Development

The project is actively evolving. Upcoming features include:

### 🔦 Shadows
- Shadows from directional and point lights.
- Cascaded Shadow Maps (CSM) for directional light.
- Soft shadows (PCF / PCSS).

### 🌍 Image-Based Lighting (IBL)
- Prefiltered environment maps (diffuse irradiance + specular prefilter).
- BRDF LUT for PBR lighting.
- Integration with the scene skybox.

### 🌫️ Particle System
- GPU-driven particle system with emitters and modulators.
- Support for various particle types (sprites, meshes).
- Editor integration.

### ⚙️ Physics — Jolt Physics
- Integration of [Jolt Physics](https://github.com/jrouwe/JoltPhysics).
- `RigidBody`, `Collider`, `CharacterController` components.
- ECS ↔ physics world transform synchronization.

### 🔊 Audio — miniaudio
- Integration of [miniaudio](https://github.com/mackron/miniaudio) as the audio backend.
- `AudioSource` (2D/3D) and `AudioListener` components.
- Support for WAV/MP3/FLAC and streaming.

### 🧊 Model Import — Assimp
- Integration of [Assimp](https://github.com/assimp/assimp) for importing popular formats (glTF, FBX, OBJ, DAE, etc.).
- Skeletal animation support.
- Automatic material creation from imported data.

### 🐍 Scripting — Lua
- Embedding [Lua](https://www.lua.org/) (via [sol2](https://github.com/ThePhD/sol2)) as the scripting language.
- `ScriptComponent` with entity lifecycle hooks (`OnCreate`, `OnUpdate`, `OnDestroy`).
- API for ECS, input, transforms, and audio access.

### Other
- Editor improvements: undo/redo, asset browser, material graphs.
- Optimization: cluster-based lighting, potential Vulkan backend.
- Cross-platform builds (Windows / Linux).

---

## 🚀 Building

### Requirements
- **Compiler:** MSVC 2022+, Clang 15+, or GCC 12+ with C++20 support.
- **GPU:** OpenGL 4.6 support with the following extensions:
  - `GL_ARB_bindless_texture`
  - `GL_ARB_shader_draw_parameters`
- **CMake:** 3.20+

### Instructions
```bash
git clone https://github.com/atomizr/BinaxEngine.git
cd BinaxEngine

cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

./build/bin/BinaxEngine
```

---

## 📂 Project Structure

```
BinaxEngine/
├── src/
│   ├── Core/            # Window, input, timer
│   ├── ECS/             # Components and systems (EnTT)
│   ├── Editor/          # Editor: UI, viewport, camera, gizmos
│   ├── Reflect/         # Component reflection system
│   ├── Renderer/        # Renderer, materials, shaders, meshes
│   ├── Serialization/   # JSON serialization for scene/materials
│   ├── Utils/           # Logging, paths, file I/O
│   └── main.cpp
├── resources/
│   ├── shaders/         # GLSL shaders
│   ├── materials/       # .bxmat files
│   ├── fonts/           # Editor fonts
│   └── icons/           # UI icons
├── assets/              # User assets
└── CMakeLists.txt
```

---

## 🎮 Editor Controls

| Action | Key |
|--------|-----|
| Move camera | RMB + WASD / QE |
| Speed up camera | Shift |
| Translate object (gizmo) | W |
| Rotate object | E |
| Scale object | R |
| Toggle World/Local | X |
| Focus on selection | F |
| Duplicate | Ctrl + D |
| Delete | Delete |
| Search in hierarchy | Ctrl + F |
| Exit | Esc |

---

## 📸 Screenshots

> _To be added as the project evolves. Stay tuned!_

---

## 🤝 Contributing

The project is at an early stage, but suggestions and pull requests are welcome. If you find a bug or want to discuss a feature, feel free to open an [issue](https://github.com/atomizr/BinaxEngine/issues).

---

## 📄 License

This project is licensed under the **MIT License**. See the [LICENSE](https://github.com/atomizr/BinaxEngine/blob/main/LICENSE) file for details.

---

## 🙏 Acknowledgements

Huge thanks to the authors of the libraries and projects the engine is built upon:

- **EnTT** — for the elegant ECS architecture.
- **Dear ImGui** and **ImGuizmo** — for the powerful and flexible editor tooling.
- **OpenGL** community — for shader techniques and GPU-driven rendering articles.
- **LearnOpenGL** and **The Book of Shaders** — for educational materials.

---

<p align="center">
  <i>Made with ❤️ for graphics and game dev.</i>
</p>
