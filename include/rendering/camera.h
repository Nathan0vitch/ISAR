#pragma once
#include <glm/glm.hpp>

class Camera {
public:
    Camera() = default;

    glm::mat4 view()               const;
    glm::mat4 projection(float aspect) const;
    glm::vec3 position()           const;

    // Call these from GLFW callbacks
    void onMouseButton(int button, int action, double x, double y, int winH);
    void onCursorPos(double x, double y);
    void onScroll(double dy);
    void reset();

    float yaw    =  30.0f;
    float pitch  =  20.0f;
    float radius =   4.5f;

private:
    bool  mouseDown_ = false;
    float lastX_     = 0.f;
    float lastY_     = 0.f;
    bool  firstMove_ = true;
};
