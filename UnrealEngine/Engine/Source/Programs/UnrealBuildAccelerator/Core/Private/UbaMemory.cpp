// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaMemory.h"
#include "UbaPlatform.h"

namespace uba
{
	ANALYSIS_NORETURN void FatalError(u32 code, const tchar* format, ...);


	MemoryBlock::MemoryBlock(u64 reserveSize_, void* baseAddress_)
	{
		Init(reserveSize_, baseAddress_);
	}

	MemoryBlock::MemoryBlock(u8* baseAddress_)
	{
		memory = baseAddress_;
	}

	MemoryBlock::~MemoryBlock()
	{
		Deinit();
	}

	void MemoryBlock::Init(u64 reserveSize_, void* baseAddress_)
	{
		reserveSize = AlignUp(reserveSize_, 1024 * 1024);

		#if PLATFORM_WINDOWS
		memory = (u8*)VirtualAlloc(baseAddress_, reserveSize, MEM_RESERVE, PAGE_READWRITE); // Max size of obj file?
		if (!memory)
			FatalError(1347, TC("Failed to reserve virtual memory (%u)"), GetLastError());
		#else
		memory = (u8*)mmap(baseAddress_, reserveSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		if (memory == MAP_FAILED)
			FatalError(1347, "mmap failed to reserve %llu bytes: %s", reserveSize, strerror(errno));
		#endif

		if (baseAddress_ && baseAddress_ != memory)
			FatalError(9881, TC("Failed to reserve virtual memory at address (%u)"), GetLastError());
	}

	void MemoryBlock::Deinit()
	{
		if (!memory)
			return;

		#if PLATFORM_WINDOWS
		VirtualFree(memory, 0, MEM_RELEASE);
		#else
		if (munmap(memory, reserveSize) == -1)
			FatalError(9885, "munmap failed to free %llu bytes: %s", reserveSize, strerror(errno));
		#endif
		memory = nullptr;
	}

	void* MemoryBlock::Allocate(u64 bytes, u64 alignment, const tchar* hint)
	{
		if (!memory)
			return aligned_alloc(alignment, bytes);

		SCOPED_WRITE_LOCK(lock, l);
		return AllocateNoLock(bytes, alignment, hint);
	}

	void* MemoryBlock::AllocateNoLock(u64 bytes, u64 alignment, const tchar* hint)
	{
		u64 startPos = AlignUp(writtenSize, alignment);
		u64 newPos = startPos + bytes;
		
		if (newPos > reserveSize)
			FatalError(9882, TC("Ran out of reserved space . Reserved %llu, Needed %llu (%s)"), reserveSize, newPos, hint);

		#if PLATFORM_WINDOWS
		if (newPos > mappedSize)
		{
			u64 toCommit = AlignUp(newPos - mappedSize, 1024 * 1024);
			if (mappedSize + toCommit > reserveSize)
				toCommit = reserveSize - mappedSize;
			if (!VirtualAlloc(memory + mappedSize, toCommit, MEM_COMMIT, PAGE_READWRITE))
				FatalError(9883, TC("Failed to commit virtual memory for memory block. Total size %llu (%u) (%s)"), mappedSize + toCommit, GetLastError(), hint);
			mappedSize += toCommit;
		}
		#endif

		void* ret = memory + startPos;
		writtenSize = newPos;
		return ret;
	}

	void MemoryBlock::Free(void* p)
	{
		if (!memory)
			aligned_free(p);
	}

	tchar* MemoryBlock::Strdup(const tchar* str)
	{
		u64 len = TStrlen(str);
		u64 memSize = (len + 1) * sizeof(tchar);
		void* mem = Allocate(memSize, sizeof(tchar), TC("Strdup"));
		const void* src = str;
		memcpy(mem, src, memSize);
		return (tchar*)mem;
	}

}
