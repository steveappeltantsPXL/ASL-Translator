#pragma once
// No GL or OpenCV headers here -- all heavy types are hidden behind pImpl so
// that main.cpp can include this header without conflicting with
// imgui_impl_opengl3_loader.h.
#include <imgui.h>

#include <memory>

namespace capture {

class CameraCapture {
   public:
    CameraCapture();
    ~CameraCapture();

    CameraCapture(const CameraCapture&) = delete;
    CameraCapture& operator=(const CameraCapture&) = delete;

    /// Open the camera device and allocate a GL texture for display.
    /// Must be called after the GL context is current.
    /// Returns false on failure; the object stays in a safe no-op state.
    [[nodiscard]] bool open(int deviceIndex = 0);

    /// Grab one frame from the camera and upload it to the GL texture.
    /// No-op if the camera is not open.  On a failed read the previous
    /// frame stays visible (no flicker).
    void grabFrame();

    /// Returns the GL texture as an ImTextureID for ImGui::Image().
    /// Returns 0 if the camera is not open.
    [[nodiscard]] ImTextureID getTexture() const;

    /// True after a successful open() and before close().
    [[nodiscard]] bool isOpen() const;

    /// Release the camera and delete the GL texture.
    /// Safe to call even if the camera was never opened.
    void close();

   private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace capture
