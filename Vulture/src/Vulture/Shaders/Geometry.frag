#version 450
layout(location = 0) in vec2 inTextureAtlasOffset;
layout(location = 1) in vec2 inTexCoords;

layout(location = 0) out vec4 outFragColor;

layout(set = 1, binding = 0) uniform sampler2D uTextureAtlas;

layout(set = 1, binding = 1) uniform AtlasInfo
{
    vec4 TilingSize; // float
} atlasInfo;

void main()
{
	vec2 atlasOffset = (inTexCoords / textureSize(uTextureAtlas, 0)) * atlasInfo.TilingSize.x;
	atlasOffset += (inTextureAtlasOffset / textureSize(uTextureAtlas, 0));

	outFragColor = texture(uTextureAtlas, atlasOffset);
}