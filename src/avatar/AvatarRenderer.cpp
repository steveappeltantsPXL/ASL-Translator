#include "AvatarRenderer.h"
#include "AvatarShaders.h"
#include "GltfLoader.h"
#include "SkinnedMesh.h"
#include "Skeleton.h"
#include "AnimationPlayer.h"
#include "AnimStateMachine.h"

#include <GL/glew.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

namespace avatar {

// ── pImpl definition ──────────────────────────────────────────────────────────

struct GpuMaterial {
    GLuint    texId     = 0;
    bool      hasTexture = false;
    glm::vec3 baseColor{0.8f, 0.8f, 0.8f};
};

struct AvatarRenderer::Impl {
    bool ready = false;

    // Shader
    GLuint program = 0;
    GLint  loc_Model{-1}, loc_View{-1}, loc_Projection{-1};
    GLint  loc_BoneMatrices{-1};
    GLint  loc_DiffuseTex{-1}, loc_HasTexture{-1}, loc_BaseColor{-1};
    GLint  loc_LightDir{-1}, loc_LightColor{-1}, loc_AmbientColor{-1}, loc_CameraPos{-1};

    // FBO
    GLuint fbo       = 0;
    GLuint colorTex  = 0;
    GLuint depthRbo  = 0;
    int    fboW      = 0;
    int    fboH      = 0;

    // Geometry + materials
    std::vector<SkinnedMesh> meshes;
    std::vector<GpuMaterial> gpuMats;

    // Animation
    std::unique_ptr<Skeleton>        skeleton;
    std::unique_ptr<AnimationPlayer> animPlayer;
    AnimStateMachine                 animSM;
    std::vector<Animation>           animations;   // kept alive
    std::vector<JointTRS>            bindPoseTRS;  // reset source each frame
    std::vector<JointTRS>            jointTRS;     // working TRS per frame
    std::vector<glm::mat4>           localTransforms;
    std::vector<glm::mat4>           skinMatrices;

    // Camera auto-framing (computed from mesh AABB)
    glm::vec3 camTarget_{0.f, 1.f, 0.f};
    float     camDistance_ = 3.f;

    // Background color
    glm::vec3 bgColor_{0.12f, 0.12f, 0.12f};

    // External pose override from MediaPipe
    bool                  hasPoseOverride = false;
    std::vector<glm::mat4> poseOverride;

    // Helpers
    [[nodiscard]] GLuint compileShader(GLenum type, const char* src) const;
    [[nodiscard]] GLuint linkProgram(GLuint vert, GLuint frag) const;
    void createFBO(int w, int h);
    void destroyFBO();
    void destroyAll();
};

// ── Shader compilation ────────────────────────────────────────────────────────

GLuint AvatarRenderer::Impl::compileShader(GLenum type, const char* src) const {
    GLuint id = glCreateShader(type);
    glShaderSource(id, 1, &src, nullptr);
    glCompileShader(id);
    GLint ok = 0;
    glGetShaderiv(id, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(id, 512, nullptr, log);
        spdlog::error("Avatar shader compile error: {}", log);
        glDeleteShader(id);
        return 0;
    }
    return id;
}

GLuint AvatarRenderer::Impl::linkProgram(GLuint vert, GLuint frag) const {
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vert);
    glAttachShader(prog, frag);
    glLinkProgram(prog);
    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(prog, 512, nullptr, log);
        spdlog::error("Avatar shader link error: {}", log);
        glDeleteProgram(prog);
        return 0;
    }
    return prog;
}

// ── FBO management ────────────────────────────────────────────────────────────

