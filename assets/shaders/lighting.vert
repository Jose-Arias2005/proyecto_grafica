#version 330 core
// =============================================================================
// lighting.vert — Vertex shader para el casco 3D con iluminación.
// Transforma vértices y propaga atributos al fragment shader.
// =============================================================================

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoords;

// ── Salidas hacia el fragment shader ──────────────────────────────────────────
out vec3 FragPos;    // Posición en espacio mundo/cámara
out vec3 Normal;     // Normal transformada
out vec2 TexCoords;  // Coordenadas de textura

// ── Matrices de transformación ────────────────────────────────────────────────
uniform mat4 model;       // Model-View matrix (pose del casco)
uniform mat4 projection;  // Proyección perspectiva

void main() {
    vec4 worldPos = model * vec4(aPosition, 1.0);
    FragPos   = vec3(worldPos);

    // Matriz normal: transpuesta de la inversa del model (para no-uniformes)
    Normal    = mat3(transpose(inverse(model))) * aNormal;
    TexCoords = aTexCoords;

    gl_Position = projection * worldPos;
}
