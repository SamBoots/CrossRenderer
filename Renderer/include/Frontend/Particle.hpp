#pragma once
#include "RenderFrontendCommon.h"
#include "Storage/Array.h"

//tutorial used
//https://alextardif.com/Particles.html#MemoryManagement

namespace BB
{
	struct Particle
	{
		float2 position;
		float2 velocity;
		float4 color;

		float lifeTime;
		float currentLifeTime;
	};

	class ParticleEmitter
	{
	public:
		ParticleEmitter(Allocator a_SystemAllocator);
		~ParticleEmitter();

		void Initialize(const char* a_Name);

		void Render(ParticleBuffer& a_ParticleBuffer);
		void Update();

		const float CurrentLifeTime() const { return m_CurrentLifeTime; }

		const uint32_t GetDescriptorOffset() const { return m_VertexDescAllocation.offset; }
		const RDescriptor GetDescriptor() const { return m_VertexDescAllocation.descriptor; }

	private:
		const char* m_Name;
		uint32_t m_MemoryPoolID;

		uint32_t m_MinParticles;
		uint32_t m_MaxParticles;

		DescriptorAllocation m_VertexDescAllocation;

		float m_LifeTime;
		float m_CurrentLifeTime;

		//we take a linear amount of particles from a pool at a time.
		//TODO, maybe just get an array of particle pointers?
		uint32_t m_ParticleCount;
		Particle* m_Particles;
	};

	class ParticleSystem
	{
	public:
		ParticleSystem(Allocator a_SystemAllocator);
		void Initialize(const char* a_Name, const uint32_t a_MemoryPoolID);

		void Render(ParticleBuffer& a_ParticleBuffer);

		Slice<ParticleEmitter*> GetEmitters() const
		{
			return Slice(m_Emitters.data(), m_Emitters.size());
		}

	private:
		const char* m_Name;
		uint32_t m_MemoryPoolID;

		RenderBufferPart m_VertexBufferPart;
		Array<ParticleEmitter*> m_Emitters;
	};

	class ParticleMemoryPool
	{
	public:
		ParticleMemoryPool(Allocator a_SystemAllocator, const uint32_t a_MaxSystems, const uint32_t a_MaxEmitters, const uint32_t a_MaxParticles);

		ParticleSystem* GetNewSystem();
		ParticleEmitter* GetNewEmitter();
		Particle* GetNewParticle();

		void FreeSystem(ParticleSystem* a_System);
		void FreeEmitter(ParticleEmitter* a_Emitter);
		void FreeParticle(Particle* a_Particle);

		const size_t OccupancyScore() const
		{
			const size_t t_Score = m_CurrentSystem + m_CurrentEmitter + m_CurrentParticle;
			const size_t t_ReuseScore = m_ReuseCurrentSystem + m_ReuseCurrentEmitter + m_ReuseCurrentParticle;
			//normal occupancy has a higher weight
			return t_Score * 2 + t_ReuseScore;
		}

	private:
		const uint32_t m_MaxSystems;
		const uint32_t m_MaxEmitters;
		const uint32_t m_MaxParticles;

		uint32_t m_CurrentSystem;
		uint32_t m_CurrentEmitter;
		uint32_t m_CurrentParticle;

		ParticleSystem* m_Systems;
		ParticleEmitter* m_Emitters;
		Particle* m_Particles;

		uint32_t m_ReuseCurrentSystem;
		uint32_t m_ReuseCurrentEmitter;
		uint32_t m_ReuseCurrentParticle;

		ParticleSystem** m_ReuseSystems;
		ParticleEmitter** m_ReuseEmitters;
		Particle** m_ReuseParticles;

	};

	class ParticleGPUMemory
	{
	public:
		ParticleGPUMemory(Allocator a_SystemAllocator, const uint32_t a_VertexAmount, const uint32_t a_PoolAmount);
		~ParticleGPUMemory();



	private:
		struct ParticleFrame
		{
			UploadBufferChunk uploadChunk;
			RenderBufferPart vertexBuffer;
			RenderBufferPart indexBuffer;
		};
		//will be replaced by a nice global upload buffer
		UploadBuffer m_VertexUploadBuffer;
		ParticleFrame m_ParticleFrames[3];

		struct ParticleGPUPool
		{
			uint32_t currentVert;
			uint32_t vertexOffset;
			const Vertex* gpuMemory;
		};
		ParticleGPUPool* m_ParticleGPUPools;
	};

	//Handles all the particles and their memory.
	class ParticleManager
	{
	public:
		ParticleManager(const uint32_t a_MaxSystems, const uint32_t a_MaxEmitters, const uint32_t a_MaxParticles);
		~ParticleManager();

		void Update();
		void Render();

	private:
		LinearAllocator_t allocator{ mbSize * 32, "Particle manager allocator" };
		//tripple buffer max

		ParticleGPUMemory m_GPUMemory;
		ParticleMemoryPool* m_MemoryPools;
		Array<ParticleSystem*> m_ActiveSystems;
	};
}