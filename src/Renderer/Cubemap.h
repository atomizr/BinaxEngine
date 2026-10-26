#pragma once

#include <cstdint>
#include <array>
#include <string>
#include <memory>

namespace Binax {

class Cubemap
{
public:
    Cubemap() = default;
    ~Cubemap();

    Cubemap(const Cubemap&) = delete;
    Cubemap& operator=(const Cubemap&) = delete;
    Cubemap(Cubemap&& other) noexcept;
    Cubemap& operator=(Cubemap&& other) noexcept;

    
    
    
    
    
    static std::unique_ptr<Cubemap> LoadFromFiles(const std::array<std::string, 6>& paths, bool srgb = true);

    void Bind(uint32_t slot = 0) const;
    void Unbind(uint32_t slot = 0) const;

    uint32_t GetID()   const { return m_ID; }
    int      GetSize() const { return m_Size; }

private:
    uint32_t m_ID = 0;
    int      m_Size = 0;
};

}
