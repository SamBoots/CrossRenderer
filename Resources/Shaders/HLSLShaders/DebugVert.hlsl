#ifdef _VULKAN
#define _BBEXT(num) [[vk::location(num)]]
#elif _DIRECTX12
#define _BBEXT(num)
#else
#define _BBEXT(num)
#endif

struct VSInput
{
    _BBEXT(0) float3 inPosition : POSITION0;
    _BBEXT(1) float3 inNormal   : NORMAL0;
    _BBEXT(2) float2 inUv       : UV0;
    _BBEXT(3) float3 inColor    : COLOR0;

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

struct ModelInstance
{
    float4x4 model;
    float4x4 normalModel;
};

struct Camera
{
    float4x4 view;
    float4x4 proj;
};

struct BindlessIndices
{
    uint model;
#ifdef _VULKAN
    uint paddingTo64Bytes[15];
#endif
};

#ifdef _VULKAN
    [[vk::push_constant]] BindlessIndices indices;
#elif _DIRECTX12
    ConstantBuffer<BindlessIndices> indices : register(b0, space0);
#endif

struct BaseFrameInfo
{
    uint staticLightCount;
    uint3 pad;
    
    float3 ambientLight;
    float ambientStrength;
};
//Maybe add in common if I find a way to combine them.
StructuredBuffer<BaseFrameInfo> baseFrameInfo : register(t0, space0);

StructuredBuffer<Camera> cam : register(t1, space0);
StructuredBuffer<ModelInstance> modelInstances : register(t2, space0);

VSOutput main(VSInput input, uint VertexIndex : SV_VertexID)
{
    VSOutput output = (VSOutput)0;
    float4x4 t_ModelMatrix = modelInstances[indices.model].model;
    
    output.pos = mul(cam[0].proj, mul(cam[0].view, mul(t_ModelMatrix, float4(input.inPosition.xyz, 1.0))));
    output.fragPos = (float3)float4((mul(t_ModelMatrix, float4(input.inPosition, 1.f))));
    output.uv = input.inUv;
    output.color = input.inColor;
    output.normal = input.inNormal;
    return output;
}