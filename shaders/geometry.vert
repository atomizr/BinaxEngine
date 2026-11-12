#version 460 core
#extension GL_ARB_shader_draw_parameters : require

layout(location = 0) in vec3  a_Position;
layout(location = 1) in vec3  a_Normal;
layout(location = 2) in vec2  a_UV;
layout(location = 3) in uvec4 a_BoneIndices;
layout(location = 4) in vec4  a_BoneWeights;

struct InstanceGPU {
    mat4 Model;
    mat4 PrevModel;
    mat4 NormalMatrix;   // FIX: нормальная матрица, передаётся с CPU
    uint MaterialId;
    uint MeshId;
    uint BoneOffset;
    uint Flags;
};

layout(std430, binding = 1) readonly buffer Instances {
    InstanceGPU instances[];
};

layout(std430, binding = 3) readonly buffer Bones {
    mat4 bones[];
};

uniform mat4 u_View;
uniform mat4 u_Projection;
uniform mat4 u_PrevViewProj;

out vec3 v_WorldPos;
out vec3 v_Normal;
out vec2 v_UV;
out vec2 v_Velocity;
flat out uint v_InstanceIndex;

void main()
{
    uint instanceId = uint(gl_BaseInstance) + uint(gl_InstanceID);
    InstanceGPU inst = instances[instanceId];

    vec4 localPos    = vec4(a_Position, 1.0);
    vec3 localNormal = a_Normal;

    if ((inst.Flags & 1u) != 0u &&
        (a_BoneWeights.x + a_BoneWeights.y + a_BoneWeights.z + a_BoneWeights.w) > 0.0)
    {
        mat4 skin =
            bones[inst.BoneOffset + a_BoneIndices.x] * a_BoneWeights.x +
            bones[inst.BoneOffset + a_BoneIndices.y] * a_BoneWeights.y +
            bones[inst.BoneOffset + a_BoneIndices.z] * a_BoneWeights.z +
            bones[inst.BoneOffset + a_BoneIndices.w] * a_BoneWeights.w;
        localPos    = skin * localPos;
        localNormal = mat3(skin) * localNormal;
    }

    vec4 worldPos = inst.Model * localPos;

    vec4 currClip = u_Projection * u_View * worldPos;
    vec4 prevClip = u_PrevViewProj * (inst.PrevModel * localPos);

    vec2 velocity = vec2(0.0);
    if (prevClip.w > 1e-5 && currClip.w > 1e-5)
        velocity = (prevClip.xy / prevClip.w - currClip.xy / currClip.w) * 0.5;

    v_Velocity = velocity;
    v_WorldPos = worldPos.xyz;

    // FIX: используем готовую нормальную матрицу вместо
    // дорогого mat3(transpose(inverse(inst.Model)))
    v_Normal = normalize(mat3(inst.NormalMatrix) * localNormal);

    v_UV            = a_UV;
    v_InstanceIndex = instanceId;
    gl_Position     = currClip;
}
