# Binax Engine

<p align="center">
  <b>A custom game engine and editor built with C++20 / OpenGL 4.6</b><br>
  Deferred rendering · PBR · CSM/Spot/Point shadows · SSAO · Volumetric Fog · Reflection probes · ECS · GPU-driven pipeline · Built-in scene editor
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

The engine includes a full-featured editor with a scene hierarchy, component inspector, gizmos, material editor, in-editor shader editor with live reload, embedded console, object picking, multi-selection, undo/redo, reflection probes with bake persistence, and JSON serialization.

> ⚠️ The project is under active development. APIs and file formats may change without backward compatibility.

---

## ✨ Current Features

### Graphics & Rendering
- **Deferred Shading** with a G-Buffer (positions, normals, albedo + AO, material parameters + probe index, velocity).
- **PBR lighting** with support for up to **32 light sources**: directional, point, and spot lights.
- **Shadows** for all light types:
  - **Directional** — Cascaded Shadow Maps (1-4 cascades, practical split scheme, texel snapping, PCF soft shadows).
  - **Spot** — per-light shadow (up to 4 spot casters, PCF soft).
  - **Point** — cubemap array shadows (up to 2 point casters, 4-tap PCF).
  - Global `shadows.bxdata` settings: resolution, cascade count, max distance, depth/normal bias, softness, fade-out.
- **SSAO** (Screen-Space Ambient Occlusion):
  - Half-resolution raymarch with 4-32 hemisphere samples.
  - Depth-aware bilinear upsample in `lighting.frag` (rejects samples across depth discontinuities).
  - Two-pass separable bilateral blur (edge-preserving, H + V).
- **Volumetric Fog / God Rays** as a post-process:
  - Half-res raymarch through the CSM shadow map with height mask (Min/Max Height) and Henyey-Greenstein phase factor.
  - Animated 3D value-noise FBM for drifting "smoke" inside the light shafts (wind direction + speed).
  - N-iteration separable gaussian blur, additive composite into HDR before TAA.
- **Reflection probes** (box zones):
  - Editor-visible wireframe boxes; hidden at runtime.
  - Bake into a `GL_TEXTURE_CUBE_MAP_ARRAY` (up to 4 probes, 128/256/512/1024 resolution per set).
  - Box projection (parallax-corrected) per probe.
  - Optional inclusion of the skybox in the bake.
  - Binary bake persistence to `resources/reflection_bakes.bxbake`.
- **Materials**:
  - **Shader types**: `Lit` (full PBR + IBL), `Simple Lit` (Lambert + Phong, no PBR maps, no IBL), `Unlit` (albedo + emissive, no lighting).
  - **Blend modes**: `Opaque` (G-Buffer), `Alpha Test` (G-Buffer + discard, foliage/fences), `Transparent` (sorted forward pass on top of HDR).
- **Bindless textures** (`GL_ARB_bindless_texture`) for efficient material handling.
- **GPU-driven rendering**:
  - Compute-shader frustum culling **at the instance-group level** (one workgroup per draw-group, one indirect draw per surviving group).
  - Indirect draws (`glMultiDrawElementsIndirectCount`).
  - Instancing with SSBOs for instances, materials, meshes, and per-group draw templates.
- **Selection outline** — screen-space dilate around selected objects; shares G-Buffer depth for correct occlusion.
- **Skybox** — cubemap with rotation and adjustable brightness.
- **Post-processing**: ACES / Reinhard / Filmic / AgX / Linear tonemapping, TAA (Halton jitter + YCoCg neighborhood clip), FXAA, Bloom, Motion Blur with depth rejection, Vignette, Film Grain, Chromatic Aberration, Lens Distortion, Fog (linear / exp / exp²), Color grading (exposure / contrast / saturation / temperature / tint / LGG).

