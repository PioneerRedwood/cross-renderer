#pragma once

#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "Math.hpp"
#include "TGA.hpp"

struct Mesh
{
    std::vector<Vector3> verts;
    std::vector<uint32_t> indices;
    std::vector<Vector2> uvs;
    std::vector<Vector3> normals;
    TGA *tga{nullptr};
    float scale { 1.0f };
    bool hasUVs{false};
    bool hasNormals{false};
};

Mesh* CreateGridPlaneMesh(int quadsX, int quadsZ, float sizeX, float sizeZ) {
    Mesh* mesh = new Mesh();
    mesh->verts.reserve((quadsX + 1) * (quadsZ + 1));
    mesh->uvs.reserve((quadsX + 1) * (quadsZ + 1));
    mesh->normals.reserve((quadsX + 1) * (quadsZ + 1));
    mesh->indices.reserve(quadsX * quadsZ * 6);

    for (int z = 0; z <= quadsZ; ++z) {
        for (int x = 0; x <= quadsX; ++x) {
            float u = static_cast<float>(x) / quadsX;
            float v = static_cast<float>(z) / quadsZ;
            float posX = (u - 0.5f) * sizeX;
            float posZ = (v - 0.5f) * sizeZ;
            mesh->verts.emplace_back(posX, 0.0f, posZ);
            mesh->uvs.emplace_back(u, v);
            mesh->normals.emplace_back(0.0f, 1.0f, 0.0f);
        }
    }

    for (int z = 0; z < quadsZ; ++z) {
        for (int x = 0; x < quadsX; ++x) {
            uint32_t i0 = z * (quadsX + 1) + x;
            uint32_t i1 = i0 + 1;
            uint32_t i2 = i0 + (quadsX + 1);
            uint32_t i3 = i2 + 1;

            mesh->indices.push_back(i0);
            mesh->indices.push_back(i2);
            mesh->indices.push_back(i1);

            mesh->indices.push_back(i1);
            mesh->indices.push_back(i2);
            mesh->indices.push_back(i3);
        }
    }

    return mesh;
}
