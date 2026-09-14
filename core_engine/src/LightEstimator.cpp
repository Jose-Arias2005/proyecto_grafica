/**
 * @file LightEstimator.cpp
 * @brief Stubs del estimador fotométrico — RESPONSABILIDAD DEL EQUIPO HUMANO.
 *
 * ╔══════════════════════════════════════════════════════════════════════════╗
 * ║  MÓDULO RESERVADO — NO MODIFICAR SIN AUTORIZACIÓN DEL EQUIPO HUMANO    ║
 * ║                                                                         ║
 * ║  Este archivo contiene únicamente placeholders (implementaciones        ║
 * ║  vacías) que retornan valores por defecto seguros.                      ║
 * ║                                                                         ║
 * ║  El equipo humano debe implementar aquí:                                ║
 * ║    1. estimate()   — Análisis fotométrico del frame                     ║
 * ║    2. calibrate()  — Calibración con frame de referencia                ║
 * ║    3. update()     — Suavizado temporal de estimaciones                 ║
 * ║    4. lastEstimate() — Getter del estado interno                        ║
 * ║                                                                         ║
 * ║  Ver LightEstimator.hpp para la documentación completa de la interfaz.  ║
 * ╚══════════════════════════════════════════════════════════════════════════╝
 */
#include "LightEstimator.hpp"

#include <iostream>

namespace arhelmet {

// ── estimate() ───────────────────────────────────────────────────────────────
//
// TODO (equipo humano): Implementar análisis fotométrico del frame.
// El placeholder retorna valores de iluminación por defecto que permiten
// que el casco sea visible durante el desarrollo.
//
LightEstimate LightEstimator::estimate(
    [[maybe_unused]] const uint8_t* frameData,
    [[maybe_unused]] int            width,
    [[maybe_unused]] int            height
) const {
    // PLACEHOLDER — Retorna valores por defecto hasta implementación real
    return LightEstimate::defaultEstimate();
}

// ── calibrate() ──────────────────────────────────────────────────────────────
//
// TODO (equipo humano): Establecer línea base de iluminación ambiental.
//
void LightEstimator::calibrate(
    [[maybe_unused]] const uint8_t* referenceFrame,
    [[maybe_unused]] int            width,
    [[maybe_unused]] int            height
) {
    // PLACEHOLDER — No-op hasta implementación real
    std::cout << "[LightEstimator] calibrate() — pendiente de implementación\n";
}

// ── update() ─────────────────────────────────────────────────────────────────
//
// TODO (equipo humano): Suavizado temporal de estimaciones.
//
void LightEstimator::update(
    [[maybe_unused]] const uint8_t* frameData,
    [[maybe_unused]] int            width,
    [[maybe_unused]] int            height
) {
    // PLACEHOLDER — No-op hasta implementación real
}

// ── lastEstimate() ───────────────────────────────────────────────────────────
//
// TODO (equipo humano): Retornar el último estado calculado.
//
LightEstimate LightEstimator::lastEstimate() const noexcept {
    // PLACEHOLDER — Retorna valores por defecto
    return LightEstimate::defaultEstimate();
}

} // namespace arhelmet
