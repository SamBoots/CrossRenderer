#include "Particle.hpp"
#include "RenderFrontend.h"

using namespace BB;

BB::ParticleManager::ParticleManager(const uint32_t a_MaxSystems, const uint32_t a_MaxEmitters, const uint32_t a_MaxParticles)
	: m_ActiveSystems(allocator, 8)
{
	constexpr uint32_t MEMORY_POOL_COUNT = 4;

	const uint32_t t_SystemsPerPool = a_MaxSystems / MEMORY_POOL_COUNT;
	const uint32_t t_EmittersPerPool = a_MaxEmitters / MEMORY_POOL_COUNT;
	const uint32_t t_ParticlesPerPool = a_MaxParticles / MEMORY_POOL_COUNT;

	m_MemoryPools = reinterpret_cast<ParticleMemoryPool*>(BBalloc(allocator, 4, ParticleMemoryPool));
	for (uint32_t i = 0; i < MEMORY_POOL_COUNT; i++)
		new (&m_MemoryPools[i])ParticleMemoryPool(allocator, t_SystemsPerPool, t_EmittersPerPool, t_ParticlesPerPool);
}

ParticleMemoryPool::ParticleMemoryPool(Allocator a_SystemAllocator, const uint32_t a_MaxSystems, const uint32_t a_MaxEmitters, const uint32_t a_MaxParticles)
{
	m_MaxSystems = a_MaxSystems;
	m_MaxEmitters = a_MaxEmitters;
	m_MaxParticles = a_MaxParticles;

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