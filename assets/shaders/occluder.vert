#version 330 core
// =============================================================================
// occluder.vert — Transforma la malla de oclusión facial al espacio de clip.
// Esta malla (face_occluder.obj) representa la geometría del rostro y se
// renderiza SOLO al Z-buffer para crear oclusión correcta con el casco 3D.
// =============================================================================

layout(location = 0) in vec3 aPosition;

uniform mat4 model;       // Model-View matrix (de PoseEstimator)
uniform mat4 projection;  // Proyección perspectiva (de intrínsecos de cámara)

void main() {
    gl_Position = projection * model * vec4(aPosition, 1.0);
}
