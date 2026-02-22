#pragma once
#include "GltfLoader.h"
#include <cstdint>

namespace avatar {

// Manages VAO/VBO/IBO for one MeshPrimitive.
// Uploaded once at init; drawn each frame.
class SkinnedMesh {
public:
    SkinnedMesh() = default;
    ~SkinnedMesh();

    SkinnedMesh(const SkinnedMesh&)            = delete;
    SkinnedMesh& operator=(const SkinnedMesh&) = delete;
    SkinnedMesh(SkinnedMesh&& other) noexcept;
    SkinnedMesh& operator=(SkinnedMesh&& other) noexcept;

    // Upload vertex + index data to GPU. GL context must be current.
    void upload(const MeshPrimitive& prim);

    // Bind VAO and issue glDrawElements. Caller sets shader + uniforms.
    void draw() const;

    [[nodiscard]] int  materialIndex() const { return materialIndex_; }
    [[nodiscard]] bool isValid()       const { return vao_ != 0; }

private:
    uint32_t vao_         = 0;
    uint32_t vbo_         = 0;
    uint32_t ibo_         = 0;
    int      indexCount_  = 0;
    int      materialIndex_ = 0;

    void release();
};

}  // namespace avatar
