#version 330 core
// =============================================================================
// background.frag — Muestra el frame BGR de la cámara como textura 2D.
// El frame se sube como GL_BGR / GL_UNSIGNED_BYTE desde pybind_wrapper.cpp.
// =============================================================================

in  vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D cameraFrame;  // Textura del frame de cámara (BGR → RGB en GL)

void main() {
    // La textura se sube como BGR; OpenGL la interpreta correctamente con
    // el swizzle configurado en Renderer::uploadCameraFrame().
    FragColor = texture(cameraFrame, TexCoord);
}
