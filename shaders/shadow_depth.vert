#version 460 core
#extension GL_ARB_shader_draw_parameters : require

layout(location = 0) in vec3  a_Position;
layout(location = 3) in uvec4 a_BoneIndices;
layout(location = 4) in vec4  a_BoneWeights;

struct InstanceGPU {
    mat4 Model;
    mat4 PrevModel;
    mat4 NormalMatrix;
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

uniform mat4 u_LightViewProj;

void main()
{
    uint instanceId = uint(gl_BaseInstance) + uint(gl_InstanceID);
    InstanceGPU inst = instances[instanceId];

    vec4 localPos = vec4(a_Position, 1.0);

    if ((inst.Flags & 1u) != 0u &&
        (a_BoneWeights.x + a_BoneWeights.y + a_BoneWeights.z + a_BoneWeights.w) > 0.0)
    {
        mat4 skin =
            bones[inst.BoneOffset + a_BoneIndices.x] * a_BoneWeights.x +
            bones[inst.BoneOffset + a_BoneIndices.y] * a_BoneWeights.y +
            bones[inst.BoneOffset + a_BoneIndices.z] * a_BoneWeights.z +
            bones[inst.BoneOffset + a_BoneIndices.w] * a_BoneWeights.w;
        localPos = skin * localPos;
    }

    vec4 worldPos = inst.Model * localPos;
    gl_Position   = u_LightViewProj * worldPos;
}