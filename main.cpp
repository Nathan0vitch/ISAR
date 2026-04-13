#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>

// ─── État global ──────────────────────────────────────────────────────────────
static int WIN_W = 1400, WIN_H = 750;

// Panneau split
static float splitFrac = 0.5f;          // fraction [0.15 ; 0.85] : limite 3D / planisphère

// Caméra 3D (arcball)
static float camYaw    = 30.0f;
static float camPitch  = 20.0f;
static float camRadius =  3.0f;

// Planisphère
static float mapCtrLon =  0.0f;         // longitude centre (°, non borné : gère le loop)
static float mapCtrLat =  0.0f;         // latitude  centre (°)
static float mapZoom   =  1.0f;         // 1 = monde entier en hauteur

// Souris
static bool  lmbDown   = false;
static float lastMX    = 0.0f, lastMY = 0.0f;
static bool  dragIn3D  = false;
static bool  dragSplit = false;

// GLFW
static GLFWwindow* gWindow    = nullptr;
static GLFWcursor* gCurResize = nullptr;
static GLFWcursor* gCurArrow  = nullptr;

// ─── Helpers split ────────────────────────────────────────────────────────────
static float splitX_px()           { return splitFrac * WIN_W; }
static bool  nearSplit(double x)   { return std::abs((float)x - splitX_px()) < 6.0f; }
static bool  inMapPanel(double x)  { return (float)x > splitX_px(); }

// Clamp latitude pour que pôle nord/sud ne puisse pas entrer dans la fenêtre.
// La demi-hauteur visible en degrés = 90 / mapZoom.
// Le pôle est visible si |mapCtrLat| > 90 − 90/mapZoom.
static void clamp_lat()
{
    float latMax = std::max(0.0f, 90.0f - 90.0f / mapZoom);
    mapCtrLat = glm::clamp(mapCtrLat, -latMax, latMax);
}

// ─── Callbacks ───────────────────────────────────────────────────────────────
static void framebuffer_size_callback(GLFWwindow*, int w, int h)
{
    WIN_W = w; WIN_H = h;
}

static void mouse_button_callback(GLFWwindow* window, int button, int action, int)
{
    if (button != GLFW_MOUSE_BUTTON_LEFT) return;

    if (action == GLFW_PRESS) {
        double cx, cy;
        glfwGetCursorPos(window, &cx, &cy);
        lmbDown = true;
        if (nearSplit(cx)) {
            dragSplit = true;
            dragIn3D  = false;
            glfwSetCursor(window, gCurResize);
        } else {
            dragSplit = false;
            dragIn3D  = !inMapPanel(cx);
        }
    } else {
        lmbDown   = false;
        dragSplit = false;
    }
}

static void cursor_pos_callback(GLFWwindow* window, double xpos, double ypos)
{
    float dx = (float)xpos - lastMX;
    float dy = (float)ypos - lastMY;

    if (dragSplit) {
        splitFrac = glm::clamp((float)xpos / WIN_W, 0.15f, 0.85f);
    } else if (lmbDown) {
        if (dragIn3D) {
            camYaw   += dx * 0.4f;
            camPitch  = glm::clamp(camPitch + dy * 0.4f, -89.0f, 89.0f);
        } else {
            float pixPerDeg = (WIN_H / 180.0f) * mapZoom;
            mapCtrLon -= dx / pixPerDeg;   // pas de clamp : loop naturel
            mapCtrLat += dy / pixPerDeg;
            clamp_lat();
        }
    } else {
        // Changement de curseur selon position
        if (nearSplit(xpos))
            glfwSetCursor(window, gCurResize);
        else
            glfwSetCursor(window, gCurArrow);
    }

    lastMX = (float)xpos;
    lastMY = (float)ypos;
}

