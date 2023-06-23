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
		LightPool(UploadBuffer& a_UploadBuffer, LinearRenderBuffer& a_GPUBuffer, const uint32_t a_LightCount);
		~LightPool();

		const LightHandle AddLight(const Light& a_Light);
		const LightHandle AddLights(const BB::Slice<Light> a_Lights);
		//If the slice is null then it uploads the entire CPU buffer to the GPU.
		void SubmitLightsToGPU(const RecordingCommandListHandle t_RecordingCmdList, const BB::Slice<LightHandle> a_LightHandles = BB::Slice<LightHandle>()) const;
		void ResetLights();

		const Slice<Light> GetLights() const { return Slice(m_LightsCPU, m_LightCount); };
		const uint32_t GetLightCount() const { return m_LightCount; }
		const uint32_t GetLightMax() const { return m_LightMax; }
		const RenderBufferPart GetBufferAllocInfo() const { return m_BufferPart; }

	private:
		const RBufferHandle m_UploadBuffer;
		uint32_t m_UploadBufferOffset;

		Light* m_LightsCPU;
		RenderBufferPart m_BufferPart;

		uint32_t m_LightCount;
		const uint32_t m_LightMax;
	};


	class LightSystem
	{
	public:
		LightSystem(const uint32_t a_LightAmount);
		~LightSystem();

		LightHandle AddLights(const BB::Slice<Light> a_Lights, const LIGHT_TYPE a_LightType, const RecordingCommandListHandle a_CmdList);
		void UploadLights();

		void UpdateDescriptor(const RDescriptorHandle a_Descriptor);

		void Editor();

		const LightPool& GetLightPool() const { return m_Lights; }

	private:
		UploadBuffer m_UploadBuffer;
		LinearRenderBuffer m_LightGPUBuffer;
		LightPool m_Lights;
	};
}