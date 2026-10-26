
#include "Shader.h"
#include "../Utils/Log.h"
#include "../Utils/Paths.h"
#include <glad/glad.h>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <stdexcept>

namespace Binax {

namespace {

std::string ReadTextFile(const std::string& path)
{
    std::ifstream file(Paths::FromUtf8(path), std::ios::binary);
    if (!file.is_open())
        throw std::runtime_error("Failed to open shader file: " + path);

    std::stringstream ss;
    ss << file.rdbuf();

    if (ss.str().empty())
        throw std::runtime_error("Shader file is empty: " + path);

    return ss.str();
}

} 



Shader::Shader(const std::string& vertexSrc, const std::string& fragmentSrc,
               Stage stage)
    : m_Stage(stage)
{
    m_RendererID = BuildProgram(vertexSrc, fragmentSrc, m_Stage);
}

std::unique_ptr<Shader> Shader::FromFiles(const std::string& vertexPath,
                                          const std::string& fragmentPath,
                                          Stage stage)
{
    std::string vsrc = ReadTextFile(vertexPath);
    std::string fsrc = (stage == Stage::Compute) ? std::string{} : ReadTextFile(fragmentPath);

    auto shader = std::unique_ptr<Shader>(new Shader(vsrc, fsrc, stage));
    shader->m_VertexPath   = vertexPath;
    shader->m_FragmentPath = fragmentPath;

    if (stage == Stage::Compute)
        BINAX_LOG_INFO("Compute shader loaded: {0}", vertexPath);
    else
        BINAX_LOG_INFO("Shader loaded: {0} + {1}", vertexPath, fragmentPath);

    return shader;
}

void Shader::Reload()
{
    if (m_VertexPath.empty() || m_FragmentPath.empty())
    {
        BINAX_LOG_WARN("Shader::Reload() called on source-only shader (no-op)");
        return;
    }

    try
    {
        const std::string vsrc = ReadTextFile(m_VertexPath);
        const std::string fsrc = (m_Stage == Stage::Compute)
                                     ? std::string{}
                                     : ReadTextFile(m_FragmentPath);

        
        
        const uint32_t newID = BuildProgram(vsrc, fsrc, m_Stage);

        if (m_RendererID != 0)
            glDeleteProgram(m_RendererID);
        m_RendererID = newID;
        m_UniformLocationCache.clear();

        BINAX_LOG_INFO("Shader reloaded: {0} + {1}", m_VertexPath, m_FragmentPath);
    }
    catch (const std::exception& e)
    {
        BINAX_LOG_ERROR("Shader reload failed, keeping old program: {0}", e.what());
    }
}

Shader::~Shader()
{
    if (m_RendererID != 0)
        glDeleteProgram(m_RendererID);
}



void Shader::Bind() const   { glUseProgram(m_RendererID); }
void Shader::Unbind() const { glUseProgram(0); }

int Shader::GetUniformLocation(const std::string& name) const
{
    auto it = m_UniformLocationCache.find(name);
    if (it != m_UniformLocationCache.end())
        return it->second;

    int location = glGetUniformLocation(m_RendererID, name.c_str());
    m_UniformLocationCache[name] = location;
    return location;
}

void Shader::SetInt  (const std::string& n, int v)               { glUniform1i(GetUniformLocation(n), v); }
void Shader::SetUInt (const std::string& n, uint32_t v)          { glUniform1ui(GetUniformLocation(n), v); }
void Shader::SetFloat(const std::string& n, float v)             { glUniform1f(GetUniformLocation(n), v); }
void Shader::SetVec2 (const std::string& n, const glm::vec2& v)  { glUniform2fv(GetUniformLocation(n), 1, &v[0]); }
void Shader::SetVec3 (const std::string& n, const glm::vec3& v)  { glUniform3fv(GetUniformLocation(n), 1, &v[0]); }
void Shader::SetVec4 (const std::string& n, const glm::vec4& v)  { glUniform4fv(GetUniformLocation(n), 1, &v[0]); }
void Shader::SetMat4 (const std::string& n, const glm::mat4& v)  { glUniformMatrix4fv(GetUniformLocation(n), 1, GL_FALSE, &v[0][0]); }



uint32_t Shader::BuildProgram(const std::string& vertexSrc,
                              const std::string& fragmentSrc,
                              Stage stage)
{
    if (stage == Stage::Compute)
    {
        const uint32_t cs = CompileShader(GL_COMPUTE_SHADER, vertexSrc);
        const uint32_t program = glCreateProgram();
        glAttachShader(program, cs);
        glLinkProgram(program);

        int success = 0;
        glGetProgramiv(program, GL_LINK_STATUS, &success);
        if (!success)
        {
            char infoLog[1024];
            glGetProgramInfoLog(program, 1024, nullptr, infoLog);
            glDeleteProgram(program);
            glDeleteShader(cs);
            throw std::runtime_error(std::string("Compute program linking failed:\n") + infoLog);
        }

        glDeleteShader(cs);
        return program;
    }

    const uint32_t vs = CompileShader(GL_VERTEX_SHADER, vertexSrc);

    
    
    
    uint32_t fs = 0;
    try
    {
        fs = CompileShader(GL_FRAGMENT_SHADER, fragmentSrc);
    }
    catch (...)
    {
        glDeleteShader(vs);
        throw;
    }

    const uint32_t program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    int success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success)
    {
        char infoLog[1024];
        glGetProgramInfoLog(program, 1024, nullptr, infoLog);
        glDeleteProgram(program);
        glDeleteShader(vs);
        glDeleteShader(fs);
        throw std::runtime_error(std::string("Shader program linking failed:\n") + infoLog);
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
}

uint32_t Shader::CompileShader(uint32_t type, const std::string& source)
{
    uint32_t shader = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    int success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        char infoLog[1024];
        glGetShaderInfoLog(shader, 1024, nullptr, infoLog);
        const char* typeName = (type == GL_VERTEX_SHADER)   ? "Vertex"
                             : (type == GL_COMPUTE_SHADER)  ? "Compute"
                                                            : "Fragment";
        glDeleteShader(shader);
        throw std::runtime_error(std::string(typeName) + " shader compilation failed:\n" + infoLog);
    }
    return shader;
}

} 
