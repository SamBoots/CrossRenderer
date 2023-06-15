#pragma once
#include <cstdlib>
#include <cstdint>

namespace BB
{	
#ifdef _DEBUG
#define BB_MEMORY_DEBUG const char* a_File, int a_Line,
#define BB_MEMORY_DEBUG_ARGS __FILE__, __LINE__,
#define BB_MEMORY_DEBUG_SEND a_File, a_Line,
#define BB_MEMORY_DEBUG_FREE nullptr, 0,
#else //No debug
#define BB_MEMORY_DEBUG 
#define BB_MEMORY_DEBUG_ARGS
#define BB_MEMORY_DEBUG_SEND
#define BB_MEMORY_DEBUG_FREE
#endif //_DEBUG
	
	typedef void* (*AllocateFunc)(BB_MEMORY_DEBUG void* a_Allocator, size_t a_Size, const size_t a_Alignment, void* a_OldPtr);
	struct Allocator
	{
		AllocateFunc func;
		void* allocator;
	};

	namespace allocators
	{
		struct BaseAllocator
		{
			BaseAllocator(const char* a_Name = "unnamed") { name = a_Name; };
			//Destructor should be handled by the child types.
			virtual operator Allocator() = 0;

			//realloc is the single allocation call that we make.
			virtual void* Alloc(size_t, size_t) = 0;
			virtual void Free(void*) = 0;
			virtual void Clear();

			//just delete these for safety, copies might cause errors.
			BaseAllocator(const BaseAllocator&) = delete;
			BaseAllocator(const BaseAllocator&&) = delete;
			BaseAllocator& operator =(const BaseAllocator&) = delete;
			BaseAllocator& operator =(BaseAllocator&&) = delete;

			struct AllocationLog
			{
				AllocationLog* prev;
				void* front;
				void* back;
				//maybe not safe due to possibly allocating more then 4 gb.
				uint32_t allocSize;
				const char* file;
				int line;
			}* frontLog = nullptr;
			const char* name;

		protected:
			//Validate the allocator by cheaking for leaks and boundry writes.
			void Validate() const;
		};

		struct LinearAllocator : public BaseAllocator
		{
			LinearAllocator(const size_t a_Size, const char* a_Name = "unnamed");
			~LinearAllocator();

			operator Allocator() override;

			//just delete these for safety, copies might cause errors.
			LinearAllocator(const LinearAllocator&) = delete;
			LinearAllocator(const LinearAllocator&&) = delete;
			LinearAllocator& operator =(const LinearAllocator&) = delete;
			LinearAllocator& operator =(LinearAllocator&&) = delete;

			void* Alloc(size_t a_Size, size_t a_Alignment) override;
			void Free(void*) override;
			void Clear() override;

		private:
			void* m_Start;
			void* m_Buffer;
			uintptr_t m_End;
		};

		struct FixedLinearAllocator : public BaseAllocator
		{
			FixedLinearAllocator(const size_t a_Size, const char* a_Name = "unnamed");
			~FixedLinearAllocator();

			operator Allocator() override;

			//just delete these for safety, copies might cause errors.
			FixedLinearAllocator(const FixedLinearAllocator&) = delete;
			FixedLinearAllocator(const FixedLinearAllocator&&) = delete;
			FixedLinearAllocator& operator =(const FixedLinearAllocator&) = delete;
			FixedLinearAllocator& operator =(FixedLinearAllocator&&) = delete;

			void* Alloc(size_t a_Size, size_t a_Alignment) override;
			void Free(void*) override;
			void Clear() override;

		private:
			void* m_Start;
			void* m_Buffer;
#ifdef _DEBUG
			uintptr_t m_End;
#endif //_DEBUG
		};

		struct FreelistAllocator : public BaseAllocator
		{
			FreelistAllocator(const size_t a_Size, const char* a_Name = "unnamed");
			~FreelistAllocator();

			operator Allocator() override;

			//just delete these for safety, copies might cause errors.
			FreelistAllocator(const FreelistAllocator&) = delete;
			FreelistAllocator(const FreelistAllocator&&) = delete;
			FreelistAllocator& operator =(const FreelistAllocator&) = delete;
			FreelistAllocator& operator =(FreelistAllocator&&) = delete;

			void* Alloc(size_t a_Size, size_t a_Alignment) override;
			void Free(void* a_Ptr) override;
			void Clear() override;

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

		struct POW_FreelistAllocator : public BaseAllocator
		{
			POW_FreelistAllocator(const size_t, const char* a_Name = "unnamed");
			~POW_FreelistAllocator();

			operator Allocator() override;

			//just delete these for safety, copies might cause errors.
			POW_FreelistAllocator(const POW_FreelistAllocator&) = delete;
			POW_FreelistAllocator(const POW_FreelistAllocator&&) = delete;
			POW_FreelistAllocator& operator =(const POW_FreelistAllocator&) = delete;
			POW_FreelistAllocator& operator =(POW_FreelistAllocator&&) = delete;

			void* Alloc(size_t a_Size, size_t) override;
			void Free(void* a_Ptr) override;
			void Clear() override;

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