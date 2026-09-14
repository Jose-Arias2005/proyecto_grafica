/**
 * @file Model.hpp
 * @brief Cargador de modelos 3D basado en Assimp.
 *
 * Carga un archivo OBJ (u otro formato soportado por Assimp) y construye
 * una colección de Mesh con sus texturas.  Las texturas se cachean por ruta
 * para evitar uploads duplicados a GPU.
 *
 * Uso:
 *   Model helmet("assets/models/helmet.obj");
 *   // En render loop:
 *   helmet.draw(lightingShader);
 */
#pragma once

#include <string>
#include <vector>
#include <unordered_map>

#include "Mesh.hpp"

// Forward declarations de Assimp (evita incluir cabeceras pesadas aquí)
struct aiScene;
struct aiNode;
struct aiMesh;
struct aiMaterial;

namespace arhelmet {

class Model {
public:
    /**
     * @brief Carga el modelo desde disco vía Assimp.
     * @param path  Ruta al archivo .obj (o cualquier formato soportado).
     * @throws std::runtime_error si Assimp no puede importar el archivo.
     */
    explicit Model(const std::string& path);

    /** Dibuja todas las sub-mallas con el shader activo. */
    void draw(const Shader& shader) const;

    // Non-copyable (posee recursos GPU a través de Mesh)
    Model(const Model&)            = delete;
    Model& operator=(const Model&) = delete;

    Model(Model&&)            = default;
    Model& operator=(Model&&) = default;

    std::size_t meshCount()     const noexcept { return m_meshes.size(); }
    const std::string& path()   const noexcept { return m_path; }

private:
    std::vector<Mesh>                            m_meshes;
    std::string                                  m_directory;  ///< Directorio del archivo
    std::string                                  m_path;       ///< Ruta completa
    mutable std::unordered_map<std::string, MeshTexture> m_texCache; ///< Cache de texturas

    void loadModel(const std::string& path);
    void processNode(const aiNode* node, const aiScene* scene);
    Mesh processMesh(const aiMesh* mesh, const aiScene* scene);

    std::vector<MeshTexture> loadMaterialTextures(
        const aiMaterial* mat, int aiType, const std::string& typeName);

    static unsigned int loadTextureFromFile(const std::string& absPath);
};

} // namespace arhelmet
