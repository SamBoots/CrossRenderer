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
    _BBEXT(1) float3 inNormal : NORMAL0;
    _BBEXT(2) float2 inUv : UV0;
    _BBEXT(3) float3 inColor : COLOR0;

};

struct VSOutput
{
    float4 pos : SV_POSITION;   
    _BBEXT(0) float2 fragUV : UV0;
    _BBEXT(1) float3 fragColor : COLOR0;
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

StructuredBuffer<Camera> cam : register(t0, space0);
StructuredBuffer<ModelInstance> modelInstances : register(t1, space0);

VSOutput main(VSInput input, uint VertexIndex : SV_VertexID)
{
    VSOutput output = (VSOutput)0;
    
    output.pos = mul(cam[0].proj, mul(cam[0].view, mul(modelInstances[indices.model].model, float4(input.inPosition.xyz, 1.0))));
    output.fragUV = input.inUv;
    output.fragColor = input.inColor;
    return output;
}