#version 460 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_UV;

uniform mat4 u_Model;
uniform mat4 u_View;
uniform mat4 u_Projection;

out vec3 v_WorldPos;
out vec3 v_Normal;
out vec2 v_UV;

void main()
{
    vec4 worldPos = u_Model * vec4(a_Position, 1.0);
    v_WorldPos = worldPos.xyz;

    // Correct normal transform (inverse-transpose of model's upper 3x3)
    mat3 normalMatrix = transpose(inverse(mat3(u_Model)));
    v_Normal = normalMatrix * a_Normal;

    v_UV = a_UV;

    gl_Position = u_Projection * u_View * worldPos;
}