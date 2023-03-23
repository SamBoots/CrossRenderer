#ifndef COMMON_HLSL
#define COMMON_HLSL

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

//Maybe add in common if I find a way to combine them.
StructuredBuffer<BaseFrameInfo> baseFrameInfo : register(t0, space0);

#endif //COMMON_HLSL