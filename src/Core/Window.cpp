#include "Window.h"
#include "Input.h"
#include "../Utils/Log.h"
#include <GLFW/glfw3.h>
#include <stdexcept>

namespace Binax {

static bool s_GLFWInitialized = false;

Window::Window(const Props& props)
    : m_Width(props.Width)
    , m_Height(props.Height)
    , m_Title(props.Title)
    , m_VSync(props.VSync)
{
    if (!s_GLFWInitialized)
    {
        glfwSetErrorCallback(GlfwErrorCallback);
        if (!glfwInit())
            throw std::runtime_error("Failed to initialize GLFW");
        s_GLFWInitialized = true;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, props.Resizable ? GLFW_TRUE : GLFW_FALSE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);

    m_Window = glfwCreateWindow(m_Width, m_Height, m_Title.c_str(), nullptr, nullptr);
    if (!m_Window)
    {
        
        
        
        s_GLFWInitialized = false;
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window");
    }

    glfwMakeContextCurrent(m_Window);
    SetVSync(m_VSync);
    glfwSetWindowUserPointer(m_Window, this);

    Input::Init();
    SetEventCallbacks();

    BINAX_LOG_INFO("Window created: {} x {}", m_Width, m_Height);
}

Window::~Window()
{
    if (m_Window)
    {
        glfwDestroyWindow(m_Window);
        m_Window = nullptr;
    }
}

void Window::SetEventCallbacks()
{
    glfwSetKeyCallback(m_Window, [](GLFWwindow* window, int key, int scancode, int action, int mods)
    {
        (void)window;
        Input::KeyCallback(key, scancode, action, mods);
    });

    glfwSetMouseButtonCallback(m_Window, [](GLFWwindow* window, int button, int action, int mods)
    {
        (void)window;
        Input::MouseButtonCallback(button, action, mods);
    });

    glfwSetCursorPosCallback(m_Window, [](GLFWwindow* window, double xpos, double ypos)
    {
        (void)window;
        Input::CursorPosCallback(xpos, ypos);
    });

    glfwSetScrollCallback(m_Window, [](GLFWwindow* window, double xoffset, double yoffset)
    {
        (void)window;
        Input::ScrollCallback(xoffset, yoffset);
    });

    glfwSetWindowFocusCallback(m_Window, [](GLFWwindow* window, int focused)
    {
        (void)window;
        Input::FocusCallback(focused);
    });

    glfwSetFramebufferSizeCallback(m_Window, [](GLFWwindow* window, int width, int height)
    {
        Window* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
        if (self)
        {
            self->m_Width = static_cast<uint32_t>(width);
            self->m_Height = static_cast<uint32_t>(height);
        }
    });
}

void Window::Update()
{
    glfwPollEvents();
}

void Window::SwapBuffers()
{
    glfwSwapBuffers(m_Window);
}

void Window::SetVSync(bool enabled)
{
    m_VSync = enabled;
    glfwSwapInterval(enabled ? 1 : 0);
}

bool Window::ShouldClose() const
{
    return glfwWindowShouldClose(m_Window);
}

void Window::SetShouldClose(bool flag)
{
    glfwSetWindowShouldClose(m_Window, flag ? GLFW_TRUE : GLFW_FALSE);
}

void Window::GlfwErrorCallback(int error, const char* description)
{
    BINAX_LOG_ERROR("GLFW Error ({0}): {1}", error, description);
}

} 
