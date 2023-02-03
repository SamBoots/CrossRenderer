struct VSoutput
{
    float4 pos : SV_POSITION;
#ifdef _VULKAN
     [[vk::location(0)]] float3 fragColor : COLOR0;
#elif _DIRECTX12
    float3 fragColor : COLOR0;
#endif
};

float4 main(VSoutput input) : SV_Target
{
    return float4(input.fragColor.xyz, 1.0);
}