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

struct VSoutput
{
    float4 pos : SV_POSITION;
    _BBEXT(0)  float2 uv : TEXCOORD0;
    _BBEXT(1)  float4 color : COLOR0;
};

_BBBIND(0, 0) SamplerState samplerColor : register(s0, space0);
_BBBIND(0, 1) Texture2D text : register(t0, space0);

float4 main(VSoutput input) : SV_Target
{
    float4 color = input.color * text.Sample(samplerColor, input.uv);
    return color;
}