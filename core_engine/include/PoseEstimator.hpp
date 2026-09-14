/**
 * @file PoseEstimator.hpp
 * @brief Estimación de pose 2D→3D: rvec/tvec → matriz Model-View de OpenGL.
 *
 * La resolución solvePnP se realiza en el lado Python (cv2.solvePnP).
 * Este módulo C++ recibe el vector de Rodrigues (rvec) y el vector de
 * traslación (tvec) ya calculados, y los convierte a una matriz 4×4
 * column-major compatible con OpenGL mediante la fórmula analítica de
 * Rodrigues (sin dependencia de OpenCV en C++).
 *
 * Coordenadas canónicas:
 *   OpenCV  : X→derecha, Y→abajo,  Z→hacia la pantalla
 *   OpenGL  : X→derecha, Y→arriba, Z→hacia el observador
 *   Conversión aplicada: diag(1, -1, -1) * R_cv,  diag(1, -1, -1) * t_cv
 *
 * Los 6 puntos 3D canónicos de la cara (en mm) están disponibles como
 * constante estática y deben coincidir con los índices de MediaPipe
 * definidos en landmark_detector.py.
 */
#pragma once

#include <array>
#include <glm/glm.hpp>

namespace arhelmet {

/** Resultado de una estimación de pose. */
struct PoseResult {
    glm::mat4            modelView;  ///< Matriz MV column-major para OpenGL
    std::array<float, 3> rvec;       ///< Vector de Rodrigues entrada [rx,ry,rz]
    std::array<float, 3> tvec;       ///< Traslación entrada [tx,ty,tz] (mm)
    bool                 valid;      ///< false si la estimación falló
};

class PoseEstimator {
public:
    /**
     * @brief Construye el estimador con los parámetros intrínsecos de la cámara.
     * @param fx,fy  Longitudes focales en píxeles.
     * @param cx,cy  Punto principal (offsets) en píxeles.
     */
    PoseEstimator(float fx, float fy, float cx, float cy) noexcept;

    /**
     * @brief Convierte rvec + tvec (de cv2.solvePnP) a matriz Model-View OpenGL.
     *
     * Algoritmo:
     *   1. Rodrigues analítico: θ = |rvec|, k = rvec/θ
     *      R = I·cos(θ) + (1-cos(θ))·k·kᵀ + sin(θ)·[k]×
     *   2. Conversión de convenio OpenCV→OpenGL: Y y Z negados
     *   3. Construcción de la matriz 4×4 column-major
     *
     * @param rvec  Puntero a float[3] con el vector de Rodrigues.
     * @param tvec  Puntero a float[3] con el vector de traslación (mm).
     * @returns     PoseResult con valid=true.
     */
    PoseResult buildModelView(const float* rvec, const float* tvec) const noexcept;

    /** Actualiza los parámetros intrínsecos (e.g. tras calibración). */
    void setIntrinsics(float fx, float fy, float cx, float cy) noexcept;

    float fx() const noexcept { return m_fx; }
    float fy() const noexcept { return m_fy; }
    float cx() const noexcept { return m_cx; }
    float cy() const noexcept { return m_cy; }

    /**
     * @brief Puntos 3D canónicos del modelo de cara (en mm).
     *
     * Orden: [nose_tip, chin, left_eye_outer, right_eye_outer,
     *         left_mouth_corner, right_mouth_corner]
     *
     * Deben coincidir con LANDMARK_INDICES en landmark_detector.py.
     */
    static const std::array<std::array<float, 3>, 6>& canonicalPoints() noexcept;

private:
    float m_fx, m_fy, m_cx, m_cy;

    /**
     * @brief Fórmula analítica de Rodrigues: vector ángulo-eje → matriz 3×3.
     * @param rv  Vector de Rodrigues (magnitud = ángulo en radianes).
     */
    static glm::mat3 rodriguesRotation(const glm::vec3& rv) noexcept;
};

} // namespace arhelmet
