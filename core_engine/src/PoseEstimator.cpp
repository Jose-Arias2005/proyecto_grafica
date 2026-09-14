/**
 * @file PoseEstimator.cpp
 * @brief Conversión rvec+tvec → matriz Model-View de OpenGL.
 *
 * No enlaza OpenCV: el solvePnP se realiza en Python (cv2.solvePnP).
 * Este módulo implementa únicamente la conversión de resultado con:
 *   - Fórmula analítica de Rodrigues para rvec → R (3×3)
 *   - Cambio de convenio de coordenadas OpenCV → OpenGL
 *   - Construcción de la matriz column-major 4×4 para glm/GLSL
 */
#include "PoseEstimator.hpp"

#include <cmath>
#include <glm/gtc/matrix_transform.hpp>

namespace arhelmet {

// ── Puntos 3D canónicos del modelo facial (en mm) ─────────────────────────────
//
// Selección de 6 landmarks MediaPipe Face Mesh que coinciden con
// LANDMARK_INDICES = [4, 152, 263, 33, 61, 291] en landmark_detector.py
//
// Sistema de coordenadas: X→derecha, Y→arriba, Z→hacia el observador
// Origen en la punta de la nariz (landmark 4).
//
// Fuente: genérico basado en modelo de cara de dlib/OpenFace adaptado a MediaPipe
static const std::array<std::array<float, 3>, 6> s_canonicalPoints = {{
    //  X        Y        Z      Landmark (MediaPipe idx)
    {  0.000f,   0.000f,   0.000f },  // 4   — Punta de la nariz (origen)
    {  0.000f, -63.600f, -12.500f },  // 152  — Mentón
    { -43.300f,  32.700f, -26.000f },  // 263  — Comisura ojo izquierdo (exterior)
    {  43.300f,  32.700f, -26.000f },  // 33   — Comisura ojo derecho (exterior)
    { -28.900f, -28.900f, -24.100f },  // 61   — Comisura boca izquierda
    {  28.900f, -28.900f, -24.100f },  // 291  — Comisura boca derecha
}};

// ── Constructor ───────────────────────────────────────────────────────────────

PoseEstimator::PoseEstimator(float fx, float fy, float cx, float cy) noexcept
    : m_fx(fx), m_fy(fy), m_cx(cx), m_cy(cy)
{}

void PoseEstimator::setIntrinsics(float fx, float fy, float cx, float cy) noexcept {
    m_fx = fx; m_fy = fy;
    m_cx = cx; m_cy = cy;
}

const std::array<std::array<float, 3>, 6>& PoseEstimator::canonicalPoints() noexcept {
    return s_canonicalPoints;
}

// ── Rodrigues analítico ───────────────────────────────────────────────────────
//
// Dado rvec = [rx, ry, rz]:
//   θ = |rvec|           (ángulo de rotación en radianes)
//   k = rvec / θ         (eje unitario de rotación)
//   R = I·cos(θ) + (1-cos(θ))·k·kᵀ + sin(θ)·K
//
// Donde K es la matriz antisimétrica (cross-product):
//   K = [[ 0,  -kz,  ky ],
//         [ kz,  0, -kx ],
//         [-ky,  kx,  0 ]]
//
glm::mat3 PoseEstimator::rodriguesRotation(const glm::vec3& rv) noexcept {
    const float theta = glm::length(rv);

    // Caso degenerado: ángulo ~0 → identidad
    if (theta < 1e-6f) {
        return glm::mat3(1.0f);
    }

    const glm::vec3 k = rv / theta;   // Eje unitario
    const float c  = std::cos(theta);
    const float s  = std::sin(theta);
    const float t  = 1.0f - c;

    // Matriz antisimétrica K de k
    const float kx = k.x, ky = k.y, kz = k.z;

    // R = I·c + t·k·kᵀ + s·K
    // Construimos column-major (GLM usa column-major)
    glm::mat3 R;
    R[0][0] = c + t*kx*kx;    R[1][0] = t*kx*ky - s*kz;  R[2][0] = t*kx*kz + s*ky;
    R[0][1] = t*kx*ky + s*kz; R[1][1] = c + t*ky*ky;     R[2][1] = t*ky*kz - s*kx;
    R[0][2] = t*kx*kz - s*ky; R[1][2] = t*ky*kz + s*kx;  R[2][2] = c + t*kz*kz;

    return R;
}

// ── buildModelView ────────────────────────────────────────────────────────────
//
// Recibe rvec+tvec de cv2.solvePnP (convenio OpenCV) y construye la
// matriz Model-View de OpenGL (convenio OpenGL).
//
// Conversión OpenCV → OpenGL:
//   OpenCV:  X→derecha, Y→abajo,  Z→hacia la pantalla (z positivo)
//   OpenGL:  X→derecha, Y→arriba, Z→fuera de pantalla (z negativo)
//
//   R_gl = diag(1, -1, -1) · R_cv
//   t_gl = diag(1, -1, -1) · t_cv
//
// La matriz resultante M = [R_gl | t_gl] transforma vértices del espacio
// objeto al espacio de ojo (camera/view space).
//
PoseResult PoseEstimator::buildModelView(
    const float* rvec, const float* tvec) const noexcept
{
    PoseResult result{};
    result.rvec = { rvec[0], rvec[1], rvec[2] };
    result.tvec = { tvec[0], tvec[1], tvec[2] };

    // 1. Rodrigues: rvec → R_cv (3×3)
    const glm::vec3 rv(rvec[0], rvec[1], rvec[2]);
    const glm::mat3 R_cv = rodriguesRotation(rv);

    // 2. Cambio de convenio: Y y Z negados
    //    diag(1, -1, -1) aplicado por filas a R y t
    glm::mat3 R_gl = R_cv;
    R_gl[0][1] = -R_cv[0][1];   // Negamos la fila Y de cada columna
    R_gl[0][2] = -R_cv[0][2];
    R_gl[1][1] = -R_cv[1][1];
    R_gl[1][2] = -R_cv[1][2];
    R_gl[2][1] = -R_cv[2][1];
    R_gl[2][2] = -R_cv[2][2];

    const glm::vec3 t_gl(tvec[0], -tvec[1], -tvec[2]);

    // 3. Construir matriz 4×4 Model-View column-major para GLM
    //    [ R | t ]
    //    [ 0 | 1 ]
    glm::mat4 mv(1.0f);
    // GLM: mv[col][row]
    mv[0][0] = R_gl[0][0]; mv[0][1] = R_gl[0][1]; mv[0][2] = R_gl[0][2];
    mv[1][0] = R_gl[1][0]; mv[1][1] = R_gl[1][1]; mv[1][2] = R_gl[1][2];
    mv[2][0] = R_gl[2][0]; mv[2][1] = R_gl[2][1]; mv[2][2] = R_gl[2][2];
    mv[3][0] = t_gl.x;     mv[3][1] = t_gl.y;     mv[3][2] = t_gl.z;

    result.modelView = mv;
    result.valid     = true;
    return result;
}

} // namespace arhelmet
