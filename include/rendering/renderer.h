#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>

struct GpuMesh {
    GLuint  vao = 0, vbo = 0, ebo = 0;
    GLsizei count = 0;
};

struct GpuDynamic {
    GLuint  vao = 0, vbo = 0;
    GLsizei count = 0;
};

class Renderer {
public:
    bool init();
    void shutdown();

    // Per-frame buffer updates
    void updateOrbitPath(const std::vector<float>& path);
    void updateSatellitePos(const glm::vec3& posGL);
    void updateTrail(const std::vector<float>& trail);

    // 3D draw calls (prog3d must be active)
    void drawSphere(const glm::mat4& mvp);
    void drawOrbit(const glm::mat4& mvp);
    void drawTrail(const glm::mat4& mvp);
    void drawAxes(const glm::mat4& mvp);
    void drawSatellite(const glm::vec3& posGL, const glm::mat4& mvp);
    void drawSigmaEllipse(const std::vector<float>& verts,
                          const glm::mat4& mvp, const glm::vec3& color);

    // 2D overlay
    void drawSlider2D(int W, int H, double simTime, double simMax);

    // Low-level upload helpers (exposed for migration from main.cpp)
    GpuMesh    uploadIndexed(const std::vector<float>& verts,
                             const std::vector<unsigned>& idx);
    GpuDynamic makeDynamic(int maxVerts);
    void       updateDynamic(GpuDynamic& mesh, const std::vector<float>& verts);

private:
    GLuint prog3d_     = 0;
    GLuint prog2d_     = 0;
    GLint  locMVP_     = -1;
    GLint  loc3dColor_ = -1;
    GLint  loc2dColor_ = -1;
    GLint  loc2dScreen_= -1;

    GpuMesh    earth_;
    GpuDynamic orbitPath_;
    GpuDynamic satPoint_;
    GpuDynamic trail_;

    GLuint axVAO_     = 0, axVBO_     = 0;
    GLuint sliderVAO_ = 0, sliderVBO_ = 0;
};
