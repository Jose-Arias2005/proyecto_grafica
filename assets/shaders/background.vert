#version 330 core
// =============================================================================
// background.vert — Quad a pantalla completa para mostrar el frame de cámara.
// No se necesita buffer de vértices: la posición se calcula en el shader.
// =============================================================================

// gl_VertexID: 0,1,2 → triángulo izquierdo; 3,4,5 → triángulo derecho
// (draw con glDrawArrays(GL_TRIANGLES, 0, 6))

out vec2 TexCoord;

void main() {
    // Coordenadas del quad a pantalla completa en NDC ([-1,1])
    const vec2 positions[6] = vec2[6](
        vec2(-1.0, -1.0),  vec2( 1.0, -1.0),  vec2(-1.0,  1.0),
        vec2(-1.0,  1.0),  vec2( 1.0, -1.0),  vec2( 1.0,  1.0)
    );
    // UVs correspondientes
    const vec2 uvs[6] = vec2[6](
        vec2(0.0, 0.0),  vec2(1.0, 0.0),  vec2(0.0, 1.0),
        vec2(0.0, 1.0),  vec2(1.0, 0.0),  vec2(1.0, 1.0)
    );

    TexCoord    = uvs[gl_VertexID];
    gl_Position = vec4(positions[gl_VertexID], 0.0, 1.0);
}
