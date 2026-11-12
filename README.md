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

The engine includes a full-featured editor with a scene hierarchy, component inspector, gizmos, material editor, in-editor shader editor with live reload, and JSON serialization.

> ⚠️ The project is under active development. APIs and file formats may change without backward compatibility.

---

## ✨ Current Features

### Graphics & Rendering
- **Deferred Shading** with a G-Buffer (positions, normals, albedo, material parameters, velocity).
- **PBR lighting** with support for up to **32 light sources**: directional, point, and spot lights.
- **Bindless textures** (`GL_ARB_bindless_texture`) for efficient material handling.
- **GPU-driven rendering**:
  - Compute-shader frustum culling **at the instance-group level** (one workgroup per draw-group, one indirect draw per surviving group).
  - Indirect draws (`glMultiDrawElementsIndirectCount`).
  - Instancing with SSBOs for instances, materials, meshes, and per-group draw templates.
- **Skybox** based on a cubemap with rotation and adjustable brightness.
- **Post-processing**:
  - Tonemapping: ACES, Reinhard, Filmic, AgX, Linear.
  - TAA (Temporal Anti-Aliasing) with projection jitter, Halton sequence, and neighborhood clipping in YCoCg space.
  - FXAA (as an alternative to TAA).
  - Bloom (multi-mip down/up-sampling).
  - Motion Blur based on the velocity buffer, with depth-rejection.
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
- **Modern dark theme** — rounded corners, OKLAB-based color interpolation for smooth state transitions.
- **Animated UI** powered by [ImAnim](https://github.com/SoufianeKHIAT/im_anim):
  - Springy buttons and icon toggles with hover/press feedback.
  - Pulsing accent glow on the viewport when hovered.
  - Slide-in Material Editor with eased opacity.
  - Smooth FPS counter with history graph and frame-budget indicator.
- **Scene hierarchy** with drag-and-drop, search, entity creation, deletion, and duplication.
- **Inspector** powered by reflection — automatic widgets for all registered components.
- **Viewport with gizmos** ([ImGuizmo](https://github.com/CedricGuillemet/ImGuizmo)): translate, rotate, scale with grid snapping.
- **Material editor** — edit PBR parameters and assign textures via the built-in file browser.
- **In-editor shader editor** — tabbed GLSL/HLSL editor with syntax highlighting, auto-indentation, and full undo/redo:
  - Save → automatically triggers `DeferredRenderer::ReloadShaders()`, so changes are visible in the viewport immediately.
  - Reload discards local edits and re-reads from disk.
  - Scans `shaders/` every 2 s for new files.
  - Supports `.vert`, `.frag`, `.comp`, `.geom`, `.tesc`, `.tese`, `.glsl`.
- **Toast notifications** ([imgui-notify](https://github.com/patrickcjk/imgui-notify) + Font Awesome 5 icons) — non-intrusive feedback for Save / Load / Delete / Duplicate / Add / Remove.
- **Stats panel** — FPS, frame time, draw calls, vertices, triangles, instances, visible/culled instances, GPU culling and post-processing state.
- **Hot shader reloading.**
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
| UI Animation | [ImAnim](https://github.com/SoufianeKHIAT/im_anim) |
| Toast Notifications | [imgui-notify](https://github.com/patrickcjk/imgui-notify) |
| Icons | Font Awesome 5 (embedded `fa_solid_900`) |
| Code Editor | Custom `TextEditor` (based on [BalazsJako/ImGuiColorTextEdit](https://github.com/BalazsJako/ImGuiColorTextEdit)) |
| Logging | [spdlog](https://github.com/gabime/spdlog) |
| Image Loading | [stb_image](https://github.com/nothings/stb) |
| JSON | [nlohmann/json](https://github.com/nlohmann/json) |
| Build System | CMake |

---

## 🆕 Recently Added

### Editor UX
- **Modern dark theme** with rounded corners and an OKLAB-interpolated accent color (`#5C8FFF`). Replaces the previous light-gray Qt-style theme.
- **ImAnim integration** across the whole editor:
  - `AnimatedButton` / `AnimatedIconButton` — spring-scaled, color-transitioned buttons for the hierarchy toolbar, viewport toolbar, and gizmo mode toggles.
  - `ViewportFocusGlow` — a pulsing accent outline around the viewport while hovered.
  - Slide-in Material Editor via `iam_tween_float` (0.25 s, `ease_out_cubic`).
  - Smoothed FPS counter + `PlotLines` history + frame-budget progress bar (16.6 ms @ 60 FPS).
  - Per-frame `iam_update_begin_frame()` + `iam_clip_update(dt)`, with `iam_gc()` / `iam_clip_gc()` running once per second.
- **Toast notifications** for every editor action:
  - `Save` / `Load` → success / error toasts.
  - `Delete` / `Duplicate` / `Add Component` / `Remove Component` → info toasts.
  - All toasts are forced on top via `BringWindowToDisplayFront` (without stealing focus), so they're never covered by docked panels.

### Shader Editor
- **In-editor shader editing** (`src/Editor/ShaderEditor.{h,cpp}`):
  - Scans `shaders/` every 2 s.
  - Tabbed editor with GLSL syntax highlighting, dirty markers (`*`), and safe close.
  - **Save** writes the file and calls `DeferredRenderer::ReloadShaders()` — every pipeline (geometry, lighting, FXAA, TAA, tonemap, motion blur, bloom down/up, frustum cull) is reloaded through `Shader::Reload()`, so changes appear in the viewport on the next frame.
  - **Reload** discards local edits.
  - Toggled with **F4** or the **`</>`** button in the Hierarchy toolbar.
- `DeferredRenderer::ReloadShaders()` — new public method that reloads every shader program from disk without touching the rest of the renderer state.

---

## 🐛 Recent Fixes

The following issues were found during code review passes and fixed. They are noted here for anyone tracking project history.

### Graphics pipeline
- **TAA reprojection (Y-flip mismatch with Motion Blur).** `taa.frag` reprojected history with `vec2(vel.x, -vel.y)`, while `motion_blur.frag` (and the velocity buffer written by `geometry.vert`) used `vel` directly. This produced vertically mirrored reprojection → visible ghosting/trailing. Reprojection now uses `vel` as-is, matching the velocity convention.
- **GPU frustum culling: draw-per-instance regression.** The compute shader wrote one indirect draw per *visible instance* (ordering non-deterministic via `atomicAdd`), so the number of draw calls grew with instance count — a performance regression vs. the CPU path which groups instances by mesh. The culling pass was rewritten to work **per instance-group**: each workgroup processes one draw-group, if any instance survives, one indirect draw is emitted with the group's `InstanceCount` and `BaseInstance`. Instance-group templates are uploaded to a new SSBO (binding 8). `BaseInstance`/`InstanceCount` are preserved, so `geometry.vert` still reads `gl_BaseInstance + gl_InstanceID` unchanged.
- **GPU culling: `atomicAdd` before bounds check.** The draw counter was incremented even for slots rejected by `slot >= u_MaxDraws`. The counter is now incremented only after the bounds check passes.
- **`DrawState` SSBO extended to 4 uints** to carry `visibleInstanceCount` — Stats panel now shows the correct "Visible / Culled" instance counts instead of deriving them from draw-call count.
- **Post-process `Enabled` toggle didn't disable tonemapping.** With `PostProcessComponent::Enabled == false`, the renderer still ran `ApplyTonemap` with `TonemapMode = 0 (ACES)`. `GatherPostProcessSettings` now returns `TonemapMode::Linear` by default, so disabling post-processing actually produces a passthrough image.
- **FXAA mutated the LDR FBO's texture filter.** `ApplyFXAA` called `glTexParameteri(..., GL_LINEAR)` on the LDR color attachment every frame — a global side effect on the FBO. The filter is now set once in `EnsurePostTargets` after (re)creation.
- **TAA / Motion Blur: stale `m_PrevJitter` after resize.** `m_PrevJitter` is now reset to zero on resize, matching TAA history and `m_HasPrevViewProj`.

### Editor
- **Font loading: latent double-free.** `ImFontAtlas::AddFontFromMemoryTTF` always takes ownership of the memory buffer, even when font parsing fails. The `else` branch in `LoadEditorFont` called `std::free(buf)` on the (now owned) buffer — a double-free waiting to happen. The branch no longer frees the buffer.
- **imgui-notify on MSVC `/Zc:strictStrings`.** `get_default_title()` returned `char*` for string literals → rejected by the strict compiler flag. Return type changed to `const char*`; `get_type` / `get_color` / `get_phase` now return by value (no dangling refs); fallback `return` added to `get_color` / `get_icon`.
- **Toasts rendered below docked panels.** `ImGuiWindowFlags_NoBringToFrontOnFocus` prevented toasts from appearing above other windows. `BringWindowToDisplayFront(GetCurrentWindow())` is now called after `Begin()` — toasts stay on top while still not stealing keyboard focus.
- **`ImGuiStyle::AnimTime` / `AnimationTime` no longer exists** in recent ImGui — the line was removed; `CollapsingHeader` animation is handled internally by ImGui 1.92+.
- **`ImGui::PushItemFlag` / `ImGuiItemFlags_NoTabStop` need `imgui_internal.h`** — added the include to `TextEditor.cpp`.

### Minor
- **`DrawState` initialisation** updated to 4 × `uint32_t` to match the new `visibleInstanceCount` field.
- **Stats panel** now correctly distinguishes *draw calls* (one per surviving group) from *visible instances*.

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

### Rendering pipeline (upcoming)
- **Per-instance GPU culling via stream compaction** — moving towards fully GPU-driven MDI.
- Shadows, CSM, PCF/PCSS.
- IBL (prefiltered env + BRDF LUT).
- Clustered forward lighting (or Light BVH) for `> 32` lights.
- Potential Vulkan backend.

### Editor (upcoming)
- Undo / redo for the scene editor.
- Asset browser with thumbnails.
- Material graphs (node-based authoring).
- Custom shader editor themes (currently the dark `TextEditor` palette).

### Other
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
│   ├── Editor/          # Editor: UI, viewport, camera, gizmos,
│   │                    #         TextEditor, ShaderEditor
│   ├── Reflect/         # Component reflection system
│   ├── Renderer/        # Renderer, materials, shaders, meshes, ImAnim
│   ├── Serialization/   # JSON serialization for scene/materials
│   ├── Utils/           # Logging, paths, file I/O
│   └── main.cpp
├── resources/
│   ├── fonts/           # Editor fonts + Font Awesome
│   ├── icons/           # UI icons
│   └── materials/       # .bxmat files
├── shaders/             # GLSL shaders (edited via the built-in Shader Editor)
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
| **Open Shader Editor** | **F4** |
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
- **ImAnim** — for the animation primitives that make the editor feel alive.
- **imgui-notify** — for lightweight, unobtrusive toast notifications.
- **Font Awesome** — for the icon set embedded in the editor.
- **ImGuiColorTextEdit** — for the `TextEditor` foundation used by the shader editor.
- **OpenGL** community — for shader techniques and GPU-driven rendering articles.
- **LearnOpenGL** and **The Book of Shaders** — for educational materials.

---

<p align="center">
  <i>Made with ❤️ for graphics and game dev.</i>
</p>
```

---
