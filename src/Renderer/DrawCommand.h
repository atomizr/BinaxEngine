#pragma once

#include <cstdint>

namespace Binax {


struct DrawElementsIndirect
{
    uint32_t Count         = 0;
    uint32_t InstanceCount = 0;
    uint32_t FirstIndex    = 0;
    uint32_t BaseVertex    = 0;
    uint32_t BaseInstance  = 0;
};
static_assert(sizeof(DrawElementsIndirect) == 20, "MDI command must be 20 bytes");

} 
