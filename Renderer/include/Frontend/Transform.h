#pragma once
#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"

#include "Storage/Array.h"

namespace BB
{
	enum class TRANSFORM_STATE : uint64_t
	{
		NOT_USED = 0,
		NO_ACTION = 1,
		REBUILD_MATRIX = 2
	};

	class Transform
	{
	public:
		Transform(const glm::vec3 a_Position);
		Transform(const glm::vec3 a_Position, const glm::vec3 a_Axis, const float a_Radians);
		Transform(const glm::vec3 a_Position, const glm::vec3 a_Axis, const float a_Radians, const glm::vec3 a_Scale);

		void Translate(const glm::vec3 a_Translation);
		void Rotate(const glm::vec3 a_Axis, const float a_Radians);

		void SetPosition(const glm::vec3 a_Position);
		void SetRotation(const glm::vec3 a_Axis, const float a_Radians);
		void SetScale(const glm::vec3 a_Scale);

		const TRANSFORM_STATE GetState() const { return m_State; };
		glm::mat4 CreateModelMatrix();
	private:
		//48 bytes class
		glm::vec3 m_Pos;
		glm::quat m_Rot;
		glm::vec3 m_Scale;
		TRANSFORM_STATE m_State = TRANSFORM_STATE::NOT_USED;
	};

	/// <summary>
	/// A special pool that handles Transform allocations.
	/// It has a secondary pool of pointers that all point to memory regions in a CPU exposed GPU buffer.
	/// </summary>
	class TransformPool
	{
	public:
		/// <param name="a_SysAllocator">The allocator that will allocate the pool.</param>
		/// <param name="a_GPUMemoryRegion">This must point to a valid CPU readable GPU buffer, the transforms will directly write to the memory regions.</param>
		/// <param name="a_MatrixSize">The amount of matrices you want to allocate. The a_GPUMemoryRegion needs to have enough space to hold them all.</param>
		TransformPool(Allocator a_SysAllocator, void* a_GPUMemoryRegion, const uint32_t a_MatrixSize);
		Transform& GetTransform();

		void UpdateTransforms();
			
	private:
		Array<Transform> m_Pool;
		//This points to a CPU readable GPU buffer used for copying 
		//the model matrices to a GPU only readable buffer.
		Array<void*> m_TransferBufferRegions;
	};

}