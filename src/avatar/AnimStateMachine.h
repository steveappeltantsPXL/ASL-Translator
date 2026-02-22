#pragma once
#include "AnimationPlayer.h"
#include "GltfLoader.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace avatar {

// Lightweight animation controller that wraps AnimationPlayer.
// Maps animation names to indices and manages crossfade transitions.
// No GL includes — safe to use from any translation unit.
class AnimStateMachine {
public:
    AnimStateMachine() = default;

    // Register all animations from a loaded model.
    // Must be called before requestAnim() or update().
    void registerAnimations(const std::vector<Animation>& animations);

    // Request a transition to the animation at the given index.
    // Triggers a crossfade from the current clip. No-op if already playing.
    void requestAnim(int index);

    // Request a transition by name. No-op if the name is not registered.
    void requestAnim(const std::string& name);

    // Advance time and drive the AnimationPlayer. Call once per frame.
    // jointTRS should be pre-populated with bind-pose values.
    void update(float dt, AnimationPlayer& player,
                const std::vector<Animation>& animations,
                std::vector<JointTRS>& jointTRS);

    [[nodiscard]] int currentIndex() const { return currentIndex_; }
    [[nodiscard]] const std::string& currentName() const;
    [[nodiscard]] int animationCount() const { return static_cast<int>(names_.size()); }
    [[nodiscard]] const std::string& animationName(int i) const;

    // Crossfade duration in seconds (default 0.3).
    void setCrossfadeDuration(float d) { crossfadeDuration_ = d; }

private:
    std::vector<std::string> names_;
    std::unordered_map<std::string, int> nameToIndex_;
    int   currentIndex_ = -1;
    int   pendingIndex_ = -1;
    float crossfadeDuration_ = 0.3f;

    static const std::string kEmptyName;
};

}  // namespace avatar
