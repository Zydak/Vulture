#version 450
layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inTexCoords;

layout (location = 0) out vec2 outTexCoords;
layout (location = 1) out vec4 outColor;

layout(set = 0, binding = 0) uniform MainUbo
{
    mat4 cameraViewProj;
} mainUbo;

layout(set = 1, binding = 0) readonly buffer ModelUbo
{
    mat4 modelMatrices[];
} transforms;

layout(push_constant) uniform Push
{
	vec4 color;
} push;

void main()
{
    outTexCoords = inTexCoords;
    vec4 posWorld = transforms.modelMatrices[gl_InstanceIndex] * vec4(inPos, 1.0);
    gl_Position = mainUbo.cameraViewProj * posWorld;
    outColor = push.color;
}