#include "Skeleton.h"
#include <cassert>

namespace avatar {

Skeleton::Skeleton(std::vector<Joint> joints) : joints_(std::move(joints)) {}

void Skeleton::computeSkinMatrices(const std::vector<glm::mat4>& localTransforms,
                                   std::vector<glm::mat4>&       skinMatrices) const {
    const int n = static_cast<int>(joints_.size());
    assert(static_cast<int>(localTransforms.size()) == n);
    assert(static_cast<int>(skinMatrices.size()) == n);

    // Joints are stored parent-before-child (guaranteed by GltfLoader depth-first traversal).
    // A single forward pass computes global transforms correctly.
    std::vector<glm::mat4> globalTransforms(n);
    for (int i = 0; i < n; ++i) {
        const int parent = joints_[i].parentJointIndex;
        if (parent < 0) {
            globalTransforms[i] = localTransforms[i];
        } else {
            globalTransforms[i] = globalTransforms[parent] * localTransforms[i];
        }
        skinMatrices[i] = globalTransforms[i] * joints_[i].inverseBindMatrix;
    }
}

}  // namespace avatar
