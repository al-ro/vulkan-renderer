#include "camera.h"

#include <math.h>

#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>

Camera::Camera(float pitch, float yaw, float distance, glm::vec3 up,
               float fov, float aspect, float near, float far) : pitch{pitch}, yaw{yaw}, distance{distance}, up{up}, fov{fov}, aspect(aspect), near{near}, far{far} {
  yaw = std::fmod(yaw, 2.0f * M_PI);
  pitch = std::fmax(0.01f, std::fmin(M_PI - 0.01f, pitch));

  position = {cos(yaw) * sin(pitch), sin(yaw) * sin(pitch), cos(pitch)};
  position = normalize(position);
  position *= distance;

  viewMatrix = glm::lookAt(position, glm::vec3(0), up);
  projectionMatrix = glm::perspective(fov, aspect, near, far);
}

void Camera::updateMatrices() {
  viewMatrix = glm::lookAt(position, glm::vec3(0), up);
  projectionMatrix = glm::perspective(fov, aspect, near, far);
}

/**
 * Update spherical coordinates (pitch and yaw) based on mouse input
 * @param delta change of cursor position on the screen
 */
void Camera::updateCoordinates(glm::vec2 delta) {
  float yawChange = std::fmod(delta.x * 0.01f, 2.0f * M_PI);
  yaw -= yawChange;

  float pitchChange = std::fmod(delta.y * 0.01f, 2.0f * M_PI);
  pitch += pitchChange;

  yaw = std::fmod(yaw, 2.0f * M_PI);
  pitch = std::fmax(0.01f, std::fmin(M_PI - 0.01f, pitch));
}

/**
 * Calculcate camera's Cartesian coordinates from spherical coordinates
 */
void Camera::updatePosition() {
  yaw = std::fmod(yaw, 2.0f * M_PI);
  pitch = std::fmax(0.01f, std::fmin(M_PI - 0.01f, pitch));

  position = {cos(yaw) * sin(pitch), sin(yaw) * sin(pitch), cos(pitch)};
  position = normalize(position);
  position *= distance;
}