static void scroll_callback(GLFWwindow* window, double, double yoffset)
{
    double cx, cy;
    glfwGetCursorPos(window, &cx, &cy);

    if (!inMapPanel(cx)) {
        camRadius = glm::clamp(camRadius - (float)yoffset * 0.25f, 1.2f, 10.0f);
    } else if (!nearSplit(cx)) {
        float factor = (yoffset > 0) ? 1.15f : (1.0f / 1.15f);
        mapZoom = glm::clamp(mapZoom * factor, 0.4f, 25.0f);
        clamp_lat();
    }
}

static void key_callback(GLFWwindow* window, int key, int, int action, int)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}

// ─── Compilation shaders ─────────────────────────────────────────────────────
static GLuint compile_shader(GLenum type, const char* src)
{
    GLuint id = glCreateShader(type);
    glShaderSource(id, 1, &src, nullptr);
    glCompileShader(id);
    GLint ok;
    glGetShaderiv(id, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char buf[512]; glGetShaderInfoLog(id, 512, nullptr, buf);
        std::cerr << "Shader error: " << buf << "\n";
    }
    return id;
}

static GLuint create_program(const char* vert, const char* frag)
{
    GLuint vs  = compile_shader(GL_VERTEX_SHADER,   vert);
    GLuint fs  = compile_shader(GL_FRAGMENT_SHADER, frag);
    GLuint pgm = glCreateProgram();
    glAttachShader(pgm, vs); glAttachShader(pgm, fs);
    glLinkProgram(pgm);
    glDeleteShader(vs); glDeleteShader(fs);
    return pgm;
}

// ─── Shader 3D Phong (sphère) ─────────────────────────────────────────────────
static const char* VERT3D = R"glsl(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
uniform mat4 uMVP;
uniform mat4 uModel;
out vec3 vNormal;
out vec3 vFragPos;
void main()
{
    vFragPos    = vec3(uModel * vec4(aPos, 1.0));
    vNormal     = mat3(transpose(inverse(uModel))) * aNormal;
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)glsl";

static const char* FRAG3D = R"glsl(
#version 330 core
in  vec3 vNormal;
in  vec3 vFragPos;
uniform vec3 uSunDir;
uniform vec3 uColor;
out vec4 FragColor;
void main()
{
    vec3 norm    = normalize(vNormal);
    float diff   = max(dot(norm, normalize(uSunDir)), 0.0);
    vec3 ambient = 0.08 * uColor;                        // côté nuit quasi-noir
    vec3 diffuse = diff * uColor;
    vec3 viewDir = normalize(-vFragPos);
    vec3 reflDir = reflect(-normalize(uSunDir), norm);
    float spec   = pow(max(dot(viewDir, reflDir), 0.0), 48.0);
    vec3 specular = 0.20 * vec3(1.0) * spec;
    FragColor = vec4(ambient + diffuse + specular, 1.0);
}
)glsl";

// ─── Shader 3D Flat (lignes, marqueurs) ───────────────────────────────────────
static const char* VERT_FLAT3D = R"glsl(
#version 330 core
layout(location = 0) in vec3 aPos;
uniform mat4 uMVP;
void main() { gl_Position = uMVP * vec4(aPos, 1.0); }
)glsl";

static const char* FRAG_FLAT3D = R"glsl(
#version 330 core
uniform vec4 uColor;
out vec4 FragColor;
void main() { FragColor = uColor; }
)glsl";

// ─── Shader 2D (planisphère) ──────────────────────────────────────────────────
static const char* VERT2D = R"glsl(
#version 330 core
layout(location = 0) in vec2 aPos;
uniform mat4 uProj;
void main() { gl_Position = uProj * vec4(aPos, 0.0, 1.0); }
)glsl";

static const char* FRAG2D = R"glsl(
#version 330 core
uniform vec4 uColor;
out vec4 FragColor;
void main() { FragColor = uColor; }
)glsl";

// ─── Géométrie sphère ────────────────────────────────────────────────────────
struct SphereGPU { GLuint vao, vbo, ebo; GLsizei count; };

