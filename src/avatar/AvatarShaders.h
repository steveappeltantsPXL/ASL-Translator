#pragma once

namespace avatar {

// ── Vertex shader ─────────────────────────────────────────────────────────────
// Locations:
//   0 = position (vec3)
//   1 = normal   (vec3)
//   2 = texCoord (vec2)
//   3 = boneIDs  (ivec4) — must be uploaded with glVertexAttribIPointer
//   4 = boneWeights (vec4)
//
// Uniforms:
//   u_BoneMatrices[100] — final skin matrices (globalTransform * inverseBindMatrix)
constexpr const char* kVertexShaderSrc = R"glsl(
#version 330 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;
layout(location = 3) in ivec4 a_BoneIDs;
layout(location = 4) in vec4 a_BoneWeights;

uniform mat4 u_Model;
uniform mat4 u_View;
uniform mat4 u_Projection;
uniform mat4 u_BoneMatrices[100];

out vec3 v_WorldPos;
out vec3 v_Normal;
out vec2 v_TexCoord;

void main() {
    mat4 skin  = u_BoneMatrices[a_BoneIDs.x] * a_BoneWeights.x;
    skin      += u_BoneMatrices[a_BoneIDs.y] * a_BoneWeights.y;
    skin      += u_BoneMatrices[a_BoneIDs.z] * a_BoneWeights.z;
    skin      += u_BoneMatrices[a_BoneIDs.w] * a_BoneWeights.w;

    // If all weights are zero (un-skinned mesh), treat as identity.
    float wSum = a_BoneWeights.x + a_BoneWeights.y + a_BoneWeights.z + a_BoneWeights.w;
    if (wSum < 0.001) skin = mat4(1.0);

    vec4 skinnedPos    = skin * vec4(a_Position, 1.0);
    vec4 skinnedNormal = skin * vec4(a_Normal,   0.0);

    vec4 worldPos = u_Model * skinnedPos;
    v_WorldPos    = worldPos.xyz;
    v_Normal      = normalize(mat3(transpose(inverse(u_Model * skin))) * a_Normal);
    v_TexCoord    = a_TexCoord;

    gl_Position = u_Projection * u_View * worldPos;
}
)glsl";

// ── Fragment shader ───────────────────────────────────────────────────────────
// Phong shading with one directional light.
// u_HasTexture == 1 → sample u_DiffuseTex; otherwise use u_BaseColor.
constexpr const char* kFragmentShaderSrc = R"glsl(
#version 330 core

in vec3 v_WorldPos;
in vec3 v_Normal;
in vec2 v_TexCoord;

out vec4 FragColor;

uniform sampler2D u_DiffuseTex;
uniform int       u_HasTexture;
uniform vec3      u_BaseColor;

uniform vec3 u_LightDir;      // world-space, points toward light source
uniform vec3 u_LightColor;
uniform vec3 u_AmbientColor;
uniform vec3 u_CameraPos;

void main() {
    vec3 albedo = (u_HasTexture == 1)
        ? texture(u_DiffuseTex, v_TexCoord).rgb
        : u_BaseColor;

    vec3 N = normalize(v_Normal);
    vec3 L = normalize(u_LightDir);
    vec3 V = normalize(u_CameraPos - v_WorldPos);
    vec3 R = reflect(-L, N);

    float diff = max(dot(N, L), 0.0);
    float spec = pow(max(dot(V, R), 0.0), 32.0);

    vec3 color = u_AmbientColor * albedo
               + diff * u_LightColor * albedo
               + spec * u_LightColor * vec3(0.3);

    FragColor = vec4(color, 1.0);
}
)glsl";

}  // namespace avatar
