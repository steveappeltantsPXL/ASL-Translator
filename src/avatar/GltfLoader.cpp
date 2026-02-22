// tinygltf implementation — defined in exactly ONE translation unit.
// Uses vcpkg nlohmann-json instead of the bundled copy.
#define TINYGLTF_NO_INCLUDE_JSON
#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_USE_CPP14

#include <nlohmann/json.hpp>
#include "GltfLoader.h"
#include <tiny_gltf.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <spdlog/spdlog.h>

#include <stdexcept>
#include <unordered_map>
#include <algorithm>
#include <cstring>

namespace avatar {

// ── Helpers ───────────────────────────────────────────────────────────────────

// Read a typed accessor into a vector.
template <typename T>
static std::vector<T> readAccessor(const tinygltf::Model& model, int accessorIdx) {
    const tinygltf::Accessor&   acc  = model.accessors[accessorIdx];
    const tinygltf::BufferView& view = model.bufferViews[acc.bufferView];
    const tinygltf::Buffer&     buf  = model.buffers[view.buffer];

    const uint8_t* base = buf.data.data() + view.byteOffset + acc.byteOffset;
    int componentSize   = tinygltf::GetComponentSizeInBytes(acc.componentType);
    int numComponents   = tinygltf::GetNumComponentsInType(acc.type);
    int elementSize     = componentSize * numComponents;
    int stride = (view.byteStride != 0) ? static_cast<int>(view.byteStride) : elementSize;

    std::vector<T> result(acc.count);
    for (size_t i = 0; i < acc.count; ++i) {
        std::memcpy(&result[i], base + i * stride, sizeof(T));
    }
    return result;
}

// Build local transform from a GLTF node.
static void nodeToTRS(const tinygltf::Node& node,
                      glm::vec3& outT, glm::quat& outR, glm::vec3& outS,
                      glm::mat4& outMat) {
    if (!node.matrix.empty()) {
        outMat = glm::make_mat4(node.matrix.data());
        // Decompose into T/R/S for animation blending.
        // Simple extraction: translation from column 3, scale from column lengths.
        outT = glm::vec3(outMat[3]);
        outS = glm::vec3(glm::length(outMat[0]), glm::length(outMat[1]), glm::length(outMat[2]));
        glm::mat3 rotMat(outMat[0] / outS.x, outMat[1] / outS.y, outMat[2] / outS.z);
        outR = glm::quat_cast(rotMat);
    } else {
        outT = node.translation.empty()
             ? glm::vec3(0.f)
             : glm::vec3(node.translation[0], node.translation[1], node.translation[2]);
        // GLTF quaternion is stored as [x, y, z, w]; glm::quat constructor is (w, x, y, z)
        outR = node.rotation.empty()
             ? glm::quat(1.f, 0.f, 0.f, 0.f)
             : glm::quat(static_cast<float>(node.rotation[3]),
                         static_cast<float>(node.rotation[0]),
                         static_cast<float>(node.rotation[1]),
                         static_cast<float>(node.rotation[2]));
        outS = node.scale.empty()
             ? glm::vec3(1.f)
             : glm::vec3(node.scale[0], node.scale[1], node.scale[2]);

        glm::mat4 T = glm::translate(glm::mat4(1.f), outT);
        glm::mat4 R = glm::mat4_cast(outR);
        glm::mat4 S = glm::scale(glm::mat4(1.f), outS);
        outMat = T * R * S;
    }
}

// ── loadGlb ──────────────────────────────────────────────────────────────────

GltfModel loadGlb(const std::string& path) {
    tinygltf::Model    tgModel;
    tinygltf::TinyGLTF loader;
    std::string        err, warn;

    bool ok = loader.LoadBinaryFromFile(&tgModel, &err, &warn, path);
    if (!warn.empty()) spdlog::warn("GLTF [{}]: {}", path, warn);
    if (!ok) throw std::runtime_error("GLTF load failed [" + path + "]: " + err);

    GltfModel result;

    // ── Materials ─────────────────────────────────────────────────────────────
    for (const auto& mat : tgModel.materials) {
        Material m;
        int texIdx = mat.pbrMetallicRoughness.baseColorTexture.index;
        if (texIdx >= 0) {
            int imgIdx = tgModel.textures[texIdx].source;
            if (imgIdx >= 0) {
                const tinygltf::Image& img = tgModel.images[imgIdx];
                if (!img.image.empty() && img.width > 0 && img.height > 0) {
                    m.hasTexture    = true;
                    m.width         = img.width;
                    m.height        = img.height;
                    m.diffusePixels = img.image;  // already RGBA decoded by tinygltf
                }
            }
        }
        const auto& bc = mat.pbrMetallicRoughness.baseColorFactor;
        if (bc.size() >= 3) m.baseColor = glm::vec3(bc[0], bc[1], bc[2]);
        result.materials.push_back(std::move(m));
    }
    // Ensure at least one material exists so meshes always have a valid index.
    if (result.materials.empty()) {
        Material fallback;
        fallback.baseColor = glm::vec3(0.7f, 0.7f, 0.8f);
        result.materials.push_back(std::move(fallback));
    }

    // ── Skeleton (skin 0) ─────────────────────────────────────────────────────
    // Map from GLTF node index → our joint index.
    std::unordered_map<int, int> nodeToJoint;

    if (!tgModel.skins.empty()) {
        const tinygltf::Skin& skin = tgModel.skins[0];
        int numJoints = static_cast<int>(skin.joints.size());
        result.joints.resize(numJoints);

        // Read inverse bind matrices.
        std::vector<glm::mat4> ibms(numJoints, glm::mat4(1.f));
        if (skin.inverseBindMatrices >= 0) {
            const tinygltf::Accessor& ibmAcc = tgModel.accessors[skin.inverseBindMatrices];
            const tinygltf::BufferView& ibmView = tgModel.bufferViews[ibmAcc.bufferView];
            const tinygltf::Buffer&     ibmBuf  = tgModel.buffers[ibmView.buffer];
            const uint8_t* base = ibmBuf.data.data() + ibmView.byteOffset + ibmAcc.byteOffset;
            for (int i = 0; i < numJoints; ++i) {
                std::memcpy(glm::value_ptr(ibms[i]), base + i * 64, 64);
            }
        }

        // Build nodeToJoint map and populate joint fields.
        for (int i = 0; i < numJoints; ++i) {
            int nodeIdx = skin.joints[i];
            nodeToJoint[nodeIdx] = i;
            result.joints[i].nodeIndex         = nodeIdx;
            result.joints[i].parentJointIndex  = -1;
            result.joints[i].inverseBindMatrix = ibms[i];

            const tinygltf::Node& node = tgModel.nodes[nodeIdx];
            nodeToTRS(node,
                      result.joints[i].bindT,
                      result.joints[i].bindR,
                      result.joints[i].bindS,
                      result.joints[i].localTransform);
        }

        // Resolve parent indices.
        for (int i = 0; i < numJoints; ++i) {
            int nodeIdx = skin.joints[i];
            for (int child : tgModel.nodes[nodeIdx].children) {
                auto it = nodeToJoint.find(child);
                if (it != nodeToJoint.end()) {
                    result.joints[it->second].parentJointIndex = i;
                }
            }
        }
    }

    // ── Mesh primitives ───────────────────────────────────────────────────────
    for (const tinygltf::Mesh& mesh : tgModel.meshes) {
        for (const tinygltf::Primitive& prim : mesh.primitives) {
            if (prim.mode != TINYGLTF_MODE_TRIANGLES) continue;

            MeshPrimitive mp;
            mp.materialIndex = (prim.material >= 0)
                ? std::min(prim.material, static_cast<int>(result.materials.size()) - 1)
                : 0;

            // ── Positions ──────────────────────────────────────────────────────
            auto posIt = prim.attributes.find("POSITION");
            if (posIt == prim.attributes.end()) continue;
            auto positions = readAccessor<glm::vec3>(tgModel, posIt->second);
            size_t vertCount = positions.size();
            mp.vertices.resize(vertCount);
            for (size_t i = 0; i < vertCount; ++i) mp.vertices[i].position = positions[i];

            // ── Normals ────────────────────────────────────────────────────────
            auto normIt = prim.attributes.find("NORMAL");
            if (normIt != prim.attributes.end()) {
                auto normals = readAccessor<glm::vec3>(tgModel, normIt->second);
                for (size_t i = 0; i < std::min(vertCount, normals.size()); ++i)
                    mp.vertices[i].normal = normals[i];
            }

            // ── Texture coordinates ────────────────────────────────────────────
            auto uvIt = prim.attributes.find("TEXCOORD_0");
            if (uvIt != prim.attributes.end()) {
                auto uvs = readAccessor<glm::vec2>(tgModel, uvIt->second);
                for (size_t i = 0; i < std::min(vertCount, uvs.size()); ++i)
                    mp.vertices[i].texCoord = uvs[i];
            }

            // ── Bone IDs (JOINTS_0) ────────────────────────────────────────────
            auto jointIt = prim.attributes.find("JOINTS_0");
            if (jointIt != prim.attributes.end()) {
                const tinygltf::Accessor& jAcc = tgModel.accessors[jointIt->second];
                const tinygltf::BufferView& jView = tgModel.bufferViews[jAcc.bufferView];
                const tinygltf::Buffer& jBuf = tgModel.buffers[jView.buffer];

                int componentSize = tinygltf::GetComponentSizeInBytes(jAcc.componentType);
                int stride = (jView.byteStride != 0) ? static_cast<int>(jView.byteStride)
                                                     : componentSize * 4;
                const uint8_t* base = jBuf.data.data() + jView.byteOffset + jAcc.byteOffset;

                for (size_t i = 0; i < std::min(vertCount, jAcc.count); ++i) {
                    const uint8_t* p = base + i * stride;
                    glm::ivec4 ids(0);
                    if (jAcc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
                        ids = glm::ivec4(p[0], p[1], p[2], p[3]);
                    } else {
                        // UNSIGNED_SHORT
                        const uint16_t* ps = reinterpret_cast<const uint16_t*>(p);
                        ids = glm::ivec4(ps[0], ps[1], ps[2], ps[3]);
                    }
                    mp.vertices[i].boneIDs = ids;
                }
            }

            // ── Bone weights (WEIGHTS_0) ───────────────────────────────────────
            auto weightIt = prim.attributes.find("WEIGHTS_0");
            if (weightIt != prim.attributes.end()) {
                auto weights = readAccessor<glm::vec4>(tgModel, weightIt->second);
                for (size_t i = 0; i < std::min(vertCount, weights.size()); ++i)
                    mp.vertices[i].boneWeights = weights[i];
            }

            // ── Indices ────────────────────────────────────────────────────────
            if (prim.indices >= 0) {
                const tinygltf::Accessor&   iAcc  = tgModel.accessors[prim.indices];
                const tinygltf::BufferView& iView = tgModel.bufferViews[iAcc.bufferView];
                const tinygltf::Buffer&     iBuf  = tgModel.buffers[iView.buffer];
                const uint8_t* base = iBuf.data.data() + iView.byteOffset + iAcc.byteOffset;

                mp.indices.resize(iAcc.count);
                for (size_t i = 0; i < iAcc.count; ++i) {
                    switch (iAcc.componentType) {
                        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: {
                            uint32_t v; std::memcpy(&v, base + i * 4, 4);
                            mp.indices[i] = v;
                            break;
                        }
                        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
                            uint16_t v; std::memcpy(&v, base + i * 2, 2);
                            mp.indices[i] = v;
                            break;
                        }
                        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: {
                            mp.indices[i] = base[i];
                            break;
                        }
                        default:
                            mp.indices[i] = 0;
                            break;
                    }
                }
            }

            if (!mp.indices.empty()) {
                result.primitives.push_back(std::move(mp));
            }
        }
    }