static SphereGPU build_sphere(float r, int stacks, int slices)
{
    std::vector<float>        verts;
    std::vector<unsigned int> idx;
    for (int i = 0; i <= stacks; ++i) {
        float phi = glm::pi<float>() * i / stacks;
        for (int j = 0; j <= slices; ++j) {
            float theta = glm::two_pi<float>() * j / slices;
            float x = std::sin(phi) * std::cos(theta);
            float y = std::cos(phi);
            float z = std::sin(phi) * std::sin(theta);
            verts.push_back(r*x); verts.push_back(r*y); verts.push_back(r*z);
            verts.push_back(x);   verts.push_back(y);   verts.push_back(z);
        }
    }
    for (int i = 0; i < stacks; ++i)
        for (int j = 0; j < slices; ++j) {
            unsigned a = i*(slices+1)+j, b = a+(slices+1);
            idx.push_back(a); idx.push_back(b); idx.push_back(a+1);
            idx.push_back(a+1); idx.push_back(b); idx.push_back(b+1);
        }
    SphereGPU g; g.count = (GLsizei)idx.size();
    glGenVertexArrays(1,&g.vao); glGenBuffers(1,&g.vbo); glGenBuffers(1,&g.ebo);
    glBindVertexArray(g.vao);
    glBindBuffer(GL_ARRAY_BUFFER, g.vbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size()*sizeof(float), verts.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size()*sizeof(unsigned), idx.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,6*sizeof(float),(void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,6*sizeof(float),(void*)(3*sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
    return g;
}

// ─── Buffer dynamique 2D ─────────────────────────────────────────────────────
struct DynBuf2D { GLuint vao, vbo; };

static DynBuf2D make_dyn2d()
{
    DynBuf2D d;
    glGenVertexArrays(1,&d.vao); glGenBuffers(1,&d.vbo);
    glBindVertexArray(d.vao);
    glBindBuffer(GL_ARRAY_BUFFER, d.vbo);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,2*sizeof(float),nullptr);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
    return d;
}

static void draw_2d(DynBuf2D& d, const std::vector<float>& pts,
                    GLenum mode, GLint locColor, glm::vec4 col)
{
    if (pts.empty()) return;
    glBindBuffer(GL_ARRAY_BUFFER, d.vbo);
    glBufferData(GL_ARRAY_BUFFER, pts.size()*sizeof(float), pts.data(), GL_DYNAMIC_DRAW);
    glUniform4fv(locColor, 1, glm::value_ptr(col));
    glBindVertexArray(d.vao);
    glDrawArrays(mode, 0, (GLsizei)(pts.size()/2));
    glBindVertexArray(0);
}

// ─── Buffer dynamique 3D (lignes, marqueurs non-éclairés) ────────────────────
struct DynBuf3D { GLuint vao, vbo; };

static DynBuf3D make_dyn3d()
{
    DynBuf3D d;
    glGenVertexArrays(1,&d.vao); glGenBuffers(1,&d.vbo);
    glBindVertexArray(d.vao);
    glBindBuffer(GL_ARRAY_BUFFER, d.vbo);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),nullptr);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
    return d;
}

static void draw_3d(DynBuf3D& d, const std::vector<float>& pts,
                    GLenum mode, GLint locColor, glm::vec4 col)
{
    if (pts.empty()) return;
    glBindBuffer(GL_ARRAY_BUFFER, d.vbo);
    glBufferData(GL_ARRAY_BUFFER, pts.size()*sizeof(float), pts.data(), GL_DYNAMIC_DRAW);
    glUniform4fv(locColor, 1, glm::value_ptr(col));
    glBindVertexArray(d.vao);
    glDrawArrays(mode, 0, (GLsizei)(pts.size()/3));
    glBindVertexArray(0);
}

// ─── Projection lon/lat → pixels écran ───────────────────────────────────────
// splitX : pixel x de la frontière 3D/planisphère
static glm::vec2 lonlat_to_px(float lon, float lat, int splitX)
{
    int panelW = WIN_W - splitX;
    float pixPerDeg = (WIN_H / 180.0f) * mapZoom;
    float sx = panelW * 0.5f + (lon - mapCtrLon) * pixPerDeg;
    float sy = WIN_H  * 0.5f - (lat - mapCtrLat) * pixPerDeg;
    return { splitX + sx, sy };
}

