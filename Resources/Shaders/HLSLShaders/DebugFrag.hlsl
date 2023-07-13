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

struct BaseFrameInfo
{
    float4x4 view;
    float4x4 proj;
    
    float3 ambientLight;
    float ambientStrength;

    uint lightCount;
    uint3 padding;
};

//pointlight
struct Light
{
    float3 pos;
    float radius;
    float4 color;
};

//Maybe add in common if I find a way to combine them.
_BBBIND(0, 2) ByteAddressBuffer baseFrameInfo : register(t0, space0);
_BBBIND(3, 2) ByteAddressBuffer lights : register(t3, space0);

_BBBIND(0, 0) SamplerState samplerColor : register(s0, space0);
_BBBIND(4, 1) Texture2D text[] : register(t4, space0);

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
    //not loading the entire buffer here.
    BaseFrameInfo t_FrameInfo = baseFrameInfo.Load<BaseFrameInfo>(0);
    float4 t_TextureColor = text.Sample(samplerColor, input.uv);
    float4 t_Color = t_TextureColor * float4(input.color.xyz, 1.0f);
    
    float4 t_Diffuse = 0;
    //Apply lights
    for (int i = 0; i < t_FrameInfo.lightCount; i++)
    {
        Light t_Light = lights.Load<Light>(sizeof(Light) * i);
        float3 t_Normal = normalize(input.normal);
        float3 t_Dir = normalize(t_Light.pos - input.fragPos);

        float t_Diff = max(dot(t_Normal, t_Dir), 0.0f);
        t_Diffuse = mul(t_Diff, t_Light.color);
    }

    //Apply the Light colors;
    float4 t_Ambient = float4(mul(t_FrameInfo.ambientLight.xyz, t_FrameInfo.ambientStrength), 1.0f);
    
    float4 t_Result = (t_Ambient + t_Diffuse) * t_Color;
    return t_Result;
}