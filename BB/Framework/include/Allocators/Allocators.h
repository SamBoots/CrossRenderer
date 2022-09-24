#pragma once
#include <cstdlib>
#include <cstdint>

namespace BB
{	
	namespace allocators
	{
		struct LinearAllocator
		{
			LinearAllocator(const size_t a_Size);
			~LinearAllocator();

			//just delete these for safety, copies might cause errors.
			LinearAllocator(const LinearAllocator&) = delete;
			LinearAllocator(const LinearAllocator&&) = delete;
			LinearAllocator& operator =(const LinearAllocator&) = delete;
			LinearAllocator& operator =(LinearAllocator&&) = delete;

			void* Alloc(size_t a_Size, size_t a_Alignment);
			void Free(void*);
			void Clear();

		private:
			void* m_Start;
			void* m_Buffer;
			uintptr_t m_End;
		};

		struct FixedLinearAllocator
		{
			FixedLinearAllocator(const size_t a_Size);
			~FixedLinearAllocator();

			//just delete these for safety, copies might cause errors.
			FixedLinearAllocator(const FixedLinearAllocator&) = delete;
			FixedLinearAllocator(const FixedLinearAllocator&&) = delete;
			FixedLinearAllocator& operator =(const FixedLinearAllocator&) = delete;
			FixedLinearAllocator& operator =(FixedLinearAllocator&&) = delete;

			void* Alloc(size_t a_Size, size_t a_Alignment);
			void Free(void*);
			void Clear();

		private:
			void* m_Start;
			void* m_Buffer;
#ifdef _DEBUG
			uintptr_t m_End;
#endif //_DEBUG
		};

		struct FreelistAllocator
		{
			FreelistAllocator(const size_t a_Size);
			~FreelistAllocator();

			//just delete these for safety, copies might cause errors.
			FreelistAllocator(const FreelistAllocator&) = delete;
			FreelistAllocator(const FreelistAllocator&&) = delete;
			FreelistAllocator& operator =(const FreelistAllocator&) = delete;
			FreelistAllocator& operator =(FreelistAllocator&&) = delete;

			void* Alloc(size_t a_Size, size_t a_Alignment);
			void Free(void* a_Ptr);
			void Clear();

			struct AllocHeader 
			{
				size_t size;
				size_t adjustment;
			};
			struct FreeBlock
			{
				size_t size;
				FreeBlock* next;
			};


			uint8_t* m_Start = nullptr;
			FreeBlock* m_FreeBlocks;
			size_t m_TotalAllocSize;
		};

		struct POW_FreelistAllocator
		{
			POW_FreelistAllocator(const size_t);
			~POW_FreelistAllocator();

			//just delete these for safety, copies might cause errors.
			POW_FreelistAllocator(const FreelistAllocator&) = delete;
			POW_FreelistAllocator(const FreelistAllocator&&) = delete;
			POW_FreelistAllocator& operator =(const FreelistAllocator&) = delete;
			POW_FreelistAllocator& operator =(FreelistAllocator&&) = delete;

			void* Alloc(size_t a_Size, size_t);
			void Free(void* a_Ptr);
			void Clear() const;

			struct FreeBlock
			{
				size_t size;
				FreeBlock* next;
			};

			struct FreeList
			{
				size_t allocSize;
				size_t fullSize;
				void* start;
				FreeBlock* freeBlock;
			};

			struct AllocHeader
			{
				FreeList* freeList;
			};

			FreeList* m_FreeLists;
			size_t m_FreeBlocksAmount;
		};

		//struct PoolAllocator
		//{
		//	PoolAllocator(const size_t a_ObjectSize, const size_t a_ObjectCount, const size_t a_Alignment);
		//	~PoolAllocator();

		//	//just delete these for safety, copies might cause errors.
		//	PoolAllocator(const PoolAllocator&) = delete;
		//	PoolAllocator(const PoolAllocator&&) = delete;
		//	PoolAllocator& operator =(const PoolAllocator&) = delete;
		//	PoolAllocator& operator =(PoolAllocator&&) = delete;

		//	void* Alloc(size_t a_Size, size_t);
		//	void Free(void* a_Ptr);
		//	void Clear();

		//	size_t m_Alignment;
		//	size_t m_ObjectCount;
		//	void** m_Start = nullptr;
		//	void** m_Pool;
		//};
	}
}



//inline void* operator new(size_t a_Bytes, BB::memory::LinearAllocator* a_Allocator)
//{
//	return a_Allocator->alloc(a_Bytes);
//};