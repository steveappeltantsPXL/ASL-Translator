#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace avatar {

// ── Vertex layout (matches shader attribute locations 0-4) ────────────────────
struct Vertex {
    glm::vec3 position;      // location 0
    glm::vec3 normal;        // location 1
    glm::vec2 texCoord;      // location 2
    glm::ivec4 boneIDs;      // location 3 — up to 4 bone influences
    glm::vec4 boneWeights;   // location 4
};

// ── One renderable sub-mesh ───────────────────────────────────────────────────
struct MeshPrimitive {
    std::vector<Vertex>   vertices;
    std::vector<uint32_t> indices;
    int                   materialIndex = -1;
};

// ── Material (diffuse only for Tier 2) ───────────────────────────────────────
struct Material {
    std::vector<uint8_t> diffusePixels;  // decoded RGBA, width*height*4 bytes
    int                  width      = 0;
    int                  height     = 0;
    bool                 hasTexture = false;
    glm::vec3            baseColor{0.8f, 0.8f, 0.8f};
};

// ── Skeleton joint ────────────────────────────────────────────────────────────
struct Joint {
    int       nodeIndex;            // corresponding tinygltf node index
    int       parentJointIndex;     // -1 for root joints
    glm::mat4 inverseBindMatrix{1.f};
    glm::mat4 localTransform{1.f};  // bind-pose local TRS
    glm::vec3 bindT{0.f};
    glm::quat bindR{1.f, 0.f, 0.f, 0.f};
    glm::vec3 bindS{1.f};
};

// ── Animation ─────────────────────────────────────────────────────────────────
enum class AnimTargetPath { Translation, Rotation, Scale };

struct AnimChannel {
    int            jointIndex;  // into GltfModel::joints
    AnimTargetPath path;
    std::vector<float>     times;
    std::vector<glm::vec3> valuesVec3;  // Translation or Scale
    std::vector<glm::quat> valuesQuat;  // Rotation
};

struct Animation {
    std::string              name;
    float                    duration = 0.f;
    std::vector<AnimChannel> channels;
};

// ── Complete loaded model ─────────────────────────────────────────────────────
struct GltfModel {
    std::vector<MeshPrimitive> primitives;
    std::vector<Material>      materials;
    std::vector<Joint>         joints;      // parent-before-child order
    std::vector<Animation>     animations;
};

// ── Loader ───────────────────────────────────────────────────────────────────
// Loads a GLB (binary GLTF 2.0) file.
// Throws std::runtime_error on failure.
[[nodiscard]] GltfModel loadGlb(const std::string& path);

}  // namespace avatar
