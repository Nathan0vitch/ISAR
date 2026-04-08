#include "rendering/camera.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

glm::mat4 Camera::view() const
{
    // TODO: port from main.cpp (arcball)
    return glm::mat4(1.0f);
}

glm::mat4 Camera::projection(float aspect) const
{
    // TODO: port from main.cpp
    (void)aspect;
    return glm::mat4(1.0f);
}

glm::vec3 Camera::position() const
{
    // TODO
    return glm::vec3(0.f);
}

void Camera::onMouseButton(int button, int action, double x, double y, int winH)
{
    // TODO: distinguish slider zone (y > winH - 50) from rotation zone
    (void)button; (void)action; (void)x; (void)y; (void)winH;
}

void Camera::onCursorPos(double x, double y)
{
    // TODO: update yaw/pitch when mouseDown_
    (void)x; (void)y;
}

void Camera::onScroll(double dy)
{
    // TODO: clamp radius
    (void)dy;
}

void Camera::reset()
{
    yaw       =  30.0f;
    pitch     =  20.0f;
    radius    =   4.5f;
    firstMove_ = true;
    mouseDown_ = false;
}
