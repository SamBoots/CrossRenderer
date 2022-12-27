struct VSoutput
{
    [[vk::location(0)]] float3 fragColor : COLOR0;
};

float4 main(VSoutput input) : SV_Target
{
    return float4(input.fragColor.xyz, 1.0);
}