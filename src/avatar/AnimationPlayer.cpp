#include "AnimationPlayer.h"
#include <glm/gtc/quaternion.hpp>
#include <algorithm>
#include <cmath>

namespace avatar {

AnimationPlayer::AnimationPlayer(int jointCount) : jointCount_(jointCount) {}

void AnimationPlayer::setAnimation(const std::vector<Animation>& animations, int index) {
    if (index < 0 || index >= static_cast<int>(animations.size())) {
        animIndex_ = -1;
    } else {
        animIndex_   = index;
        currentTime_ = 0.f;
    }
}

int AnimationPlayer::findKeyframeIndex(const std::vector<float>& times, float t) {
    if (times.size() == 1) return 0;
    // Upper-bound search then back one.
    auto it = std::upper_bound(times.begin(), times.end(), t);
    if (it == times.begin()) return 0;
    --it;
    // Clamp to second-to-last so we always have a valid next frame.
    int idx = static_cast<int>(it - times.begin());
    return std::min(idx, static_cast<int>(times.size()) - 2);
}

glm::vec3 AnimationPlayer::sampleVec3(const AnimChannel& ch, float t) {
    if (ch.valuesVec3.empty()) return glm::vec3(0.f);
    if (ch.valuesVec3.size() == 1) return ch.valuesVec3[0];

    int i = findKeyframeIndex(ch.times, t);
    int j = i + 1;
    float span  = ch.times[j] - ch.times[i];
    float alpha = (span > 1e-6f) ? (t - ch.times[i]) / span : 0.f;
    alpha = std::clamp(alpha, 0.f, 1.f);
    return glm::mix(ch.valuesVec3[i], ch.valuesVec3[j], alpha);
}

glm::quat AnimationPlayer::sampleQuat(const AnimChannel& ch, float t) {
    if (ch.valuesQuat.empty()) return glm::quat(1.f, 0.f, 0.f, 0.f);
    if (ch.valuesQuat.size() == 1) return ch.valuesQuat[0];

    int i = findKeyframeIndex(ch.times, t);
    int j = i + 1;
    float span  = ch.times[j] - ch.times[i];
    float alpha = (span > 1e-6f) ? (t - ch.times[i]) / span : 0.f;
    alpha = std::clamp(alpha, 0.f, 1.f);
    return glm::slerp(ch.valuesQuat[i], ch.valuesQuat[j], alpha);
}

void AnimationPlayer::tick(float dt, const std::vector<Animation>& animations,
                           std::vector<JointTRS>& jointTRS) {
    if (animIndex_ < 0 || animIndex_ >= static_cast<int>(animations.size())) return;

    const Animation& anim = animations[animIndex_];
    if (anim.duration <= 0.f || anim.channels.empty()) return;

    // Advance and loop.
    currentTime_ += dt;
    if (currentTime_ > anim.duration)
        currentTime_ = std::fmod(currentTime_, anim.duration);

    // Apply each channel — overwrites the relevant TRS component for that joint.
    for (const AnimChannel& ch : anim.channels) {
        if (ch.jointIndex < 0 || ch.jointIndex >= jointCount_) continue;
        JointTRS& trs = jointTRS[ch.jointIndex];
        switch (ch.path) {
            case AnimTargetPath::Translation:
                trs.t = sampleVec3(ch, currentTime_);
                break;
            case AnimTargetPath::Rotation:
                trs.r = sampleQuat(ch, currentTime_);
                break;
            case AnimTargetPath::Scale:
                trs.s = sampleVec3(ch, currentTime_);
                break;
        }
    }
}

}  // namespace avatar
