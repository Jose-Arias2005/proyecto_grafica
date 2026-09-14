# =============================================================================
# AR-Helmet Core Engine — CMakeLists.txt
# =============================================================================
# Compila el módulo nativo ar_helmet_core (.pyd en Windows, .so en Linux)
# importable directamente desde Python con:  import ar_helmet_core
#
# Dependencias gestionadas 100% por FetchContent (sin instalación del sistema):
#   - pybind11 v2.13.6   : Puente Python ↔ C++
#   - GLFW 3.4           : Contexto OpenGL + ventana
#   - GLM 1.0.1          : Matemáticas de gráficos (glm::mat4, vec3, etc.)
#   - Assimp 5.4.3       : Carga de modelos OBJ (solo importador OBJ activo)
#   - glad2              : Loader de funciones OpenGL 3.3 Core (cmake-nativo)
#   - stb                : stb_image para carga de texturas PNG/JPG
#
# Plataformas soportadas: Windows (MSVC / MinGW-w64) · Linux (GCC / Clang)
# Python requerido: 3.10 – 3.12 con componentes Interpreter + Development.Module
# =============================================================================

cmake_minimum_required(VERSION 3.20)
project(ar_helmet_core VERSION 0.1.0 LANGUAGES CXX C)

# ─── C++17 ────────────────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Mejor diagnóstico de errores en MSVC
if(MSVC)
    add_compile_options(/diagnostics:column)
endif()

# ─── Python 3.10 – 3.12 ───────────────────────────────────────────────────────
find_package(Python3 3.10...<3.16 REQUIRED COMPONENTS Interpreter Development.Module)
message(STATUS "[AR-Helmet] Python : ${Python3_EXECUTABLE} v${Python3_VERSION}")

# ─── FetchContent ─────────────────────────────────────────────────────────────
include(FetchContent)
set(FETCHCONTENT_QUIET OFF)   # Mostrar progreso de descarga

# ── pybind11 v2.13.6 ──────────────────────────────────────────────────────────
FetchContent_Declare(pybind11
    GIT_REPOSITORY https://github.com/pybind/pybind11.git
    GIT_TAG        v2.13.6
    GIT_SHALLOW    TRUE
)

# ── GLFW 3.4 (sin documentación ni ejemplos) ──────────────────────────────────
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)
FetchContent_Declare(glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
    GIT_SHALLOW    TRUE
)

# ── GLM 1.0.1 ─────────────────────────────────────────────────────────────────
FetchContent_Declare(glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.1
    GIT_SHALLOW    TRUE
)

# ── Assimp 5.4.3 (solo importador OBJ para build rápido) ──────────────────────
set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_OBJ_IMPORTER            ON  CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_MTL_IMPORTER            ON  CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_TESTS                   OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS            OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL                       OFF CACHE BOOL "" FORCE)
set(ASSIMP_INJECT_DEBUG_POSTFIX          OFF CACHE BOOL "" FORCE)
set(ASSIMP_NO_EXPORT                     ON  CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS                    OFF CACHE BOOL "" FORCE)
FetchContent_Declare(assimp
    GIT_REPOSITORY https://github.com/assimp/assimp.git
    GIT_TAG        v5.4.3
    GIT_SHALLOW    TRUE
)

# ── glad v2 (genera loader OpenGL 3.3 Core en tiempo de configuración CMake) ──
# glad2 usa CMake nativo para generar — no requiere el paquete Python 'glad'.
FetchContent_Declare(glad2
    GIT_REPOSITORY https://github.com/Dav1dde/glad.git
    GIT_TAG        glad2
    GIT_SHALLOW    TRUE
    SOURCE_SUBDIR  cmake
)

# ── stb (header-only: stb_image para texturas) ────────────────────────────────
FetchContent_Declare(stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
)

# ── Descarga e inicialización de todas las dependencias ───────────────────────
FetchContent_MakeAvailable(pybind11 glfw glm assimp glad2)

# stb es header-only: solo poblar, sin add_subdirectory
FetchContent_GetProperties(stb)
if(NOT stb_POPULATED)
    FetchContent_Populate(stb)
endif()

