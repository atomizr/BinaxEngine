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

The engine includes a full-featured editor with a scene hierarchy, component inspector, gizmos, material editor, in-editor shader editor with live reload, embedded console, object picking, multi-selection, undo/redo, and JSON serialization.

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
- **Selection outline** — screen-space dilate around the selected object(s); shares G-Buffer depth for correct occlusion, so the outline never bleeds through walls. Group outline draws a single border around the entire selection.
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
- **Snapshot-based Undo/Redo** — serializes the whole scene to JSON on every edit; 30-state history, automatic pruning on new branch.

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
- **Object picking** — click any mesh in the viewport to select it. Uses the G-Buffer position texture (no CPU-side raycasts), so clicking through walls correctly picks only visible pixels.
- **Multi-selection** — `Shift + Click` in the viewport or hierarchy adds/removes entities to the selection. Outline is drawn around the whole group; gizmo operates on the primary (last-clicked) object.
- **Undo / Redo** — `Ctrl + Z` (undo), `Ctrl + Y` / `Ctrl + Shift + Z` (redo). Wired into entity create/delete/duplicate, add/remove component, reparent, gizmo drags, and inspector edits.
- **Material editor** — edit PBR parameters and assign textures via the built-in file browser.
- **In-editor shader editor** — tabbed GLSL/HLSL editor with syntax highlighting, auto-indentation, and full undo/redo:
  - Save → automatically triggers `DeferredRenderer::ReloadShaders()`, so changes are visible in the viewport immediately.
  - Reload discards local edits and re-reads from disk.
  - Scans `shaders/` every 2 s for new files.
  - Supports `.vert`, `.frag`, `.comp`, `.geom`, `.tesc`, `.tese`, `.glsl`.
