
#pragma once

#include <glm/glm.hpp>
#include <string>
#include <unordered_map>
#include <memory>

namespace Binax {

class Shader
{
public:
    
    Shader(const std::string& vertexSrc, const std::string& fragmentSrc);

    
    static std::unique_ptr<Shader> FromFiles(const std::string& vertexPath,
                                             const std::string& fragmentPath);

    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    void Bind() const;
    void Unbind() const;

    void SetInt  (const std::string& name, int value);
    void SetFloat(const std::string& name, float value);
    void SetVec2 (const std::string& name, const glm::vec2& value);
    void SetVec3 (const std::string& name, const glm::vec3& value);
    void SetVec4 (const std::string& name, const glm::vec4& value);
    void SetMat4 (const std::string& name, const glm::mat4& value);

    
    
    
    
    void Reload();

private:
    uint32_t m_RendererID = 0;
    mutable std::unordered_map<std::string, int> m_UniformLocationCache;

    
    std::string m_VertexPath;
    std::string m_FragmentPath;

    int GetUniformLocation(const std::string& name) const;

    
    
    static uint32_t BuildProgram(const std::string& vertexSrc,
                                 const std::string& fragmentSrc);

    static uint32_t CompileShader(uint32_t type, const std::string& source);
};

} 