# ── Generar loader OpenGL 3.3 Core con glad2 ──────────────────────────────────
glad_add_library(glad_gl_core_33
    REPRODUCIBLE        # Build determinista (mismo hash → mismo output)
    EXCLUDE_FROM_ALL
    LANGUAGE c
    API gl:core=3.3     # OpenGL 3.3 Core Profile
)

# ─── Fuentes del módulo ────────────────────────────────────────────────────────
set(CORE_SOURCES
    src/Shader.cpp
    src/Model.cpp
    src/PoseEstimator.cpp
    src/LightEstimator.cpp
    src/Renderer.cpp
)

set(BINDING_SOURCES
    bindings/pybind_wrapper.cpp
)

# ─── Target: módulo Python nativo ─────────────────────────────────────────────
pybind11_add_module(ar_helmet_core MODULE
    ${CORE_SOURCES}
    ${BINDING_SOURCES}
)

# ── Include directories ───────────────────────────────────────────────────────
target_include_directories(ar_helmet_core PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${stb_SOURCE_DIR}       # Para #include "stb_image.h"
)

# ── Linkeo de librerías ───────────────────────────────────────────────────────
target_link_libraries(ar_helmet_core PRIVATE
    glfw
    glm::glm
    assimp::assimp
    glad_gl_core_33
    ${CMAKE_DL_LIBS}            # -ldl en Linux
)

# ── OpenGL por plataforma ──────────────────────────────────────────────────────
if(WIN32)
    target_link_libraries(ar_helmet_core PRIVATE opengl32)
elseif(APPLE)
    find_library(OPENGL_FW OpenGL REQUIRED)
    target_link_libraries(ar_helmet_core PRIVATE ${OPENGL_FW})
else()   # Linux
    find_package(OpenGL REQUIRED)
    target_link_libraries(ar_helmet_core PRIVATE OpenGL::GL)
endif()

# ─── Propiedades del target ───────────────────────────────────────────────────
# Los módulos Python no llevan prefijo "lib"
set_target_properties(ar_helmet_core PROPERTIES PREFIX "")

# Ubicar el .pyd/.so en la raíz del repositorio para `import ar_helmet_core`
set(_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/..")
set_target_properties(ar_helmet_core PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${_OUTPUT_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${_OUTPUT_DIR}"
)
# Soporte para generadores multi-config (Visual Studio, Xcode)
foreach(_cfg IN ITEMS Debug Release RelWithDebInfo MinSizeRel)
    set_target_properties(ar_helmet_core PROPERTIES
        "LIBRARY_OUTPUT_DIRECTORY_${_cfg}" "${_OUTPUT_DIR}"
        "RUNTIME_OUTPUT_DIRECTORY_${_cfg}" "${_OUTPUT_DIR}"
    )
endforeach()

# ─── Flags del compilador ─────────────────────────────────────────────────────
if(MSVC)
    target_compile_options(ar_helmet_core PRIVATE
        /W4             # Nivel de avisos alto
        /WX-            # No tratar avisos como errores (FetchContent puede generar)
        /O2             # Optimización de velocidad
        /MP             # Compilación multi-proceso
        /utf-8          # Encoding UTF-8 en fuentes
        /wd4251         # Suprimir C4251 (DLL interface) de cabeceras Assimp
        /wd4996         # Suprimir deprecation warnings de STL
    )
    target_compile_definitions(ar_helmet_core PRIVATE
        _CRT_SECURE_NO_WARNINGS
        NOMINMAX                # Evitar conflicto min/max de windows.h con STL
        WIN32_LEAN_AND_MEAN
    )
else()   # GCC / Clang (Linux, MinGW)
    target_compile_options(ar_helmet_core PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -O2
        -ffast-math
    )
endif()

# ─── Resumen de configuración ─────────────────────────────────────────────────
message(STATUS "")
message(STATUS "╔══════════════════════════════════════════════════╗")
message(STATUS "║         AR-Helmet Core Engine configurado        ║")
message(STATUS "╠══════════════════════════════════════════════════╣")
message(STATUS "║  Target  : ar_helmet_core (módulo Python)        ║")
message(STATUS "║  Python  : ${Python3_VERSION}")
message(STATUS "║  Output  : ${_OUTPUT_DIR}/")
message(STATUS "║  Build   : ${CMAKE_BUILD_TYPE}")
message(STATUS "╚══════════════════════════════════════════════════╝")
message(STATUS "")
