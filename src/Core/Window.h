
#pragma once

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <memory>
#include <string>

namespace Binax {

class Window
{
public:
    struct Props
    {
        std::string Title = "Binax Engine";
        uint32_t Width = 1920;   
        uint32_t Height = 1080;
        bool VSync = true;
        bool Resizable = true;
    };

    Window(const Props& props);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    void Update();
    void SwapBuffers();
    void SetVSync(bool enabled);
    bool ShouldClose() const;
    void SetShouldClose(bool flag);

    uint32_t GetWidth() const { return m_Width; }
    uint32_t GetHeight() const { return m_Height; }
    float GetAspect() const { return static_cast<float>(m_Width) / static_cast<float>(m_Height); }
    GLFWwindow* GetGLFWWindow() const { return m_Window; }

    void SetEventCallbacks();

private:
    static void GlfwErrorCallback(int error, const char* description);

    GLFWwindow* m_Window;
    uint32_t m_Width, m_Height;
    std::string m_Title;
    bool m_VSync;
};

} 