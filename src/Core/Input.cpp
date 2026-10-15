
#include "Input.h"
#include <GLFW/glfw3.h>

namespace Binax {

std::array<bool, Input::kMaxKeys>     Input::s_KeyState{};
std::array<bool, Input::kMaxKeys>     Input::s_KeyJustPressed{};
std::array<bool, Input::kMaxKeys>     Input::s_KeyJustReleased{};
std::array<bool, Input::kMaxMouseBtn> Input::s_MouseState{};
std::array<bool, Input::kMaxMouseBtn> Input::s_MouseJustPressed{};
std::array<bool, Input::kMaxMouseBtn> Input::s_MouseJustReleased{};
glm::vec2 Input::s_MousePosition = glm::vec2(0.0f);
glm::vec2 Input::s_ScrollOffset = glm::vec2(0.0f);
GLFWwindow* Input::s_Window = nullptr;
bool Input::s_FocusRegained = false;

void Input::Init()
{
    s_KeyState.fill(false);
    s_KeyJustPressed.fill(false);
    s_KeyJustReleased.fill(false);
    s_MouseState.fill(false);
    s_MouseJustPressed.fill(false);
    s_MouseJustReleased.fill(false);
    s_FocusRegained = false;
}

void Input::Update()
{
    s_KeyJustPressed.fill(false);
    s_KeyJustReleased.fill(false);
    s_MouseJustPressed.fill(false);
    s_MouseJustReleased.fill(false);
    s_ScrollOffset = glm::vec2(0.0f);
    
    
}

void Input::Shutdown()
{
    s_KeyState.fill(false);
    s_KeyJustPressed.fill(false);
    s_KeyJustReleased.fill(false);
    s_MouseState.fill(false);
    s_MouseJustPressed.fill(false);
    s_MouseJustReleased.fill(false);
}

void Input::Reset()
{
    s_KeyState.fill(false);
    s_KeyJustPressed.fill(false);
    s_KeyJustReleased.fill(false);
    s_MouseState.fill(false);
    s_MouseJustPressed.fill(false);
    s_MouseJustReleased.fill(false);
    s_ScrollOffset = glm::vec2(0.0f);
}

bool Input::ConsumeFocusRegained()
{
    bool v = s_FocusRegained;
    s_FocusRegained = false;
    return v;
}

void Input::SetWindow(GLFWwindow* window)
{
    s_Window = window;
}

void Input::SetCursorMode(int mode)
{
    if (s_Window)
        glfwSetInputMode(s_Window, GLFW_CURSOR, mode);
}

int Input::GetCursorMode()
{
    if (s_Window)
        return glfwGetInputMode(s_Window, GLFW_CURSOR);
    return GLFW_CURSOR_NORMAL;
}

bool Input::IsKeyPressed(int key)
{
    return key >= 0 && key < kMaxKeys && s_KeyState[key];
}

bool Input::IsKeyJustPressed(int key)
{
    return key >= 0 && key < kMaxKeys && s_KeyJustPressed[key];
}

bool Input::IsKeyJustReleased(int key)
{
    return key >= 0 && key < kMaxKeys && s_KeyJustReleased[key];
}

bool Input::IsMouseButtonPressed(int button)
{
    return button >= 0 && button < kMaxMouseBtn && s_MouseState[button];
}

bool Input::IsMouseButtonJustPressed(int button)
{
    return button >= 0 && button < kMaxMouseBtn && s_MouseJustPressed[button];
}

bool Input::IsMouseButtonJustReleased(int button)
{
    return button >= 0 && button < kMaxMouseBtn && s_MouseJustReleased[button];
}

glm::vec2 Input::GetMousePosition()
{
    return s_MousePosition;
}

float Input::GetMouseX() { return s_MousePosition.x; }
float Input::GetMouseY() { return s_MousePosition.y; }

void Input::SetMousePosition(const glm::vec2& pos)
{
    s_MousePosition = pos;
    if (s_Window)
        glfwSetCursorPos(s_Window, pos.x, pos.y);
}

void Input::KeyCallback(int key, int scancode, int action, int mods)
{
    (void)scancode;
    (void)mods;

    if (key < 0 || key >= kMaxKeys)
        return;

    if (action == GLFW_PRESS)
    {
        s_KeyState[key] = true;
        s_KeyJustPressed[key] = true;
    }
    else if (action == GLFW_RELEASE)
    {
        s_KeyState[key] = false;
        s_KeyJustReleased[key] = true;
    }
    else if (action == GLFW_REPEAT)
    {
        s_KeyState[key] = true;
    }
}

void Input::MouseButtonCallback(int button, int action, int mods)
{
    (void)mods;

    if (button < 0 || button >= kMaxMouseBtn)
        return;

    if (action == GLFW_PRESS)
    {
        s_MouseState[button] = true;
        s_MouseJustPressed[button] = true;
    }
    else if (action == GLFW_RELEASE)
    {
        s_MouseState[button] = false;
        s_MouseJustReleased[button] = true;
    }
}

void Input::CursorPosCallback(double xpos, double ypos)
{
    s_MousePosition.x = static_cast<float>(xpos);
    s_MousePosition.y = static_cast<float>(ypos);
}

void Input::ScrollCallback(double xoffset, double yoffset)
{
    s_ScrollOffset.x += static_cast<float>(xoffset);
    s_ScrollOffset.y += static_cast<float>(yoffset);
}

void Input::FocusCallback(int focused)
{
    if (focused == GLFW_FALSE)
    {
        Reset();
    }
    else
    {
        s_FocusRegained = true;
    }
}

} 