#ifdef _VULKAN
#define _BBEXT(num) [[vk::location(num)]]
#elif _DIRECTX12
#define _BBEXT(num)
#else
#define _BBEXT(num)
#endif

struct VSInput
{
    _BBEXT(0)  float2 inPosition : POSITION0;
    _BBEXT(1)  float2 inUV : UV0;
    _BBEXT(2)  float4 inColor : COLOR0;
};

struct guiinfo
{
    float2 uScale;
    float2 uTranslate;
};

#ifdef _VULKAN
    [[vk::push_constant]] guiinfo GuiInfo;
#elif _DIRECTX12
    ConstantBuffer<guiinfo> GuiInfo : register(b0, space0);
#endif

struct VSOutput
{
    //not sure if needed, check directx12 later.
    float4 pos : SV_POSITION;
    _BBEXT(0)   float3 color : COLOR0;
    _BBEXT(1)   float2 uv : UV0;
};

VSOutput main(VSInput input, uint VertexIndex : SV_VertexID)
{
    VSOutput output = (VSOutput) 0;
    output.pos = float4(mul(input.inPosition * GuiInfo.uScale) + GuiInfo.uTranslate, 0, 1);
    output.color = input.inColor;
    output.uv = input.inUV;
    return output;
}