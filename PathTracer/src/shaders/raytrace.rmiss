#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "raycommon.glsl"

layout(location = 0) rayPayloadInEXT hitPayload prd;

layout(push_constant) uniform _PushConstantRay
{
	PushConstantRay pcRay;
};

void main()
{
	if(prd.Depth == 0)
	{
		prd.HitValue = pcRay.ClearColor.xyz;
		prd.MissedAllGeometry = true;
	}
	else
		prd.HitValue = vec3(0.0f);
	prd.Depth = 100;
}