#version 450

layout(location = 0) in vec2 inTexCoords;

layout(location = 0) out vec4 outFragColor;

layout(set = 0, binding = 0) uniform sampler2D uHDR;

void main()
{
    const float gamma = 1.0;
    const float exposure = 1.0;

    vec3 hdrColor = texture(uHDR, inTexCoords).rgb;

    vec3 mapped = vec3(1.0) - exp(-hdrColor * exposure);
    mapped = pow(mapped, vec3(1.0 / gamma));

    outFragColor = vec4(mapped, 1.0);
}