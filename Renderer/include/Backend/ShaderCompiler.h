#pragma once
#include "Common.h"
#include "BBMemory.h"
#include "RenderBackendCommon.h"

namespace BB
{
	namespace Shader
	{
		using ShaderCodeHandle = FrameworkHandle<struct ShaderCodeHandleTag>;

		void InitShaderCompiler();

		const ShaderCodeHandle CompileShader(const wchar_t* a_FullPath, const wchar_t* a_Entry, const RENDER_SHADER_STAGE a_ShaderType, const RENDER_API a_RenderAPI);
		void ReleaseShaderCode(const ShaderCodeHandle a_Handle);

		void GetShaderCodeBuffer(const ShaderCodeHandle a_Handle, Buffer& a_Buffer);
	}
}