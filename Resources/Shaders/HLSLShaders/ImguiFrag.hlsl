#ifdef _VULKAN
#define _BBEXT(num) [[vk::location(num)]]
#elif _DIRECTX12
#define _BBEXT(num)
#else
#define _BBEXT(num)
#endif

struct VSoutput
{
    float4 pos : SV_POSITION;
    _BBEXT(0)  float2 uv : TEXCOORD0;
    _BBEXT(1)  float4 color : COLOR0;
};

SamplerState samplerColor : register(s0, space0);

#ifdef _VULKAN
Texture2D text : register(t0, space1);
#elif _DIRECTX12
Texture2D text : register(t0, space1);
#endif

float4 main(VSoutput input) : SV_Target
{
    float4 color = input.color * text.Sample(samplerColor, input.uv);
    return color;
}