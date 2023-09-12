#ifdef _VULKAN
#define _BBEXT(num) [[vk::location(num)]]
#define _BBBIND(bind, set) [[vk::binding(bind, set)]]
#define SPACE_IMMUTABLE_SAMPLER 0
#define SPACE_GLOBAL 1
#define SPACE_PER_SCENE 2
#define SPACE_PER_MATERIAL 3
#define SPACE_PER_MESH 4
#elif _DIRECTX12
#define _BBEXT(num)
#define _BBBIND(bind, set)
#define SPACE_IMMUTABLE_SAMPLER 0
#define SPACE_GLOBAL 0
#define SPACE_PER_SCENE 1
#define SPACE_PER_MATERIAL 2
#define SPACE_PER_MESH 3
#else
#define _BBEXT(num)
#define _BBBIND(bind, set)
#define SPACE_IMMUTABLE_SAMPLER 9
#define SPACE_GLOBAL 9
#define SPACE_PER_SCENE 9
#define SPACE_PER_MATERIAL 9
#define SPACE_PER_MESH 9
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

struct BindlessIndices
{
    uint transform;
    uint albedo;
#ifdef _VULKAN
    uint paddingTo64Bytes[14];
#endif
};

#ifdef _VULKAN
    [[vk::push_constant]] BindlessIndices indices;
#elif _DIRECTX12
    ConstantBuffer<BindlessIndices> indices : register(b0, space0);
#endif

//Maybe add in common if I find a way to combine them.
_BBBIND(0, SPACE_GLOBAL) Texture2D text[] : register(t0, space0);
_BBBIND(0, SPACE_PER_SCENE) ByteAddressBuffer sceneBuffer : register(t0, space1);
_BBBIND(2, SPACE_PER_SCENE) ByteAddressBuffer lights : register(t2, space1);

_BBBIND(0, SPACE_IMMUTABLE_SAMPLER) SamplerState samplerColor : register(s0, space0);

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
    SceneInfo t_SceneInfo = sceneBuffer.Load<SceneInfo>(0);
    float4 t_TextureColor = text[indices.albedo].Sample(samplerColor, input.uv);
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