#include "Particle.hpp"
#include "RenderFrontend.h"

using namespace BB;

BB::ParticleManager::ParticleManager(const uint32_t a_MaxSystems, const uint32_t a_MaxEmitters, const uint32_t a_MaxParticles)
	: m_ActiveSystems(allocator, 8), m_GPUMemory(a_MaxParticles * 6)
{
	constexpr uint32_t MEMORY_POOL_COUNT = 4;

	const uint32_t t_SystemsPerPool = a_MaxSystems / MEMORY_POOL_COUNT;
	const uint32_t t_EmittersPerPool = a_MaxEmitters / MEMORY_POOL_COUNT;
	const uint32_t t_ParticlesPerPool = a_MaxParticles / MEMORY_POOL_COUNT;

	m_MemoryPools = reinterpret_cast<ParticleMemoryPool*>(BBalloc(allocator, 4, ParticleMemoryPool));
	for (uint32_t i = 0; i < MEMORY_POOL_COUNT; i++)
		new (&m_MemoryPools[i])ParticleMemoryPool(allocator, t_SystemsPerPool, t_EmittersPerPool, t_ParticlesPerPool);
}

ParticleGPUMemory::ParticleGPUMemory(Allocator a_SystemAllocator, const uint32_t a_VertexAmount, const uint32_t a_PoolAmount)
	: m_VertexUploadBuffer(a_VertexAmount * sizeof(Vertex) * 3) //3 is backbuffer amount
{
	const size_t t_ParticleVertexBufferSize = a_VertexAmount * sizeof(Vertex);
	const size_t t_ParticleIndexBufferSize = a_VertexAmount * sizeof(uint32_t);
	//Normally we have 3 backbuffers, in case we don't? Maybe do a check with the renderIO.
	//For now, memory ahoy
	for (size_t i = 0; i < 3; i++)
	{
		m_ParticleFrames[i].uploadChunk = m_VertexUploadBuffer.Alloc(t_ParticleVertexBufferSize);
		m_ParticleFrames[i].vertexBuffer = Render::AllocateFromVertexBuffer(t_ParticleVertexBufferSize);
		m_ParticleFrames[i].indexBuffer = Render::AllocateFromIndexBuffer(t_ParticleIndexBufferSize);
	}

	const size_t t_VerticesPerPool = t_ParticleVertexBufferSize / a_PoolAmount;
	m_ParticleGPUPools = BBnewArr(a_SystemAllocator, a_PoolAmount, ParticleGPUPool);
	for (size_t i = 0; i < a_PoolAmount; i++)
	{
		m_ParticleGPUPools[i].currentVert = 0;
		m_ParticleGPUPools[i].vertexOffset = t_VerticesPerPool * i;
	}
}

ParticleGPUMemory::~ParticleGPUMemory()
{
	//set memory back to vertices again. But should we? 
	//When this gets deleted then the entire application may be shut down.
	//Beter yet, all the memory that we allocate here for the GPU are suballocated from
	//bigger GPU buffers.
}

ParticleMemoryPool::ParticleMemoryPool(Allocator a_SystemAllocator, const uint32_t a_MaxSystems, const uint32_t a_MaxEmitters, const uint32_t a_MaxParticles)
	:	m_MaxSystems(a_MaxSystems), 
		m_MaxEmitters(a_MaxEmitters), 
		m_MaxParticles(a_MaxParticles)
{
	m_CurrentSystem = 0;
	m_CurrentEmitter = 0;
	m_CurrentParticle = 0;

	//we handle constructors and destructors ourselves.
	m_Systems = (ParticleSystem*)BBalloc(a_SystemAllocator, m_MaxSystems * sizeof(ParticleSystem));
	m_Emitters = (ParticleEmitter*)BBalloc(a_SystemAllocator, m_MaxEmitters * sizeof(ParticleEmitter));
	m_Particles = (Particle*)BBalloc(a_SystemAllocator, m_MaxParticles * sizeof(Particle));
	
	//Construct the systems to allocate their required memory.
	for (size_t i = 0; i < m_MaxSystems; i++)
		new (&m_Systems[i])ParticleSystem(a_SystemAllocator);
	for (size_t i = 0; i < m_MaxEmitters; i++)
		new (&m_Emitters[i])ParticleEmitter(a_SystemAllocator);

	m_ReuseCurrentSystem = 0;
	m_ReuseCurrentEmitter = 0;
	m_ReuseCurrentParticle = 0;

	m_ReuseSystems = BBnewArr(a_SystemAllocator, m_MaxSystems, ParticleSystem*);
	m_ReuseEmitters = BBnewArr(a_SystemAllocator, m_MaxEmitters, ParticleEmitter*);
	m_ReuseParticles = BBnewArr(a_SystemAllocator, m_MaxParticles, Particle*);
}

ParticleSystem* ParticleMemoryPool::GetNewSystem()
{
	if (m_CurrentSystem <= m_MaxSystems)
		return &m_Systems[m_CurrentSystem++];
	else if (m_ReuseCurrentSystem > 0)
		return m_ReuseSystems[m_ReuseCurrentSystem--];

	return nullptr;
}

ParticleEmitter* ParticleMemoryPool::GetNewEmitter()
{
	if (m_CurrentEmitter <= m_MaxEmitters)
		return &m_Emitters[m_CurrentEmitter++];
	else if (m_ReuseCurrentEmitter > 0)
		return m_ReuseEmitters[m_ReuseCurrentEmitter--];

	return nullptr;
}

Particle* ParticleMemoryPool::GetNewParticle()
{
	if (m_CurrentParticle <= m_MaxParticles)
		return &m_Particles[m_CurrentParticle++];
	else if (m_ReuseCurrentParticle > 0)
		return m_ReuseParticles[m_ReuseCurrentParticle--];

	return nullptr;
}

void ParticleMemoryPool::FreeSystem(ParticleSystem* a_System)
{
	//do set defaults?
	m_ReuseSystems[m_ReuseCurrentSystem++] = a_System;
}

void ParticleMemoryPool::FreeEmitter(ParticleEmitter* a_Emitter)
{
	//do set defaults?
	m_ReuseEmitters[m_ReuseCurrentEmitter++] = a_Emitter;
}

void ParticleMemoryPool::FreeParticle(Particle* a_Particle)
{
	//do set defaults?
	m_ReuseParticles[m_ReuseCurrentParticle++] = a_Particle;
}

BB::ParticleSystem::ParticleSystem(Allocator a_SystemAllocator)
	:	m_Emitters(a_SystemAllocator)
{
	m_Name = nullptr;
	m_MemoryPoolID = 0;
	memset(&m_VertexBufferPart, 0, sizeof(m_VertexBufferPart));
}

void ParticleSystem::Initialize(const char* a_Name, const uint32_t a_MemoryPoolID)
{
	m_Name = a_Name;
	m_MemoryPoolID = a_MemoryPoolID;
}

void ParticleSystem::Render(ParticleBuffer& a_ParticleBuffer)
{

}