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
struct VSoutput
{
    float4 pos : SV_POSITION;
    _BBEXT(0)  float4 color : COLOR0;
    _BBEXT(1)  float2 uv : TEXCOORD0;
};

struct GuiInfo
{
    float2 uScale;
    float2 uTranslate;
    int textureIndex;
};

#ifdef _VULKAN
    [[vk::push_constant]] GuiInfo guiInfo;
#elif _DIRECTX12
    ConstantBuffer<GuiInfo> guiInfo : register(b0, space0);
#endif

_BBBIND(0, SPACE_IMMUTABLE_SAMPLER) SamplerState samplerColor : register(s0, space0);
//Maybe add in common if I find a way to combine them.
_BBBIND(0, SPACE_GLOBAL) Texture2D text[] : register(t0, space0);

float4 main(VSoutput input) : SV_Target
{
    float4 color = input.color * text[guiInfo.textureIndex].Sample(samplerColor, input.uv);
    return color;
}