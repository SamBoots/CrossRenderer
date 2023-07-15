#ifdef _VULKAN
#define SPACE_IMMUTABLE_SAMPLER = 0
#define SPACE_GLOBAL = 1
#define SPACE_PER_SCENE = 2
#define SPACE_PER_MATERIAL = 3
#define SPACE_PER_MESH = 4
#define _BBEXT(num) [[vk::location(num)]]
#define _BBBIND(bind, set) [[vk::binding(bind, set)]]
#elif _DIRECTX12
#define SPACE_IMMUTABLE_SAMPLER = 0
#define SPACE_GLOBAL = 0
#define SPACE_PER_SCENE = 1
#define SPACE_PER_MATERIAL = 2
#define SPACE_PER_MESH = 3
#define _BBEXT(num)
#define _BBBIND(bind, set)
#else
#define _BBEXT(num)
#define _BBBIND(bind, set)
#endif

struct SceneInfo
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
_BBBIND(0, SPACE_GLOBAL)    ByteAddressBuffer sceneInfo : register(t1, SPACE_GLOBAL);
_BBBIND(1, SPACE_GLOBAL)    Texture2D text[] : register(t4, SPACE_GLOBAL);
_BBBIND(3, SPACE_PER_SCENE) ByteAddressBuffer lights : register(t3, space0);

_BBBIND(0, SPACE_IMMUTABLE_SAMPLER)  SamplerState samplerColor : register(s0, space0);

struct VSoutput
{
    float4 pos : SV_POSITION;
    _BBEXT(0)  float3 fragPos   : POSITION0;
    _BBEXT(1)  float3 color     : COLOR0;
    _BBEXT(2)  float2 uv        : UV0;
    _BBEXT(3)  float3 normal    : NORMAL0;
};

float4 main(VSoutput input) : SV_Target
{
    //not loading the entire buffer here.
    SceneInfo t_SceneInfo = sceneInfo.Load<SceneInfo>(0);
    float4 t_TextureColor = text.Sample(samplerColor, input.uv);
    float4 t_Color = t_TextureColor * float4(input.color.xyz, 1.0f);
    
    float4 t_Diffuse = 0;
    //Apply lights
    for (int i = 0; i < t_SceneInfo.lightCount; i++)
    {
        Light t_Light = lights.Load<Light>(sizeof(Light) * i);
        float3 t_Normal = normalize(input.normal);
        float3 t_Dir = normalize(t_Light.pos - input.fragPos);

        float t_Diff = max(dot(t_Normal, t_Dir), 0.0f);
        t_Diffuse = mul(t_Diff, t_Light.color);
    }

    //Apply the Light colors;
    float4 t_Ambient = float4(mul(t_SceneInfo.ambientLight.xyz, t_SceneInfo.ambientStrength), 1.0f);
    
    float4 t_Result = (t_Ambient + t_Diffuse) * t_Color;
    return t_Result;
}