// Demi-longitude visible (en degrés), avec marge
static float half_vis_lon(int splitX, float margin = 5.0f)
{
    int panelW = WIN_W - splitX;
    float pixPerDeg = (WIN_H / 180.0f) * mapZoom;
    return panelW / (2.0f * pixPerDeg) + margin;
}

// ─── Graticule avec loop ──────────────────────────────────────────────────────
static std::vector<float> build_graticule(int splitX, int lonStep, int latStep)
{
    std::vector<float> pts;
    float hvl = half_vis_lon(splitX, (float)lonStep * 2);

    // Méridiens dans la plage visible (gère automatiquement le loop)
    int lonMin = (int)std::floor((mapCtrLon - hvl) / lonStep) * lonStep;
    int lonMax = (int)std::ceil ((mapCtrLon + hvl) / lonStep) * lonStep;
    for (int lon = lonMin; lon <= lonMax; lon += lonStep) {
        for (int s = 0; s < 8; ++s) {
            float lat0 = -90.0f + s * (180.0f/8);
            float lat1 = lat0 + (180.0f/8);
            auto p0 = lonlat_to_px((float)lon, lat0, splitX);
            auto p1 = lonlat_to_px((float)lon, lat1, splitX);
            pts.push_back(p0.x); pts.push_back(p0.y);
            pts.push_back(p1.x); pts.push_back(p1.y);
        }
    }

    // Parallèles sur toute la plage visible
    float lon0 = mapCtrLon - hvl, lon1 = mapCtrLon + hvl;
    for (int lat = -90; lat <= 90; lat += latStep) {
        auto p0 = lonlat_to_px(lon0, (float)lat, splitX);
        auto p1 = lonlat_to_px(lon1, (float)lat, splitX);
        pts.push_back(p0.x); pts.push_back(p0.y);
        pts.push_back(p1.x); pts.push_back(p1.y);
    }
    return pts;
}

// Ligne horizontale pleine largeur (équateur, tropiques…)
static std::vector<float> lat_line(float lat, int splitX)
{
    float hvl = half_vis_lon(splitX);
    auto p0 = lonlat_to_px(mapCtrLon - hvl, lat, splitX);
    auto p1 = lonlat_to_px(mapCtrLon + hvl, lat, splitX);
    return { p0.x, p0.y, p1.x, p1.y };
}

// Ligne méridien unique (avec copies loop ±360°)
static std::vector<float> lon_line(float lon, int splitX)
{
    float hvl = half_vis_lon(splitX);
    std::vector<float> pts;
    for (int n = -1; n <= 1; ++n) {
        float L = lon + n * 360.0f;
        if (L < mapCtrLon - hvl - 1 || L > mapCtrLon + hvl + 1) continue;
        for (int s = 0; s < 8; ++s) {
            float lat0 = -90.0f + s * 22.5f;
            auto p0 = lonlat_to_px(L, lat0,        splitX);
            auto p1 = lonlat_to_px(L, lat0 + 22.5f, splitX);
            pts.push_back(p0.x); pts.push_back(p0.y);
            pts.push_back(p1.x); pts.push_back(p1.y);
        }
    }
    return pts;
}

// ─── Hexagone 2D centré sur un pixel ─────────────────────────────────────────
static std::vector<float> hex_2d(float cx, float cy, float r)
{
    std::vector<float> pts;
    for (int i = 0; i <= 6; ++i) {
        float a = i * glm::pi<float>() / 3.0f;
        pts.push_back(cx + r * std::cos(a));
        pts.push_back(cy + r * std::sin(a));
    }
    return pts;
}

