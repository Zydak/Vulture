#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_GOOGLE_include_directive : enable

#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_buffer_reference2 : require

#include "raycommon.glsl"
layout(location = 0) rayPayloadInEXT hitPayload payload;

layout(set = 0, binding = 0) uniform accelerationStructureEXT uTopLevelAS;
#include "DisneySampling.glsl"

hitAttributeEXT vec2 attribs;

layout(push_constant) uniform _PushConstantRay { PushConstantRay push; };

layout(buffer_reference, scalar) buffer Vertices { Vertex v[]; };
layout(buffer_reference, scalar) buffer Indices { int i[]; };

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

struct HitState
{
    vec3 RayDir;
    vec3 HitValue;
    vec3 RayOrigin;
    vec3 Weight;

    bool Valid;
};

HitState ClosestHit(Material mat, Surface surface, vec3 worldPos)
{
    HitState state;
    
    vec3 hitValue = mat.Emissive.xyz;

    vec3 rayDir;
    float pdf;
    vec3 bsdf = SampleDisney(mat, -payload.RayDirection, surface, rayDir, pdf);

    if((pdf <= 0.0f) || isnan(bsdf.x) || isnan(bsdf.y) || isnan(bsdf.z)) 
    {
        state.Valid = false;
        return state;
    }
    
// TODO: sampling env map is insanely slow for some reason, disabling this gives me almost 200% more performance
#ifdef SAMPLE_ENV_MAP
    vec3 dirToLight;
    vec3 lightContribution = vec3(0.0f);
    vec3 randVal = vec3(Rnd(payload.Seed), Rnd(payload.Seed), Rnd(payload.Seed));
    vec4 envColor = SampleImportanceEnvMap(uEnvMap, randVal, dirToLight);

    bool lightRayValid = (dot(dirToLight, surface.Normal) > 0.0f);

    if (lightRayValid)
    {
        float envPdf;
        vec3 envBsdf = EvaluateDisney(mat, -payload.RayDirection, surface, dirToLight, envPdf);
        if(envPdf > 0.0f)
        {
            const float misWeight = envColor.w / (envColor.w + envPdf);
            const vec3 w = (envColor.xyz / envColor.w) * misWeight;
            
            lightContribution += w * envBsdf;
        }

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
        
        if (payload.Depth == DEPTH_INFINITE) // Didn't hit anything
        {
            hitValue += lightContribution;
        }
    
        payload.Depth = prevDepth;
        payload.MissedAllGeometry = false;
    }
#endif

    state.Weight       = bsdf / pdf;
    state.RayDir = rayDir;
    vec3 offsetDir       = dot(payload.RayDirection, surface.Normal) > 0 ? surface.Normal : -surface.Normal;
    state.RayOrigin    = OffsetRay(worldPos, offsetDir);
    state.HitValue = hitValue;

    state.Valid = true;

    return state;
}

void main() 
{
    // -------------------------------------------
    // Get Data From Buffers
    // -------------------------------------------
    MeshAdresses meshAdresses = uMeshAdresses[gl_InstanceCustomIndexEXT];
    Indices indices = Indices(meshAdresses.IndexBuffer);
    Vertices vertices = Vertices(meshAdresses.VertexBuffer);
    Material material = uMaterials[gl_InstanceCustomIndexEXT];
    
    int i1 = indices.i[(gl_PrimitiveID * 3)];
    int i2 = indices.i[(gl_PrimitiveID * 3)+1];
    int i3 = indices.i[(gl_PrimitiveID * 3)+2];
    
    Vertex v0 = vertices.v[i1];
    Vertex v1 = vertices.v[i2];
    Vertex v2 = vertices.v[i3];

    // -------------------------------------------
    // Calculate Surface Properties
    // -------------------------------------------

    const vec3 barycentrics = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);
    vec2 texCoord = v0.TexCoord.xy * barycentrics.x + v1.TexCoord.xy * barycentrics.y + v2.TexCoord.xy * barycentrics.z;

    const vec3 pos      = v0.Position.xyz * barycentrics.x + v1.Position.xyz * barycentrics.y + v2.Position.xyz * barycentrics.z;
    const vec3 worldPos = vec3(gl_ObjectToWorldEXT * vec4(pos, 1.0));  // Transforming the position to world space
    
    // Computing the normal at hit position
    const vec3 nrm      = v0.Normal.xyz * barycentrics.x + v1.Normal.xyz * barycentrics.y + v2.Normal.xyz * barycentrics.z;
    vec3 worldNrm = normalize(vec3(nrm * gl_ObjectToWorldEXT));  // Transforming the normal to world space

    material.eta = dot(gl_WorldRayDirectionEXT, worldNrm) < 0.0 ? (1.0 / material.Ior) : material.Ior;

    const vec3 V = -gl_WorldRayDirectionEXT;
    if (dot(worldNrm, V) < 0)
    {
        worldNrm = -worldNrm;
    }

    Surface surface;
    surface.Normal = worldNrm;
    surface.GeoNormal = worldNrm;
    CalculateTangents1(worldNrm, surface.Tangent, surface.Bitangent);
    
#ifdef USE_NORMAL_MAPS
    vec3 normalMapVal = texture(uNormalTextures[gl_InstanceCustomIndexEXT], texCoord).xyz;
    normalMapVal = normalMapVal * 2.0f - 1.0f;
    
    normalMapVal = TangentToWorld(surface.Tangent, surface.Bitangent, worldNrm, normalMapVal);
    surface.Normal = normalize(normalMapVal);
    
    CalculateTangents1(worldNrm, surface.Tangent, surface.Bitangent);
#endif

    // -------------------------------------------
    // Calculate Material Properties
    // -------------------------------------------
    float aspect = sqrt(1.0 - material.Anisotropic * 0.99);
    material.ax = max(0.001, material.Roughness / aspect);
    material.ay = max(0.001, material.Roughness * aspect);
    
#ifdef USE_ALBEDO
    material.Albedo *= texture(uAlbedoTextures[gl_InstanceCustomIndexEXT], texCoord);
#else
    material.Albedo = vec4(0.5f);
#endif

    // -------------------------------------------
    // Calculate Hit State
    // -------------------------------------------
    HitState hitState = ClosestHit(material, surface, worldPos);

    if (hitState.Valid == false)
    {
        payload.Depth = DEPTH_INFINITE;
        return;
    }
    
    payload.Weight       = hitState.Weight;
    payload.RayDirection = hitState.RayDir;
    payload.RayOrigin    = hitState.RayOrigin;
    payload.HitValue     = hitState.HitValue;
}