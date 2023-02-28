#ifndef COMMON_HLSL
#define COMMON_HLSL

#ifdef _VULKAN
#define _BBEXT(num) [[vk::location(num)]]
#elif _DIRECTX12
#define _BBEXT(num)
#else
#define _BBEXT(num)
#endif


#endif //COMMON_HLSL