// ─── Hexagone 3D sur la surface de la sphère ─────────────────────────────────
static std::vector<float> hex_sphere(float lat_deg, float lon_deg, float r)
{
    float lat = glm::radians(lat_deg);
    float lon = glm::radians(lon_deg);
    glm::vec3 P = {
        std::cos(lat) * std::cos(lon),
        std::sin(lat),
        std::cos(lat) * std::sin(lon)
    };
    glm::vec3 east  = glm::normalize(glm::vec3(-std::sin(lon), 0.0f, std::cos(lon)));
    glm::vec3 north = glm::normalize(glm::vec3(
        -std::sin(lat)*std::cos(lon),  std::cos(lat),
        -std::sin(lat)*std::sin(lon)));
    std::vector<float> pts;
    for (int i = 0; i <= 6; ++i) {
        float a = i * glm::pi<float>() / 3.0f;
        glm::vec3 v = P * 1.005f + r * (std::cos(a)*east + std::sin(a)*north);
        pts.push_back(v.x); pts.push_back(v.y); pts.push_back(v.z);
    }
    return pts;
}

// ─── Rectangle plein ─────────────────────────────────────────────────────────
static std::vector<float> make_rect(float x0, float y0, float x1, float y1)
{
    return { x0,y0, x1,y0, x1,y1, x1,y1, x0,y1, x0,y0 };
}

