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

struct VSInput
{
    _BBEXT(0)   float2 inPosition : POSITION;
    _BBEXT(1)  float2 inUV : TEXCOORD0;
    _BBEXT(2)  float4 inColor : COLOR0;
};

struct VSOutput
{
    float4 pos : SV_POSITION;
    _BBEXT(0)   float4 color : COLOR0;
    _BBEXT(1)   float2 uv : TEXCOORD0;
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

VSOutput main(VSInput input)
{
    VSOutput output = (VSOutput) 0;
    output.pos = float4((input.inPosition * guiInfo.uScale) + guiInfo.uTranslate, 0, 1);
    output.color = input.inColor;
    output.uv = input.inUV;
    return output;
}