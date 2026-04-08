#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <vector>
#include <cmath>

//Dimensions de la fenêtre
const int SCR_WIDTH = 900;
const int SCR_HEIGHT = 700;

//État caméra (arcball simple)
float camYaw = 30.0f;   // degrés
float camPitch = 20.0f;   // degrés
float camRadius = 3.0f;    // distance au centre

bool  mouseDown = false;
float lastX = 0.0f, lastY = 0.0f;

//Callbacks
void framebuffer_size_callback(GLFWwindow*, int w, int h)
{
    glViewport(0, 0, w, h);
}

void mouse_button_callback(GLFWwindow*, int button, int action, int)
{
    if (button == GLFW_MOUSE_BUTTON_LEFT)
        mouseDown = (action == GLFW_PRESS);
}

void cursor_pos_callback(GLFWwindow*, double xpos, double ypos)
{
    static bool first = true;
    if (first) { lastX = (float)xpos; lastY = (float)ypos; first = false; }

    if (mouseDown) {
        float dx = (float)xpos - lastX;
        float dy = (float)ypos - lastY;
        camYaw += dx * 0.4f;
        camPitch = glm::clamp(camPitch + dy * 0.4f, -89.0f, 89.0f);
    }
    lastX = (float)xpos;
    lastY = (float)ypos;
}

void scroll_callback(GLFWwindow*, double, double yoffset)
{
    camRadius = glm::clamp(camRadius - (float)yoffset * 0.2f, 1.2f, 10.0f);
}

void key_callback(GLFWwindow* window, int key, int, int action, int)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}

//Shaders GLSL
const char* VERT_SRC = R"(
#version 330 core
layout(location = 0) in vec3 aPos;

uniform mat4 uMVP;

void main()
{
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)";

const char* FRAG_SRC = R"(
#version 330 core
out vec4 FragColor;

uniform vec3 uColor;

void main()
{
    FragColor = vec4(uColor, 1.0);
}
)";

//Compilation shader
GLuint compile_shader(GLenum type, const char* src)
{
    GLuint id = glCreateShader(type);
    glShaderSource(id, 1, &src, nullptr);
    glCompileShader(id);

    GLint ok;
    glGetShaderiv(id, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(id, 512, nullptr, log);
        std::cerr << "Erreur shader : " << log << "\n";
    }
    return id;
}

GLuint create_program(const char* vert, const char* frag)
{
    GLuint vs = compile_shader(GL_VERTEX_SHADER, vert);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, frag);
    GLuint pgm = glCreateProgram();
    glAttachShader(pgm, vs);
    glAttachShader(pgm, fs);
    glLinkProgram(pgm);
    glDeleteShader(vs);
    glDeleteShader(fs);
    return pgm;
}

//Génération de la sphère
//
//Renvoie les indices pour GL_LINES afin d'avoir un vrai wireframe :
//chaque arête (horizontal + vertical) est explicitement listée.
//
struct SphereMesh {
    std::vector<float>        vertices;  // xyz par sommet
    std::vector<unsigned int> indices;   // paires pour GL_LINES
};

SphereMesh make_sphere(float radius, int stacks, int slices)
{
    SphereMesh m;

    //Sommets
    for (int i = 0; i <= stacks; ++i) {
        float phi = glm::pi<float>() * i / stacks;        // [0, π]
        for (int j = 0; j <= slices; ++j) {
            float theta = glm::two_pi<float>() * j / slices;  // [0, 2π]
            float x = radius * std::sin(phi) * std::cos(theta);
            float y = radius * std::cos(phi);
            float z = radius * std::sin(phi) * std::sin(theta);
            m.vertices.push_back(x);
            m.vertices.push_back(y);
            m.vertices.push_back(z);
        }
    }

    //Arêtes horizontales (parallèles)
    for (int i = 0; i <= stacks; ++i) {
        for (int j = 0; j < slices; ++j) {
            unsigned int a = i * (slices + 1) + j;
            unsigned int b = a + 1;
            m.indices.push_back(a);
            m.indices.push_back(b);
        }
    }

    //Arêtes verticales (méridiens)
    for (int i = 0; i < stacks; ++i) {
        for (int j = 0; j <= slices; ++j) {
            unsigned int a = i * (slices + 1) + j;
            unsigned int b = (i + 1) * (slices + 1) + j;
            m.indices.push_back(a);
            m.indices.push_back(b);
        }
    }

    return m;
}

