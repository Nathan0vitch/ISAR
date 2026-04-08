#include "rendering/renderer.h"
#include <iostream>

bool Renderer::init()
{
    // TODO: compile shaders, upload Earth mesh, init dynamic buffers
    return false;
}

void Renderer::shutdown()
{
    // TODO: glDelete* for all GPU objects
}

GpuMesh Renderer::uploadIndexed(const std::vector<float>& verts,
                                 const std::vector<unsigned>& idx)
{
    // TODO: port from main.cpp
    (void)verts; (void)idx;
    return {};
}

GpuDynamic Renderer::makeDynamic(int maxVerts)
{
    // TODO: port from main.cpp
    (void)maxVerts;
    return {};
}

void Renderer::updateDynamic(GpuDynamic& mesh, const std::vector<float>& verts)
{
    // TODO: glBufferSubData
    (void)mesh; (void)verts;
}

void Renderer::updateOrbitPath(const std::vector<float>& path)  { (void)path; }
void Renderer::updateSatellitePos(const glm::vec3& posGL)        { (void)posGL; }
void Renderer::updateTrail(const std::vector<float>& trail)      { (void)trail; }

void Renderer::drawSphere(const glm::mat4& mvp)   { (void)mvp; }
void Renderer::drawOrbit(const glm::mat4& mvp)    { (void)mvp; }
void Renderer::drawTrail(const glm::mat4& mvp)    { (void)mvp; }
void Renderer::drawAxes(const glm::mat4& mvp)     { (void)mvp; }

void Renderer::drawSatellite(const glm::vec3& posGL, const glm::mat4& mvp)
{
    (void)posGL; (void)mvp;
}

void Renderer::drawSigmaEllipse(const std::vector<float>& verts,
                                 const glm::mat4& mvp, const glm::vec3& color)
{
    (void)verts; (void)mvp; (void)color;
}

void Renderer::drawSlider2D(int W, int H, double simTime, double simMax)
{
    // TODO: port slider draw from main.cpp
    (void)W; (void)H; (void)simTime; (void)simMax;
}