void AvatarRenderer::Impl::createFBO(int w, int h) {
    destroyFBO();
    fboW = w; fboH = h;

    glGenTextures(1, &colorTex);
    glBindTexture(GL_TEXTURE_2D, colorTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenRenderbuffers(1, &depthRbo);
    glBindRenderbuffer(GL_RENDERBUFFER, depthRbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTex, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRbo);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        spdlog::error("Avatar FBO incomplete: 0x{:X}", static_cast<unsigned>(status));
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void AvatarRenderer::Impl::destroyFBO() {
    if (fbo)      { glDeleteFramebuffers(1,  &fbo);      fbo = 0; }
    if (colorTex) { glDeleteTextures(1,      &colorTex); colorTex = 0; }
    if (depthRbo) { glDeleteRenderbuffers(1, &depthRbo); depthRbo = 0; }
    fboW = fboH = 0;
}

void AvatarRenderer::Impl::destroyAll() {
    meshes.clear();
    for (auto& mat : gpuMats) {
        if (mat.texId) { glDeleteTextures(1, &mat.texId); }
    }
    gpuMats.clear();
    if (program) { glDeleteProgram(program); program = 0; }
    destroyFBO();
    ready = false;
}

// ── AvatarRenderer public API ─────────────────────────────────────────────────

AvatarRenderer::AvatarRenderer() : impl_(std::make_unique<Impl>()) {}
AvatarRenderer::~AvatarRenderer() { shutdown(); }

bool AvatarRenderer::init(const std::string& glbPath) {
    Impl& I = *impl_;

    // Initialise GLEW (idempotent — safe to call multiple times on the same context).
    glewExperimental = GL_TRUE;
    GLenum glewErr = glewInit();
    if (glewErr != GLEW_OK && glewErr != GLEW_ERROR_NO_GLX_DISPLAY) {
        spdlog::error("GLEW init failed: {}", reinterpret_cast<const char*>(glewGetErrorString(glewErr)));
        return false;
    }

    // Compile shaders.
    GLuint vert = I.compileShader(GL_VERTEX_SHADER,   kVertexShaderSrc);
    GLuint frag = I.compileShader(GL_FRAGMENT_SHADER, kFragmentShaderSrc);
    if (!vert || !frag) { glDeleteShader(vert); glDeleteShader(frag); return false; }
    I.program = I.linkProgram(vert, frag);
    glDeleteShader(vert);
    glDeleteShader(frag);
    if (!I.program) return false;

    // Cache uniform locations.
    I.loc_Model        = glGetUniformLocation(I.program, "u_Model");
    I.loc_View         = glGetUniformLocation(I.program, "u_View");
    I.loc_Projection   = glGetUniformLocation(I.program, "u_Projection");
    I.loc_BoneMatrices = glGetUniformLocation(I.program, "u_BoneMatrices");
    I.loc_DiffuseTex   = glGetUniformLocation(I.program, "u_DiffuseTex");
    I.loc_HasTexture   = glGetUniformLocation(I.program, "u_HasTexture");
    I.loc_BaseColor    = glGetUniformLocation(I.program, "u_BaseColor");
    I.loc_LightDir     = glGetUniformLocation(I.program, "u_LightDir");
    I.loc_LightColor   = glGetUniformLocation(I.program, "u_LightColor");
    I.loc_AmbientColor = glGetUniformLocation(I.program, "u_AmbientColor");
    I.loc_CameraPos    = glGetUniformLocation(I.program, "u_CameraPos");

    // Load model.
    GltfModel model;
    try {
        model = loadGlb(glbPath);
    } catch (const std::exception& ex) {
        spdlog::error("Avatar model load failed: {}", ex.what());
        I.destroyAll();
        return false;
    }

    // Upload meshes.
    I.meshes.reserve(model.primitives.size());
    for (auto& prim : model.primitives) {
        SkinnedMesh sm;
        sm.upload(prim);
        I.meshes.push_back(std::move(sm));
    }

    // Upload material textures.
    I.gpuMats.resize(model.materials.size());
    for (size_t mi = 0; mi < model.materials.size(); ++mi) {
        const Material& mat = model.materials[mi];
        GpuMaterial&    gm  = I.gpuMats[mi];
        gm.baseColor   = mat.baseColor;
        gm.hasTexture  = mat.hasTexture;
        if (mat.hasTexture) {
            glGenTextures(1, &gm.texId);
            glBindTexture(GL_TEXTURE_2D, gm.texId);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                         mat.width, mat.height, 0,
                         GL_RGBA, GL_UNSIGNED_BYTE,
                         mat.diffusePixels.data());
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glGenerateMipmap(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
    }

    // Skeleton.
    int numJoints = static_cast<int>(model.joints.size());
    if (numJoints == 0) {
        // Model has no skin — create a single dummy joint so the shader has a valid matrix.
        Joint dummy;
        dummy.nodeIndex        = 0;
        dummy.parentJointIndex = -1;
        model.joints.push_back(dummy);
        numJoints = 1;
    }
    I.skeleton = std::make_unique<Skeleton>(std::move(model.joints));

    // Build bind-pose TRS array.
    I.bindPoseTRS.resize(numJoints);
    for (int i = 0; i < numJoints; ++i) {
        I.bindPoseTRS[i].t = I.skeleton->joints()[i].bindT;
        I.bindPoseTRS[i].r = I.skeleton->joints()[i].bindR;
        I.bindPoseTRS[i].s = I.skeleton->joints()[i].bindS;
    }
    I.jointTRS       = I.bindPoseTRS;
    I.localTransforms.assign(numJoints, glm::mat4(1.f));
    I.skinMatrices.assign(numJoints, glm::mat4(1.f));

    // Animation.
    I.animations  = std::move(model.animations);
    I.animPlayer  = std::make_unique<AnimationPlayer>(numJoints);
    I.animSM.registerAnimations(I.animations);
    if (!I.animations.empty()) {
        I.animPlayer->setAnimation(I.animations, 0);
        spdlog::info("Avatar: {} animation(s), playing '{}'",
                     I.animations.size(), I.animations[0].name);
    }

    // Compute AABB from all vertex positions for camera auto-framing.
    {
        glm::vec3 aabbMin(std::numeric_limits<float>::max());
        glm::vec3 aabbMax(std::numeric_limits<float>::lowest());
        bool hasVerts = false;
        for (const auto& prim : model.primitives) {
            for (const auto& v : prim.vertices) {
                aabbMin = glm::min(aabbMin, v.position);
                aabbMax = glm::max(aabbMax, v.position);
                hasVerts = true;
            }
        }
        if (hasVerts) {
            I.camTarget_   = (aabbMin + aabbMax) * 0.5f;
            float extent   = glm::length(aabbMax - aabbMin);
            I.camDistance_  = extent * 0.85f;
            spdlog::info("Avatar AABB: ({:.1f},{:.1f},{:.1f})-({:.1f},{:.1f},{:.1f}), "
                         "cam distance={:.2f}",
                         aabbMin.x, aabbMin.y, aabbMin.z,
                         aabbMax.x, aabbMax.y, aabbMax.z,
                         I.camDistance_);
        }
    }

    // Create a minimal FBO (resized on first render call).
    I.createFBO(1, 1);

    I.ready = true;
    spdlog::info("AvatarRenderer ready — {} meshes, {} joints",
                 I.meshes.size(), numJoints);
    return true;
}

void AvatarRenderer::render(float width, float height, float dt) {
    Impl& I = *impl_;
    if (!I.ready) return;

    // Resize FBO if panel dimensions changed.
    int w = std::max(1, static_cast<int>(width));
    int h = std::max(1, static_cast<int>(height));
    if (w != I.fboW || h != I.fboH) I.createFBO(w, h);

    // ── Render to FBO ─────────────────────────────────────────────────────────
    glBindFramebuffer(GL_FRAMEBUFFER, I.fbo);
    glViewport(0, 0, I.fboW, I.fboH);
    glClearColor(I.bgColor_.r, I.bgColor_.g, I.bgColor_.b, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    // ── Camera (auto-framed from mesh AABB) ────────────────────────────────
    float aspect  = static_cast<float>(I.fboW) / static_cast<float>(I.fboH);
    float farPlane = std::max(100.f, I.camDistance_ * 10.f);
    glm::mat4 proj = glm::perspective(glm::radians(45.f), aspect, 0.01f, farPlane);
    glm::vec3 camPos = I.camTarget_ + glm::vec3{0.f, 0.f, I.camDistance_};
    glm::mat4 view = glm::lookAt(camPos, I.camTarget_, glm::vec3{0.f, 1.f, 0.f});
    glm::mat4 model(1.f);

    // ── Update skeleton pose ──────────────────────────────────────────────────
    int n = I.skeleton->jointCount();
    if (I.hasPoseOverride) {
        // Direct skin matrices from MediaPipe — skip animation.
        I.skinMatrices = I.poseOverride;
        I.hasPoseOverride = false;
    } else {
        // Reset to bind pose, then apply animation via state machine.
        I.jointTRS = I.bindPoseTRS;
        I.animSM.update(dt, *I.animPlayer, I.animations, I.jointTRS);

        // Compose TRS → local transform matrices.
        for (int i = 0; i < n; ++i) {
            const JointTRS& trs = I.jointTRS[i];
            I.localTransforms[i] =
                glm::translate(glm::mat4(1.f), trs.t) *
                glm::mat4_cast(trs.r) *
                glm::scale(glm::mat4(1.f), trs.s);
        }
        I.skeleton->computeSkinMatrices(I.localTransforms, I.skinMatrices);
    }

    // ── Set shader uniforms ───────────────────────────────────────────────────
    glUseProgram(I.program);
    glUniformMatrix4fv(I.loc_Model,      1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(I.loc_View,       1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(I.loc_Projection, 1, GL_FALSE, glm::value_ptr(proj));
    glUniformMatrix4fv(I.loc_BoneMatrices,
                       static_cast<GLsizei>(I.skinMatrices.size()),
                       GL_FALSE,
                       glm::value_ptr(I.skinMatrices[0]));

    // Light: slightly above and to the right, in world space.
    glm::vec3 lightDir = glm::normalize(glm::vec3{1.f, 2.f, 2.f});
    glUniform3fv(I.loc_LightDir,     1, glm::value_ptr(lightDir));
    glUniform3fv(I.loc_LightColor,   1, glm::value_ptr(glm::vec3{1.f, 1.f, 1.f}));
    glUniform3fv(I.loc_AmbientColor, 1, glm::value_ptr(glm::vec3{0.2f, 0.2f, 0.25f}));
    glUniform3fv(I.loc_CameraPos,    1, glm::value_ptr(camPos));

    // ── Draw each mesh ────────────────────────────────────────────────────────
    for (const SkinnedMesh& sm : I.meshes) {
        if (!sm.isValid()) continue;
        int mi = std::clamp(sm.materialIndex(), 0, static_cast<int>(I.gpuMats.size()) - 1);
        const GpuMaterial& gm = I.gpuMats[mi];

        glUniform1i(I.loc_HasTexture, gm.hasTexture ? 1 : 0);
        if (gm.hasTexture) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, gm.texId);
            glUniform1i(I.loc_DiffuseTex, 0);
        } else {
            glUniform3fv(I.loc_BaseColor, 1, glm::value_ptr(gm.baseColor));
        }
        sm.draw();
    }

    // ── Restore default state for ImGui ───────────────────────────────────────
    glUseProgram(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

ImTextureID AvatarRenderer::getTexture() const {
    if (!impl_->ready || impl_->colorTex == 0) return static_cast<ImTextureID>(0);
    return static_cast<ImTextureID>(impl_->colorTex);
}

void AvatarRenderer::setPose(std::span<const glm::mat4> boneMatrices) {
    Impl& I = *impl_;
    if (!I.ready) return;
    int expected = I.skeleton->jointCount();
    if (static_cast<int>(boneMatrices.size()) != expected) {
        spdlog::warn("AvatarRenderer::setPose: expected {} matrices, got {}",
                     expected, boneMatrices.size());
        return;
    }
    I.poseOverride.assign(boneMatrices.begin(), boneMatrices.end());
    I.hasPoseOverride = true;
}

void AvatarRenderer::shutdown() {
    if (impl_) impl_->destroyAll();
}

int AvatarRenderer::jointCount() const {
    return impl_ && impl_->skeleton ? impl_->skeleton->jointCount() : 0;
}

bool AvatarRenderer::isReady() const {
    return impl_ && impl_->ready;
}

int AvatarRenderer::animationCount() const {
    return impl_ ? impl_->animSM.animationCount() : 0;
}

std::string AvatarRenderer::animationName(int index) const {
    return impl_ ? impl_->animSM.animationName(index) : std::string{};
}

void AvatarRenderer::selectAnimation(int index) {
    if (impl_ && impl_->ready) {
        impl_->animSM.requestAnim(index);
    }
}

void AvatarRenderer::setBackgroundColor(const glm::vec3& color) {
    if (impl_) impl_->bgColor_ = color;
}

}  // namespace avatar
