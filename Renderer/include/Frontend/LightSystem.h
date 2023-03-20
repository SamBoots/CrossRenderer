#pragma once
#include "RenderFrontend.h"

namespace BB
{
	struct Light
	{
		float3 pos;
		float radius;
		float4 color;
	};

	class LightPool
	{
	public:
		LightPool(LinearRenderBuffer& a_GPUBuffer, const uint32_t a_LightCount);
		~LightPool();

		void SubmitLights(const RecordingCommandListHandle t_RecordingCmdList, UploadBuffer& a_UploadBuffer, const Light* a_Lights, const uint32_t a_Count);
		void ResetLights();

	private:
		RenderBufferPart m_BufferPart;
		uint32_t m_LightCount;
		const uint32_t m_LightMax;
	};
}