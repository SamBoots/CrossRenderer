#pragma once
#include "RenderFrontendCommon.h"

namespace BB
{
	enum class MATERIAL_MESH_TYPE : uint32_t
	{
		NORMAL_3D
	};

	struct MaterialCreateInfo
	{
		MATERIAL_MESH_TYPE meshType;

	};

	RMaterialHandle CreateMaterial(const MaterialCreateInfo& a_CreateInfo);
}