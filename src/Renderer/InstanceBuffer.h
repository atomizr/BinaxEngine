#pragma once

#include <glm/glm.hpp>
#include <cstdint>
#include <vector>

namespace Binax {

struct InstanceGPU
{
    glm::mat4 Model;
    uint32_t  MaterialId = 0;
    uint32_t  MeshId     = 0;
    uint32_t  BoneOffset = 0;
    uint32_t  Flags      = 0;
};
static_assert(sizeof(InstanceGPU) == 80, "InstanceGPU must be 80 bytes");

namespace InstanceFlags {
    enum : uint32_t { None = 0u, Skinned = 1u << 0 };
}

class InstanceBuffer
{
public:
    static InstanceBuffer& Get();

    void Upload(const std::vector<InstanceGPU>& instances);
    uint32_t GetSSBO() const     { return m_SSBO; }
    uint32_t GetCapacity() const { return m_Capacity; }
    uint32_t GetCount() const    { return m_Count; }
    void Clear();

private:
    InstanceBuffer() = default;

    uint32_t m_SSBO     = 0;
    uint32_t m_Capacity = 0;
    uint32_t m_Count    = 0;
};

} // namespace Binax