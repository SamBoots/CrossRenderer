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

struct VSoutput
{
    float4 pos : SV_POSITION;
    _BBEXT(0)  float2 uv : TEXCOORD0;
    _BBEXT(1)  float4 color : COLOR0;
};

_BBBIND(0, SPACE_IMMUTABLE_SAMPLER)  SamplerState samplerColor : register(s0, space0);
//should be using the SPACE_GLOBAL, but texture manager not yet implemented.
_BBBIND(0, SPACE_PER_SCENE)  Texture2D text : register(t0, space0);

float4 main(VSoutput input) : SV_Target
{
    float4 color = input.color * text.Sample(samplerColor, input.uv);
    return color;
}