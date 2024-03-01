#version 450
layout(location = 0) in vec2 inTextureAtlasOffset;
layout(location = 1) in vec2 inTexCoords;

layout(location = 0) out vec4 outFragColor;

layout(set = 2, binding = 0) uniform sampler2D uTextureAtlas;

layout(set = 2, binding = 1) uniform AtlasInfo
{
    vec4 TilingSize; // float
} atlasInfo;

void main()
{
	ivec2 atlasOffset = ivec2(inTexCoords * atlasInfo.TilingSize.x);
	ivec2 atlasOffsetInt = atlasOffset + ivec2(inTextureAtlasOffset);

	vec4 color = texelFetch(uTextureAtlas, atlasOffsetInt, 0);
	if (color.a == 0)
		discard;
	outFragColor = color;
}