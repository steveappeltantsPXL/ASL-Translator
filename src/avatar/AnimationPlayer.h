#pragma once
#include "GltfLoader.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>

namespace avatar {

// Per-joint decomposed TRS — kept separate for animation blending.
struct JointTRS {
    glm::vec3 t{0.f};
    glm::quat r{1.f, 0.f, 0.f, 0.f};  // identity: w=1, xyz=0
    glm::vec3 s{1.f};
};

class AnimationPlayer {
public:
    explicit AnimationPlayer(int jointCount);

    // Select animation by index into the animations vector. -1 disables playback.
    void setAnimation(const std::vector<Animation>& animations, int index);

    // Advance time by dt and write per-joint local transforms into localTransforms.
    // localTransforms is pre-populated with bind-pose values; channels overwrite
    // only the joints they affect.
    void tick(float dt, const std::vector<Animation>& animations,
              std::vector<JointTRS>& jointTRS);

    [[nodiscard]] float currentTime() const { return currentTime_; }

    // Sample helpers (public so AvatarRenderer can reuse if needed).
    [[nodiscard]] static glm::vec3 sampleVec3(const AnimChannel& ch, float t);
    [[nodiscard]] static glm::quat sampleQuat(const AnimChannel& ch, float t);

private:
    int   jointCount_;
    int   animIndex_ = -1;
    float currentTime_ = 0.f;

    static int findKeyframeIndex(const std::vector<float>& times, float t);
};

}  // namespace avatar
