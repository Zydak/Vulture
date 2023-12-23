#version 420 core
layout (location = 0) out vec4 outFragColor;

layout (location = 0) in vec2 inTexCoords;
layout (location = 1) in vec4 inColor;

layout (set = 2, binding = 0) uniform sampler2D uAtlas;

float screenPxRange()
{
    const float pxRange = 2.0f;
    vec2 unitRange = vec2(pxRange)/vec2(textureSize(uAtlas, 0));
    vec2 screenTexSize = vec2(1.0)/fwidth(inTexCoords);
    return max(0.5*dot(unitRange, screenTexSize), 1.0);
}

float median(float r, float g, float b)
{
    return max(min(r, g), min(max(r, g), b));
}

void main()
{
    vec3 msd = texture(uAtlas, inTexCoords).rgb;
    float sd = median(msd.r, msd.g, msd.b);
    float screenPxDistance = screenPxRange()*(sd - 0.5);
    float opacity = clamp(screenPxDistance + 0.5, 0.0, 1.0);
    if (opacity < 0.1)
        discard;
    outFragColor = vec4(mix(vec3(0.0), inColor.xyz * inColor.w, opacity), 1.0f);
    outFragColor = vec4(1.0f);
}