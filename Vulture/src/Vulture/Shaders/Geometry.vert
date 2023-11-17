#version 450
layout(location = 0) in vec3 inPos;
layout(location = 1) in vec2 inTexCoords;

layout(location = 0) out vec2 outTextureAtlasOffset;
layout(location = 1) out vec2 outTexCoords;

struct Transform
{
    mat4 ModelMatrix;
    vec4 TextureAtlasOffset; // vec2
};

layout(set = 0, binding = 0) readonly buffer ObjectUbo
{
    Transform ObjectInfos[];
} Objects;

layout(push_constant) uniform Push
{
	mat4 ProjectionViewMatrix;
} push;

void main()
{
    vec3 worldPos  = vec3(Objects.ObjectInfos[gl_InstanceIndex].ModelMatrix * vec4(inPos, 1.0));
    gl_Position = push.ProjectionViewMatrix * vec4(worldPos, 1.0);

    outTextureAtlasOffset = Objects.ObjectInfos[gl_InstanceIndex].TextureAtlasOffset.xy;
    outTexCoords = inTexCoords;
}