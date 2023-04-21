#ifdef _VULKAN
#define _BBEXT(num) [[vk::location(num)]]
#elif _DIRECTX12
#define _BBEXT(num)
#else
#define _BBEXT(num)
#endif

struct VSoutput
{
    //not sure if needed, check directx12 later.
    float4 pos : SV_POSITION;
    _BBEXT(0)  float4 color : COLOR0;
    _BBEXT(1)  float2 uv : UV0;
};

Texture2D text : register(t0, space0);
SamplerState samplerColor : register(s0, space0);

float4 main(VSoutput input) : SV_Target
{
    float4 color = mul(input.color, text.Sample(samplerColor, input.uv));
    return color;
}