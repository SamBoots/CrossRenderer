#ifdef _VULKAN
#define _BBEXT(num) [[vk::location(num)]]
#elif _DIRECTX12
#define _BBEXT(num)
#else
#define _BBEXT(num)
#endif

struct BaseFrameInfo
{
    uint staticLightCount;
};

//pointlight
struct Light
{
    float3 pos;
    float radius;
    float4 color;
};
//Maybe add in common if I find a way to combine them.
StructuredBuffer<BaseFrameInfo> baseFrameInfo : register(t0, space0);
StructuredBuffer<Light> lights : register(t3, space0);

Texture2D text : register(t0, space1);
SamplerState samplerColor : register(s0, space1);

struct VSoutput
{
    float4 pos : SV_POSITION;
    _BBEXT(0)  float2 fragUV : UV0;
    _BBEXT(1)  float3 fragColor : COLOR0;
};

float4 main(VSoutput input) : SV_Target
{
    float4 textureColor = text.Sample(samplerColor, input.fragUV);
    float4 color = textureColor * float4(input.fragColor.xyz, 1.0f);
    return color;

}