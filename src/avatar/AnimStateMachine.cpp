#include "AnimStateMachine.h"

namespace avatar {

const std::string AnimStateMachine::kEmptyName;

void AnimStateMachine::registerAnimations(const std::vector<Animation>& animations) {
    names_.clear();
    nameToIndex_.clear();
    names_.reserve(animations.size());
    for (int i = 0; i < static_cast<int>(animations.size()); ++i) {
        const std::string& name = animations[i].name.empty()
            ? ("Animation " + std::to_string(i))
            : animations[i].name;
        names_.push_back(name);
        nameToIndex_[name] = i;
    }
    // If we have animations but nothing is selected, start at 0.
    if (!animations.empty() && currentIndex_ < 0) {
        currentIndex_ = 0;
    }
}

void AnimStateMachine::requestAnim(int index) {
    if (index < 0 || index >= static_cast<int>(names_.size())) return;
    if (index == currentIndex_) return;
    pendingIndex_ = index;
}

void AnimStateMachine::requestAnim(const std::string& name) {
    auto it = nameToIndex_.find(name);
    if (it != nameToIndex_.end()) {
        requestAnim(it->second);
    }
}

void AnimStateMachine::update(float dt, AnimationPlayer& player,
                               const std::vector<Animation>& animations,
                               std::vector<JointTRS>& jointTRS) {
    // Process pending transition.
    if (pendingIndex_ >= 0 && pendingIndex_ != currentIndex_) {
        player.crossfadeTo(animations, pendingIndex_, crossfadeDuration_);
        currentIndex_ = pendingIndex_;
        pendingIndex_ = -1;
    }

    player.tick(dt, animations, jointTRS);
}

const std::string& AnimStateMachine::currentName() const {
    if (currentIndex_ >= 0 && currentIndex_ < static_cast<int>(names_.size()))
        return names_[currentIndex_];
    return kEmptyName;
}

const std::string& AnimStateMachine::animationName(int i) const {
    if (i >= 0 && i < static_cast<int>(names_.size()))
        return names_[i];
    return kEmptyName;
}

}  // namespace avatar
