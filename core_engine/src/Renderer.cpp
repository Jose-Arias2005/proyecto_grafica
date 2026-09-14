/**
 * @file Renderer.cpp
 * @brief Pipeline OpenGL de tres pasadas para AR con casco 3D.
 *
 * Pasada 1 — Background  : Frame de cámara como textura en quad completo.
 * Pasada 2 — Occluder    : Malla facial → Z-buffer solo (sin color).
 * Pasada 3 — Helmet      : Malla 3D con uniforms de iluminación.
 *
 * El contexto GL es offscreen (GLFW_VISIBLE = FALSE), el render target es
 * un FBO.  Los píxeles se recuperan con glReadPixels para Python.
 */
#include "Renderer.hpp"

#include <stdexcept>
#include <iostream>
#include <string>

#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

#include "Shader.hpp"
#include "Model.hpp"

namespace arhelmet {

// ── Constructor ───────────────────────────────────────────────────────────────

Renderer::Renderer(int width, int height)
    : m_width(width), m_height(height)
{
    initGLFW();
    initGLAD();
    initFBO();
    initCameraTexture();
}

Renderer::~Renderer() {
    // Liberar recursos GL en orden inverso
    if (m_quadVAO)    glDeleteVertexArrays(1, &m_quadVAO);  // Fix: VAO persistente
    if (m_fbo)        glDeleteFramebuffers(1, &m_fbo);
    if (m_colorTex)   glDeleteTextures(1, &m_colorTex);
    if (m_depthRbo)   glDeleteRenderbuffers(1, &m_depthRbo);
    if (m_cameraTex)  glDeleteTextures(1, &m_cameraTex);

    // Liberar shaders y modelos (unique_ptr los destruye automáticamente)
    m_backgroundShader.reset();
    m_occluderShader.reset();
    m_lightingShader.reset();
    m_helmetModel.reset();
    m_occluderModel.reset();

    if (m_window) {
        glfwDestroyWindow(m_window);
        glfwTerminate();
    }
}

// ── Inicialización ────────────────────────────────────────────────────────────

void Renderer::initGLFW() {
    if (!glfwInit()) {
        throw std::runtime_error("[Renderer] Error al inicializar GLFW");
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);   // Contexto offscreen

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    m_window = glfwCreateWindow(m_width, m_height,
                                 "AR-Helmet Offscreen", nullptr, nullptr);
    if (!m_window) {
        glfwTerminate();
        throw std::runtime_error("[Renderer] No se pudo crear la ventana GLFW offscreen");
    }
    glfwMakeContextCurrent(m_window);
    std::cout << "[Renderer] Contexto GLFW 3.3 Core creado ("
              << m_width << "×" << m_height << ")\n";
}

void Renderer::initGLAD() {
    if (!gladLoadGL(glfwGetProcAddress)) {
        throw std::runtime_error("[Renderer] Error al cargar funciones OpenGL con glad");
    }
    std::cout << "[Renderer] OpenGL " << glGetString(GL_VERSION) << "\n";
    std::cout << "[Renderer] GPU: "   << glGetString(GL_RENDERER) << "\n";
}

void Renderer::initFBO() {
    // ── Framebuffer Object (render target offscreen) ──────────────────────────
    glGenFramebuffers(1, &m_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

    // Textura de color (GL_RGB8) — se recupera con glReadPixels
    glGenTextures(1, &m_colorTex);
    glBindTexture(GL_TEXTURE_2D, m_colorTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,
                 m_width, m_height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, m_colorTex, 0);

    // Renderbuffer de profundidad+stencil (24+8 bits)
    glGenRenderbuffers(1, &m_depthRbo);
    glBindRenderbuffer(GL_RENDERBUFFER, m_depthRbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, m_width, m_height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                              GL_RENDERBUFFER, m_depthRbo);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        throw std::runtime_error("[Renderer] FBO incompleto");
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    std::cout << "[Renderer] FBO offscreen inicializado\n";
}

void Renderer::initCameraTexture() {
    glGenTextures(1, &m_cameraTex);
    glBindTexture(GL_TEXTURE_2D, m_cameraTex);
    // Inicializar con un pixel negro
    const unsigned char black[] = {0, 0, 0};
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0,
                 GL_RGB, GL_UNSIGNED_BYTE, black);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Fix: VAO permanente para el quad de fondo.
    // background.vert genera sus 6 vértices via gl_VertexID (sin VBO),
    // por lo que sólo necesitamos un VAO vacío inicializado una sola vez.
    glGenVertexArrays(1, &m_quadVAO);
}

// ── init(): cargar shaders y modelos ─────────────────────────────────────────

void Renderer::init(const std::string& shaderDir,
                    const std::string& helmetPath,
                    const std::string& occluderPath)
{
    auto p = [&](const std::string& f) { return shaderDir + "/" + f; };

    m_backgroundShader = std::make_unique<Shader>(
        p("background.vert"), p("background.frag"));
    m_occluderShader   = std::make_unique<Shader>(
        p("occluder.vert"),   p("occluder.frag"));
    m_lightingShader   = std::make_unique<Shader>(
        p("lighting.vert"),   p("lighting.frag"));

    m_helmetModel   = std::make_unique<Model>(helmetPath);
    m_occluderModel = std::make_unique<Model>(occluderPath);

    m_initialized = true;
    std::cout << "[Renderer] Shaders y modelos cargados ✓\n";
}

// ── setProjection() ───────────────────────────────────────────────────────────
//
// Proyección perspectiva OpenGL-compatible desde parámetros pinhole de OpenCV.
// Equivalente a la transformación implícita de cv2.projectPoints.
//
void Renderer::setProjection(float fx, float fy, float cx, float cy,
                              float nearPlane, float farPlane)
{
    const float W = static_cast<float>(m_width);
    const float H = static_cast<float>(m_height);
    const float n = nearPlane, f = farPlane;

    // Derivación directa desde parámetros intrínsecos (matriz de cámara):
    //   [ 2*fx/W    0      1-2*cx/W      0        ]
    //   [ 0       2*fy/H  -(1-2*cy/H)   0        ]
    //   [ 0         0    -(f+n)/(f-n)  -2fn/(f-n) ]
    //   [ 0         0       -1           0        ]
    m_projection = glm::mat4(0.0f);
    m_projection[0][0] =  2.0f * fx / W;
    m_projection[1][1] =  2.0f * fy / H;
    m_projection[2][0] =  1.0f - 2.0f * cx / W;
    m_projection[2][1] = -(1.0f - 2.0f * cy / H);
    m_projection[2][2] = -(f + n) / (f - n);
    m_projection[2][3] = -1.0f;
    m_projection[3][2] = -2.0f * f * n / (f - n);
}

// ── uploadCameraFrame() ───────────────────────────────────────────────────────

void Renderer::uploadCameraFrame(const uint8_t* data, int width, int height) {
    glBindTexture(GL_TEXTURE_2D, m_cameraTex);

    if (width != m_width || height != m_height) {
        // Redimensionar textura si el frame cambió de tamaño
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,
                     width, height, 0, GL_BGR, GL_UNSIGNED_BYTE, data);
    } else {
        // Actualización rápida (sin reallocación)
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
                        width, height, GL_BGR, GL_UNSIGNED_BYTE, data);
    }
}

