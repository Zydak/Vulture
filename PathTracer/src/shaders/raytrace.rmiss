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

vec3 SampleHenyeyGreenstein(vec3 incomingDir, float g, float rand1, float rand2) 
{
    float phi = 2.0 * M_PI * rand1;
    float cosTheta;
    if (abs(g) < 1e-3) 
    {
        cosTheta = 1.0 - 2.0 * rand2;
    } 
    else 
    {
        float sqrTerm = (1.0 - g * g) / (1.0 - g + 2.0 * g * rand2);
        cosTheta = (1.0 + g * g - sqrTerm * sqrTerm) / (2.0 * g);
    }
    float sinTheta = sqrt(max(0.0, 1.0 - cosTheta * cosTheta));
    vec3 tangent = normalize(cross(incomingDir, abs(incomingDir.x) > 0.1 ? vec3(0, 1, 0) : vec3(1, 0, 0)));
    vec3 bitangent = cross(incomingDir, tangent);
    vec3 scatteredDir = incomingDir * cosTheta + tangent * sinTheta * cos(phi) + bitangent * sinTheta * sin(phi);
    return normalize(scatteredDir);
}

float SampleExponentialDistance(float extinctionCoefficient, float rand) 
{
    return -log(1.0 - rand) / extinctionCoefficient;
}

float CalculateOpticalDepth(float extinctionCoefficient, float distance) 
{
    return extinctionCoefficient * distance;
}

bool ShouldScatter(float opticalDepth, float rand) 
{
    return rand <= 1.0 - exp(-opticalDepth);
}

void main()
{
#ifdef USE_FOG
    float distance = Rnd(prd.Seed) * 100.0f;

    float aFactor = 0.1f;
    float sFactor = 0.1f;
    float tFactor = sFactor + aFactor;
    float Tr = exp(-tFactor * distance);
    float r0 = Rnd(prd.Seed);
    float r1 = Rnd(prd.Seed);
    float r2 = Rnd(prd.Seed);
    float r3 = Rnd(prd.Seed);

    float opticalDepth = CalculateOpticalDepth(aFactor, distance);

    float scatterDist = SampleExponentialDistance(aFactor, r3);
    if (scatterDist < distance)
    {
        prd.RayOrigin = prd.RayOrigin + prd.RayDirection * scatterDist;
        prd.RayDirection = SampleHenyeyGreenstein(prd.RayDirection, 0.0f, r1, r2);

        
		vec3 rayDir = prd.RayDirection;
		rayDir = Rotate(rayDir, vec3(1, 0, 0), -pcRay.EnvAltitude);
		rayDir = Rotate(rayDir, vec3(0, 1, 0), -pcRay.EnvAzimuth);
		vec2 uv = directionToSphericalEnvmap(rayDir);
		vec3 color = texture(uEnvMap, uv).rgb;

        prd.HitValue = color;
        prd.Weight = vec3(1.0f) * (1.0f - Tr);
        
        return;
    }
#endif

    vec3 rayDir = prd.RayDirection;
	rayDir = Rotate(rayDir, vec3(1, 0, 0), -pcRay.EnvAltitude);
	rayDir = Rotate(rayDir, vec3(0, 1, 0), -pcRay.EnvAzimuth);
	vec2 uv = directionToSphericalEnvmap(rayDir);
	vec4 color = texture(uEnvMap, uv);
	if (prd.Depth != 0)
    {
#ifdef SAMPLE_ENV_MAP
        const float misWeight = color.w / (color.w + prd.Pdf);
        const vec3 w = (color.xyz / color.w) * misWeight;

        vec3 contribution = w * prd.Bsdf;

	    prd.HitValue += contribution;
#else
        prd.HitValue += color.rgb;
#endif
    }
    else
	{
	    prd.HitValue += color.rgb;
		prd.MissedAllGeometry = true; // set to true to eliminate collecting more samples of the pixel
	}

	prd.Depth = DEPTH_INFINITE;
}