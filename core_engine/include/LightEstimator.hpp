/**
 * @file LightEstimator.hpp
 * @brief Interfaz de estimación fotométrica — RESPONSABILIDAD DEL EQUIPO HUMANO.
 *
 * ╔══════════════════════════════════════════════════════════════════════════╗
 * ║  ATENCIÓN — MÓDULO RESERVADO                                            ║
 * ║  Este archivo define SOLO la interfaz pública (firmas de métodos).      ║
 * ║  La implementación interna del cálculo de radiancia, gradientes de      ║
 * ║  iluminación y análisis fotométrico es responsabilidad EXCLUSIVA del    ║
 * ║  equipo humano en LightEstimator.cpp.                                   ║
 * ║                                                                         ║
 * ║  NO implementar lógica aquí. NO modificar las firmas.                   ║
 * ╚══════════════════════════════════════════════════════════════════════════╝
 *
 * El estimador analiza frames BGRA/BGR de la cámara y determina:
 *   - lightDir    : Dirección principal de la fuente de luz (espacio mundo)
 *   - lightColor  : Color e intensidad RGB de la luz difusa
 *   - ambientColor: Color del término ambiental (iluminación de relleno)
 *
 * Estos valores son provistos al Renderer en cada frame mediante
 * Renderer::setLighting(LightEstimate) y se pasan como uniforms al
 * fragment shader lighting.frag.
 */
#pragma once

#include <cstdint>
#include <glm/glm.hpp>

namespace arhelmet {

/**
 * @brief Resultado de la estimación de iluminación para un frame.
 *
 * Todos los valores de color están en rango [0, 1]³.
 * lightDir debe estar normalizado.
 */
struct LightEstimate {
    glm::vec3 lightDir;      ///< Dirección de la fuente de luz (espacio cámara/mundo)
    glm::vec3 lightColor;    ///< Color RGB de la luz difusa
    glm::vec3 ambientColor;  ///< Color RGB del término ambiental

    /** Retorna valores por defecto seguros (luz frontal blanca, ambiente suave). */
    static LightEstimate defaultEstimate() noexcept {
        return {
            glm::normalize(glm::vec3(0.0f, -1.0f, -1.0f)),  // Desde arriba-adelante
            glm::vec3(1.0f, 1.0f, 1.0f),                      // Blanco puro
            glm::vec3(0.15f, 0.15f, 0.20f)                    // Azul tenue de relleno
        };
    }
};

class LightEstimator {
public:
    LightEstimator()  = default;
    ~LightEstimator() = default;

    LightEstimator(const LightEstimator&)            = delete;
    LightEstimator& operator=(const LightEstimator&) = delete;

    // ── Interfaz pública (implementar en LightEstimator.cpp) ─────────────────

    /**
     * @brief Estima la dirección y color de la luz a partir de un frame.
     *
     * TODO (equipo humano): Implementar análisis fotométrico.
     *   Algoritmos sugeridos:
     *     · Gradiente de Sobel para estimar dirección de luz dominante
     *     · Histograma de luminancia para separar componente ambiental
     *     · Esfera de entorno (environment map) aproximada por SH de grado 2
     *
     * @param frameData  Buffer de imagen BGR, uint8, row-major, stride = width*3.
     * @param width      Ancho del frame en píxeles.
     * @param height     Alto del frame en píxeles.
     * @returns          Estimación de iluminación para este frame.
     */
    LightEstimate estimate(
        const uint8_t* frameData,
        int            width,
        int            height
    ) const;

    /**
     * @brief Calibra con un frame de referencia neutro (sin cara, sin objetos).
     *
     * TODO (equipo humano): Implementar calibración de referencia.
     *   Llamar una sola vez al inicio con un frame del fondo vacío para
     *   establecer la línea base de iluminación ambiental.
     *
     * @param referenceFrame  Buffer BGR uint8 del frame de referencia.
     * @param width           Ancho en píxeles.
     * @param height          Alto en píxeles.
     */
    void calibrate(
        const uint8_t* referenceFrame,
        int            width,
        int            height
    );

    /**
     * @brief Actualización incremental para suavizado temporal.
     *
     * TODO (equipo humano): Implementar filtro temporal (opcional).
     *   Sugerencia: filtro de paso bajo exponencial:
     *     estimate_t = alpha * estimate_new + (1 - alpha) * estimate_{t-1}
     *
     * @param frameData  Buffer BGR uint8 del frame actual.
     * @param width      Ancho en píxeles.
     * @param height     Alto en píxeles.
     */
    void update(
        const uint8_t* frameData,
        int            width,
        int            height
    );

    /**
     * @brief Retorna la última estimación calculada (para debug/UI).
     * TODO (equipo humano): Implementar getter del estado interno.
     */
    LightEstimate lastEstimate() const noexcept;
};

} // namespace arhelmet
