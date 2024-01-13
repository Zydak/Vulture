#version 460

layout (location = 0) out vec4 outAlbedo;
layout (location = 1) out vec4 outRoughness;
layout (location = 2) out vec4 outMetallness;
layout (location = 3) out vec4 outNormal;

struct Material
{
	vec4 Albedo;
	vec4 Emissive;
	float Metallic;
	float Roughness;
};

struct DataIn
{
	Material material;
	mat3 TBN;
	vec2 TexCoord;
};

layout (location = 0) in DataIn dataIn;

void main()
{
	vec3 normalMapTest = vec3(0.5f, 0.5f, 1.0f); // default normal value in normal maps
	normalMapTest = normalMapTest * 2.0f - 1.0f;

	outAlbedo = dataIn.material.Albedo;
	outRoughness = vec4(dataIn.material.Roughness);
	outMetallness = vec4(dataIn.material.Metallic);
	outNormal = 0.5 * (vec4(normalize(dataIn.TBN * normalMapTest), 1.0f) + 1.0f);
}