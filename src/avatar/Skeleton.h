#pragma once
#include "GltfLoader.h"
#include <glm/glm.hpp>
#include <vector>

namespace avatar {

class Skeleton {
public:
    explicit Skeleton(std::vector<Joint> joints);

    [[nodiscard]] int jointCount() const { return static_cast<int>(joints_.size()); }

    // Compute final GPU skin matrices from per-joint local transforms.
    // localTransforms: one mat4 per joint (parent space TRS).
    // skinMatrices:    output — globalTransform * inverseBindMatrix per joint.
    void computeSkinMatrices(const std::vector<glm::mat4>& localTransforms,
                             std::vector<glm::mat4>&       skinMatrices) const;

    [[nodiscard]] const std::vector<Joint>& joints() const { return joints_; }

private:
    std::vector<Joint> joints_;  // parent-before-child order guaranteed by GltfLoader
};

}  // namespace avatar
