
#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Binax {












class EditorCamera
{
public:
    EditorCamera(float aspectRatio, float fov = 45.0f, float nearPlane = 0.1f, float farPlane = 100.0f);

    void Update(float deltaTime);
    void OnResize(float aspectRatio);
    void SetMouseOver(bool hovered) { m_MouseOver = hovered; }
    void SetGizmoActive(bool active) { m_GizmoActive = active; }

    const glm::mat4& GetViewMatrix() const { return m_ViewMatrix; }
    const glm::mat4& GetProjectionMatrix() const { return m_ProjectionMatrix; }
    glm::mat4 GetViewProjection() const { return m_ProjectionMatrix * m_ViewMatrix; }
    const glm::vec3& GetPosition() const { return m_Position; }
    const glm::quat& GetOrientation() const { return m_Orientation; }

    
    void SetPosition(const glm::vec3& pos)    { m_Position = pos; }
    void SetOrientation(const glm::quat& o);
    void SetFOV(float fov)                    { m_FOV = fov; }
    void SetNearFar(float n, float f)         { m_NearPlane = n; m_FarPlane = f; }
    void SetMoveSpeed(float s)                { m_MoveSpeed = s; }
    void SetRotationSpeed(float s)            { m_RotationSpeed = s; }
    void SetSpeedMultiplier(float m)          { m_SpeedMultiplier = m; }

    float GetFOV() const             { return m_FOV; }
    float GetNearPlane() const       { return m_NearPlane; }
    float GetFarPlane() const        { return m_FarPlane; }
    float GetMoveSpeed() const       { return m_MoveSpeed; }
    float GetRotationSpeed() const   { return m_RotationSpeed; }
    float GetSpeedMultiplier() const { return m_SpeedMultiplier; }

    
    static constexpr float kMaxPitchRadians = 1.5690511f;

private:
    glm::vec3 m_Position;
    glm::quat m_Orientation;                
    float m_Yaw   = 0.0f;
    float m_Pitch = 0.0f;

    float m_FOV;
    float m_NearPlane;
    float m_FarPlane;
    float m_AspectRatio;

    glm::mat4 m_ViewMatrix       = glm::mat4(1.0f);
    glm::mat4 m_ProjectionMatrix = glm::mat4(1.0f);

    float m_MoveSpeed;
    float m_RotationSpeed;
    float m_SpeedMultiplier;

    float m_LastMouseX = 0.0f;
    float m_LastMouseY = 0.0f;
    bool  m_IsRotating = false;
    bool  m_MouseOver  = false;
    bool  m_GizmoActive = false;
    bool  m_WasRightPressed = false;

    void UpdateMatrices();
    void RebuildOrientation();
};

} 