- **Embedded console** — in-editor log panel fed by a custom `spdlog` sink (ring buffer of 8192 entries). Filter by level (`Trace` … `Critical`), full-text search, auto-scroll. Toggled with **`~`** (grave). The native Win32 console window is hidden at startup; all logs go to `debug.log` and to the embedded console.
- **Toast notifications** ([imgui-notify](https://github.com/patrickcjk/imgui-notify) + Font Awesome 5 icons) — non-intrusive feedback for Save / Load / Delete / Duplicate / Add / Remove.
- **Stats overlay** — click the `Stats` button in the viewport toolbar to pin a semi-transparent Stats panel to the viewport's top-right corner. Shows FPS, frame time, GPU culling state, draw calls, vertices, triangles, visible/culled instances, TAA / PostFX state, mesh / material / instance counts.
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

### Selection & Picking
- **Object picking in viewport** — click any visible mesh to select it.
  - Reads world position from the G-Buffer's `POSITION` attachment at the clicked pixel (`glReadPixels`).
  - Searches instances for a local-space AABB hit, returns the matching entity.
  - Skipped when the click is over the gizmo (`ImGuizmo::IsOver()`), over a Stats overlay, over text input, or on the sky. No CPU-side raycast — the depth buffer already filters occluded pixels.
- **Multi-selection** (`Shift + Click`):
  - Viewport clicks and hierarchy clicks both support Shift for toggle.
  - `EditorUI::SelectOnly` / `ToggleSelect` / `ClearSelection` manage a `std::unordered_set<entt::entity>` alongside the primary `m_SelectedEntity`.
  - **Group outline**: `RenderOutlinePass` accepts the full selection, draws the mask of every selected entity into a single FBO, then the pixel-space dilate in `lighting.frag` produces a single outline around the whole group (no internal borders between adjacent selected objects).
- **Selection outline (screen-space)**:
  - `RenderOutlinePass` renders selected meshes into a dedicated FBO, sharing G-Buffer depth for correct occlusion (`glFramebufferTexture2D(..., GL_DEPTH_STENCIL_ATTACHMENT, ...)`).
  - `OutlineComposite` in `lighting.frag` does a radius-2 circular dilate with `GL_NEAREST` sampling — stable thickness in **pixels**, no artifacts on cube edges, no dropouts at grazing angles.
  - Applied in both the sky branch and the standard lighting branch, so the outline works even when the selected object floats in the sky.
  - `glPolygonOffset(-1, -1)` during mask render eliminates z-fighting between coplanar geometry.

### Undo / Redo
- **Snapshot-based history** (`Ctrl+Z` / `Ctrl+Y` / `Ctrl+Shift+Z`):
  - `SceneSerializer::SerializeToString` / `DeserializeFromString` reuse the existing JSON pipeline.
  - 30-state history with automatic pruning of the redo branch.
  - Hooked into: entity create/delete/duplicate, add/remove component, reparent, gizmo drags (recorded once on release), inspector widget edits (recorded when the active item is released), material asset swap.
  - `ResetUndo()` on `LoadScene` — new scene, new history.
  - `Ctrl+Z` inside the Shader Editor goes to the `TextEditor`'s own undo, not to the scene (because `WantTextInput` is checked first).

### Console
- **Embedded log console** (`src/Editor/Console.{h,cpp}` + `src/Utils/LogSink.h`):
  - Custom `spdlog::sinks::base_sink<std::mutex>` that keeps a ring buffer of 8192 entries.
  - Registered as an extra sink in `Log::Init()` alongside stdout and `debug.log`.
  - Filter by level (`Trace` / `Debug` / `Info` / `Warn` / `Error` / `Critical`), full-text case-insensitive search, auto-scroll.
  - Toggled with **`~`** (grave).
  - **Win32 cmd window is hidden** at startup (`ShowWindow(GetConsoleWindow(), SW_HIDE)`), so the only console the user sees is the in-editor one.

### Stats Overlay
- **Button in viewport toolbar** (top-right) pins the Stats window to the viewport's top-right corner.
  - Repositioned **every frame** via `SetNextWindowPos(ImGuiCond_Always)` — the overlay follows the viewport on resize.
  - Semi-transparent background (`SetNextWindowBgAlpha(0.92f)`).
  - Raised above other panels via `BringWindowToDisplayFront(GetCurrentWindow())` — but without stealing focus (unlike `BringWindowToFocusFront`).
  - Closing via the X resets the toggle in `EditorUI` so the button state stays in sync.

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
- **Outline: inflated-silhouette artifacts.** The first outline implementation used vertex inflation (`pos + normal * thickness`) and stencil. On cube edges the three vertex normals diverge → torn outline; at grazing angles the projected thickness dropped below 1 px → outline disappeared. Rewritten to screen-space dilate in `lighting.frag` with a solid-object mask in a separate FBO. Fixed thickness in **pixels**, stable everywhere.
- **Outline: z-fighting against coplanar geometry.** Outline mask is rendered with `glPolygonOffset(-1, -1)` and depth shared from the G-Buffer, so coplanar surfaces no longer flicker.
- **Outline: invisible around objects in the sky.** `lighting.frag` has an early return for sky pixels (`dot(Nraw, Nraw) < 1e-4`), which skipped the outline composite. `OutlineComposite` is now called in both branches.

### Editor
- **Font loading: latent double-free.** `ImFontAtlas::AddFontFromMemoryTTF` always takes ownership of the memory buffer, even when font parsing fails. The `else` branch in `LoadEditorFont` called `std::free(buf)` on the (now owned) buffer — a double-free waiting to happen. The branch no longer frees the buffer.
- **imgui-notify on MSVC `/Zc:strictStrings`.** `get_default_title()` returned `char*` for string literals → rejected by the strict compiler flag. Return type changed to `const char*`; `get_type` / `get_color` / `get_phase` now return by value (no dangling refs); fallback `return` added to `get_color` / `get_icon`.
- **Toasts rendered below docked panels.** `ImGuiWindowFlags_NoBringToFrontOnFocus` prevented toasts from appearing above other windows. `BringWindowToDisplayFront(GetCurrentWindow())` is now called after `Begin()` — toasts stay on top while still not stealing keyboard focus.
- **`ImGuiStyle::AnimTime` / `AnimationTime` no longer exists** in recent ImGui — the line was removed; `CollapsingHeader` animation is handled internally by ImGui 1.92+.
- **`ImGui::PushItemFlag` / `ImGuiItemFlags_NoTabStop` need `imgui_internal.h`** — added the include to `TextEditor.cpp`.
- **`imgui_notify.h` requires `<imgui.h>` before itself.** Self-contained header now includes `<imgui.h>` and `<imgui_internal.h>` at the top, so it can be included from anywhere in the project.
- **`PushItemFlag` in `TextEditor.cpp` on ImGui 1.92+** — the function moved to `imgui_internal.h`; added the include.

### Minor
- **`DrawState` initialisation** updated to 4 × `uint32_t` to match the new `visibleInstanceCount` field.
- **Stats panel** now correctly distinguishes *draw calls* (one per surviving group) from *visible instances*.

---

## 🚧 Roadmap / In Development

The project is actively evolving. Upcoming features include:

### 🎮 Gameplay / runtime
- **Play / Pause / Stop mode** — clone the registry into a runtime world via `SceneSerializer::SerializeToString`, play, restore on Stop. Play / Stop is essentially free now that snapshot serialization exists.
- **Input actions** — abstract "Jump" / "Fire" from raw `GLFW_KEY_*`.

### 🔦 Shadows
- Shadows from directional and point lights.
- Cascaded Shadow Maps (CSM) for directional light.
- Soft shadows (PCF / PCSS).

### 🌍 Image-Based Lighting (IBL)
- Prefiltered environment maps (diffuse irradiance + specular prefilter).
- BRDF LUT for PBR lighting.
- Integration with the scene skybox.

### 🌫️ Transparency / Forward pass
- Sorted forward pass after deferred for glass, water, particles, foliage.
- Blending modes per material.

### 🌑 SSAO
- Screen-space ambient occlusion from G-Buffer position + normal.
- Blur pass and composite in `lighting.frag`.

### 🌫️ Particle System
- GPU-driven particle system with emitters and modulators.
- Support for various particle types (sprites, meshes).
- Editor integration.

### ⚙️ Physics — Jolt Physics
- Integration of [Jolt Physics](https://github.com/jrouwe/JoltPhysics).
- `RigidBody`, `Collider`, `CharacterController` components.
- ECS ↔ physics world transform sync.

### 🔊 Audio — miniaudio
- Integration of [miniaudio](https://github.com/mackron/miniaudio) as the audio backend.
- `AudioSource` (2D/3D) and `AudioListener` components.
- WAV/MP3/FLAC support and streaming.

### 🧊 Model Import — Assimp
- Integration of [Assimp](https://github.com/assimp/assimp) for glTF / FBX / OBJ / DAE.
- Skeletal animation support.
- Automatic material creation from imported data.

### 🐍 Scripting — Lua
- Embedding [Lua](https://www.lua.org/) (via [sol2](https://github.com/ThePhD/sol2)).
- `ScriptComponent` with `OnCreate` / `OnUpdate` / `OnDestroy`.
- API for ECS, input, transforms, audio.

### Rendering pipeline (upcoming)
- **Per-instance GPU culling via stream compaction** — moving towards fully GPU-driven MDI.
- Shadows, CSM, PCF/PCSS.
- IBL (prefiltered env + BRDF LUT).
- Clustered forward lighting (or Light BVH) for `> 32` lights.
- Potential Vulkan backend.

### Editor (upcoming)
- **Marquee selection** — LMB drag over empty space paints a rectangle, all meshes inside get selected.
- **Hover-preview** — hover highlight of the object under the cursor (using the same G-Buffer pick).
- **Group transform** — gizmo moves/rotates/scales the whole multi-selection, not just the primary.
- **Camera bookmarks** — `Ctrl+1..9` saves, `Alt+1..9` jumps.
- **Prefab system** — save a subtree as a reusable asset, instantiate with drag-drop.
- **Content browser** with thumbnails for materials / textures / shaders.
- **Undo / redo improvements** — use `EditorState` instead of full-scene snapshots for very large scenes, so each edit is <1 ms.
- **Material graphs** — node-based authoring.

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
│   │                    #         TextEditor, ShaderEditor, Console
│   ├── Reflect/         # Component reflection system
│   ├── Renderer/        # Renderer, materials, shaders, meshes, ImAnim
│   ├── Serialization/   # JSON serialization for scene/materials
│   ├── Utils/           # Logging, paths, file I/O, LogSink
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
| Undo | Ctrl + Z |
| Redo | Ctrl + Y / Ctrl + Shift + Z |
| Multi-select (toggle) | Shift + LMB (viewport or hierarchy) |
| Open Shader Editor | F4 |
| Open Console | ` (grave) |
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
- **spdlog** — for the logging foundation and the custom sink pipeline.
- **OpenGL** community — for shader techniques and GPU-driven rendering articles.
- **LearnOpenGL** and **The Book of Shaders** — for educational materials.

---

<p align="center">
  <i>Made with ❤️ for graphics and game dev.</i>
</p>
```
