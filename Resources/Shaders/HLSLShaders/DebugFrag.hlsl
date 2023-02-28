#ifdef _VULKAN
#define _BBEXT(num) [[vk::location(num)]]
#elif _DIRECTX12
#define _BBEXT(num)
#else
#define _BBEXT(num)
#endif

[[vk::combinedImageSampler]]
Texture2D text : register(t0, space0);
[[vk::combinedImageSampler]]
SamplerState samplerArray : register(s1, space0);

struct VSoutput
{
    float4 pos : SV_POSITION;
    _BBEXT(0)  float2 fragUV : UV0;
    _BBEXT(1)  float3 fragColor : COLOR0;
};

float4 main(VSoutput input) : SV_Target
{
    float4 fragColor = text.Gather(samplerArray, input.fragUV.xy);
    fragColor = mul(fragColor, float4(input.fragColor.xyz, 1.0f));
    return float4(input.fragColor.xyz, 1.0);
}