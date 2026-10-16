#include "Renderer/InstanceBuffer.h"

#include <glad/glad.h>

namespace Binax {

InstanceBuffer& InstanceBuffer::Get()
{
    static InstanceBuffer instance;
    return instance;
}

void InstanceBuffer::Upload(const std::vector<InstanceGPU>& instances)
{
    m_Count = (uint32_t)instances.size();

    const uint32_t needed = (m_Count == 0) ? 1u : m_Count;

    if (m_SSBO == 0)
        glGenBuffers(1, &m_SSBO);

    if (needed > m_Capacity)
    {
        uint32_t cap = (m_Capacity == 0) ? 256u : m_Capacity;
        while (cap < needed) cap *= 2;
        m_Capacity = cap;

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_SSBO);
        glBufferData(GL_SHADER_STORAGE_BUFFER,
                     (GLsizeiptr)(m_Capacity * sizeof(InstanceGPU)),
                     nullptr, GL_DYNAMIC_DRAW);
    }

    if (m_Count > 0)
    {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_SSBO);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0,
                        (GLsizeiptr)(m_Count * sizeof(InstanceGPU)),
                        instances.data());
    }
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void InstanceBuffer::Clear()
{
    m_Count = 0;
    m_Capacity = 0;
    if (m_SSBO) { glDeleteBuffers(1, &m_SSBO); m_SSBO = 0; }
}

} // namespace Binax