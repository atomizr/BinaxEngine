
#include "EditorCamera.h"
#include "Core/Input.h"
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/common.hpp>
#include <cmath>

namespace Binax {

EditorCamera::EditorCamera(float aspectRatio, float fov, float nearPlane, float farPlane)
    : m_Position(0.0f, 0.0f, 3.0f)
    , m_Orientation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f))
    , m_Yaw(0.0f)
    , m_Pitch(0.0f)
    , m_FOV(fov)
    , m_NearPlane(nearPlane)
    , m_FarPlane(farPlane)
    , m_AspectRatio(aspectRatio)
    , m_MoveSpeed(5.0f)
    , m_RotationSpeed(2.0f)
    , m_SpeedMultiplier(2.0f)
{
    UpdateMatrices();
}

void EditorCamera::Update(float deltaTime)
{
    
    
    const bool focusRegained = Input::ConsumeFocusRegained();

    
    
    
    if (m_GizmoActive)
    {
        if (m_IsRotating)
        {
            Input::SetCursorMode(GLFW_CURSOR_NORMAL);
            m_IsRotating = false;
        }
        m_WasRightPressed = Input::IsMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT);
        UpdateMatrices();
        return;
    }

    const bool pressed = Input::IsMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT);

    if (pressed && !m_WasRightPressed && m_MouseOver)
    {
        Input::SetCursorMode(GLFW_CURSOR_DISABLED);
        m_LastMouseX = Input::GetMouseX();
        m_LastMouseY = Input::GetMouseY();
        m_IsRotating = true;
    }
    else if (!pressed && m_WasRightPressed)
    {
        Input::SetCursorMode(GLFW_CURSOR_NORMAL);
        m_IsRotating = false;
    }
    m_WasRightPressed = pressed;

    
    
    
    if (m_IsRotating)
    {
        glm::vec3 forward = m_Orientation * glm::vec3(0.0f, 0.0f, -1.0f);
        glm::vec3 right   = m_Orientation * glm::vec3(1.0f, 0.0f, 0.0f);
        glm::vec3 up      = m_Orientation * glm::vec3(0.0f, 1.0f, 0.0f);

        float speed = m_MoveSpeed * deltaTime;
        if (Input::IsKeyPressed(GLFW_KEY_LEFT_SHIFT))
            speed *= m_SpeedMultiplier;

        if (Input::IsKeyPressed(GLFW_KEY_W)) m_Position += forward * speed;
        if (Input::IsKeyPressed(GLFW_KEY_S)) m_Position -= forward * speed;
        if (Input::IsKeyPressed(GLFW_KEY_A)) m_Position -= right   * speed;
        if (Input::IsKeyPressed(GLFW_KEY_D)) m_Position += right   * speed;
        if (Input::IsKeyPressed(GLFW_KEY_Q)) m_Position -= up      * speed;
        if (Input::IsKeyPressed(GLFW_KEY_E)) m_Position += up      * speed;
    }

    if (Input::GetCursorMode() == GLFW_CURSOR_DISABLED)
    {
        if (focusRegained)
        {
            m_LastMouseX = Input::GetMouseX();
            m_LastMouseY = Input::GetMouseY();
        }
        else
        {
            float dx = Input::GetMouseX() - m_LastMouseX;
            float dy = Input::GetMouseY() - m_LastMouseY;
            m_LastMouseX = Input::GetMouseX();
            m_LastMouseY = Input::GetMouseY();

            if (dx != 0.0f || dy != 0.0f)
            {
                constexpr float mouseSensitivity = 0.01f;
                constexpr float deg2rad = 3.14159265358979323846f / 180.0f;

                m_Yaw   -= dx * m_RotationSpeed * mouseSensitivity * deg2rad;
                m_Pitch -= dy * m_RotationSpeed * mouseSensitivity * deg2rad;
                m_Pitch  = glm::clamp(m_Pitch, -kMaxPitchRadians, kMaxPitchRadians);

                RebuildOrientation();
            }
        }
    }

    
    UpdateMatrices();
}

void EditorCamera::OnResize(float aspectRatio)
{
    m_AspectRatio = aspectRatio;
    
}

void EditorCamera::SetOrientation(const glm::quat& orient)
{
    
    const glm::vec3 forward = orient * glm::vec3(0.0f, 0.0f, -1.0f);
    m_Pitch = std::asin(glm::clamp(forward.y, -1.0f, 1.0f));
    m_Yaw   = std::atan2(-forward.x, -forward.z);
    m_Pitch = glm::clamp(m_Pitch, -kMaxPitchRadians, kMaxPitchRadians);

    RebuildOrientation();
    
}

void EditorCamera::UpdateMatrices()
{
    m_ViewMatrix = glm::lookAt(m_Position,
                               m_Position + m_Orientation * glm::vec3(0.0f, 0.0f, -1.0f),
                               m_Orientation * glm::vec3(0.0f, 1.0f, 0.0f));
    m_ProjectionMatrix = glm::perspective(glm::radians(m_FOV), m_AspectRatio, m_NearPlane, m_FarPlane);
}

void EditorCamera::RebuildOrientation()
{
    const glm::quat qYaw   = glm::angleAxis(m_Yaw,   glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::quat qPitch = glm::angleAxis(m_Pitch, glm::vec3(1.0f, 0.0f, 0.0f));
    m_Orientation = glm::normalize(qYaw * qPitch);
}

} 
