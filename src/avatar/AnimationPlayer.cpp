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
    blending_ = false;
}

void AnimationPlayer::crossfadeTo(const std::vector<Animation>& animations,
                                   int index, float duration) {
    if (index < 0 || index >= static_cast<int>(animations.size())) return;
    if (index == animIndex_ && !blending_) return;  // already playing this clip

    // Save the outgoing animation state.
    prevAnimIndex_ = animIndex_;
    prevTime_      = currentTime_;

    // Start the new clip.
    animIndex_     = index;
    currentTime_   = 0.f;

    // Begin blend.
    blending_      = true;
    blendAlpha_    = 0.f;
    blendDuration_ = std::max(duration, 0.001f);

    // Pre-allocate prev pose buffer.
    prevJointTRS_.resize(jointCount_);
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

void AnimationPlayer::sampleAnimation(const Animation& anim, float t,
                                       std::vector<JointTRS>& jointTRS) const {
    for (const AnimChannel& ch : anim.channels) {
        if (ch.jointIndex < 0 || ch.jointIndex >= jointCount_) continue;
        JointTRS& trs = jointTRS[ch.jointIndex];
        switch (ch.path) {
            case AnimTargetPath::Translation:
                trs.t = sampleVec3(ch, t);
                break;
            case AnimTargetPath::Rotation:
                trs.r = sampleQuat(ch, t);
                break;
            case AnimTargetPath::Scale:
                trs.s = sampleVec3(ch, t);
                break;
        }
    }
}

void AnimationPlayer::tick(float dt, const std::vector<Animation>& animations,
                           std::vector<JointTRS>& jointTRS) {
    if (animIndex_ < 0 || animIndex_ >= static_cast<int>(animations.size())) return;

    const Animation& anim = animations[animIndex_];
    if (anim.duration <= 0.f || anim.channels.empty()) return;

    // Advance current clip and loop.
    currentTime_ += dt;
    if (currentTime_ > anim.duration)
        currentTime_ = std::fmod(currentTime_, anim.duration);

    if (blending_ && prevAnimIndex_ >= 0 &&
        prevAnimIndex_ < static_cast<int>(animations.size())) {
        // Advance blend factor.
        blendAlpha_ += dt / blendDuration_;
        if (blendAlpha_ >= 1.f) {
            blendAlpha_ = 1.f;
            blending_ = false;
        }

        const Animation& prevAnim = animations[prevAnimIndex_];

        // Advance outgoing clip time and loop.
        prevTime_ += dt;
        if (prevAnim.duration > 0.f && prevTime_ > prevAnim.duration)
            prevTime_ = std::fmod(prevTime_, prevAnim.duration);

        // Sample the outgoing pose.
        prevJointTRS_ = jointTRS;  // start from bind pose
        sampleAnimation(prevAnim, prevTime_, prevJointTRS_);

        // Sample the incoming pose.
        sampleAnimation(anim, currentTime_, jointTRS);

        // Blend: lerp T/S, slerp R.
        float a = blendAlpha_;
        for (int i = 0; i < jointCount_; ++i) {
            jointTRS[i].t = glm::mix(prevJointTRS_[i].t, jointTRS[i].t, a);
            jointTRS[i].r = glm::slerp(prevJointTRS_[i].r, jointTRS[i].r, a);
            jointTRS[i].s = glm::mix(prevJointTRS_[i].s, jointTRS[i].s, a);
        }
    } else {
        // No blending — sample current clip directly.
        sampleAnimation(anim, currentTime_, jointTRS);
    }
}

}  // namespace avatar
