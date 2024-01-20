#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "raycommon.glsl"

layout(location = 0) rayPayloadInEXT hitPayload prd;

layout (set = 1, binding = 1) uniform samplerCube uSamplerCubeMap;

layout(push_constant) uniform _PushConstantRay
{
	PushConstantRay pcRay;
};

void main()
{
	vec3 color = texture(uSamplerCubeMap, prd.RayDirection).xyz;
	if(prd.Depth == 0)
	{
		prd.HitValue = color;
		prd.MissedAllGeometry = true;
	}
	else
		prd.HitValue = color;
	prd.Depth = 100;
}