#pragma once
#include "BBMemory.h"
#include "Common.h"
#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"

namespace BB
{
	struct ModelBufferInfo
	{
		glm::mat4 model;
		glm::mat4 inverseModel;
	};

	enum class TRANSFORM_STATE : uint32_t
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

		//44 bytes class
		glm::vec3 m_Pos; //12
		glm::quat m_Rot; //28
		glm::vec3 m_Scale; //40
		TRANSFORM_STATE m_State = TRANSFORM_STATE::NOT_USED; //44
	};

	using TransformHandle = FrameworkHandle<struct TransformHandleTag>;
	/// <summary>
	/// A special pool that handles Transform allocations.
	/// It has a secondary pool of pointers that all point to memory regions in a CPU exposed GPU buffer.
	/// </summary>
	class TransformPool
	{
	public:
		/// <param name="a_SysAllocator">The allocator that will allocate the pool.</param>
		/// <param name="a_MatrixSize">The amount of matrices you want to allocate. The a_GPUMemoryRegion needs to have enough space to hold them all.</param>
		TransformPool(Allocator a_SysAllocator, const uint32_t a_MatrixSize);
		~TransformPool();

		TransformHandle CreateTransform(const glm::vec3 a_Position);
		TransformHandle CreateTransform(const glm::vec3 a_Position, const glm::vec3 a_Axis, const float a_Radians);
		TransformHandle CreateTransform(const glm::vec3 a_Position, const glm::vec3 a_Axis, const float a_Radians, const glm::vec3 a_Scale);
		void FreeTransform(const TransformHandle a_Handle);
		Transform& GetTransform(const TransformHandle a_Handle) const;

		void UpdateTransforms();

		const uint32_t PoolSize() const;
		const class UploadBuffer& PoolGPUUploadBuffer();
			
	private:
		struct TransformPool_inst* inst;
	};

}