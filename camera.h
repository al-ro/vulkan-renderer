#include <glm/glm.hpp>

/**
 * Orbit camera looking at the origin
 */
class Camera {
 public:
  glm::mat4 viewMatrix;
  glm::mat4 projectionMatrix;
  glm::vec3 position;
  glm::vec3 up;
  float pitch;
  float yaw;
  float distance = 1.0f;
  float aspect;
  float near;
  float far;
  float fov;

  Camera() = default;
  Camera(float pitch, float yaw, float distance, glm::vec3 up, float fov, float aspect, float near, float far);

  Camera(const Camera& camera) = default;
  ~Camera() = default;

  void updateMatrices();
  void updateCoordinates(glm::vec2 delta);
  void updatePosition();
};