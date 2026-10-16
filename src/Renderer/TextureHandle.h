#pragma once

#include <cstdint>

namespace Binax {

// Bindless texture handle. 0 = невалидный.
// Для fallback-пути (если GL_ARB_bindless_texture недоступен) сюда можно
// будет положить упакованный (slot, arrayIdx) — API не изменится.
using TextureHandle = uint64_t;
constexpr TextureHandle kInvalidTextureHandle = 0ull;

} // namespace Binax