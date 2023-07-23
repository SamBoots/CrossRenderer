#ifdef _VULKAN
#define _BBEXT(num) [[vk::location(num)]]
#define _BBBIND(bind, set) [[vk::binding(bind, set)]]
#define SPACE_IMMUTABLE_SAMPLER 0
#define SPACE_GLOBAL 1
#define SPACE_PER_SCENE 2
#define SPACE_PER_MATERIAL 3
#define SPACE_PER_MESH 4
#elif _DIRECTX12
#define _BBEXT(num)
#define _BBBIND(bind, set)
#define SPACE_IMMUTABLE_SAMPLER 0
#define SPACE_GLOBAL 0
#define SPACE_PER_SCENE 1
#define SPACE_PER_MATERIAL 2
#define SPACE_PER_MESH 3
#else
#define _BBEXT(num)
#define _BBBIND(bind, set)
#define SPACE_IMMUTABLE_SAMPLER 9
#define SPACE_GLOBAL 9
#define SPACE_PER_SCENE 9
#define SPACE_PER_MATERIAL 9
#define SPACE_PER_MESH 9
#endif

struct Vertex
{
    float3 position;
    float3 normal;
    float2 uv;
    float3 color;
};

struct VSOutput
{
    //not sure if needed, check directx12 later.
    float4 pos : SV_POSITION; 
    _BBEXT(0) float3 fragPos    : POSITION0;
    _BBEXT(1) float3 color      : COLOR0;
    _BBEXT(2) float2 uv         : UV0;
    _BBEXT(3) float3 normal     : NORMAL0;
};

struct SceneInfo
{
    float4x4 view;
    float4x4 proj;
    
    float3 ambientLight;
    float ambientStrength;

    uint lightCount;
    uint3 padding;
};

struct ModelInstance
{
    float4x4 model;
    float4x4 inverse;
};

struct BindlessIndices
{
    uint model;
    uint texture1;
#ifdef _VULKAN
    uint paddingTo64Bytes[14];
#endif
};

#ifdef _VULKAN
    [[vk::push_constant]] BindlessIndices indices;
#elif _DIRECTX12
    ConstantBuffer<BindlessIndices> indices : register(b0, space0);
#endif

_BBBIND(0, SPACE_PER_SCENE)     ByteAddressBuffer sceneBuffer;
_BBBIND(1, SPACE_PER_SCENE)     ByteAddressBuffer modelInstances;
 _BBBIND(0, SPACE_PER_MATERIAL) ByteAddressBuffer vertData;

VSOutput main(uint VertexIndex : SV_VertexID)
{
    ModelInstance t_ModelInstance = modelInstances.Load<ModelInstance>(sizeof(ModelInstance) * indices.model);
    SceneInfo t_SceneInfo = sceneBuffer.Load < SceneInfo > (0);
    
    float4x4 t_Model = t_ModelInstance.model;
    float4x4 t_InverseModel = t_ModelInstance.inverse;
    
    const uint t_VertIndex = VertexIndex * sizeof(Vertex);
    Vertex t_Vertex;// = vertData.Load < Vertex > (t_VertIndex);
    t_Vertex.position = asfloat(vertData.Load3(t_VertIndex));
    t_Vertex.normal = asfloat(vertData.Load3(t_VertIndex + 12));
    t_Vertex.uv = asfloat(vertData.Load2(t_VertIndex + 24));
    t_Vertex.color = asfloat(vertData.Load3(t_VertIndex + 32));
    
    VSOutput output = (VSOutput) 0;
    output.pos = mul(t_SceneInfo.proj, mul(t_SceneInfo.view, mul(t_Model, float4(t_Vertex.position.xyz, 1.0))));
    output.fragPos = float4(mul(t_Model, float4(t_Vertex.position, 1.0f))).xyz;
    output.uv = t_Vertex.uv;
    output.color = t_Vertex.color;
    output.normal = mul(transpose(t_InverseModel), float4(t_Vertex.normal.xyz, 0)).xyz;
    return output;
}