struct hitPayload
{
    vec3 hitValue;
};

struct GlobalUniforms
{
    mat4 ViewProjectionMat;
    mat4 ViewInverse;
    mat4 ProjInverse;
};

struct PushConstantRay
{
    vec4 clearColor;
	vec4 lightPosition; // vec3
};

struct Vertex
{
    vec3 Position;
    vec3 Normal;
    vec2 TexCoord;
};

struct MeshAdresses
{
    uint64_t VertexBuffer;
    uint64_t IndexBuffer;
};