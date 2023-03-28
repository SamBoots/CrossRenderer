#pragma once
#include "RenderFrontend.h"
#include "RenderBuffers.h"

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

		void SubmitLights(const RecordingCommandListHandle t_RecordingCmdList, UploadBuffer& a_UploadBuffer, const BB::Slice<Light> a_Lights);
		void ResetLights();

		const uint32_t GetLightCount() const { return m_LightCount; }
		const uint32_t GetLightMax() const { return m_LightMax; }
		const RenderBufferPart GetBufferAllocInfo() const { return m_BufferPart; }

	private:
		RenderBufferPart m_BufferPart;
		uint32_t m_LightCount;
		const uint32_t m_LightMax;
	};
}