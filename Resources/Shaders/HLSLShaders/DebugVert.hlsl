struct VSInput
{
#ifdef _VULKAN
    [[vk::location(0)]] float2 inPosition : POSITION0;
    [[vk::location(1)]] float3 inColor : COLOR0;
#elif _DIRECTX12
    float2 inPosition : POSITION0;
    float3 inColor : COLOR0;
#endif
};

struct VSOutput
{
    float4 pos : SV_POSITION;   
#ifdef _VULKAN
    [[vk::location(0)]] float3 fragColor : COLOR0;
#elif _DIRECTX12
    float3 fragColor : COLOR0;
#endif
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

StructuredBuffer<Camera> cam : register(t0, space0);

struct BindlessIndices
{
    uint model;
    uint paddingTo64Bytes[15];
};

#ifdef _VULKAN
    [[vk::push_constant]] BindlessIndices indices;
#elif _DIRECTX12

#endif


RWStructuredBuffer<ModelInstance> modelInstances : register(u1, space0);

VSOutput main(VSInput input, uint VertexIndex : SV_VertexID)
{
    VSOutput output = (VSOutput)0;
    
    output.pos = mul(cam[0].proj, mul(cam[0].view, mul(modelInstances[indices.model].model, float4(input.inPosition.xy, 0.0, 1.0))));
    output.fragColor = input.inColor;
    return output;
}