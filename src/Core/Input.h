
#pragma once

#include <glm/glm.hpp>
#include <array>

struct GLFWwindow;

namespace Binax {

class Input
{
public:
    static void Init();
    static void Update();
    static void Shutdown();

    
    static void Reset();

    
    
    
    static bool ConsumeFocusRegained();

    
    static bool IsKeyPressed(int key);
    static bool IsKeyJustPressed(int key);
    static bool IsKeyJustReleased(int key);

    
    static bool IsMouseButtonPressed(int button);
    static bool IsMouseButtonJustPressed(int button);
    static bool IsMouseButtonJustReleased(int button);
    static glm::vec2 GetMousePosition();
    static float GetMouseX();
    static float GetMouseY();
    static void SetMousePosition(const glm::vec2& pos);

    
    static glm::vec2 GetScrollOffset() { return s_ScrollOffset; }

    
    static void SetCursorMode(int mode);
    static int GetCursorMode();

    static void SetWindow(GLFWwindow* window);

    
    static void KeyCallback(int key, int scancode, int action, int mods);
    static void MouseButtonCallback(int button, int action, int mods);
    static void CursorPosCallback(double xpos, double ypos);
    static void ScrollCallback(double xoffset, double yoffset);
    static void FocusCallback(int focused);

private:
    
    
    static constexpr int kMaxKeys     = 512;
    static constexpr int kMaxMouseBtn = 16;

    static std::array<bool, kMaxKeys>     s_KeyState;
    static std::array<bool, kMaxKeys>     s_KeyJustPressed;
    static std::array<bool, kMaxKeys>     s_KeyJustReleased;
    static std::array<bool, kMaxMouseBtn> s_MouseState;
    static std::array<bool, kMaxMouseBtn> s_MouseJustPressed;
    static std::array<bool, kMaxMouseBtn> s_MouseJustReleased;

    static glm::vec2 s_MousePosition;
    static glm::vec2 s_ScrollOffset;
    static GLFWwindow* s_Window;

    static bool s_FocusRegained;
};

} 