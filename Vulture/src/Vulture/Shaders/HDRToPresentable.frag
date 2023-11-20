#version 450

layout(location = 0) in vec2 inTexCoords;

layout(location = 0) out vec4 outFragColor;

layout(set = 0, binding = 0) uniform sampler2D uHDR;

void main()
{
    vec3 color = texture(uHDR, inTexCoords).rgb;

    outFragColor = vec4(color, 1.0);
}