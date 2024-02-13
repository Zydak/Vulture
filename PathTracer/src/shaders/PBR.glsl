#ifndef PBR
#define PBR

#include "raycommon.glsl"

#define EVENT_TYPE_END 100
#define EVENT_TYPE_OK 1

struct BsdfEvaluateData
{
    vec3  k1;            // [in] Toward the incoming ray
    vec3  k2;            // [in] Toward the sampled light
    vec3  bsdfDiffuse;   // [out] Diffuse contribution
    vec3  bsdfGlossy;    // [out] Specular contribution
    float pdf;           // [out] PDF
};

struct BsdfSampleData
{
    vec3  k1;             // [in] Toward the incoming ray
    vec3  k2;             // [in] Toward the sampled light
    vec4  xi;             // [in] 4 random [0..1]
    float pdf;            // [out] PDF
    vec3  bsdfOverPdf;    // [out] contribution / PDF
    int   eventType;      // [out] one of the event above
};

vec3 BrdfLambertian(vec3 diffuseColor, float metallic)
{
    return (1.0F - metallic) * (diffuseColor / M_PI);
}

float DistributionGGX(float NdotH, float alphaRoughness)
{
    // alphaRoughness    = roughness * roughness;
    float alphaSqr = max(alphaRoughness * alphaRoughness, 1e-07);

    float NdotHSqr = NdotH * NdotH;
    float denom = NdotHSqr * (alphaSqr - 1.0) + 1.0;

    return alphaSqr / (M_PI * denom * denom);
}

vec3 FresnelSchlick(vec3 f0, vec3 f90, float VdotH)
{
    return f0 + (f90 - f0) * pow(clamp(vec3(1.0F) - VdotH, vec3(0.0F), vec3(1.0F)), vec3(5.0F));
}

float SmithJointGGX(float NdotL, float NdotV, float alphaRoughness)
{
    float alphaRoughnessSq = max(alphaRoughness * alphaRoughness, 1e-07);

    float ggxV = NdotL * sqrt(NdotV * NdotV * (1.0F - alphaRoughnessSq) + alphaRoughnessSq);
    float ggxL = NdotV * sqrt(NdotL * NdotL * (1.0F - alphaRoughnessSq) + alphaRoughnessSq);

    float ggx = ggxV + ggxL;
    if (ggx > 0.0F)
    {
        return 0.5F / ggx;
    }
    return 0.0F;
}

vec3 BrdfSpecularGGX(vec3 f0, vec3 f90, float alphaRoughness, float VdotH, float NdotL, float NdotV, float nDotH)
{
    vec3  f = FresnelSchlick(f0, f90, VdotH);
    float vis = SmithJointGGX(NdotL, NdotV, alphaRoughness);  // Vis = G / (4 * NdotL * NdotV)
    float d = DistributionGGX(nDotH, alphaRoughness);

    return f * vis * d;
}

float ClampedDot(vec3 x, vec3 y)
{
    return clamp(dot(x, y), 0.0F, 1.0F);
}

void OrthonormalBasis(in vec3 normal, out vec3 tangent, out vec3 bitangent)
{
    float sgn = normal.z > 0.0F ? 1.0F : -1.0F;
    float a = -1.0F / (sgn + normal.z);
    float b = normal.x * normal.y * a;

    tangent = vec3(1.0f + sgn * normal.x * normal.x * a, sgn * b, -sgn * normal.x);
    bitangent = vec3(b, sgn + normal.y * normal.y * a, -normal.y);
}

vec3 GgxSampling(float alphaRoughness, float r1, float r2)
{
    float alphaSqr = max(alphaRoughness * alphaRoughness, 1e-07);

    float phi = 2.0 * M_PI * r1;
    float cosTheta = sqrt((1.0 - r2) / (1.0 + (alphaSqr - 1.0) * r2));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

    return vec3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
}

vec3 CosineSampleHemisphere(float r1, float r2)
{
    float r = sqrt(r1);
    float phi = M_TWO_PI * r2;
    vec3  dir;
    dir.x = r * cos(phi);
    dir.y = r * sin(phi);
    dir.z = sqrt(max(0.0, 1.0 - dir.x * dir.x - dir.y * dir.y));
    return dir;
}

