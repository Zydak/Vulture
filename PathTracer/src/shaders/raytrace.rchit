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

void main() 
{
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

    // Vector toward the light
    vec3 lDir               = pcRay.lightPosition.xyz - worldPos;
    float lightDistance     = length(lDir);
    float lightIntensity    = 80.0f / (lightDistance * lightDistance);
    vec3  L                 = normalize(lDir);

    float dotProduct = dot(worldNrm, L);

    if (dotProduct > 0.0f)
	{
		shadow = 0.0f;
		
        float tMin   = 0.001;
        float tMax   = lightDistance;
        vec3  origin = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
        vec3  rayDir = L;
        uint  flags = gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT;

        traceRayEXT(topLevelAS,  // acceleration structure
            flags,       // rayFlags
            0xFF,        // cullMask
            0,           // sbtRecordOffset
            0,           // sbtRecordStride
            1,           // missIndex
            origin,      // ray origin
            tMin,        // ray min range
            rayDir,      // ray direction
            tMax,        // ray max range
            1            // payload (location = 1)
        );
	}

    vec3 ambient = vec3(0.05f);

    vec3 Lo = clamp(vec3(1.0f) * dotProduct * lightIntensity * shadow, 0.0f, 1.0f);

    vec3 color = Lo + ambient;
    
    prd.hitValue = color;
}