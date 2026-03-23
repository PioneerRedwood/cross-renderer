#pragma once

#include "Mesh.hpp"

struct Model {
    Mesh* mesh;
    Matrix4x4 modelMatrix;
};