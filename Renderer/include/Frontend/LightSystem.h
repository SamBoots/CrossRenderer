#pragma once
#include "RenderFrontend.h"
#include "RenderBuffers.h"

namespace BB
{
	using LightHandle = FrameworkHandle<struct OSFileHandleTag>;

	struct Light
	{
		float3 pos;
		float radius;
		float4 color;
	};

	class LightPool
	{
	public:
		LightPool(Allocator a_SystemAllocator, LinearRenderBuffer& a_GPUBuffer, const uint32_t a_LightCount);
		~LightPool();

		const LightHandle AddLight(Light& a_Light);
		const LightHandle AddLights(const BB::Slice<Light> a_Lights);
		//If the slice is null then it uploads the entire CPU buffer to the GPU.
		void SubmitLightsToGPU(const RecordingCommandListHandle t_RecordingCmdList, UploadBuffer& a_UploadBuffer, const BB::Slice<LightHandle> a_LightHandles = BB::Slice<LightHandle>()) const;
		void ResetLights();

		const uint32_t GetLightCount() const { return m_LightCount; }
		const uint32_t GetLightMax() const { return m_LightMax; }
		const RenderBufferPart GetBufferAllocInfo() const { return m_BufferPart; }

	private:
		Light* m_LightsCPU;
		RenderBufferPart m_BufferPart;
		uint32_t m_LightCount;
		const uint32_t m_LightMax;
	};
}