// ── Paramètres de l'orbite de démo ───────────────────────────────────────────
// a = 7500 km, e = 0.08 → périastre ≈ 522 km, apoastre ≈ 1722 km d'altitude
const float R_EARTH_KM  = 6378.137f;
const float ORB_A_GL    = 7500.0f  / R_EARTH_KM;  // demi-grand axe en unités GL
const float ORB_E       = 0.08f;
const float ORB_I       = glm::radians(28.5f);     // inclinaison
const float ORB_RAAN    = glm::radians(30.0f);     // ascension droite du nœud ascendant
const float ORB_OMEGA   = glm::radians(60.0f);     // argument du périastre
const float MARKER_SIZE = 0.05f;                   // taille des triangles en unités GL

// Matrice de rotation PQW → ECI : Rz(−Ω) · Rx(−i) · Rz(−ω)
static glm::mat3 pqw_to_eci_mat(float raan, float inc, float omega)
{
    glm::mat4 R = glm::rotate(glm::mat4(1.0f), -raan,  glm::vec3(0, 0, 1))
                * glm::rotate(glm::mat4(1.0f), -inc,   glm::vec3(1, 0, 0))
                * glm::rotate(glm::mat4(1.0f), -omega, glm::vec3(0, 0, 1));
    return glm::mat3(R);
}

// Génère N×3 floats pour GL_LINE_LOOP représentant l'ellipse orbitale
static std::vector<float> make_orbit_polyline(float a, float e,
                                               float inc, float raan, float omega,
                                               int steps = 360)
{
    glm::mat3 R = pqw_to_eci_mat(raan, inc, omega);
    float p = a * (1.0f - e * e);  // paramètre orbital (semi-latus rectum)
    std::vector<float> verts;
    verts.reserve(steps * 3);
    for (int k = 0; k < steps; ++k) {
        float nu  = glm::two_pi<float>() * k / steps;
        float r   = p / (1.0f + e * std::cos(nu));
        glm::vec3 eci = R * glm::vec3(r * std::cos(nu), r * std::sin(nu), 0.0f);
        verts.push_back(eci.x);
        verts.push_back(eci.y);
        verts.push_back(eci.z);
    }
    return verts;
}

// Génère 9 floats (3 sommets) pour le triangle marqueur d'une apside.
// apex_outward = true  → apoastre  ▲ (sommet vers l'extérieur, loin de la Terre)
// apex_outward = false → périastre ▽ (sommet vers la Terre)
static std::vector<float> make_apsis_triangle(glm::vec3 pos, glm::vec3 normal,
                                               bool apex_outward, float size)
{
    glm::vec3 radial  = glm::normalize(pos);
    glm::vec3 tangent = glm::normalize(glm::cross(normal, radial));

    glm::vec3 apex, v1, v2;
    if (apex_outward) {
        apex          = pos + radial * size;
        glm::vec3 base = pos - radial * (size * 0.5f);
        v1 = base + tangent * (size * 0.866f);
        v2 = base - tangent * (size * 0.866f);
    } else {
        apex          = pos - radial * size;
        glm::vec3 base = pos + radial * (size * 0.5f);
        v1 = base + tangent * (size * 0.866f);
        v2 = base - tangent * (size * 0.866f);
    }
    return { apex.x, apex.y, apex.z,
             v1.x,   v1.y,   v1.z,
             v2.x,   v2.y,   v2.z };
}

