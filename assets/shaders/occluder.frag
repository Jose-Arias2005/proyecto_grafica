#version 330 core
// =============================================================================
// occluder.frag — Máscara de oclusión: NO escribe color, solo profundidad.
// El color write se deshabilita desde el CPU con glColorMask(false,...).
// Este shader existe únicamente como placeholder requerido por el pipeline.
// =============================================================================

// Sin output de color — glColorMask(GL_FALSE, ...) lo descarta de todas formas.
// gl_FragDepth se escribe automáticamente con la interpolación de z.
out vec4 FragColor;

void main() {
    // Descartar fragmento explícitamente — el Z-buffer ya fue escrito
    // por la rasterización antes de llegar aquí.
    FragColor = vec4(0.0);  // Nunca visible (glColorMask = false)
}
