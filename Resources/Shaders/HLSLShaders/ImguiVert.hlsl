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
    _BBEXT(0)  float2 inPosition : POSITION0;
    _BBEXT(1)  float2 inUV : TEXCOORD0;
    _BBEXT(2)  float4 inColor : COLOR0;
};

struct VSOutput
{
    //not sure if needed, check directx12 later.
    float4 pos : SV_POSITION;
    _BBEXT(0)  float2 uv : TEXCOORD0;
    _BBEXT(1)  float4 color : COLOR0;
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

VSOutput main(VSInput input)
{
    VSOutput output = (VSOutput) 0;
    output.pos = float4((input.inPosition * GuiInfo.uScale) + GuiInfo.uTranslate, 0, 1);
    output.color = input.inColor;
    output.uv = input.inUV;
    return output;
}