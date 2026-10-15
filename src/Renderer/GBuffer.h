#pragma once

#include "Framebuffer.h"
#include <array>

namespace Binax {

class GBuffer : public Framebuffer
{
public:
    GBuffer(uint32_t width, uint32_t height);

    enum TextureType : uint32_t
    {
        POSITION   = 0,
        NORMAL     = 1,
        ALBEDO_AO  = 2,
        MATERIAL   = 3,
        COUNT      = 4
    };

    uint32_t GetTexture(TextureType type) const
    { return GetColorAttachment(static_cast<uint32_t>(type)); }

    void BindTextures(uint32_t startSlot) const;

private:
    void Init(uint32_t width, uint32_t height);
};

}