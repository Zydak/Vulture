struct hitPayload
{
    vec3 HitValue;
    uint Seed;
    uint Depth;
    vec3 RayOrigin;
    vec3 RayDirection;
    vec3 Weight;
    bool MissedAllGeometry;
};

struct GlobalUniforms
{
    mat4 ViewProjectionMat;
    mat4 ViewInverse;
    mat4 ProjInverse;
};

struct PushConstantRay
{
    vec4 ClearColor;
    int Frame;
    int MaxDepth;
    int SamplesPerFrame;

    float FocalLenght;
    float DoFStrenght;
};

struct Vertex
{
    vec3 Position;
    vec3 Normal;
    vec3 Tangent;
    vec3 Bitangent;
    vec2 TexCoord;
};

struct MeshAdresses
{
    uint64_t VertexBuffer;
    uint64_t IndexBuffer;
};

struct Material
{
    vec4 Albedo;
    vec4 Emissive;
    float Metallic;
    float Roughness;
};

uint PCG(inout uint seed)
{
    uint state = seed * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    seed = (word >> 22u) ^ word;
    return seed & 0x00FFFFFF;
}

float Rnd(inout uint prev)
{
    return (float(PCG(prev)) / float(0x01000000));
}

vec3 SamplingHemisphere(inout uint seed, in vec3 x, in vec3 y, in vec3 z)
{
    #define M_PI 3.14159265

    float r1 = Rnd(seed);
    float r2 = Rnd(seed);
    float sq = sqrt(r1);
    
    vec3 direction = vec3(cos(2 * M_PI * r2) * sq, sin(2 * M_PI * r2) * sq, sqrt(1. - r1));
    direction      = direction.x * x + direction.y * y + direction.z * z;
    
    return direction;
}

// Return the tangent and binormal from the incoming normal
void CreateCoordinateSystem(in vec3 N, out vec3 Nt, out vec3 Nb)
{
    if(abs(N.x) > abs(N.y))
        Nt = vec3(N.z, 0, -N.x) / sqrt(N.x * N.x + N.z * N.z);
    else
        Nt = vec3(0, -N.z, N.y) / sqrt(N.y * N.y + N.z * N.z);
    Nb = cross(N, Nt);
}

vec2 RandomPointInCircle(inout uint seed)
{
    float angle = Rnd(seed) * 2.0 * M_PI;
    vec2 pointOnCircle = vec2(cos(angle), sin(angle));
    return pointOnCircle * sqrt(Rnd(seed));
}