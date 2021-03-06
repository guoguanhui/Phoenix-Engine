#version 460 core
layout (location = 0) in vec3 gPos;
layout (location = 1) in vec2 gTexCoords;
layout (location = 2) in vec3 gNormal;

out VS_OUT {
    vec3 WorldPos;
    vec3 WorldNormal;
    vec2 TexCoords;
    vec4 LightSpacePos;
} vs_out;

uniform mat4 gWVP;
uniform mat4 gWorldMatrix;
uniform mat4 gLightSpaceVP;
uniform mat3 gNormalMatrix;

void main()
{
    vs_out.WorldPos = vec3(gWorldMatrix * vec4(gPos, 1.0f));
    vs_out.WorldNormal = gNormalMatrix * gNormal;
    vs_out.TexCoords = gTexCoords;
    vs_out.LightSpacePos = gLightSpaceVP * vec4(vs_out.WorldPos, 1.0f);
    gl_Position = gWVP * vec4(gPos, 1.0f);
}