void BsdfEvaluate(inout BsdfEvaluateData data, in vec3 normal, in Material mat)
{
    // Initialization
    vec3 surfaceNormal = normal;
    vec3 viewDir = data.k1;
    vec3 lightDir = data.k2;
    vec3 albedo = mat.Albedo.rgb;
    float metallic = mat.Metallic;
    float roughness = mat.Roughness;
    vec3 f0 = mat.Albedo.rgb; // TODO: properly calculate material f0, for now it's just albedo
    vec3 f90 = vec3(1.0F);

    // Specular roughness
    float alpha = roughness * roughness;

    // Compute half vector
    vec3 halfVector = normalize(viewDir + lightDir);

    // Compute various "angles" between vectors
    float NdotV = ClampedDot(surfaceNormal, viewDir);
    float NdotL = ClampedDot(surfaceNormal, lightDir);
    float VdotH = ClampedDot(viewDir, halfVector);
    float NdotH = ClampedDot(surfaceNormal, halfVector);
    float LdotH = ClampedDot(lightDir, halfVector);

    // Contribution
    vec3 fDiffuse = BrdfLambertian(albedo, metallic);
    vec3 fSpecular = BrdfSpecularGGX(f0, f90, alpha, VdotH, NdotL, NdotV, NdotH);

    // Calculate PDF (probability density function)
    float diffuseRatio = 0.5F * (1.0F - metallic);
    float diffusePDF = (NdotL * M_1_OVER_PI);
    float specularPDF = DistributionGGX(NdotH, alpha) * NdotH / (4.0F * LdotH);

    // Results
    data.bsdfDiffuse = fDiffuse * NdotL;
    data.bsdfGlossy = fSpecular * NdotL;
    data.pdf = mix(specularPDF, diffusePDF, diffuseRatio);
}

void BsdfSample(inout BsdfSampleData data, in vec3 normal, in Material mat)
{
    // Initialization
    vec3  surfaceNormal = normal;
    vec3  viewDir = data.k1;
    float roughness = mat.Roughness;
    float metallic = mat.Metallic;

    // Random numbers for importance sampling
    float r1 = data.xi.x;
    float r2 = data.xi.y;
    float r3 = data.xi.z;

    // Create tangent space
    vec3 tangent, binormal;
    OrthonormalBasis(surfaceNormal, tangent, binormal);

    // Specular roughness
    float alpha = roughness * roughness;

    // Find Half vector for diffuse or glossy reflection
    float diffuseRatio = 0.5F * (1.0F - metallic);
    vec3  halfVector;
    if (r3 < diffuseRatio)
        halfVector = CosineSampleHemisphere(r1, r2);  // Diffuse
    else
        halfVector = GgxSampling(alpha, r1, r2);  // Glossy

    // Transform the half vector to the hemisphere's tangent space
    halfVector = tangent * halfVector.x + binormal * halfVector.y + surfaceNormal * halfVector.z;

    // Compute the reflection direction from the sampled half vector and view direction
    vec3 reflectVector = reflect(-viewDir, halfVector);

    // Early out: avoid internal reflection
    if (dot(surfaceNormal, reflectVector) < 0.0F)
    {
        data.eventType = EVENT_TYPE_END;
        return;
    }

    // Evaluate the refection coefficient with this new ray direction
    BsdfEvaluateData evalData;
    evalData.k1 = viewDir;
    evalData.k2 = reflectVector;
    BsdfEvaluate(evalData, surfaceNormal, mat);

    // Return values
    data.bsdfOverPdf = (evalData.bsdfDiffuse + evalData.bsdfGlossy) / evalData.pdf;
    data.pdf = evalData.pdf;
    data.eventType = EVENT_TYPE_OK;
    data.k2 = reflectVector;

    // Avoid internal reflection
    if (data.pdf <= 0.00001)
        data.eventType = EVENT_TYPE_END;

    if (isnan(data.bsdfOverPdf.x) || isnan(data.bsdfOverPdf.y) || isnan(data.bsdfOverPdf.z))
        data.eventType = EVENT_TYPE_END;


    return;
}

#else
#endif