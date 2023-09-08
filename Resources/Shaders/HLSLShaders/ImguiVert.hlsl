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
    _BBEXT(0)   float2 inPosition : POSITION;
    _BBEXT(1)  float2 inUV : TEXCOORD0;
    _BBEXT(2)  float4 inColor : COLOR0;
};

struct VSOutput
{
    float4 pos : SV_POSITION;
    _BBEXT(0)   float4 color : COLOR0;
    _BBEXT(1)   float2 uv : TEXCOORD0;
};

struct guiinfo
{
    float4x4 ProjectionMatrix;
};

#ifdef _VULKAN
    [[vk::push_constant]] guiinfo GuiInfo;
#elif _DIRECTX12
    ConstantBuffer<guiinfo> GuiInfo : register(b0, space0);
#endif

VSOutput main(VSInput input)
{
    VSOutput output = (VSOutput) 0;
    output.pos = mul(GuiInfo.ProjectionMatrix, float4(input.inPosition.xy, 0.f, 1.f));
    output.color = input.inColor;
    output.uv = input.inUV;
    return output;
}