#pragma once
// No GL headers here — all GL types are hidden behind pImpl so that main.cpp
// can include this header without conflicting with imgui_impl_opengl3_loader.h.
#include <imgui.h>
#include <glm/glm.hpp>
#include <memory>
#include <span>
#include <string>

namespace avatar {

class AvatarRenderer {
public:
    AvatarRenderer();
    ~AvatarRenderer();

    AvatarRenderer(const AvatarRenderer&)            = delete;
    AvatarRenderer& operator=(const AvatarRenderer&) = delete;

    // Load model from a GLB file and compile shaders.
    // Must be called after the GL context is current.
    // Returns false on failure; the renderer stays in a safe no-op state.
    bool init(const std::string& glbPath);

    // Render the avatar to an internal FBO.
    // width/height: content area of the ImGui avatar panel in pixels.
    // dt: frame delta time in seconds (used for animation).
    // Call this BEFORE ImGui::NewFrame() each frame.
    void render(float width, float height, float dt);

    // Returns the FBO colour texture as an ImTextureID for ImGui::Image().
    // Returns nullptr if init() has not been called successfully.
    [[nodiscard]] ImTextureID getTexture() const;

    // Override the skeleton pose for one frame (e.g. from MediaPipe landmarks).
    // boneMatrices: final skin matrices, one per joint (globalTransform * invBind).
    // Must have exactly jointCount() entries. Call before render().
    void setPose(std::span<const glm::mat4> boneMatrices);

    // Release all GL resources. Must be called while the GL context is still
    // current, before ImGui_ImplOpenGL3_Shutdown() and SDL_GL_DestroyContext().
    void shutdown();

    [[nodiscard]] int  jointCount() const;
    [[nodiscard]] bool isReady()    const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace avatar