    // ── Animations ────────────────────────────────────────────────────────────
    for (const tinygltf::Animation& anim : tgModel.animations) {
        Animation a;
        a.name = anim.name;

        for (const tinygltf::AnimationChannel& ch : anim.channels) {
            if (ch.target_node < 0) continue;
            auto it = nodeToJoint.find(ch.target_node);
            if (it == nodeToJoint.end()) continue;

            const tinygltf::AnimationSampler& sampler = anim.samplers[ch.sampler];

            AnimChannel ac;
            ac.jointIndex = it->second;
            ac.times = readAccessor<float>(tgModel, sampler.input);

            // Track animation duration.
            if (!ac.times.empty()) {
                a.duration = std::max(a.duration, ac.times.back());
            }

            if (ch.target_path == "translation") {
                ac.path = AnimTargetPath::Translation;
                ac.valuesVec3 = readAccessor<glm::vec3>(tgModel, sampler.output);
            } else if (ch.target_path == "rotation") {
                ac.path = AnimTargetPath::Rotation;
                // GLTF stores quaternions as vec4 (xyzw); convert to glm::quat (w,x,y,z).
                auto raw = readAccessor<glm::vec4>(tgModel, sampler.output);
                ac.valuesQuat.reserve(raw.size());
                for (const auto& v : raw)
                    ac.valuesQuat.emplace_back(v.w, v.x, v.y, v.z);
            } else if (ch.target_path == "scale") {
                ac.path = AnimTargetPath::Scale;
                ac.valuesVec3 = readAccessor<glm::vec3>(tgModel, sampler.output);
            } else {
                continue;
            }

            a.channels.push_back(std::move(ac));
        }

        if (!a.channels.empty()) {
            result.animations.push_back(std::move(a));
        }
    }

    spdlog::info("GLTF loaded: {} primitives, {} joints, {} animations",
                 result.primitives.size(), result.joints.size(), result.animations.size());

    return result;
}

}  // namespace avatar