// ─── Main ────────────────────────────────────────────────────────────────────
int main()
{
    if (!glfwInit()) { std::cerr << "glfwInit failed\n"; return -1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);

    gWindow = glfwCreateWindow(WIN_W, WIN_H, "OrbitalSim", nullptr, nullptr);
    if (!gWindow) { std::cerr << "glfwCreateWindow failed\n"; glfwTerminate(); return -1; }
    glfwMakeContextCurrent(gWindow);
    glfwSwapInterval(1);

    gCurResize = glfwCreateStandardCursor(GLFW_HRESIZE_CURSOR);
    gCurArrow  = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "GLAD failed\n"; return -1;
    }

    glfwSetFramebufferSizeCallback(gWindow, framebuffer_size_callback);
    glfwSetMouseButtonCallback    (gWindow, mouse_button_callback);
    glfwSetCursorPosCallback      (gWindow, cursor_pos_callback);
    glfwSetScrollCallback         (gWindow, scroll_callback);
    glfwSetKeyCallback            (gWindow, key_callback);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    glEnable(GL_SCISSOR_TEST);

    // ── Shaders ──────────────────────────────────────────────────────────────
    GLuint shader3D     = create_program(VERT3D,      FRAG3D);
    GLuint shaderFlat3D = create_program(VERT_FLAT3D, FRAG_FLAT3D);
    GLuint shader2D     = create_program(VERT2D,      FRAG2D);

    GLint loc3D_MVP    = glGetUniformLocation(shader3D, "uMVP");
    GLint loc3D_Model  = glGetUniformLocation(shader3D, "uModel");
    GLint loc3D_Sun    = glGetUniformLocation(shader3D, "uSunDir");
    GLint loc3D_Color  = glGetUniformLocation(shader3D, "uColor");

    GLint locFlat_MVP  = glGetUniformLocation(shaderFlat3D, "uMVP");
    GLint locFlat_Col  = glGetUniformLocation(shaderFlat3D, "uColor");

    GLint loc2D_Proj   = glGetUniformLocation(shader2D, "uProj");
    GLint loc2D_Color  = glGetUniformLocation(shader2D, "uColor");

    // ── Géométries ───────────────────────────────────────────────────────────
    SphereGPU sphere = build_sphere(1.0f, 48, 64);
    DynBuf2D  dyn2   = make_dyn2d();
    DynBuf3D  dyn3   = make_dyn3d();

    // Direction du soleil (lumière parallèle, rayons normalisés)
    // Angle "matin" depuis la droite et légèrement au-dessus — terminator visible
    const glm::vec3 sunDir = glm::normalize(glm::vec3(1.6f, 0.8f, 0.7f));

    // Données fixes Paris
    const float PARIS_LAT = 48.85f, PARIS_LON = 2.35f;
    auto parisHex3D = hex_sphere(PARIS_LAT, PARIS_LON, 0.055f);

    // ── Boucle de rendu ───────────────────────────────────────────────────────
    while (!glfwWindowShouldClose(gWindow))
    {
        glfwPollEvents();

        int fbW, fbH;
        glfwGetFramebufferSize(gWindow, &fbW, &fbH);
        int splitX = (int)(splitFrac * fbW);

        // Clear global
        glScissor(0, 0, fbW, fbH);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // ════════════════════════════════════════════════════════════════════
        // PANNEAU GAUCHE — vue 3D
        // ════════════════════════════════════════════════════════════════════
        glViewport(0, 0, splitX, fbH);
        glScissor (0, 0, splitX, fbH);
        glEnable(GL_DEPTH_TEST);

        glClearColor(0.04f, 0.05f, 0.10f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Matrices 3D
        float yRad = glm::radians(camYaw), pRad = glm::radians(camPitch);
        glm::vec3 camPos = {
            camRadius * std::cos(pRad) * std::sin(yRad),
            camRadius * std::sin(pRad),
            camRadius * std::cos(pRad) * std::cos(yRad)
        };
        float aspect3D = (float)splitX / (float)fbH;
        glm::mat4 proj3D  = glm::perspective(glm::radians(45.0f), aspect3D, 0.1f, 100.0f);
        glm::mat4 view3D  = glm::lookAt(camPos, glm::vec3(0.0f), glm::vec3(0,1,0));
        glm::mat4 model3D = glm::mat4(1.0f);
        glm::mat4 mvp3D   = proj3D * view3D * model3D;

        // — Sphère (Phong) ---------------------------------------------------
        glUseProgram(shader3D);
        glUniformMatrix4fv(loc3D_MVP,   1, GL_FALSE, glm::value_ptr(mvp3D));
        glUniformMatrix4fv(loc3D_Model, 1, GL_FALSE, glm::value_ptr(model3D));
        glUniform3fv(loc3D_Sun,   1, glm::value_ptr(sunDir));
        glUniform3f (loc3D_Color, 0.14f, 0.40f, 0.80f);   // bleu Terre

        glBindVertexArray(sphere.vao);
        glDrawElements(GL_TRIANGLES, sphere.count, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);

        // — Éléments flat (lignes, marqueurs) ---------------------------------
        glUseProgram(shaderFlat3D);
        glUniformMatrix4fv(locFlat_MVP, 1, GL_FALSE, glm::value_ptr(mvp3D));

        // Pôle Nord : trait rouge
        draw_3d(dyn3, { 0.0f, 1.0f, 0.0f,  0.0f, 1.5f, 0.0f },
                GL_LINES, locFlat_Col, { 1.0f, 0.22f, 0.18f, 1.0f });

        // Pôle Sud : trait bleu
        draw_3d(dyn3, { 0.0f, -1.0f, 0.0f,  0.0f, -1.5f, 0.0f },
                GL_LINES, locFlat_Col, { 0.22f, 0.55f, 1.0f, 1.0f });

        // Paris : hexagone doré
        draw_3d(dyn3, parisHex3D, GL_LINE_STRIP, locFlat_Col, { 1.0f, 0.85f, 0.2f, 1.0f });

        // ════════════════════════════════════════════════════════════════════
        // PANNEAU DROIT — planisphère
        // ════════════════════════════════════════════════════════════════════
        glViewport(splitX, 0, fbW - splitX, fbH);
        glScissor (splitX, 0, fbW - splitX, fbH);
        glDisable(GL_DEPTH_TEST);

        glClearColor(0.04f, 0.07f, 0.13f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glm::mat4 proj2D = glm::ortho(0.0f,(float)fbW,(float)fbH,0.0f,-1.0f,1.0f);
        glUseProgram(shader2D);
        glUniformMatrix4fv(loc2D_Proj, 1, GL_FALSE, glm::value_ptr(proj2D));

        // — Fond océan -------------------------------------------------------
        {
            auto bg = make_rect((float)splitX, 0.0f, (float)fbW, (float)fbH);
            draw_2d(dyn2, bg, GL_TRIANGLES, loc2D_Color, {0.04f,0.07f,0.13f,1.0f});
        }

        // — Graticule fin (10°) ----------------------------------------------
        draw_2d(dyn2, build_graticule(splitX, 10, 10), GL_LINES,
                loc2D_Color, {0.07f, 0.16f, 0.28f, 1.0f});

        // — Graticule principal (30°) ----------------------------------------
        draw_2d(dyn2, build_graticule(splitX, 30, 30), GL_LINES,
                loc2D_Color, {0.13f, 0.26f, 0.44f, 1.0f});

        // — Équateur ---------------------------------------------------------
        draw_2d(dyn2, lat_line(0.0f, splitX),
                GL_LINES, loc2D_Color, {0.30f, 0.55f, 0.82f, 1.0f});

        // — Méridien de Greenwich --------------------------------------------
        draw_2d(dyn2, lon_line(0.0f, splitX),
                GL_LINES, loc2D_Color, {0.30f, 0.55f, 0.82f, 1.0f});

        // — Tropiques (23.5°) ------------------------------------------------
        draw_2d(dyn2, lat_line( 23.5f, splitX), GL_LINES, loc2D_Color, {0.20f,0.38f,0.58f,1.0f});
        draw_2d(dyn2, lat_line(-23.5f, splitX), GL_LINES, loc2D_Color, {0.20f,0.38f,0.58f,1.0f});

        // — Cercles polaires (66.5°) -----------------------------------------
        draw_2d(dyn2, lat_line( 66.5f, splitX), GL_LINES, loc2D_Color, {0.20f,0.38f,0.58f,1.0f});
        draw_2d(dyn2, lat_line(-66.5f, splitX), GL_LINES, loc2D_Color, {0.20f,0.38f,0.58f,1.0f});

        // — Paris (3 copies pour le loop) ------------------------------------
        for (int n = -1; n <= 1; ++n) {
            float lon = PARIS_LON + n * 360.0f;
            auto c = lonlat_to_px(lon, PARIS_LAT, splitX);
            draw_2d(dyn2, hex_2d(c.x, c.y, 8.0f), GL_LINE_STRIP,
                    loc2D_Color, {1.0f, 0.85f, 0.2f, 1.0f});
        }

        // ════════════════════════════════════════════════════════════════════
        // SÉPARATEUR + POIGNÉE
        // ════════════════════════════════════════════════════════════════════
        glViewport(0, 0, fbW, fbH);
        glScissor (0, 0, fbW, fbH);
        glUseProgram(shader2D);
        glUniformMatrix4fv(loc2D_Proj, 1, GL_FALSE, glm::value_ptr(proj2D));

        {
            float sx = (float)splitX;
            // Barre de 2 px
            draw_2d(dyn2,
                    make_rect(sx - 1.0f, 0.0f, sx + 1.0f, (float)fbH),
                    GL_TRIANGLES, loc2D_Color, {0.50f, 0.72f, 1.0f, 1.0f});

            // Poignée centrale (rectangle légèrement plus large, 16 px haut)
            float mid = fbH * 0.5f;
            draw_2d(dyn2,
                    make_rect(sx - 4.0f, mid - 20.0f, sx + 4.0f, mid + 20.0f),
                    GL_TRIANGLES, loc2D_Color, {0.70f, 0.85f, 1.0f, 1.0f});
        }

        glfwSwapBuffers(gWindow);
    }

    // Nettoyage
    glDeleteVertexArrays(1,&sphere.vao);
    glDeleteBuffers(1,&sphere.vbo);
    glDeleteBuffers(1,&sphere.ebo);
    glDeleteVertexArrays(1,&dyn2.vao); glDeleteBuffers(1,&dyn2.vbo);
    glDeleteVertexArrays(1,&dyn3.vao); glDeleteBuffers(1,&dyn3.vbo);
    glDeleteProgram(shader3D);
    glDeleteProgram(shaderFlat3D);
    glDeleteProgram(shader2D);
    glfwDestroyCursor(gCurResize);
    glfwDestroyCursor(gCurArrow);
    glfwTerminate();
    return 0;
}
