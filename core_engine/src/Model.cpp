/**
 * @file Model.cpp
 * @brief Cargador de modelos 3D via Assimp + loader de texturas con stb_image.
 */
#include "Model.hpp"

#include <stdexcept>
#include <iostream>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <glad/gl.h>

// stb_image — header-only, compilado UNA SOLA VEZ aquí
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace arhelmet {

// ── Constructor ───────────────────────────────────────────────────────────────

Model::Model(const std::string& path) : m_path(path) {
    loadModel(path);
}

// ── Draw ──────────────────────────────────────────────────────────────────────

void Model::draw(const Shader& shader) const {
    for (const auto& mesh : m_meshes) {
        mesh.draw(shader);
    }
}

// ── Carga del modelo ─────────────────────────────────────────────────────────

void Model::loadModel(const std::string& path) {
    Assimp::Importer importer;

    // Flags de post-procesado:
    //   Triangulate      : Convertir polígonos n>3 a triángulos
    //   GenSmoothNormals : Generar normales suaves si el modelo no las tiene
    //   FlipUVs          : Corregir UV.Y invertido (OpenGL vs. DirectX)
    //   CalcTangentSpace : Calcular tangentes/bitangentes para normal maps
    const aiScene* scene = importer.ReadFile(path,
        aiProcess_Triangulate      |
        aiProcess_GenSmoothNormals |
        aiProcess_FlipUVs          |
        aiProcess_CalcTangentSpace
    );

    if (!scene
        || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE)
        || !scene->mRootNode)
    {
        throw std::runtime_error(
            std::string("[Model] Error de Assimp: ") + importer.GetErrorString()
            + " (archivo: " + path + ")");
    }

    // Extraer directorio para búsqueda relativa de texturas
    const std::size_t sep = path.find_last_of("/\\");
    m_directory = (sep != std::string::npos) ? path.substr(0, sep) : ".";

    processNode(scene->mRootNode, scene);

    std::cout << "[Model] '" << path << "' cargado — "
              << m_meshes.size() << " mallas\n";
}

void Model::processNode(const aiNode* node, const aiScene* scene) {
    // Procesar mallas de este nodo
    for (unsigned int i = 0; i < node->mNumMeshes; ++i) {
        const aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        m_meshes.push_back(processMesh(mesh, scene));
    }
    // Procesar hijos recursivamente
    for (unsigned int i = 0; i < node->mNumChildren; ++i) {
        processNode(node->mChildren[i], scene);
    }
}

Mesh Model::processMesh(const aiMesh* mesh, const aiScene* scene) {
    std::vector<Vertex>       vertices;
    std::vector<unsigned int> indices;
    std::vector<MeshTexture>  textures;

    vertices.reserve(mesh->mNumVertices);

    // ── Vértices ──────────────────────────────────────────────────────────────
    for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
        Vertex v{};

        v.position = { mesh->mVertices[i].x,
                       mesh->mVertices[i].y,
                       mesh->mVertices[i].z };

        if (mesh->HasNormals()) {
            v.normal = { mesh->mNormals[i].x,
                         mesh->mNormals[i].y,
                         mesh->mNormals[i].z };
        }

        // Assimp soporta hasta 8 juegos de UV; usamos el primero
        if (mesh->mTextureCoords[0]) {
            v.texCoords = { mesh->mTextureCoords[0][i].x,
                            mesh->mTextureCoords[0][i].y };
        }
        vertices.push_back(v);
    }

    // ── Índices ───────────────────────────────────────────────────────────────
    for (unsigned int i = 0; i < mesh->mNumFaces; ++i) {
        const aiFace& face = mesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; ++j) {
            indices.push_back(face.mIndices[j]);
        }
    }

    // ── Materiales / texturas ─────────────────────────────────────────────────
    if (mesh->mMaterialIndex >= 0) {
        const aiMaterial* mat = scene->mMaterials[mesh->mMaterialIndex];

        auto diff = loadMaterialTextures(mat, aiTextureType_DIFFUSE,  "texture_diffuse");
        auto spec = loadMaterialTextures(mat, aiTextureType_SPECULAR, "texture_specular");
        auto norm = loadMaterialTextures(mat, aiTextureType_NORMALS,  "texture_normal");

        textures.insert(textures.end(), diff.begin(), diff.end());
        textures.insert(textures.end(), spec.begin(), spec.end());
        textures.insert(textures.end(), norm.begin(), norm.end());
    }

    return Mesh(std::move(vertices), std::move(indices), std::move(textures));
}

std::vector<MeshTexture> Model::loadMaterialTextures(
    const aiMaterial* mat, int aiType, const std::string& typeName)
{
    std::vector<MeshTexture> result;
    const auto type = static_cast<aiTextureType>(aiType);
    const unsigned int count = mat->GetTextureCount(type);
    result.reserve(count);

    for (unsigned int i = 0; i < count; ++i) {
        aiString str;
        mat->GetTexture(type, i, &str);
        const std::string fullPath = m_directory + "/" + str.C_Str();

        // Cache: evitar subir la misma textura dos veces a GPU
        auto it = m_texCache.find(fullPath);
        if (it != m_texCache.end()) {
            result.push_back(it->second);
        } else {
            MeshTexture tex;
            tex.id   = loadTextureFromFile(fullPath);
            tex.type = typeName;
            tex.path = fullPath;
            m_texCache[fullPath] = tex;
            result.push_back(tex);
        }
    }
    return result;
}

unsigned int Model::loadTextureFromFile(const std::string& path) {
    unsigned int texID = 0;
    glGenTextures(1, &texID);

    int w = 0, h = 0, nCh = 0;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &nCh, 0);

    if (!data) {
        std::cerr << "[Model] No se pudo cargar textura: " << path << "\n";
        // Retorna textura 1×1 blanca como fallback
        const unsigned char white[] = {255, 255, 255};
        glBindTexture(GL_TEXTURE_2D, texID);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0,
                     GL_RGB, GL_UNSIGNED_BYTE, white);
        return texID;
    }

    const GLenum fmt = (nCh == 4) ? GL_RGBA
                     : (nCh == 3) ? GL_RGB
                     :               GL_RED;

    glBindTexture(GL_TEXTURE_2D, texID);
    glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(fmt),
                 w, h, 0, fmt, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(data);
    return texID;
}

} // namespace arhelmet