### Architecture
- **ECS based on [EnTT](https://github.com/skypjack/entt)** — components: `Transform`, `MeshComponent`, `MaterialComponent`, `CameraComponent`, `LightComponent`, `SkyboxComponent`, `PostProcessComponent`, `ReflectionProbeComponent`, `EntityFlags`, `ParentComponent`, `WorldTransform`, `PrevTransform`, etc.
- **Entity flags** (`EntityFlags::Enabled` / `Static`) — Unity-style on/off switch shown in the Inspector header; disabled entities are grayed out in the Hierarchy and skipped by the renderer.
- **Transform hierarchy** with cached world matrices and cycle protection.
- **Reflection system** for automatic inspector widget generation and serialization, with per-field `VisibleWhen` predicates for context-dependent UI.
- **JSON serialization** with versioning:
  - Scene (`.bxlvl`)
  - Materials (`.bxmat`)
  - Post-processing (`.bxdata`)
  - Shadow settings (`.bxdata`)
- **Binary caches**:
  - Mesh data (`.bxmesh`) — re-loads imported geometry without re-running Assimp.
  - Reflection probe bakes (`.bxbake`) — re-loads pre-baked cubemaps without re-baking.
- **Snapshot-based Undo/Redo** — serializes the whole scene to JSON on every edit; 30-state history, automatic pruning on new branch.

### Editor
- **Modern dark theme** — rounded corners, OKLAB-based color interpolation.
- **Animated UI** powered by [ImAnim](https://github.com/SoufianeKHIAT/im_anim).
- **Hierarchy** with drag-and-drop, search, multi-select, per-object enable/disable, and:
  - **File** menu: Save / Load.
  - **Settings** menu: Shadow Settings.
  - **+ New** menu: Create Empty · Light (Directional / Spot / Point) · Reflection Box · Import Mesh… · Primitives (Cube).
- **Inspector** powered by reflection — automatic widgets for all registered components, with custom sections for `Material` (shader type, blend mode, texture slots) and `Reflection Probe` (resolution, bake, save, load).
- **Viewport with gizmos** ([ImGuizmo](https://github.com/CedricGuillemet/ImGuizmo)): translate, rotate, scale with grid snapping.
- **Debug gizmos**:
  - **Light gizmos** — point (3 circles of range), spot (inner + outer cone + axis), directional (arrow). Shown only for selected lights, toggleable from the viewport toolbar.
  - **Reflection probe gizmos** — wireframe box matching the probe's `BoxExtents`.
- **Object picking** — click any mesh in the viewport to select it (no CPU raycasts; reads G-Buffer position).
- **Multi-selection** — `Shift + Click` in viewport or hierarchy.
- **Undo / Redo** — `Ctrl + Z` / `Ctrl + Y` / `Ctrl + Shift + Z`.
- **Material editor** — edit PBR parameters and assign textures.
- **In-editor shader editor** (F4) — tabbed GLSL/HLSL editor with syntax highlighting and full undo/redo. Save triggers `DeferredRenderer::ReloadShaders()` — changes are visible in the viewport immediately. Includes all shadow, SSAO, volumetric-fog, and probe shaders.
- **Embedded console** (`~`) — filter by level, full-text search, auto-scroll. Backed by a custom `spdlog` sink (8192-entry ring buffer). The native Win32 console window is hidden at startup.
- **Toast notifications** ([imgui-notify](https://github.com/patrickcjk/imgui-notify) + Font Awesome 5 icons).
- **Stats overlay** — viewport-pinned panel with FPS, frame time, draw calls, vertices, triangles, visible/culled instances, TAA/PostFX state, mesh/material/instance/probe counts.
- **Import dialog** for meshes:
  - Preview of bounds, mesh/material count.
  - Auto-fit / Original / Custom scale.
  - Place on floor (recenter pivot).
  - UV V/U flip and normal flip fix-ups.
  - Auto-focus camera on the imported model.

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
| Model Import | [Assimp](https://github.com/assimp/assimp) (static, `assimp-vc143-mt`) |
| JSON | [nlohmann/json](https://github.com/nlohmann/json) |
| Build System | CMake |

---

## 🆕 Recently Added

### Shadows — CSM / Spot / Point
- **`ShadowSettings`** (`shadows.bxdata`): enabled toggle, cascade count (1-4), global resolution (512-4096), max distance, split lambda, depth bias, normal bias, softness, fade-out distance. **Settings → Shadow Settings** in the Hierarchy menu; auto-saves on change.
- **Directional CSM** — practical split scheme with texel snapping (stabilized shadows), PCF soft shadows via `sampler2DArray`, slope-scaled depth bias + normal offset bias, per-cascade plane distances.
- **Spot shadows** — up to 4 casters, per-light shadow map layer in a `GL_TEXTURE_2D_ARRAY`, PCF soft.
- **Point shadows** — up to 2 casters, per-light cubemap in a `GL_TEXTURE_CUBE_MAP_ARRAY`, 4-tap PCF around a tangent frame.
- **`LightComponent`**: new fields `ShadowMode` (None/Hard/Soft), `ShadowResolutionMode` (Global/Custom), `ShadowResolution`, `ShadowStrength`, `BiasMultiplier`. All shown in the Inspector for every light type.
- Shadow depth shaders (`shadow_depth.vert/frag`, `shadow_depth_point.vert/frag`) are hot-reloadable through the in-editor Shader Editor.

### SSAO
- Half-resolution SSAO pass with 4-32 hemisphere samples.
- Two-pass separable bilateral blur (H → V), ping-pong between two half-res FBOs.
- **Depth-aware bilinear upsample** in `lighting.frag` — samples across depth discontinuities contribute zero weight, so edges stay crisp and no "halo" appears around silhouettes.
- Multiplies the ambient contribution only (not direct lighting, not emissive).
- Configurable in Post-Processing: `Enabled`, `Radius`, `Intensity`, `Bias`, `Power`, `Samples`. All persisted to `postprocessing.bxdata`.

### Volumetric Fog / God Rays
- Half-res raymarch through the CSM shadow map with height mask (between `Min Height` and `Max Height`) and Henyey-Greenstein phase factor.
- **Animated 3D noise** (2-octave FBM, per-step sample) — drifting smoke inside the light shafts. Params: `Noise`, `Noise Speed`, `Noise Scale`, `Noise Intensity`.
- N-iteration separable gaussian blur, additive composite into HDR **before** TAA.
- Auto-picks the first shadowed Directional as the sun. Uses non-jittered projection so the fog doesn't flicker under TAA.
- Full parameter set: Color, Ray Brightness, Ray Density, Min/Max Height, Max Distance, Steps (8-128), Blur Iterations (0-8), Scattering (0-0.95). All saved to `postprocessing.bxdata`.

### Materials — Blend Modes and Shader Types
- **`MaterialShaderType`** (Unity-style): `Lit`, `Simple Lit`, `Unlit`. Combo in the Inspector and shown in the Material Editor header.
- **`MaterialBlendMode`**:
  - `Opaque` — rendered in the G-Buffer (default).
  - `Alpha Test` — `discard` when alpha < cutoff (foliage, fences, perforated meshes). Alpha cutoff slider.
  - `Transparent` — excluded from G-Buffer, rendered in a **sorted back-to-front forward pass** onto the HDR FBO. Supports alpha blending with `SRC_ALPHA / ONE_MINUS_SRC_ALPHA` and depth test against opaque geometry without writing depth.
- Alpha multiplier and cutoff stored in the free `SkyReflParams.w` and `StrengthB.zw` slots — layout of `MaterialGPU` is preserved (128 bytes).
- `blendMode`, `alphaCutoff`, `alphaMultiplier` persisted to `.bxmat` (backward compatible with older files).

### Reflection Probes
- **`ReflectionProbeComponent`**: `BoxExtents` (AABB half-size), `Realtime` (re-bake every ~1s), `BoxProjection` (parallax correction), `ReflectSkybox` (include skybox in bake), `Resolution` (128/256/512/1024, set by the first probe), `ProbeIndex` (transient), `NeedsBake` (transient).
- **`ReflectionProbeLibrary`**: `GL_TEXTURE_CUBE_MAP_ARRAY` (up to 4 probes), per-probe FBO bake target, `RecreateArray` preserves baked layers via `glBlitFramebuffer`, `RecreateAtResolution` drops data on resolution change.
- **Bake** (`DeferredRenderer::BakeReflectionProbe`): renders 6 faces via `forward.frag` (PBR + IBL). **Backface culling ON** so a probe inside geometry doesn't see inner walls. Optional skybox background via `probe_skybox.frag`.
- **Sample** in `lighting.frag`: `textureLod(probeCubeArray, reflDir, roughness * 4)` with corrected box projection `(pExt - originToWorld) / R` (with `isinf`/`isnan` guards and inside-box check).
- **Persistence** (`.bxbake`): magic `BXPB`, version 1, resolution + count, `RGBA/HALF_FLOAT` payload via `glGetTexImage` / `glTexSubImage3D`. Auto-loads on scene start if it matches the current probe count and resolution; auto-saves on shutdown and via "Bake + Save".
- **Editor**: `+New → Reflection Box` creates a probe at `(0, 3, 0)` with default 5×5×5 extents and a yellow wireframe gizmo. Inspector shows `Resolution` dropdown + **Bake Now** / **Bake + Save** / **Load From File** buttons.

### Model Import — Assimp
- `MeshImporter` — static import (skeletal animation deferred). Supports `.obj`, `.fbx`, `.gltf`, `.glb`, `.dae`, `.3ds`, `.stl`, `.ply`, `.blend`, `.x`.
- Full assimp post-processing: triangulation, smooth normals, tangent space, vertex welding.
- Per-mesh materials with albedo, metallic, roughness, AO, emissive, and texture map paths (auto-resolved relative to the model file).
- **Import dialog**: shows bounds + mesh/material count, target size preview; scale modes (Auto-fit / Original / Custom); recenter pivot (bottom-center to `(0,0,0)`); per-asset **UV flip U/V** and **normal flip** fix-ups.
- Imported meshes are stored as `.bxmesh` binary cache in `resources/imported/` and referenced from the scene via `MeshComponent::SourcePath`.

### Hierarchy — Menus and Creation
- **File** menu: Save (`Ctrl+S`), Load (`Ctrl+O`).
- **Settings** menu: Shadow Settings window.
- **+ New** popup: `Create Empty`, `Light ▸ Directional / Spot / Point`, `Reflection Box`, `Import Mesh…`, `Primitives ▸ Cube`.
- **Undo** / **Redo** buttons next to the `+ New` menu.

### Light and Probe Gizmos
- **Light gizmos**: point (3 circles of range), spot (inner + outer base circles + axis + 4 side lines), directional (arrow with head feathers). Toggle in the viewport toolbar (`Lights` checkbox). Only shown for selected lights, respecting `EntityFlags::Enabled`.
- **Probe gizmos**: yellow wireframe box for all `ReflectionProbeComponent` entities (dimmer when not selected).

### Entity Flags
- **`EntityFlags`** component: `Enabled` (checkbox in the Inspector header, left) and `Static` (right, reserved for future baked lighting / culling).
- Disabled entities are grayed out in the Hierarchy, skipped by `RenderSystem::BuildFrameData`, `CollectLights`, and `TransformSystem`.
- Persisted in the scene JSON.

### Inspector — Reflection `VisibleWhen`
- New `PropertyInfo::SetVisibleWhen(predicate)` — used by `LightComponent` shadow fields (`ShadowMode`, `ShadowResolutionMode`, `ShadowResolution`, `ShadowStrength`, `BiasMultiplier` visible for all light types) and by `ReflectionProbeComponent::Resolution` (hidden from the generic widget loop so only the custom combo triggers `RecreateAtResolution`).

### Minor
- `MeshCache` moved to namespace `Binax` (was `Binax::Serialization`).
- Single `MeshLibrary::Flush()` at the end of `SceneSerializer::DeserializeFromString` instead of per-mesh flushes.
- `MeshComponent::SourcePath` is now copied by `DuplicateSingle`, so duplicated imported meshes reload correctly from cache after restart.
- New scene entities auto-focus the editor camera on the newly created object (import + cube + light creation).

---

## 🐛 Recent Fixes

### Graphics pipeline
- **TAA reprojection (Y-flip mismatch with Motion Blur)** — reprojection used `vec2(vel.x, -vel.y)` while the velocity buffer was written raw. Fixed.
- **GPU frustum culling: draw-per-instance regression** — rewritten to work **per instance-group** (one draw per surviving group). New SSBO for group templates (binding 8).
- **GPU culling: `atomicAdd` before bounds check** — fixed.
- **`DrawState` SSBO extended to 4 uints** for `visibleInstanceCount`.
- **Post-process `Enabled` toggle didn't disable tonemapping** — `GatherPostProcessSettings` now returns `TonemapMode::Linear` when disabled.
- **FXAA mutated LDR FBO filter** — filter now set once in `EnsurePostTargets`.
- **Outline: inflated-silhouette artifacts** — rewritten from vertex inflation to screen-space dilate with a dedicated mask FBO. Fixed thickness in pixels, stable everywhere.
- **Outline: z-fighting against coplanar geometry** — `glPolygonOffset(-1, -1)` on the mask render.
- **Outline: invisible around sky-drawn objects** — `OutlineComposite` now called in both the sky branch and the standard lighting branch.
- **Spot light inner/outer cutoff default swapped** — inverted spotlight cone fixed (`InnerCutoff = 0.9`, `OuterCutoff = 0.8`).
- **`geometry.frag` unlit branch wrote `vec3` to `g_Position`** — now `vec4(worldPos, probeIdx)` for all shader types.
- **`RenderOutlinePass` leaked `glClearColor`** — saves and restores around the outline mask render.
- **`RenderOutlinePass` used unresolved `width`/`height` in `glViewport`** — resolves `targetW`/`targetH` once.

### Shadows
- **`CollectLights` hardcoded fallback sun** — removed. If no lights exist, ambient-only lighting is used.
- **`EnsureShadowMap` / `EnsureSpotShadowMap` / `EnsurePointShadowMap` leak on recreate** — FBO and texture are properly freed before re-allocating.

### Editor
- **Font loading: latent double-free** — `AddFontFromMemoryTTF` always owns the buffer; the `else` free branch is gone.
- **imgui-notify on MSVC `/Zc:strictStrings`** — `get_default_title()` returns `const char*`, color/phase/type return by value.
- **Toasts rendered below docked panels** — `BringWindowToDisplayFront` after `Begin()`.
- **`ImGuiStyle::AnimTime` / `AnimationTime` removed** — removed from code (handled internally by ImGui 1.92+).
- **`PropertyWidgets::Draw` ignored `VisibleWhen`** — was drawing every registered property, including intentionally hidden ones. This made `ReflectionProbeComponent::Resolution` appear **twice** in the Inspector (generic + custom combo) — changing the generic one updated `rp.Resolution` but **never called** `RecreateAtResolution`, so the library stayed at the old size.
- **`LightingPass` set reflection-probe uniforms after `Unbind`** — the `u_ProbeCount`/`u_ProbeCubeArray`/`u_ProbePos` uploads were silently no-ops. Entire block moved **before** `DrawFullscreenQuad`.
- **`DeferredRenderer.h` class body spilled past the closing `}`** — volumetric-fog members were declared in namespace scope, causing `C2039` / `C2065`. Consolidated inside the class.
- **`main.cpp` probe rebuild ran before `registry` was declared** — moved after scene load.
- **`main.cpp` bake loop used `skyboxParams` before declaration** — moved `GatherSkyboxParams` / `GatherPostProcessSettings` before `RenderShadowPass`.
- **`main.cpp` duplicate `viewport` variable** — removed.
- **`main.cpp` shutdown saved baked probes *after* `Clear()`** — moved `SaveToFile` before `Clear()`.
- **`DuplicateSingle` lost `MeshComponent::SourcePath`** — now copies the full `MeshComponent`, so duplicated imported meshes reload from `.bxmesh` cache correctly.

### Serialization
- **`MeshCache` was in `Binax::Serialization` but callers were in `Binax`** — moved to `Binax`.
- **`MaterialSerializer` didn't persist shader type / blend mode** — added `shaderType`, `blendMode`, `alphaCutoff`, `alphaMultiplier`.
- **`DeserializeFromString` had no rollback** — snapshot is taken to JSON before destroying entities and restored on any `ApplyEntityData` failure (with a thread-local recursion guard).

### Misc
- **`DrawState` initialization** updated to 4 × `uint32_t`.
- **Stats panel** distinguishes *draw calls* (one per surviving group) from *visible instances*.
- **`frustum_cull.comp` OOB read on `MeshId`** — added `u_MeshCount` uniform and an early-out per workgroup.
- **Spot shadow `LightSpaceMatrix` shared across cascades** — now indexed per spot slot.
- **`ShadowMode` / `ShadowStrength` / `BiasMultiplier` not passed to shader** — added to `LightUniformNames` and uploaded per light.

---

## 🚧 Roadmap / In Development

### 🎮 Gameplay / runtime
- **Play / Pause / Stop mode** — clone the registry into a runtime world via `SceneSerializer::SerializeToString`, play, restore on Stop.
- **Input actions** — abstract "Jump" / "Fire" from raw `GLFW_KEY_*`.
- **Physics — Jolt Physics** — `RigidBody`, `Collider`, `CharacterController`, ECS ↔ physics-world sync.
- **Audio — miniaudio** — `AudioSource` (2D/3D) and `AudioListener` components.
- **Scripting — Lua (sol2)** — `ScriptComponent` with `OnCreate` / `OnUpdate` / `OnDestroy`, hot-reload, ECS/input/transform API.

### 🌍 IBL and image-based lighting
- Prefiltered environment maps (diffuse irradiance + specular prefilter).
- BRDF LUT for PBR lighting.
- Integration with the scene skybox. **Reflection probes are an intermediate step** — they give local reflections but not full IBL prefiltering.

### 🌫️ Additional rendering
- **Particle system** — GPU-driven emitters, modulators, sprites and mesh particles.
- **SSR** — screen-space reflections for wet surfaces and polished floors.
- **Procedural sky** — Preetham / Hosek-Wilkie with time-of-day cycle.
- **Contact shadows** — small screen-space shadows under feet / wheels.
- **Decals** — projected forward pass for blood, tire marks, graffiti.
- **Volumetric fog: radial blur from sun** — currently uses a generic gaussian; a radial variant would give more "cinematic" god rays.

### Rendering pipeline (upcoming)
- **Per-instance GPU culling via stream compaction** — move towards fully GPU-driven MDI.
- **Clustered forward lighting** (or Light BVH) for `> 32` lights.
- **Skinning / skeletal animation** — needs skinning support in `geometry.vert` and `forward.vert`, plus animation state machine.
- **Potential Vulkan backend.**

### Editor (upcoming)
- **Marquee selection** — LMB drag over empty space paints a rectangle; all meshes inside get selected.
- **Hover-preview** — hover highlight of the object under the cursor.
- **Group transform** — gizmo moves the whole multi-selection, not just the primary.
- **Camera bookmarks** — `Ctrl+1..9` saves, `Alt+1..9` jumps.
- **Prefab system** — save a subtree as a reusable asset, instantiate with drag-drop.
- **Content browser** with thumbnails for materials, textures, shaders, meshes.
- **Per-instance probe resolution** — currently one resolution for all probes.
- **Probe blending near boundaries** — currently picks the nearest containing probe with no transition zone.
- **OBB probes** — currently axis-aligned only.
- **Undo/Redo improvements** — use `EditorState` deltas instead of full-scene snapshots for very large scenes.
- **Material graphs** — node-based authoring.

### Other
- **Cross-platform builds** (Windows / Linux).

---

## 🚀 Building

### Requirements
- **Compiler:** MSVC 2022+ (toolset 14.44.35207 or newer), Clang 15+, or GCC 12+ with C++20 support.
- **GPU:** OpenGL 4.6 support with the following extensions:
  - `GL_ARB_bindless_texture`
  - `GL_ARB_shader_draw_parameters`
  - `GL_ARB_shader_storage_buffer_object` (core)
  - `GL_ARB_compute_shader` (core)
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
│   ├── ECS/
│   │   ├── Components/  # Transform, Mesh, Material, Light, Camera,
│   │   │                # ReflectionProbe, EntityFlags, Skybox, PostProcess,
│   │   │                # Parent, WorldTransform, PrevTransform, ...
│   │   └── Systems/     # RenderSystem, TransformSystem
│   ├── Editor/          # UI, viewport, camera, gizmos, TextEditor,
│   │                    # ShaderEditor, Console, PropertyWidgets
│   ├── Reflect/         # Component reflection system
│   ├── Renderer/        # Renderer (deferred, shadows, SSAO, volumetric fog,
│   │                    # reflection probes, forward transparent pass),
│   │                    # MeshLibrary, MeshImporter, MeshCache, MaterialLibrary,
│   │                    # ShadowMap, ShadowCubemap, ReflectionProbeLibrary, ImAnim
│   ├── Serialization/   # Scene / Material / PostProcess / MeshCache / ShadowSettings
│   ├── Utils/           # Logging, paths, file I/O, LogSink, Frustum
│   └── main.cpp
├── resources/
│   ├── fonts/           # Editor fonts + Font Awesome
│   ├── icons/           # UI icons
│   ├── imported/        # .bxmesh binary caches for imported meshes
│   ├── materials/       # .bxmat files
│   ├── reflection_bakes.bxbake  # reflection probe bake cache
│   ├── shadows.bxdata   # CSM/spot/point shadow settings
│   └── postprocessing.bxdata    # post-process + SSAO + volumetric fog params
├── shaders/             # GLSL shaders (edited via the built-in Shader Editor)
├── assets/              # User assets (models, textures)
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
| Toggle Light gizmos | `Lights` checkbox in the viewport toolbar |
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
- **Assimp** — for the robust, multi-format model importer.
- **OpenGL** community — for shader techniques and GPU-driven rendering articles.
- **LearnOpenGL** and **The Book of Shaders** — for educational materials.

---

<p align="center">
  <i>Made with ❤️ for graphics and game dev.</i>
</p>
```
