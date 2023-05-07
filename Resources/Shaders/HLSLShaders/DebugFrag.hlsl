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
    uint3 pad;
    
    float3 ambientLight;
    float ambientStrength;
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

SamplerState samplerColor : register(s0, space1);
#ifdef _VULKAN
Texture2D text : register(t1, space1);
#elif _DIRECTX12
Texture2D text : register(t0, space1);
#endif

struct VSoutput
{
    //not sure if needed, check directx12 later.
    float4 pos : SV_POSITION;
    _BBEXT(0)  float3 fragPos   : POSITION0;
    _BBEXT(1)  float3 color     : COLOR0;
    _BBEXT(2)  float2 uv        : UV0;
    _BBEXT(3)  float3 normal    : NORMAL0;
};

float4 main(VSoutput input) : SV_Target
{
    float4 t_TextureColor = text.Sample(samplerColor, input.uv);
    float4 t_Color = t_TextureColor * float4(input.color.xyz, 1.0f);
    
    float4 t_Diffuse;
    //Apply lights
    for (int i = 0; i < baseFrameInfo[0].staticLightCount; i++)
    {
        float3 t_Normal = normalize(input.normal);
        float3 t_Dir = normalize(lights[i].pos - input.fragPos);

        float t_Diff = max(dot(t_Normal, t_Dir), 0.0f);
        t_Diffuse = mul(t_Diff, lights[i].color);
    }

    //Apply the Light colors;
    float4 t_Ambient = float4(mul(baseFrameInfo[0].ambientLight, baseFrameInfo[0].ambientStrength), 1.0f);
    
    float4 t_Result = (t_Ambient + t_Diffuse) * t_Color;
    return t_Result;
}