// ── setLighting() ─────────────────────────────────────────────────────────────

void Renderer::setLighting(const LightEstimate& est) {
    m_lightDir     = est.lightDir;
    m_lightColor   = est.lightColor;
    m_ambientColor = est.ambientColor;
}

// ── render() — 3 pasadas ──────────────────────────────────────────────────────

void Renderer::render(const glm::mat4& modelView) {
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glViewport(0, 0, m_width, m_height);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    passBackground();
    passOccluder(modelView);
    passHelmet(modelView);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glfwPollEvents();  // Mantener el contexto GLFW activo
}

// ── Pasada 1: Background ──────────────────────────────────────────────────────

void Renderer::passBackground() {
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    m_backgroundShader->use();
    m_backgroundShader->setInt("cameraFrame", 0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_cameraTex);

    // Fix: usar el VAO persistente m_quadVAO (creado una sola vez en
    // initCameraTexture). Evita el overhead de Gen/Delete 60 veces/seg.
    // background.vert genera posiciones via gl_VertexID: no necesita VBO.
    glBindVertexArray(m_quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

// ── Pasada 2: Occluder → solo Z-buffer ───────────────────────────────────────

void Renderer::passOccluder(const glm::mat4& mv) {
    if (!m_occluderModel) return;

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
    // Deshabilitar escritura de color — solo queremos depth
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

    m_occluderShader->use();
    m_occluderShader->setMat4("model",      mv);
    m_occluderShader->setMat4("projection", m_projection);

    m_occluderModel->draw(*m_occluderShader);

    // Restaurar escritura de color
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
}

// ── Pasada 3: Casco 3D con iluminación ───────────────────────────────────────

void Renderer::passHelmet(const glm::mat4& mv) {
    if (!m_helmetModel) return;

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);

    m_lightingShader->use();
    m_lightingShader->setMat4("model",       mv);
    m_lightingShader->setMat4("projection",  m_projection);

    // Default: sin textura difusa. Mesh::draw() lo sobreescribe a 1
    // si el material contiene una textura cargada.
    m_lightingShader->setInt("has_diffuse_texture", 0);

    // Uniforms de iluminación (provistos por LightEstimator)
    m_lightingShader->setVec3("lightDir",     m_lightDir);
    m_lightingShader->setVec3("lightColor",   m_lightColor);
    m_lightingShader->setVec3("ambientColor", m_ambientColor);

    m_helmetModel->draw(*m_lightingShader);
}

// ── getRenderedFrame() ────────────────────────────────────────────────────────

std::vector<uint8_t> Renderer::getRenderedFrame() const {
    std::vector<uint8_t> pixels(static_cast<std::size_t>(m_width * m_height * 3));

    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    // glReadPixels lee en formato RGB por defecto
    glReadPixels(0, 0, m_width, m_height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Convertir RGB→BGR y voltear verticalmente (OpenGL tiene Y=0 abajo)
    const int stride = m_width * 3;
    std::vector<uint8_t> output(pixels.size());
    for (int row = 0; row < m_height; ++row) {
        const int srcRow = m_height - 1 - row;  // Voltear Y
        const uint8_t* src = pixels.data() + srcRow * stride;
        uint8_t*       dst = output.data() + row   * stride;
        for (int col = 0; col < m_width; ++col) {
            // RGB → BGR
            dst[col * 3 + 0] = src[col * 3 + 2];  // B ← R
            dst[col * 3 + 1] = src[col * 3 + 1];  // G ← G
            dst[col * 3 + 2] = src[col * 3 + 0];  // R ← B
        }
    }
    return output;
}

} // namespace arhelmet
