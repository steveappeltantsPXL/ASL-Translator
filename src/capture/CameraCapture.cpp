#include "capture/CameraCapture.h"

// GL loader -- must NOT include imgui_impl_opengl3.h in the same TU.
#include <GL/glew.h>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>
#include <spdlog/spdlog.h>

namespace capture {

struct CameraCapture::Impl {
    cv::VideoCapture cap;
    cv::Mat frameBGR;
    cv::Mat frameRGB;
    GLuint texId = 0;
    int texW = 0;
    int texH = 0;
    bool open_ = false;
};

CameraCapture::CameraCapture() : impl_(std::make_unique<Impl>()) {}

CameraCapture::~CameraCapture() {
    close();
}

bool CameraCapture::open(int deviceIndex) {
    close();

    // Try DirectShow first (fast cold-start on Windows 11).
#ifdef _WIN32
    if (!impl_->cap.open(deviceIndex, cv::CAP_DSHOW)) {
        spdlog::warn("CameraCapture: CAP_DSHOW failed, falling back to CAP_ANY");
        if (!impl_->cap.open(deviceIndex, cv::CAP_ANY)) {
            spdlog::error("CameraCapture: could not open camera {}", deviceIndex);
            return false;
        }
    }
#else
    if (!impl_->cap.open(deviceIndex, cv::CAP_ANY)) {
        spdlog::error("CameraCapture: could not open camera {}", deviceIndex);
        return false;
    }
#endif

    // Advisory -- the driver may ignore these.
    impl_->cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
    impl_->cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
    impl_->cap.set(cv::CAP_PROP_FPS, 30);

    // Grab one test frame to confirm the device works.
    if (!impl_->cap.read(impl_->frameBGR) || impl_->frameBGR.empty()) {
        spdlog::error("CameraCapture: camera {} opened but first read failed", deviceIndex);
        impl_->cap.release();
        return false;
    }

    cv::cvtColor(impl_->frameBGR, impl_->frameRGB, cv::COLOR_BGR2RGB);
    impl_->texW = impl_->frameRGB.cols;
    impl_->texH = impl_->frameRGB.rows;

    // Allocate the GL texture.
    glGenTextures(1, &impl_->texId);
    glBindTexture(GL_TEXTURE_2D, impl_->texId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_RGB8,
                 impl_->texW,
                 impl_->texH,
                 0,
                 GL_RGB,
                 GL_UNSIGNED_BYTE,
                 impl_->frameRGB.data);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

    glBindTexture(GL_TEXTURE_2D, 0);

    impl_->open_ = true;
    spdlog::info("CameraCapture: opened camera {} ({}x{})", deviceIndex, impl_->texW, impl_->texH);
    return true;
}

void CameraCapture::grabFrame() {
    if (!impl_->open_)
        return;

    if (!impl_->cap.read(impl_->frameBGR) || impl_->frameBGR.empty()) {
        return;  // keep the previous frame visible
    }

    cv::cvtColor(impl_->frameBGR, impl_->frameRGB, cv::COLOR_BGR2RGB);

    glBindTexture(GL_TEXTURE_2D, impl_->texId);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    if (impl_->frameRGB.cols == impl_->texW && impl_->frameRGB.rows == impl_->texH) {
        // Fast path: same resolution -- no realloc.
        glTexSubImage2D(GL_TEXTURE_2D,
                        0,
                        0,
                        0,
                        impl_->texW,
                        impl_->texH,
                        GL_RGB,
                        GL_UNSIGNED_BYTE,
                        impl_->frameRGB.data);
    } else {
        // Resolution changed (rare).
        impl_->texW = impl_->frameRGB.cols;
        impl_->texH = impl_->frameRGB.rows;
        glTexImage2D(GL_TEXTURE_2D,
                     0,
                     GL_RGB8,
                     impl_->texW,
                     impl_->texH,
                     0,
                     GL_RGB,
                     GL_UNSIGNED_BYTE,
                     impl_->frameRGB.data);
    }

    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glBindTexture(GL_TEXTURE_2D, 0);
}

ImTextureID CameraCapture::getTexture() const {
    return static_cast<ImTextureID>(impl_->texId);
}

bool CameraCapture::isOpen() const {
    return impl_->open_;
}

void CameraCapture::close() {
    if (!impl_)
        return;

    if (impl_->cap.isOpened()) {
        impl_->cap.release();
    }
    if (impl_->texId != 0) {
        glDeleteTextures(1, &impl_->texId);
        impl_->texId = 0;
    }
    impl_->texW = 0;
    impl_->texH = 0;
    impl_->open_ = false;
}

}  // namespace capture
