cmake_minimum_required(VERSION 3.16)
project(OrbitalSim VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ─── Options ───────────────────────────────────────────────────────────────────
option(ORBITAL_SIM_DEBUG "Enable debug output" ON)

# ─── Dependencies ──────────────────────────────────────────────────────────────
find_package(OpenGL REQUIRED)
find_package(glfw3 3.3 REQUIRED)

# GLM (header-only) – fallback to FetchContent if not installed
find_package(glm QUIET)
if(NOT glm_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG        0.9.9.8
    )
    FetchContent_MakeAvailable(glm)
endif()

# GLAD (bundled in vendor/)
add_library(glad STATIC vendor/glad/src/glad.c)
target_include_directories(glad PUBLIC vendor/glad/include)

# ─── Sources ───────────────────────────────────────────────────────────────────
set(SIM_SOURCES
    src/simulation/Physics.cpp
    src/simulation/Spacecraft.cpp
    src/simulation/Atmosphere.cpp
    src/simulation/OrbitalElements.cpp
)

set(RENDER_SOURCES
    src/rendering/Renderer.cpp
    src/rendering/Shader.cpp
    src/rendering/Camera.cpp
    src/rendering/Mesh.cpp
)

set(CORE_SOURCES
    src/core/Window.cpp
    src/core/InputHandler.cpp
    src/core/Timer.cpp
    src/main.cpp
)

add_executable(OrbitalSim
    ${SIM_SOURCES}
    ${RENDER_SOURCES}
    ${CORE_SOURCES}
)

# ─── Includes ──────────────────────────────────────────────────────────────────
target_include_directories(OrbitalSim PRIVATE
    include
    vendor/glad/include
)

# ─── Links ─────────────────────────────────────────────────────────────────────
target_link_libraries(OrbitalSim PRIVATE
    glad
    glfw
    OpenGL::GL
    glm::glm
)

# ─── Definitions ───────────────────────────────────────────────────────────────
if(ORBITAL_SIM_DEBUG)
    target_compile_definitions(OrbitalSim PRIVATE ORBITAL_DEBUG)
endif()

# ─── Warnings ──────────────────────────────────────────────────────────────────
if(MSVC)
    target_compile_options(OrbitalSim PRIVATE /W4)
else()
    target_compile_options(OrbitalSim PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ─── Copy shaders to build dir ─────────────────────────────────────────────────
add_custom_command(TARGET OrbitalSim POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/shaders
        $<TARGET_FILE_DIR:OrbitalSim>/shaders
    COMMENT "Copying shaders to build directory"
)

message(STATUS "OrbitalSim configured — OpenGL orbital & reentry simulation")
