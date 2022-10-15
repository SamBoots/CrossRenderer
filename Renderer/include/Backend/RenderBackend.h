#pragma once
#include "TemporaryAllocator.h"
#include "RenderBackendCommon.h"

namespace BB
{
	namespace RenderBackend
	{
		void InitBackend(const RenderBackendCreateInfo& a_CreateInfo);
		void DestroyBackend();

		void Update();
		void ResizeWindow(uint32_t a_X, uint32_t a_Y);

		void CreateShader(const ShaderCreateInfo& t_ShaderInfo);
	};
}