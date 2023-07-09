#ifdef _VULKAN
#define _BBEXT(num) [[vk::location(num)]]
#define _BBBIND(bind, set) [[vk::binding(bind, set)]]
#elif _DIRECTX12
#define _BBEXT(num)
#define _BBBIND(bind, set)
#else
#define _BBEXT(num)
#define _BBBIND(bind, set)
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
    float4x4 inverse;
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

_BBBIND(1, 1) ByteAddressBuffer cam : register(t1, space0);
_BBBIND(2, 1) ByteAddressBuffer modelInstances : register(t2, space0);

VSOutput main(VSInput input, uint VertexIndex : SV_VertexID)
{
    ModelInstance t_ModelInstance = modelInstances.Load<ModelInstance>(sizeof(ModelInstance) * indices.model);
    Camera t_Cam = cam.Load<Camera>(0);
    
    VSOutput output = (VSOutput)0;
    float4x4 t_Model = t_ModelInstance.model;
    float4x4 t_InverseModel = t_ModelInstance.inverse;
    
    output.pos = mul(t_Cam.proj, mul(t_Cam.view, mul(t_Model, float4(input.inPosition.xyz, 1.0))));
    output.fragPos = float4(mul(t_Model, float4(input.inPosition, 1.0f))).xyz;
    output.uv = input.inUv;
    output.color = input.inColor;
    output.normal = mul(transpose(t_InverseModel), float4(input.inNormal.xyz, 0)).xyz;
    return output;
}