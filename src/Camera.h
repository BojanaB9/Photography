#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Camera {
public:
    // Defaults match what you currently use
    Camera(
        glm::vec3 position = glm::vec3(0.0f, 0.0f, 3.0f),
        glm::vec3 up       = glm::vec3(0.0f, 1.0f, 0.0f),
        float yawDeg       = -90.0f,
        float pitchDeg     = 0.0f
    );

    // Matrices
    glm::mat4 getViewMatrix() const;

    // Controls
    void processKeyboard(bool forward, bool backward, bool left, bool right, float deltaTime);
    void processMouse(float xpos, float ypos, bool captured);
    void processScroll(float yoffset);

    // Accessors
    const glm::vec3& position() const { return m_position; }
    const glm::vec3& front() const { return m_front; }
    const glm::vec3& up() const { return m_up; }
    float fov() const { return m_fov; }

    // Settings
    void setMouseSensitivity(float s) { m_mouseSensitivity = s; }
    void setSpeed(float s) { m_speed = s; }

    // Useful when recapturing mouse to avoid jump
    void resetFirstMouse() { m_firstMouse = true; }

private:
    glm::vec3 m_position;
    glm::vec3 m_front;
    glm::vec3 m_worldUp;
    glm::vec3 m_up;

    float m_yaw;
    float m_pitch;
    float m_fov;

    float m_speed = 2.5f;
    float m_mouseSensitivity = 0.1f;

    float m_lastX = 0.0f;
    float m_lastY = 0.0f;
    bool  m_firstMouse = true;

    void updateVectors();
};

