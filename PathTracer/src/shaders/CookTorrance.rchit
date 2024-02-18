#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_GOOGLE_include_directive : enable

#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_buffer_reference2 : require

#include "raycommon.glsl"
#include "CookTorranceSampling.glsl"

hitAttributeEXT vec2 attribs;

layout(location = 0) rayPayloadInEXT hitPayload payload;

layout(push_constant) uniform _PushConstantRay { PushConstantRay push; };

layout(buffer_reference, scalar) buffer Vertices { Vertex v[]; };
layout(buffer_reference, scalar) buffer Indices { int i[]; };

layout(set = 0, binding = 0) uniform accelerationStructureEXT uTopLevelAS;
layout(set = 0, binding = 2, scalar) buffer MeshAdressesUbo { MeshAdresses uMeshAdresses[]; };
layout(set = 0, binding = 3, scalar) buffer MaterialsUbo { Material uMaterials[]; };
layout(set = 0, binding = 4) uniform sampler2D uAlbedoTextures[];
layout(set = 0, binding = 5) uniform sampler2D uNormalTextures[];
layout(set = 0, binding = 6) uniform sampler2D uRoghnessTextures[];
layout(set = 0, binding = 7) uniform sampler2D uMetallnessTextures[];
layout(set = 1, binding = 1) uniform sampler2D uEnvMap;

layout(set = 1, binding = 2) readonly buffer uEnvMapAccel
{
    EnvAccel[] uAccels;
};

#define SAMPLE_IMPORTANCE
#include "envSampling.glsl"

void main() 
{
    MeshAdresses meshAdresses = uMeshAdresses[gl_InstanceCustomIndexEXT];
    Indices indices = Indices(meshAdresses.IndexBuffer);
    Vertices vertices = Vertices(meshAdresses.VertexBuffer);
    
    int i1 = indices.i[(gl_PrimitiveID * 3)];
    int i2 = indices.i[(gl_PrimitiveID * 3)+1];
    int i3 = indices.i[(gl_PrimitiveID * 3)+2];
    
    Vertex v0 = vertices.v[i1];
    Vertex v1 = vertices.v[i2];
    Vertex v2 = vertices.v[i3];

    const vec3 barycentrics = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);
    vec2 texCoord = v0.TexCoord.xy * barycentrics.x + v1.TexCoord.xy * barycentrics.y + v2.TexCoord.xy * barycentrics.z;

    Material material = uMaterials[gl_InstanceCustomIndexEXT];
    material.Albedo *= texture(uAlbedoTextures[gl_InstanceCustomIndexEXT], texCoord);

    // Computing the normal at hit position
    const vec3 nrm      = v0.Normal.xyz * barycentrics.x + v1.Normal.xyz * barycentrics.y + v2.Normal.xyz * barycentrics.z;
    vec3 worldNrm = normalize(vec3(nrm * gl_ObjectToWorldEXT));  // Transforming the normal to world space
    
    const vec3 V = -gl_WorldRayDirectionEXT;
    if (dot(worldNrm, V) < 0)
    {
        worldNrm = -worldNrm;
    }

    // Computing the coordinates of the hit position
    const vec3 pos      = v0.Position.xyz * barycentrics.x + v1.Position.xyz * barycentrics.y + v2.Position.xyz * barycentrics.z;
    const vec3 worldPos = vec3(gl_ObjectToWorldEXT * vec4(pos, 1.0));  // Transforming the position to world space

    vec3 hitValue = material.Emissive.xyz;

    vec3 dirToLight;
    vec3 contribution = vec3(0.0f);
    vec3 randVal = vec3(Rnd(payload.Seed), Rnd(payload.Seed), Rnd(payload.Seed));
    vec4 envColor = SampleImportanceEnvMap(uEnvMap, randVal, dirToLight);

    bool nextEventValid = (dot(dirToLight, worldNrm) > 0.0f);

    if (nextEventValid)
    {
        BsdfEvaluateData evalData;
        evalData.k1   = -gl_WorldRayDirectionEXT;
        evalData.k2   = dirToLight;

        BsdfEvaluate(evalData, worldNrm, material);
        if(evalData.pdf > 0.0)
        {
            const float misWeight = envColor.w / (envColor.w + evalData.pdf);
            const vec3 w = (envColor.xyz / envColor.w) * misWeight;

            contribution += w * evalData.bsdfDiffuse;
            contribution += w * evalData.bsdfGlossy;
        }
    }
    
    BsdfSampleData sampleData;
    //TODO: IOR and refraction
    //sampleData.ior1 = vec3(1.0F);                // IOR current medium
    //sampleData.ior2 = vec3(1.0F);                // IOR other side
    sampleData.k1   = -gl_WorldRayDirectionEXT;  // outgoing direction
    sampleData.xi   = vec4(Rnd(payload.Seed), Rnd(payload.Seed), Rnd(payload.Seed), Rnd(payload.Seed));
    BsdfSample(sampleData, worldNrm, material);
    payload.HitValue = hitValue;
    
    if (sampleData.eventType == EVENT_TYPE_END)
    {
        payload.Depth = DEPTH_INFINITE;
        nextEventValid = false;
        return;
    }
    
    payload.Weight       = sampleData.bsdfOverPdf;
    payload.RayDirection = sampleData.k2;
    vec3 offsetDir       = dot(payload.RayDirection, worldNrm) > 0 ? worldNrm : -worldNrm;
    payload.RayOrigin    = OffsetRay(worldPos, offsetDir);
    
    if (nextEventValid)
    {
        uint prevDepth = payload.Depth;

        float tMin     = 0.001;
        float tMax     = 10000.0;
        uint flags = gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT;
        
        traceRayEXT(
            uTopLevelAS,        // acceleration structure
            flags,              // rayFlags
            0xFF,               // cullMask
            0,                  // sbtRecordOffset
            0,                  // sbtRecordStride
            0,                  // missIndex
            worldPos,           // ray origin
            tMin,               // ray min range
            dirToLight,         // ray direction
            tMax,               // ray max range
            0                   // payload (location = 0)
        );
        
        payload.HitValue = hitValue;
        if (payload.Depth == DEPTH_INFINITE) // Didn't hit anything
        {
            payload.HitValue += contribution;
        }

        payload.Depth = prevDepth;
        payload.MissedAllGeometry = false;
    }
}