#include "Camera.h"
#include <cmath>

Camera::Camera(glm::vec3 position, glm::vec3 up, float yawDeg, float pitchDeg)
    : m_position(position),
      m_front(glm::vec3(0.0f, 0.0f, -1.0f)),
      m_worldUp(up),
      m_up(up),
      m_yaw(yawDeg),
      m_pitch(pitchDeg),
      m_fov(45.0f)
{
    updateVectors();
}

glm::mat4 Camera::getViewMatrix() const {
    return glm::lookAt(m_position, m_position + m_front, m_up);
}

void Camera::processKeyboard(bool forward, bool backward, bool left, bool right, float deltaTime) {
    float velocity = m_speed * deltaTime;
    if (forward)  m_position += m_front * velocity;
    if (backward) m_position -= m_front * velocity;

    glm::vec3 cameraRight = glm::normalize(glm::cross(m_front, m_up));
    if (left)  m_position -= cameraRight * velocity;
    if (right) m_position += cameraRight * velocity;
}

void Camera::processMouse(float xpos, float ypos, bool captured) {
    if (!captured) return;

    if (m_firstMouse) {
        m_lastX = xpos;
        m_lastY = ypos;
        m_firstMouse = false;
    }

    float xoffset = xpos - m_lastX;
    float yoffset = m_lastY - ypos; // reversed like you had

    m_lastX = xpos;
    m_lastY = ypos;

    xoffset *= m_mouseSensitivity;
    yoffset *= m_mouseSensitivity;

    m_yaw   += xoffset;
    m_pitch += yoffset;

    if (m_pitch > 89.0f)  m_pitch = 89.0f;
    if (m_pitch < -89.0f) m_pitch = -89.0f;

    updateVectors();
}

void Camera::processScroll(float yoffset) {
    m_fov -= yoffset;
    if (m_fov < 1.0f)  m_fov = 1.0f;
    if (m_fov > 90.0f) m_fov = 90.0f;
}

void Camera::updateVectors() {
    glm::vec3 front;
    front.x = std::cos(glm::radians(m_yaw)) * std::cos(glm::radians(m_pitch));
    front.y = std::sin(glm::radians(m_pitch));
    front.z = std::sin(glm::radians(m_yaw)) * std::cos(glm::radians(m_pitch));
    m_front = glm::normalize(front);

    glm::vec3 right = glm::normalize(glm::cross(m_front, m_worldUp));
    m_up = glm::normalize(glm::cross(right, m_front));
}
