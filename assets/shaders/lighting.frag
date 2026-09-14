#version 330 core
// =============================================================================
// lighting.frag — Fragment shader del casco 3D.
//
// ╔══════════════════════════════════════════════════════════════════════════╗
// ║  ATENCIÓN: La lógica de sombreado (Blinn-Phong / PBR) es               ║
// ║  responsabilidad EXCLUSIVA del equipo humano.                           ║
// ║  Solo se define la estructura con los uniforms requeridos.              ║
// ║  NO implementar el cálculo de radiancia/BRDF en este archivo.           ║
// ╚══════════════════════════════════════════════════════════════════════════╝
// =============================================================================

// ─── ENTRADAS DEL VERTEX SHADER ───────────────────────────────────────────────
in vec3 FragPos;    // Posición del fragmento (espacio mundo/cámara)
in vec3 Normal;     // Normal interpolada del fragmento
in vec2 TexCoords;  // Coordenadas UV del fragmento

// ─── UNIFORMS DE ILUMINACIÓN (provistos por LightEstimator en cada frame) ────
uniform vec3 lightDir;       // Dirección de la luz (normalizada, espacio cámara)
uniform vec3 lightColor;     // Color e intensidad de la luz difusa   [0, 1]³
uniform vec3 ambientColor;   // Color del término ambiental (relleno)  [0, 1]³

// ─── UNIFORMS DE MATERIAL ─────────────────────────────────────────────────────
uniform sampler2D texture_diffuse1;   // Albedo / mapa difuso
uniform sampler2D texture_specular1;  // Mapa especular (opcional)
uniform sampler2D texture_normal1;    // Mapa de normales (opcional)
uniform vec3      viewPos;            // Posición de la cámara (espacio mundo)

// Fallback de desarrollo: 1 si la malla tiene textura difusa cargada; 0 si no.
// Seteado desde Renderer::passHelmet() y sobreescrito por Mesh::draw().
// El equipo humano puede eliminar este uniform cuando implemente el shader completo.
uniform int has_diffuse_texture;

// ─── SALIDA ───────────────────────────────────────────────────────────────────
out vec4 FragColor;

// =============================================================================
//  TODO (equipo humano): Implementar el modelo de iluminación aquí.
//
//  Variables disponibles en este scope:
//    - FragPos      : vec3  — posición 3D del fragmento
//    - Normal       : vec3  — normal interpolada (sin normalizar aún)
//    - TexCoords    : vec2  — UV del fragmento
//    - lightDir     : vec3  — dirección de luz (de LightEstimator)
//    - lightColor   : vec3  — color de luz difusa
//    - ambientColor : vec3  — color ambiental
//    - viewPos      : vec3  — posición del observador
//    - texture_diffuse1, texture_specular1, texture_normal1 : sampler2D
//
//  Modelos sugeridos:
//    a) Blinn-Phong clásico
//    b) Cook-Torrance PBR simplificado
//    c) Toon shading / cel-shading para efecto estilizado
// =============================================================================
void main() {
    // PLACEHOLDER: muestra textura difusa o gris neutro si no hay material cargado.
    // Cuando has_diffuse_texture == 0 (malla OBJ sin .mtl o sin UV mapeadas),
    // texture() sobre un sampler no enlazado retorna (0,0,0,1) — negro puro.
    // El fallback gris garantiza visibilidad inmediata durante desarrollo.
    //
    // TODO (equipo humano): Reemplazar TODA la lógica de main() con el modelo
    //   de iluminación físico (Blinn-Phong / PBR). Los uniforms lightDir,
    //   lightColor y ambientColor están listos para usarse.
    vec4 albedo;
    if (has_diffuse_texture == 1) {
        albedo = texture(texture_diffuse1, TexCoords);
    } else {
        // Gris neutro de desarrollo — visible sin necesitar un archivo .mtl
        albedo = vec4(0.8, 0.8, 0.8, 1.0);
    }
    FragColor = albedo;
}
