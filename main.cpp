#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>

// ─── Dimensions fenêtre ──────────────────────────────────────────────────────
static int WIN_W = 1400;
static int WIN_H =  750;

// ─── Caméra 3D (arcball) ─────────────────────────────────────────────────────
static float camYaw    =  30.0f;
static float camPitch  =  20.0f;
static float camRadius =   3.0f;

// ─── Planisphère ─────────────────────────────────────────────────────────────
static float mapCtrLon =   0.0f;   // longitude du centre (degrés)
static float mapCtrLat =   0.0f;   // latitude  du centre (degrés)
static float mapZoom   =   1.0f;   // facteur de zoom (1 = monde entier en hauteur)

// ─── Souris ──────────────────────────────────────────────────────────────────
static bool  lmbDown      = false;
static float lastMouseX   = 0.0f;
static float lastMouseY   = 0.0f;
static bool  dragIn3D     = false;  // true = drag commencé dans le panneau 3D

static bool in3DPanel(double x) { return x < WIN_W * 0.5; }

// ─── Callbacks ───────────────────────────────────────────────────────────────
static void framebuffer_size_callback(GLFWwindow*, int w, int h)
{
    WIN_W = w;
    WIN_H = h;
}

static void mouse_button_callback(GLFWwindow* window, int button, int action, int)
{
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        lmbDown = (action == GLFW_PRESS);
        if (lmbDown) {
            double cx, cy;
            glfwGetCursorPos(window, &cx, &cy);
            dragIn3D = in3DPanel(cx);
        }
    }
}

static void cursor_pos_callback(GLFWwindow*, double xpos, double ypos)
{
    float dx =  (float)xpos - lastMouseX;
    float dy =  (float)ypos - lastMouseY;

    if (lmbDown) {
        if (dragIn3D) {
            // Rotation arcball
            camYaw   += dx * 0.4f;
            camPitch  = glm::clamp(camPitch + dy * 0.4f, -89.0f, 89.0f);
        } else {
            // Déplacement planisphère
            int panelH = WIN_H;
            float pixPerDeg = (panelH / 180.0f) * mapZoom;
            mapCtrLon -= dx / pixPerDeg;
            mapCtrLat += dy / pixPerDeg;
            mapCtrLon  = glm::clamp(mapCtrLon, -180.0f, 180.0f);
            mapCtrLat  = glm::clamp(mapCtrLat,  -90.0f,  90.0f);
        }
    }

    lastMouseX = (float)xpos;
    lastMouseY = (float)ypos;
}

static void scroll_callback(GLFWwindow* window, double, double yoffset)
{
    double cx, cy;
    glfwGetCursorPos(window, &cx, &cy);

    if (in3DPanel(cx)) {
        camRadius = glm::clamp(camRadius - (float)yoffset * 0.25f, 1.2f, 10.0f);
    } else {
        float factor = (yoffset > 0) ? 1.15f : (1.0f / 1.15f);
        mapZoom = glm::clamp(mapZoom * factor, 0.4f, 25.0f);
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
        char log[512];
        glGetShaderInfoLog(id, 512, nullptr, log);
        std::cerr << "Shader error: " << log << "\n";
    }
    return id;
}

static GLuint create_program(const char* vert, const char* frag)
{
    GLuint vs  = compile_shader(GL_VERTEX_SHADER,   vert);
    GLuint fs  = compile_shader(GL_FRAGMENT_SHADER, frag);
    GLuint pgm = glCreateProgram();
    glAttachShader(pgm, vs);
    glAttachShader(pgm, fs);
    glLinkProgram(pgm);
    glDeleteShader(vs);
    glDeleteShader(fs);
    return pgm;
}

// ─── Shader 3D (Phong simplifié) ─────────────────────────────────────────────
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
uniform vec3 uLightDir;
uniform vec3 uColor;
out vec4 FragColor;
void main()
{
    vec3 norm    = normalize(vNormal);
    float diff   = max(dot(norm, normalize(uLightDir)), 0.0);
    vec3 ambient = 0.20 * uColor;
    vec3 diffuse = diff  * uColor;
    // Specular simple
    vec3 viewDir = normalize(-vFragPos);
    vec3 reflDir = reflect(-normalize(uLightDir), norm);
    float spec   = pow(max(dot(viewDir, reflDir), 0.0), 32.0);
    vec3 specular = 0.15 * vec3(1.0) * spec;
    FragColor = vec4(ambient + diffuse + specular, 1.0);
}
)glsl";

// ─── Shader 2D (planisphère, UI) ─────────────────────────────────────────────
static const char* VERT2D = R"glsl(
#version 330 core
layout(location = 0) in vec2 aPos;
uniform mat4 uProj;
void main()
{
    gl_Position = uProj * vec4(aPos, 0.0, 1.0);
}
)glsl";

