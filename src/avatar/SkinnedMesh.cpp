#include "SkinnedMesh.h"
#include <GL/glew.h>
#include <utility>
#include <cstddef>

namespace avatar {

SkinnedMesh::~SkinnedMesh() {
    release();
}

SkinnedMesh::SkinnedMesh(SkinnedMesh&& other) noexcept
    : vao_(other.vao_), vbo_(other.vbo_), ibo_(other.ibo_),
      indexCount_(other.indexCount_), materialIndex_(other.materialIndex_) {
    other.vao_ = other.vbo_ = other.ibo_ = 0;
    other.indexCount_ = 0;
}

SkinnedMesh& SkinnedMesh::operator=(SkinnedMesh&& other) noexcept {
    if (this != &other) {
        release();
        vao_           = other.vao_;
        vbo_           = other.vbo_;
        ibo_           = other.ibo_;
        indexCount_    = other.indexCount_;
        materialIndex_ = other.materialIndex_;
        other.vao_ = other.vbo_ = other.ibo_ = 0;
        other.indexCount_ = 0;
    }
    return *this;
}

void SkinnedMesh::release() {
    if (vao_) { glDeleteVertexArrays(1, &vao_); vao_ = 0; }
    if (vbo_) { glDeleteBuffers(1, &vbo_);      vbo_ = 0; }
    if (ibo_) { glDeleteBuffers(1, &ibo_);      ibo_ = 0; }
    indexCount_ = 0;
}

void SkinnedMesh::upload(const MeshPrimitive& prim) {
    release();
    materialIndex_ = prim.materialIndex;
    indexCount_    = static_cast<int>(prim.indices.size());

    glGenVertexArrays(1, &vao_);
    glBindVertexArray(vao_);

    // Vertex buffer
    glGenBuffers(1, &vbo_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(prim.vertices.size() * sizeof(Vertex)),
                 prim.vertices.data(), GL_STATIC_DRAW);

    // Index buffer
    glGenBuffers(1, &ibo_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(prim.indices.size() * sizeof(uint32_t)),
                 prim.indices.data(), GL_STATIC_DRAW);

    // location 0: position (vec3 float)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, position)));

    // location 1: normal (vec3 float)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, normal)));

    // location 2: texCoord (vec2 float)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, texCoord)));

    // location 3: boneIDs (ivec4) — must use IPointer for integer attributes
    glEnableVertexAttribArray(3);
    glVertexAttribIPointer(3, 4, GL_INT, sizeof(Vertex),
                           reinterpret_cast<void*>(offsetof(Vertex, boneIDs)));

    // location 4: boneWeights (vec4 float)
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, boneWeights)));

    glBindVertexArray(0);
}

void SkinnedMesh::draw() const {
    if (!isValid() || indexCount_ == 0) return;
    glBindVertexArray(vao_);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indexCount_), GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

}  // namespace avatar
