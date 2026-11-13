#include "Serialization/MeshCache.h"
#include "Utils/Log.h"
#include "Utils/Paths.h"

#include <filesystem>
#include <fstream>

namespace Binax {   // ← было Binax::Serialization

namespace {
constexpr uint32_t kMagic    = 0x4D584842u; // "BXHM"
constexpr uint32_t kVersion  = 1u;
constexpr uint32_t kMaxVerts = 200'000'000u;
constexpr uint32_t kMaxIdx   = 600'000'000u;
}

bool MeshCache::Save(const std::string& filepath,
                     const std::vector<Vertex>& vertices,
                     const std::vector<uint32_t>& indices)
{
    const std::filesystem::path path = Paths::FromUtf8(filepath);
    std::error_code ec;
    if (path.has_parent_path())
        std::filesystem::create_directories(path.parent_path(), ec);

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file.is_open())
    {
        BINAX_LOG_ERROR("MeshCache: cannot write '{}'", filepath);
        return false;
    }

    const uint32_t vcount = (uint32_t)vertices.size();
    const uint32_t icount = (uint32_t)indices.size();

    file.write((const char*)&kMagic, 4);
    file.write((const char*)&kVersion, 4);
    file.write((const char*)&vcount, 4);
    file.write((const char*)&icount, 4);

    if (vcount > 0)
        file.write((const char*)vertices.data(), vcount * sizeof(Vertex));
    if (icount > 0)
        file.write((const char*)indices.data(), icount * sizeof(uint32_t));

    file.flush();
    const bool ok = file.good();
    if (!ok)
        BINAX_LOG_ERROR("MeshCache: write failed for '{}'", filepath);
    return ok;
}

bool MeshCache::Load(const std::string& filepath,
                     std::vector<Vertex>& outVertices,
                     std::vector<uint32_t>& outIndices)
{
    std::ifstream file(Paths::FromUtf8(filepath), std::ios::binary);
    if (!file.is_open())
    {
        BINAX_LOG_ERROR("MeshCache: cannot open '{}'", filepath);
        return false;
    }

    uint32_t magic = 0, version = 0, vcount = 0, icount = 0;
    file.read((char*)&magic,   4);
    file.read((char*)&version, 4);
    file.read((char*)&vcount,  4);
    file.read((char*)&icount,  4);

    if (magic != kMagic || version != kVersion)
    {
        BINAX_LOG_ERROR("MeshCache: '{}' bad magic/version", filepath);
        return false;
    }
    if (vcount > kMaxVerts || icount > kMaxIdx)
    {
        BINAX_LOG_ERROR("MeshCache: '{}' insane counts ({} verts, {} idx)",
                        filepath, vcount, icount);
        return false;
    }

    outVertices.resize(vcount);
    outIndices.resize(icount);
    if (vcount > 0)
        file.read((char*)outVertices.data(), vcount * sizeof(Vertex));
    if (icount > 0)
        file.read((char*)outIndices.data(), icount * sizeof(uint32_t));

    if (!file.good())
    {
        BINAX_LOG_ERROR("MeshCache: short read on '{}'", filepath);
        return false;
    }
    return true;
}

}