static const char* FRAG2D = R"glsl(
#version 330 core
uniform vec4 uColor;
out vec4 FragColor;
void main()
{
    FragColor = uColor;
}
)glsl";

// ─── Géométrie sphère (pos + normale) ────────────────────────────────────────
struct SphereGPU {
    GLuint vao, vbo, ebo;
    GLsizei count;
};

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
            verts.push_back(r * x); verts.push_back(r * y); verts.push_back(r * z);  // pos
            verts.push_back(x);     verts.push_back(y);     verts.push_back(z);       // normal
        }
    }

    for (int i = 0; i < stacks; ++i) {
        for (int j = 0; j < slices; ++j) {
            unsigned a = i * (slices + 1) + j;
            unsigned b = a + (slices + 1);
            idx.push_back(a);   idx.push_back(b);   idx.push_back(a + 1);
            idx.push_back(a+1); idx.push_back(b);   idx.push_back(b + 1);
        }
    }

    SphereGPU g;
    g.count = (GLsizei)idx.size();
    glGenVertexArrays(1, &g.vao);
    glGenBuffers(1, &g.vbo);
    glGenBuffers(1, &g.ebo);
    glBindVertexArray(g.vao);
    glBindBuffer(GL_ARRAY_BUFFER, g.vbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * sizeof(unsigned), idx.data(), GL_STATIC_DRAW);
    // attrib 0 : position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // attrib 1 : normale
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
    return g;
}

// ─── Buffer 2D dynamique ─────────────────────────────────────────────────────
struct DynBuf2D {
    GLuint vao, vbo;
};

static DynBuf2D make_dyn_buf2d()
{
    DynBuf2D d;
    glGenVertexArrays(1, &d.vao);
    glGenBuffers(1, &d.vbo);
    glBindVertexArray(d.vao);
    glBindBuffer(GL_ARRAY_BUFFER, d.vbo);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
    return d;
}

static void draw_2d(DynBuf2D& d, const std::vector<float>& pts,
                    GLenum mode, GLint locColor, glm::vec4 col)
{
    if (pts.empty()) return;
    glBindBuffer(GL_ARRAY_BUFFER, d.vbo);
    glBufferData(GL_ARRAY_BUFFER, pts.size() * sizeof(float), pts.data(), GL_DYNAMIC_DRAW);
    glUniform4fv(locColor, 1, glm::value_ptr(col));
    glBindVertexArray(d.vao);
    glDrawArrays(mode, 0, (GLsizei)(pts.size() / 2));
    glBindVertexArray(0);
}

// ─── Projection lon/lat → pixels écran ───────────────────────────────────────
// Le panneau droit occupe [halfW, WIN_W] x [0, WIN_H] en pixels écran (y=0 en haut)
static glm::vec2 lonlat_to_px(float lon, float lat, int halfW)
{
    int panelW = WIN_W - halfW;
    int panelH = WIN_H;

    float pixPerDeg = (panelH / 180.0f) * mapZoom;

    float sx = panelW * 0.5f + (lon - mapCtrLon) * pixPerDeg;
    float sy = panelH * 0.5f - (lat - mapCtrLat) * pixPerDeg;   // y vers le bas

    return { halfW + sx, sy };
}

// ─── Construction du graticule ────────────────────────────────────────────────
static std::vector<float> build_graticule(int halfW, int lonStep, int latStep)
{
    std::vector<float> pts;
    constexpr int SEG = 8;   // segments par ligne (pour la découpe si reprojection future)

    // Méridiens
    for (int lon = -180; lon <= 180; lon += lonStep) {
        for (int s = 0; s < SEG; ++s) {
            float lat0 = -90.0f + s       * (180.0f / SEG);
            float lat1 = -90.0f + (s + 1) * (180.0f / SEG);
            auto p0 = lonlat_to_px((float)lon, lat0, halfW);
            auto p1 = lonlat_to_px((float)lon, lat1, halfW);
            pts.push_back(p0.x); pts.push_back(p0.y);
            pts.push_back(p1.x); pts.push_back(p1.y);
        }
    }

    // Parallèles
    for (int lat = -90; lat <= 90; lat += latStep) {
        for (int s = 0; s < SEG; ++s) {
            float lon0 = -180.0f + s       * (360.0f / SEG);
            float lon1 = -180.0f + (s + 1) * (360.0f / SEG);
            auto p0 = lonlat_to_px(lon0, (float)lat, halfW);
            auto p1 = lonlat_to_px(lon1, (float)lat, halfW);
            pts.push_back(p0.x); pts.push_back(p0.y);
            pts.push_back(p1.x); pts.push_back(p1.y);
        }
    }

    return pts;
}

