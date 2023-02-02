#include "ShaderCompiler.h"

#include <Windows.h>
#include <combaseapi.h>
#include "../../../Libs/DXC/inc/dxcapi.h"

using namespace BB;
using namespace BB::Shader;

struct ShaderCompiler
{
	IDxcUtils* utils;
	IDxcCompiler3* compiler;
	IDxcLibrary* library;
};

static ShaderCompiler shaderCompiler;

void BB::Shader::InitShaderCompiler()
{
	DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&shaderCompiler.utils));
	DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&shaderCompiler.compiler));
	DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(&shaderCompiler.library));
}

const ShaderCodeHandle BB::Shader::CompileShader(const wchar_t* a_FullPath, const wchar_t* a_Entry, const RENDER_SHADER_STAGE a_ShaderType, const RENDER_API a_RenderAPI)
{
	LPCWSTR shaderType;
	switch (a_ShaderType)
	{
	case RENDER_SHADER_STAGE::VERTEX:
		shaderType = L"vs_6_0";
		break;
	case RENDER_SHADER_STAGE::FRAGMENT_PIXEL:
		shaderType = L"ps_6_0";
		break;
	}

	//Lots of arguments, since we will add some extra.
	LPCWSTR t_ShaderCompileArgs[32] =
	{
		a_FullPath,
		L"-E", a_Entry,		// Entry point.
		L"-T", shaderType,	// Shader Type
		L"-Zs"				// Enable debug
	};

	uint32_t t_CompileArgCount = 6; //Current elements inside the standard shader compiler args

	switch (a_RenderAPI)
	{
	case RENDER_API::VULKAN:
		t_ShaderCompileArgs[t_CompileArgCount++] = L"-spirv";
		t_ShaderCompileArgs[t_CompileArgCount++] = L"-D";
		t_ShaderCompileArgs[t_CompileArgCount++] = L"_VULKAN";
		break;
	case RENDER_API::DX12:
		t_ShaderCompileArgs[t_CompileArgCount++] = L"-D";
		t_ShaderCompileArgs[t_CompileArgCount++] = L"_DIRECTX12";
		break;
	}

	IDxcBlobEncoding* t_SourceBlob;
	shaderCompiler.utils->LoadFile(a_FullPath, nullptr, &t_SourceBlob);
	DxcBuffer t_Source;
	t_Source.Ptr = t_SourceBlob->GetBufferPointer();
	t_Source.Size = t_SourceBlob->GetBufferSize();
	t_Source.Encoding = DXC_CP_ACP;

	IDxcResult* t_Result;
	HRESULT t_HR;

	t_HR = shaderCompiler.compiler->Compile(
		&t_Source,
		t_ShaderCompileArgs,
		t_CompileArgCount,
		nullptr,
		IID_PPV_ARGS(&t_Result)
	);

	IDxcBlobUtf8* t_Errors = nullptr;
	t_Result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&t_Errors), nullptr);

	if (t_Errors != nullptr && t_Errors->GetStringLength() != 0)
	{
		wprintf(L"Shader Compilation failed with errors:\n%hs\n",
			(const char*)t_Errors->GetStringPointer());
		t_Errors->Release();
	}

	t_Result->GetStatus(&t_HR);
	if (FAILED(t_HR))
	{
		BB_ASSERT(false, "Failed to load shader.");
	}

	IDxcBlob* t_ShaderCode;
	t_Result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&t_ShaderCode), nullptr);
	if (t_ShaderCode->GetBufferPointer() == nullptr)
	{
		BB_ASSERT(false, "Something went wrong with DXC shader compiling.");
	}

	t_SourceBlob->Release();
	t_Result->Release();

	return ShaderCodeHandle(t_ShaderCode);
}

void BB::Shader::ReleaseShaderCode(const ShaderCodeHandle a_Handle)
{
	reinterpret_cast<IDxcBlob*>(a_Handle.ptrHandle)->Release();
}

void BB::Shader::GetShaderCodeBuffer(const ShaderCodeHandle a_Handle, Buffer& a_Buffer)
{
	a_Buffer.data = reinterpret_cast<IDxcBlob*>(a_Handle.ptrHandle)->GetBufferPointer();
	a_Buffer.size = reinterpret_cast<IDxcBlob*>(a_Handle.ptrHandle)->GetBufferSize();
}