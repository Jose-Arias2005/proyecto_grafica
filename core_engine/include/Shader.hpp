/**
 * @file Shader.hpp
 * @brief RAII wrapper para programas GLSL de OpenGL.
 *
 * Carga y compila vertex + fragment shaders desde disco, los enlaza en un
 * programa GL y provee setters tipados para uniforms.  El programa se libera
 * automáticamente en el destructor.
 *
 * Uso:
 *   Shader sh("shaders/lighting.vert", "shaders/lighting.frag");
 *   sh.use();
 *   sh.setVec3("lightDir", glm::normalize(glm::vec3(1,1,-1)));
 */
#pragma once

#include <string>
#include <unordered_map>

#include <glm/glm.hpp>

namespace arhelmet {

class Shader {
public:
    /**
     * @brief Carga, compila y enlaza el programa GLSL.
     * @param vertPath  Ruta al vertex shader (.vert).
     * @param fragPath  Ruta al fragment shader (.frag).
     * @throws std::runtime_error en error de compilación o enlazado.
     */
    Shader(const std::string& vertPath, const std::string& fragPath);
    ~Shader();

    // Non-copyable, moveable
    Shader(const Shader&)             = delete;
    Shader& operator=(const Shader&)  = delete;
    Shader(Shader&& other) noexcept;
    Shader& operator=(Shader&& other) noexcept;

    /** Activa este programa como shader actual de OpenGL. */
    void use() const;

    // ── Setters de uniforms ───────────────────────────────────────────────────
    void setBool (const std::string& name, bool         value) const;
    void setInt  (const std::string& name, int          value) const;
    void setFloat(const std::string& name, float        value) const;
    void setVec2 (const std::string& name, const glm::vec2& v) const;
    void setVec3 (const std::string& name, const glm::vec3& v) const;
    void setVec4 (const std::string& name, const glm::vec4& v) const;
    void setMat3 (const std::string& name, const glm::mat3& m) const;
    void setMat4 (const std::string& name, const glm::mat4& m) const;

    /** Handle GL del programa (para llamadas GL directas si es necesario). */
    unsigned int id() const noexcept { return m_id; }

private:
    unsigned int m_id = 0;
    // Cache de ubicaciones de uniforms para evitar glGetUniformLocation repetido
    mutable std::unordered_map<std::string, int> m_uniformCache;

    static std::string  readFile(const std::string& path);
    static unsigned int compileShader(const std::string& src, unsigned int type);
    int                 uniformLocation(const std::string& name) const;
};

} // namespace arhelmet