// ─── Ligne unique lon/lat → pixels ───────────────────────────────────────────
static std::vector<float> lonlat_line(float lon0, float lat0,
                                       float lon1, float lat1, int halfW)
{
    auto p0 = lonlat_to_px(lon0, lat0, halfW);
    auto p1 = lonlat_to_px(lon1, lat1, halfW);
    return { p0.x, p0.y, p1.x, p1.y };
}

// ─── Rectangle plein (2 triangles) ───────────────────────────────────────────
static std::vector<float> make_rect(float x0, float y0, float x1, float y1)
{
    return { x0, y0,  x1, y0,  x1, y1,
             x1, y1,  x0, y1,  x0, y0 };
}

// ─── Main ────────────────────────────────────────────────────────────────────
int main()
{
    if (!glfwInit()) { std::cerr << "glfwInit failed\n"; return -1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);

    GLFWwindow* window = glfwCreateWindow(WIN_W, WIN_H, "OrbitalSim", nullptr, nullptr);
    if (!window) { std::cerr << "glfwCreateWindow failed\n"; glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);  // VSync

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "GLAD failed\n"; return -1;
    }

    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetMouseButtonCallback   (window, mouse_button_callback);
    glfwSetCursorPosCallback     (window, cursor_pos_callback);
    glfwSetScrollCallback        (window, scroll_callback);
    glfwSetKeyCallback           (window, key_callback);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    glEnable(GL_SCISSOR_TEST);

    // ── Shaders ─────────────────────────────────────────────────────────────
    GLuint shader3D = create_program(VERT3D, FRAG3D);
    GLuint shader2D = create_program(VERT2D, FRAG2D);

    GLint loc3D_MVP      = glGetUniformLocation(shader3D, "uMVP");
    GLint loc3D_Model    = glGetUniformLocation(shader3D, "uModel");
    GLint loc3D_LightDir = glGetUniformLocation(shader3D, "uLightDir");
    GLint loc3D_Color    = glGetUniformLocation(shader3D, "uColor");

    GLint loc2D_Proj  = glGetUniformLocation(shader2D, "uProj");
    GLint loc2D_Color = glGetUniformLocation(shader2D, "uColor");

    // ── Géométrie ────────────────────────────────────────────────────────────
    SphereGPU sphere = build_sphere(1.0f, 48, 64);
    DynBuf2D  dyn    = make_dyn_buf2d();

    // ── Boucle de rendu ───────────────────────────────────────────────────────
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        int fbW, fbH;
        glfwGetFramebufferSize(window, &fbW, &fbH);
        int halfW = fbW / 2;

        // Clear global
        glScissor(0, 0, fbW, fbH);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // ════════════════════════════════════════════════════════════════════
        // PANNEAU GAUCHE — vue 3D
        // ════════════════════════════════════════════════════════════════════
        glViewport(0, 0, halfW, fbH);
        glScissor (0, 0, halfW, fbH);
        glEnable(GL_DEPTH_TEST);

        glClearColor(0.05f, 0.06f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        {
            float yRad = glm::radians(camYaw);
            float pRad = glm::radians(camPitch);
            glm::vec3 camPos = {
                camRadius * std::cos(pRad) * std::sin(yRad),
                camRadius * std::sin(pRad),
                camRadius * std::cos(pRad) * std::cos(yRad)
            };

            float aspect3D = (float)halfW / (float)fbH;
            glm::mat4 proj3D  = glm::perspective(glm::radians(45.0f), aspect3D, 0.1f, 100.0f);
            glm::mat4 view3D  = glm::lookAt(camPos, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            glm::mat4 model3D = glm::mat4(1.0f);
            glm::mat4 mvp3D   = proj3D * view3D * model3D;

            glUseProgram(shader3D);
            glUniformMatrix4fv(loc3D_MVP,   1, GL_FALSE, glm::value_ptr(mvp3D));
            glUniformMatrix4fv(loc3D_Model, 1, GL_FALSE, glm::value_ptr(model3D));
            glUniform3f(loc3D_LightDir, 3.0f, 4.0f, 5.0f);
            glUniform3f(loc3D_Color,    0.15f, 0.42f, 0.82f);  // bleu Terre

            glBindVertexArray(sphere.vao);
            glDrawElements(GL_TRIANGLES, sphere.count, GL_UNSIGNED_INT, 0);
            glBindVertexArray(0);
        }

        // ════════════════════════════════════════════════════════════════════
        // PANNEAU DROIT — planisphère
        // ════════════════════════════════════════════════════════════════════
        glViewport(halfW, 0, fbW - halfW, fbH);
        glScissor (halfW, 0, fbW - halfW, fbH);
        glDisable(GL_DEPTH_TEST);

        glClearColor(0.04f, 0.07f, 0.14f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Projection orthographique : espace pixel écran entier (x : 0→fbW, y : 0→fbH)
        glm::mat4 proj2D = glm::ortho(0.0f, (float)fbW, (float)fbH, 0.0f, -1.0f, 1.0f);
        glUseProgram(shader2D);
        glUniformMatrix4fv(loc2D_Proj, 1, GL_FALSE, glm::value_ptr(proj2D));

        // — Fond océan -------------------------------------------------------
        {
            auto bg = make_rect((float)halfW, 0.0f, (float)fbW, (float)fbH);
            draw_2d(dyn, bg, GL_TRIANGLES, loc2D_Color, { 0.04f, 0.07f, 0.14f, 1.0f });
        }

        // — Graticule (30°) --------------------------------------------------
        {
            auto grid = build_graticule(halfW, 30, 30);
            draw_2d(dyn, grid, GL_LINES, loc2D_Color, { 0.12f, 0.25f, 0.42f, 1.0f });
        }

        // — Graticule fin (10°) ----------------------------------------------
        {
            auto grid = build_graticule(halfW, 10, 10);
            draw_2d(dyn, grid, GL_LINES, loc2D_Color, { 0.08f, 0.17f, 0.29f, 1.0f });
        }

        // — Équateur ---------------------------------------------------------
        {
            auto eq = lonlat_line(-180.0f, 0.0f, 180.0f, 0.0f, halfW);
            draw_2d(dyn, eq, GL_LINES, loc2D_Color, { 0.30f, 0.55f, 0.80f, 1.0f });
        }

        // — Méridien principal -----------------------------------------------
        {
            auto pm = lonlat_line(0.0f, -90.0f, 0.0f, 90.0f, halfW);
            draw_2d(dyn, pm, GL_LINES, loc2D_Color, { 0.30f, 0.55f, 0.80f, 1.0f });
        }

        // — Tropiques (23.5°) ------------------------------------------------
        {
            auto tc = lonlat_line(-180.0f,  23.5f, 180.0f,  23.5f, halfW);
            auto cc = lonlat_line(-180.0f, -23.5f, 180.0f, -23.5f, halfW);
            draw_2d(dyn, tc, GL_LINES, loc2D_Color, { 0.22f, 0.40f, 0.60f, 1.0f });
            draw_2d(dyn, cc, GL_LINES, loc2D_Color, { 0.22f, 0.40f, 0.60f, 1.0f });
        }

        // — Cercles polaires (66.5°) -----------------------------------------
        {
            auto na = lonlat_line(-180.0f,  66.5f, 180.0f,  66.5f, halfW);
            auto sa = lonlat_line(-180.0f, -66.5f, 180.0f, -66.5f, halfW);
            draw_2d(dyn, na, GL_LINES, loc2D_Color, { 0.22f, 0.40f, 0.60f, 1.0f });
            draw_2d(dyn, sa, GL_LINES, loc2D_Color, { 0.22f, 0.40f, 0.60f, 1.0f });
        }

        // ════════════════════════════════════════════════════════════════════
        // SÉPARATEUR
        // ════════════════════════════════════════════════════════════════════
        glViewport(0, 0, fbW, fbH);
        glScissor (0, 0, fbW, fbH);
        glUseProgram(shader2D);
        glUniformMatrix4fv(loc2D_Proj, 1, GL_FALSE, glm::value_ptr(proj2D));

        {
            float sx = (float)halfW;
            std::vector<float> sep = { sx - 1.0f, 0.0f,  sx + 1.0f, 0.0f,
                                       sx + 1.0f, (float)fbH,
                                       sx + 1.0f, (float)fbH, sx - 1.0f, (float)fbH,
                                       sx - 1.0f, 0.0f };
            draw_2d(dyn, sep, GL_TRIANGLES, loc2D_Color, { 0.55f, 0.75f, 1.0f, 1.0f });
        }

        glfwSwapBuffers(window);
    }

    // Nettoyage
    glDeleteVertexArrays(1, &sphere.vao);
    glDeleteBuffers(1, &sphere.vbo);
    glDeleteBuffers(1, &sphere.ebo);
    glDeleteVertexArrays(1, &dyn.vao);
    glDeleteBuffers(1, &dyn.vbo);
    glDeleteProgram(shader3D);
    glDeleteProgram(shader2D);
    glfwTerminate();
    return 0;
}
