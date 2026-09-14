/**
 * @file Renderer.hpp
 * @brief Orquestador del pipeline OpenGL 3.3 de tres pasadas para AR.
 *
 * Pipeline de renderizado:
 *   Pasada 1 — Background : Full-screen quad con textura del frame de cámara.
 *   Pasada 2 — Occluder   : Malla facial → solo Z-buffer (sin color).
 *   Pasada 3 — Helmet     : Malla 3D del casco con uniforms de iluminación.
 *
 * El Renderer inicializa un contexto GLFW oculto (offscreen) y renderiza
 * en un Framebuffer Object (FBO).  Los píxeles se recuperan mediante
 * glReadPixels() como buffer uint8 BGR para composición en Python/PyQt.
 *
 * RESTRICCIÓN DE HILO:
 *   Todos los métodos deben invocarse desde el mismo hilo que creó el
 *   contexto OpenGL (típicamente el hilo principal de Python).
 *
 * Uso típico desde pybind_wrapper.cpp:
 *   auto r = Renderer(1280, 720);
 *   r.init("assets/shaders", "assets/models/helmet.obj",
 *                             "assets/models/face_occluder.obj");
 *   r.setProjection(fx, fy, cx, cy);
 *   // Por frame:
 *   r.uploadCameraFrame(bgr_ptr, w, h);
 *   r.setLighting(light_estimate);
 *   r.render(model_view_mat);
 *   auto pixels = r.getRenderedFrame(); // vector<uint8_t> BGR
 */
#pragma once

#include <memory>
#include <string>
#include <vector>
#include <cstdint>

#include <glm/glm.hpp>

#include "LightEstimator.hpp"   // Para LightEstimate

// Forward declarations (evitar incluir GLFW/glad en este header)
struct GLFWwindow;

namespace arhelmet {

class Shader;
class Model;

class Renderer {
public:
    /**
     * @brief Inicializa GLFW y crea un contexto OpenGL 3.3 Core offscreen.
     * @param width   Ancho del framebuffer de salida (px).
     * @param height  Alto del framebuffer de salida (px).
     * @throws std::runtime_error si GLFW o GLAD fallan.
     */
    Renderer(int width, int height);
    ~Renderer();

    // Non-copyable, non-moveable (posee handles GL)
    Renderer(const Renderer&)            = delete;
    Renderer& operator=(const Renderer&) = delete;

    /**
     * @brief Carga shaders y modelos desde disco.
     *
     * Debe llamarse después del constructor, una sola vez.
     * @param shaderDir    Ruta al directorio que contiene los .vert/.frag.
     * @param helmetPath   Ruta al OBJ del casco (assets/models/helmet.obj).
     * @param occluderPath Ruta al OBJ del occluder facial.
     */
    void init(const std::string& shaderDir,
              const std::string& helmetPath,
              const std::string& occluderPath);

    /**
     * @brief Configura la proyección perspectiva desde intrínsecos de cámara.
     *
     * Construye una matriz de proyección compatible con OpenGL desde los
     * parámetros de cámara del modelo pinhole (igual que cv2.projectPoints).
     *
     * @param fx,fy     Longitudes focales en píxeles.
     * @param cx,cy     Punto principal en píxeles.
     * @param nearPlane Plano cercano en mm (default 10 mm).
     * @param farPlane  Plano lejano en mm (default 10 m).
     */
    void setProjection(float fx, float fy, float cx, float cy,
                       float nearPlane = 10.0f, float farPlane = 10000.0f);

    /**
     * @brief Sube un frame BGR uint8 de la cámara como textura de fondo.
     *
     * El buffer debe ser contiguo: H × W × 3, uint8, row-major (orden BGR).
     * Convierte BGR→RGB internamente con swizzle de textura GL.
     *
     * @param data    Puntero al buffer de pixels.
     * @param width   Ancho del frame en píxeles.
     * @param height  Alto del frame en píxeles.
     */
    void uploadCameraFrame(const uint8_t* data, int width, int height);

    /**
     * @brief Establece los parámetros de iluminación del frame actual.
     *
     * Los valores se pasan como uniforms en la Pasada 3 (helmet).
     * Llamar antes de render() en cada frame.
     *
     * @param estimate  Resultado del LightEstimator para este frame.
     */
    void setLighting(const LightEstimate& estimate);

    /**
     * @brief Ejecuta las 3 pasadas de renderizado.
     *
     * Renderiza al FBO interno.  Llamar a getRenderedFrame() después para
     * recuperar los píxeles.
     *
     * @param modelView  Matriz Model-View 4×4 column-major (de PoseEstimator).
     */
    void render(const glm::mat4& modelView);

    /**
     * @brief Lee los píxeles del FBO y los retorna como buffer BGR uint8.
     *
     * Nota de rendimiento: glReadPixels() sincroniza CPU y GPU.  Para >60 FPS
     * considerar PBOs (Pixel Buffer Objects) como optimización futura.
     *
     * @returns Vector de tamaño width × height × 3 (BGR, row-major desde abajo).
     */
    std::vector<uint8_t> getRenderedFrame() const;

    // ── Accessors ─────────────────────────────────────────────────────────────
    int  width()     const noexcept { return m_width;  }
    int  height()    const noexcept { return m_height; }
    bool isReady()   const noexcept { return m_initialized; }

private:
    // ── Contexto GL ──────────────────────────────────────────────────────────
    GLFWwindow* m_window      = nullptr;
    int         m_width       = 0;
    int         m_height      = 0;
    bool        m_initialized = false;

    // ── FBO (offscreen render target) ─────────────────────────────────────────
    unsigned int m_fbo      = 0;
    unsigned int m_colorTex = 0;   ///< Textura de color del FBO (GL_RGB8)
    unsigned int m_depthRbo = 0;   ///< Renderbuffer de profundidad (GL_DEPTH24_STENCIL8)

    // ── Textura de cámara (background) ───────────────────────────────────────
    unsigned int m_cameraTex = 0;

    // ── Shaders ──────────────────────────────────────────────────────────────
    std::unique_ptr<Shader> m_backgroundShader;
    std::unique_ptr<Shader> m_occluderShader;
    std::unique_ptr<Shader> m_lightingShader;

    // ── Modelos ───────────────────────────────────────────────────────────────
    std::unique_ptr<Model> m_helmetModel;
    std::unique_ptr<Model> m_occluderModel;

    // ── Estado de transformación ──────────────────────────────────────────────
    glm::mat4 m_projection{1.0f};

    // ── Estado de iluminación (actualizado por setLighting) ───────────────────
    glm::vec3 m_lightDir    {0.0f, -1.0f, -1.0f};
    glm::vec3 m_lightColor  {1.0f,  1.0f,  1.0f};
    glm::vec3 m_ambientColor{0.15f, 0.15f, 0.20f};

    // ── Helpers de inicialización ─────────────────────────────────────────────
    void initGLFW();
    void initGLAD();
    void initFBO();
    void initCameraTexture();

    // ── Pasadas de render ─────────────────────────────────────────────────────
    void passBackground();
    void passOccluder(const glm::mat4& mv);
    void passHelmet  (const glm::mat4& mv);
};

} // namespace arhelmet
