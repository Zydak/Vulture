#version 460 core

#extension GL_EXT_nonuniform_qualifier : enable

layout (local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout (set = 0, binding = 0) uniform sampler2D inputImage;
layout (set = 0, binding = 1, rgba16f) uniform image2D outputImage;

struct BloomInfo
{
	float Threshold;
	float Strength;
	int MipCount;
};

layout (push_constant) uniform PushContants
{
	BloomInfo pBloomInfo;
};

void main()
{
    if(gl_GlobalInvocationID.xy != clamp(gl_GlobalInvocationID.xy, vec2(0.0F), imageSize(outputImage)))
		return;

	vec2 pixelCoord = vec2(gl_GlobalInvocationID.xy) + 0.5f;
	vec2 textureSize = vec2(textureSize(inputImage, 0));
	vec2 texCoord = pixelCoord / (textureSize / 2.0f);

	vec3 color = vec3(0.0f);
	int range = 1;
	for (int i = -range; i <= range; i++)
	{
		for (int j = -range; j <= range; j++)
		{
			color += texture(inputImage, texCoord + (vec2(i, j)) / (textureSize)).rgb;
		}
	}

	color /= float((2 * range + 1) * (2 * range + 1));

	ivec2 outputTexCoords = ivec2(gl_GlobalInvocationID.xy);
	imageStore(outputImage, outputTexCoords, vec4(color, 1.0f));
}