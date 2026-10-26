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
        VELOCITY   = 4,
        COUNT      = 5
    };

    uint32_t GetTexture(TextureType type) const
    { return GetColorAttachment(static_cast<uint32_t>(type)); }

    
    
    
    void BindTextures(uint32_t startSlot, uint32_t count = 4) const;

private:
    void Init(uint32_t width, uint32_t height);
};

}
