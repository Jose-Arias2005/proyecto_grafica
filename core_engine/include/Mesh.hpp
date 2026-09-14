/**
 * @file Mesh.hpp
 * @brief Layout de vértice, gestión de VAO/VBO/EBO y llamada de dibujo.
 *
 * Mesh es un tipo RAII que gestiona sus recursos GPU.  Se crea una vez con
 * los datos de vértices e índices, y se libera en el destructor.
 *
 * La implementación es inline (header-only) porque está fuertemente acoplada
 * a los tipos GLuint y al contexto GL activo.
 *
 * Vertex layout en GPU (interleaved):
 *   location 0 → position  (vec3, offset 0)
 *   location 1 → normal    (vec3, offset 12)
 *   location 2 → texCoords (vec2, offset 24)
 */
#pragma once

#include <vector>
#include <string>
#include <cstddef>   // offsetof

#include <glad/gl.h>
#include <glm/glm.hpp>

#include "Shader.hpp"

namespace arhelmet {

// ── Estructuras de datos ──────────────────────────────────────────────────────

/** Datos por vértice en layout interleaved. */
struct Vertex {
    glm::vec3 position;   ///< Posición XYZ en espacio objeto
    glm::vec3 normal;     ///< Normal (puede ser generada por Assimp)
    glm::vec2 texCoords;  ///< Coordenadas UV
};

/** Textura asociada a una malla. */
struct MeshTexture {
    unsigned int id   = 0;               ///< Handle GL_TEXTURE_2D
    std::string  type;                   ///< "texture_diffuse" | "texture_specular" | "texture_normal"
    std::string  path;                   ///< Ruta en disco (para cache)
};

// ── Mesh (header-only RAII) ───────────────────────────────────────────────────

class Mesh {
public:
    // ── Construcción ─────────────────────────────────────────────────────────
    Mesh(std::vector<Vertex>       vertices,
         std::vector<unsigned int> indices,
         std::vector<MeshTexture>  textures)
        : m_vertices(std::move(vertices))
        , m_indices (std::move(indices))
        , m_textures(std::move(textures))
    {
        setupGPU();
    }

    // Non-copyable (posee handles GPU únicos)
    Mesh(const Mesh&)            = delete;
    Mesh& operator=(const Mesh&) = delete;

    // Moveable (transfiere ownership de handles)
    Mesh(Mesh&& o) noexcept
        : m_vertices(std::move(o.m_vertices))
        , m_indices (std::move(o.m_indices))
        , m_textures(std::move(o.m_textures))
        , m_VAO(o.m_VAO), m_VBO(o.m_VBO), m_EBO(o.m_EBO)
    {
        o.m_VAO = o.m_VBO = o.m_EBO = 0;
    }

    Mesh& operator=(Mesh&& o) noexcept {
        if (this != &o) {
            freeGPU();
            m_vertices = std::move(o.m_vertices);
            m_indices  = std::move(o.m_indices);
            m_textures = std::move(o.m_textures);
            m_VAO = o.m_VAO; m_VBO = o.m_VBO; m_EBO = o.m_EBO;
            o.m_VAO = o.m_VBO = o.m_EBO = 0;
        }
        return *this;
    }

    ~Mesh() { freeGPU(); }

    // ── Dibujo ───────────────────────────────────────────────────────────────
    /**
     * @brief Enlaza texturas a sus slots convencionales y emite la draw call.
     * @param shader  Programa activo (debe estar en uso antes de llamar draw).
     */
    void draw(const Shader& shader) const {
        unsigned int nDiff = 0, nSpec = 0, nNorm = 0;

        for (unsigned int i = 0; i < static_cast<unsigned int>(m_textures.size()); ++i) {
            glActiveTexture(GL_TEXTURE0 + i);

            const std::string& type = m_textures[i].type;
            unsigned int number = 0;
            if      (type == "texture_diffuse")  number = ++nDiff;
            else if (type == "texture_specular")  number = ++nSpec;
            else if (type == "texture_normal")    number = ++nNorm;

            shader.setInt(type + std::to_string(number), static_cast<int>(i));
            glBindTexture(GL_TEXTURE_2D, m_textures[i].id);
        }

        // Informar al shader si hay textura difusa enlazada.
        // Permite el fallback gris en lighting.frag cuando no hay .mtl cargado.
        shader.setInt("has_diffuse_texture", nDiff > 0 ? 1 : 0);

        glBindVertexArray(m_VAO);
        glDrawElements(GL_TRIANGLES,
                       static_cast<GLsizei>(m_indices.size()),
                       GL_UNSIGNED_INT, nullptr);
        glBindVertexArray(0);
        glActiveTexture(GL_TEXTURE0);
    }

    // ── Accessors ─────────────────────────────────────────────────────────────
    std::size_t vertexCount() const { return m_vertices.size(); }
    std::size_t indexCount()  const { return m_indices.size();  }

private:
    std::vector<Vertex>       m_vertices;
    std::vector<unsigned int> m_indices;
    std::vector<MeshTexture>  m_textures;

    GLuint m_VAO = 0, m_VBO = 0, m_EBO = 0;

    void setupGPU() {
        glGenVertexArrays(1, &m_VAO);
        glGenBuffers(1, &m_VBO);
        glGenBuffers(1, &m_EBO);

        glBindVertexArray(m_VAO);

        // Subir vértices
        glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
        glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(m_vertices.size() * sizeof(Vertex)),
                     m_vertices.data(), GL_STATIC_DRAW);

        // Subir índices
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(m_indices.size() * sizeof(unsigned int)),
                     m_indices.data(), GL_STATIC_DRAW);

        // Atributo 0: position (vec3)
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
            reinterpret_cast<void*>(offsetof(Vertex, position)));
        glEnableVertexAttribArray(0);

        // Atributo 1: normal (vec3)
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
            reinterpret_cast<void*>(offsetof(Vertex, normal)));
        glEnableVertexAttribArray(1);

        // Atributo 2: texCoords (vec2)
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
            reinterpret_cast<void*>(offsetof(Vertex, texCoords)));
        glEnableVertexAttribArray(2);

        glBindVertexArray(0);
    }

    void freeGPU() noexcept {
        if (m_VAO) { glDeleteVertexArrays(1, &m_VAO); m_VAO = 0; }
        if (m_VBO) { glDeleteBuffers(1, &m_VBO);      m_VBO = 0; }
        if (m_EBO) { glDeleteBuffers(1, &m_EBO);      m_EBO = 0; }
    }
};

} // namespace arhelmet
