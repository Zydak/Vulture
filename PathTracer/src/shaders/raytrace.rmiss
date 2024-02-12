#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "raycommon.glsl"

layout(location = 0) rayPayloadInEXT hitPayload prd;

layout (set = 1, binding = 1) uniform sampler2D uEnvMap;

layout(push_constant) uniform _PushConstantRay
{
	PushConstantRay pcRay;
};

void main()
{
	if(prd.Depth == 0)
	{
		vec3 rayDir = prd.RayDirection;
		vec2 uv = directionToSphericalEnvmap(rayDir);
		vec3 color = texture(uEnvMap, uv).rgb;
		prd.HitValue = color;
		prd.MissedAllGeometry = true; // set to true to eliminate collecting more samples of the pixel
	}
	else
	{
		vec3 rayDir = prd.RayDirection;
		vec2 uv = directionToSphericalEnvmap(rayDir);
		vec3 color = texture(uEnvMap, uv).rgb;
		prd.HitValue = color;
	}
	prd.Depth = DEPTH_INFINITE;
}