//Main
int main()
{
    //Init GLFW
    if (!glfwInit()) {
        std::cerr << "Échec glfwInit\n";
        return -1;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);  // antialiasing

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT,
        "Sphère Filaire", nullptr, nullptr);
    if (!window) {
        std::cerr << "Échec glfwCreateWindow\n";
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    //Init GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Échec GLAD\n";
        return -1;
    }

    //Callbacks
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCursorPosCallback(window, cursor_pos_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetKeyCallback(window, key_callback);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);

    //Shader
    GLuint shader = create_program(VERT_SRC, FRAG_SRC);

    //Géométrie
    SphereMesh sphere = make_sphere(1.0f, 24, 36);  // rayon 1, 24 stacks, 36 slices

    GLuint VAO, VBO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER,
        sphere.vertices.size() * sizeof(float),
        sphere.vertices.data(),
        GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
        sphere.indices.size() * sizeof(unsigned int),
        sphere.indices.data(),
        GL_STATIC_DRAW);

    // Attribut position : location 0, 3 floats
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);

    GLsizei indexCount = (GLsizei)sphere.indices.size();

    // ── Orbite ───────────────────────────────────────────────────────────────
    glm::mat3 R          = pqw_to_eci_mat(ORB_RAAN, ORB_I, ORB_OMEGA);
    glm::vec3 normalECI  = glm::normalize(R * glm::vec3(0.0f, 0.0f, 1.0f));
    glm::vec3 posApo     = R * glm::vec3(-ORB_A_GL * (1.0f + ORB_E), 0.0f, 0.0f);
    glm::vec3 posPeri    = R * glm::vec3( ORB_A_GL * (1.0f - ORB_E), 0.0f, 0.0f);

    // Polyline orbitale
    auto orbitVerts = make_orbit_polyline(ORB_A_GL, ORB_E, ORB_I, ORB_RAAN, ORB_OMEGA);

    GLuint orbitVAO, orbitVBO;
    glGenVertexArrays(1, &orbitVAO);
    glGenBuffers(1, &orbitVBO);
    glBindVertexArray(orbitVAO);
    glBindBuffer(GL_ARRAY_BUFFER, orbitVBO);
    glBufferData(GL_ARRAY_BUFFER, orbitVerts.size() * sizeof(float),
                 orbitVerts.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
    GLsizei orbitVertCount = (GLsizei)(orbitVerts.size() / 3);

    // Triangles marqueurs (apoastre ▲ puis périastre ▽ dans le même VBO)
    auto apoTri  = make_apsis_triangle(posApo,  normalECI, true,  MARKER_SIZE);
    auto periTri = make_apsis_triangle(posPeri, normalECI, false, MARKER_SIZE);
    std::vector<float> markerVerts;
    markerVerts.insert(markerVerts.end(), apoTri.begin(),  apoTri.end());
    markerVerts.insert(markerVerts.end(), periTri.begin(), periTri.end());

    GLuint markerVAO, markerVBO;
    glGenVertexArrays(1, &markerVAO);
    glGenBuffers(1, &markerVBO);
    glBindVertexArray(markerVAO);
    glBindBuffer(GL_ARRAY_BUFFER, markerVBO);
    glBufferData(GL_ARRAY_BUFFER, markerVerts.size() * sizeof(float),
                 markerVerts.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    //Uniforms
    GLint locMVP = glGetUniformLocation(shader, "uMVP");
    GLint locColor = glGetUniformLocation(shader, "uColor");

    //Boucle de rendu
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        glClearColor(0.08f, 0.08f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        //Matrice MVP
        float aspect = (float)SCR_WIDTH / SCR_HEIGHT;

        // Projection perspective
        glm::mat4 proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);

        // Caméra arcball autour de l'origine
        float yRad = glm::radians(camYaw);
        float pRad = glm::radians(camPitch);
        glm::vec3 camPos = {
            camRadius * std::cos(pRad) * std::sin(yRad),
            camRadius * std::sin(pRad),
            camRadius * std::cos(pRad) * std::cos(yRad)
        };
        glm::mat4 view = glm::lookAt(camPos, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

        // Pas de rotation sur le modèle — la sphère reste centrée
        glm::mat4 model = glm::mat4(1.0f);

        glm::mat4 mvp = proj * view * model;

        //Draw
        glUseProgram(shader);
        glUniformMatrix4fv(locMVP, 1, GL_FALSE, glm::value_ptr(mvp));
        glUniform3f(locColor, 0.2f, 0.7f, 1.0f);  // bleu cyan

        glBindVertexArray(VAO);
        glDrawElements(GL_LINES, indexCount, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);

        // Orbite (jaune-orangé)
        glUniform3f(locColor, 1.0f, 0.75f, 0.2f);
        glBindVertexArray(orbitVAO);
        glDrawArrays(GL_LINE_LOOP, 0, orbitVertCount);

        // Apoastre ▲ (vert)
        glUniform3f(locColor, 0.2f, 1.0f, 0.35f);
        glBindVertexArray(markerVAO);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        // Périastre ▽ (rouge)
        glUniform3f(locColor, 1.0f, 0.3f, 0.2f);
        glDrawArrays(GL_TRIANGLES, 3, 3);
        glBindVertexArray(0);

        glfwSwapBuffers(window);
    }

    //Nettoyage
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    glDeleteVertexArrays(1, &orbitVAO);
    glDeleteBuffers(1, &orbitVBO);
    glDeleteVertexArrays(1, &markerVAO);
    glDeleteBuffers(1, &markerVBO);
    glDeleteProgram(shader);
    glfwTerminate();
    return 0;
}