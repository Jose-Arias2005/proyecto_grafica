/**
 * @file Shader.cpp
 * @brief Implementación del wrapper RAII para programas GLSL.
 */
#include "Shader.hpp"

#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>

#include <glad/gl.h>
#include <glm/gtc/type_ptr.hpp>

namespace arhelmet {

// ── Construcción / destrucción ────────────────────────────────────────────────

Shader::Shader(const std::string& vertPath, const std::string& fragPath) {
    const std::string vertSrc = readFile(vertPath);
    const std::string fragSrc = readFile(fragPath);

    const unsigned int vert = compileShader(vertSrc, GL_VERTEX_SHADER);
    const unsigned int frag = compileShader(fragSrc, GL_FRAGMENT_SHADER);

    m_id = glCreateProgram();
    glAttachShader(m_id, vert);
    glAttachShader(m_id, frag);
    glLinkProgram(m_id);

    int  success = 0;
    char log[1024]{};
    glGetProgramiv(m_id, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(m_id, sizeof(log), nullptr, log);
        glDeleteProgram(m_id);
        m_id = 0;
        throw std::runtime_error(
            "[Shader] Error de enlazado (" + vertPath + " | " + fragPath + "):\n" + log);
    }

    // Los shaders individuales ya no son necesarios tras el linkeo
    glDeleteShader(vert);
    glDeleteShader(frag);
}

Shader::~Shader() {
    if (m_id) glDeleteProgram(m_id);
}

Shader::Shader(Shader&& o) noexcept
    : m_id(o.m_id)
    , m_uniformCache(std::move(o.m_uniformCache))
{
    o.m_id = 0;
}

Shader& Shader::operator=(Shader&& o) noexcept {
    if (this != &o) {
        if (m_id) glDeleteProgram(m_id);
        m_id           = o.m_id;
        m_uniformCache = std::move(o.m_uniformCache);
        o.m_id         = 0;
    }
    return *this;
}

// ── Activación ────────────────────────────────────────────────────────────────

void Shader::use() const {
    glUseProgram(m_id);
}

// ── Setters de uniforms ───────────────────────────────────────────────────────

void Shader::setBool(const std::string& n, bool v) const {
    glUniform1i(uniformLocation(n), static_cast<int>(v));
}
void Shader::setInt(const std::string& n, int v) const {
    glUniform1i(uniformLocation(n), v);
}
void Shader::setFloat(const std::string& n, float v) const {
    glUniform1f(uniformLocation(n), v);
}
void Shader::setVec2(const std::string& n, const glm::vec2& v) const {
    glUniform2fv(uniformLocation(n), 1, glm::value_ptr(v));
}
void Shader::setVec3(const std::string& n, const glm::vec3& v) const {
    glUniform3fv(uniformLocation(n), 1, glm::value_ptr(v));
}
void Shader::setVec4(const std::string& n, const glm::vec4& v) const {
    glUniform4fv(uniformLocation(n), 1, glm::value_ptr(v));
}
void Shader::setMat3(const std::string& n, const glm::mat3& m) const {
    glUniformMatrix3fv(uniformLocation(n), 1, GL_FALSE, glm::value_ptr(m));
}
void Shader::setMat4(const std::string& n, const glm::mat4& m) const {
    glUniformMatrix4fv(uniformLocation(n), 1, GL_FALSE, glm::value_ptr(m));
}

// ── Helpers privados ──────────────────────────────────────────────────────────

int Shader::uniformLocation(const std::string& name) const {
    auto it = m_uniformCache.find(name);
    if (it != m_uniformCache.end()) return it->second;

    const int loc = glGetUniformLocation(m_id, name.c_str());
    if (loc == -1) {
        // Advertencia en debug; no lanzar excepción (puede ser uniform optimizado)
        std::cerr << "[Shader] Advertencia: uniform '" << name
                  << "' no encontrado en programa " << m_id << "\n";
    }
    m_uniformCache.emplace(name, loc);
    return loc;
}

std::string Shader::readFile(const std::string& path) {
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("[Shader] No se puede abrir el archivo: " + path);
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

unsigned int Shader::compileShader(const std::string& src, unsigned int type) {
    const char* code = src.c_str();
    const unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &code, nullptr);
    glCompileShader(shader);

    int  success = 0;
    char log[1024]{};
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        glDeleteShader(shader);
        const std::string typeStr = (type == GL_VERTEX_SHADER) ? "VERTEX" : "FRAGMENT";
        throw std::runtime_error(
            "[Shader] Error de compilación " + typeStr + ":\n" + std::string(log));
    }
    return shader;
}

} // namespace arhelmet
