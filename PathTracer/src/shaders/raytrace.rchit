#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_GOOGLE_include_directive : enable

#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_buffer_reference2 : require

#include "raycommon.glsl"

hitAttributeEXT vec2 attribs;

layout(location = 0) rayPayloadInEXT hitPayload prd;
layout(location = 1) rayPayloadEXT float shadow;

layout(push_constant) uniform _PushConstantRay { PushConstantRay pcRay; };

layout(buffer_reference, scalar) buffer Vertices {Vertex v[]; };
layout(buffer_reference, scalar) buffer Indices {int i[]; };

layout(set = 0, binding = 0) uniform accelerationStructureEXT topLevelAS;
layout(set = 0, binding = 2, scalar) buffer MeshAdressesUbo { MeshAdresses i[]; } meshAdresses;
layout(set = 0, binding = 3, scalar) buffer MaterialsUbo { Material i[]; } materials;
layout(set = 0, binding = 4) uniform sampler2D albedoTextures[];
layout(set = 0, binding = 5) uniform sampler2D normalTextures[];
layout(set = 0, binding = 6) uniform sampler2D roghnessTextures[];
layout(set = 0, binding = 7) uniform sampler2D metallnessTextures[];

vec3 brdfLambertian(vec3 diffuseColor, float metallic)
{
    return (1.0F - metallic) * (diffuseColor / M_PI);
}

void main() 
{
    Material material = materials.i[gl_InstanceCustomIndexEXT];
    MeshAdresses meshAdresses = meshAdresses.i[gl_InstanceCustomIndexEXT];
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

    // Computing the normal at hit position
    const vec3 nrm      = v0.Normal.xyz * barycentrics.x + v1.Normal.xyz * barycentrics.y + v2.Normal.xyz * barycentrics.z;
    const vec3 worldNrm = normalize(vec3(nrm * gl_WorldToObjectEXT));  // Transforming the normal to world space
    
    // Computing the coordinates of the hit position
    const vec3 pos      = v0.Position.xyz * barycentrics.x + v1.Position.xyz * barycentrics.y + v2.Position.xyz * barycentrics.z;
    const vec3 worldPos = vec3(gl_ObjectToWorldEXT * vec4(pos, 1.0));  // Transforming the position to world space

    // Pick a random direction.
    vec3 tangent, bitangent;
    CreateCoordinateSystem(worldNrm, tangent, bitangent);
    vec3 rayOrigin    = worldPos;

    vec3 randomDirection = SamplingHemisphere(prd.Seed, tangent, bitangent, worldNrm);

    // I - 2.0 * dot(N, I) * N
    // where I is the incident vector and N is the surface normal.
    vec3 reflectDirection = reflect(prd.RayDirection, worldNrm);

    float roughness = texture(roghnessTextures[gl_InstanceCustomIndexEXT], texCoord).r * material.Roughness;
    vec3 rayDirection = mix(reflectDirection, randomDirection, roughness);

    const float cos_theta = dot(rayDirection, worldNrm);
    const float p = cos_theta / M_PI;

    vec3 albedo = (material.Albedo.xyz * texture(albedoTextures[gl_InstanceCustomIndexEXT], texCoord).rgb) / M_PI;

    prd.RayOrigin = rayOrigin;
    prd.RayDirection = rayDirection;
    prd.HitValue = material.Emissive.xyz;
    prd.Weight = albedo * cos_theta / p;

    // Not sure if I can do that but I stop the ray when light source is hit
    // Chance of hitting more than one light source is very low so it should produce correct image anyway
    // And chance of hitting the same light source over and over again is very high in closed spaces
    // which means that more bounces(depth) will produce brighter image which isn't correct? idk
    if (material.Emissive.xyz != vec3(0.0f))
        prd.Depth = 100;
}