// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "GenericPlatform/GenericPlatformMemory.h"
#include "Windows/WindowsSystemIncludes.h"

#include <malloc.h>

class FString;
class FMalloc;
struct FGenericMemoryStats;

/**
 *	Windows implementation of the FGenericPlatformMemoryStats.
 */
struct FPlatformMemoryStats
	: public FGenericPlatformMemoryStats
{
	/** Default constructor, clears all variables. */
	FPlatformMemoryStats()
		: FGenericPlatformMemoryStats()
		, WindowsSpecificMemoryStat(0)
		, MemoryPressureStatus(EMemoryPressureStatus::Unknown)
	{ }

	EMemoryPressureStatus GetMemoryPressureStatus() const
	{
		return MemoryPressureStatus;
	}

	/** Example of a memory stat that is specific to Windows. */
	SIZE_T WindowsSpecificMemoryStat;
	/** Status reported by QueryMemoryResourceNotification. */
	EMemoryPressureStatus MemoryPressureStatus;
};


/**
* Windows implementation of the memory OS functions
**/
struct FWindowsPlatformMemory
	: public FGenericPlatformMemory
{
	enum EMemoryCounterRegion
	{
		MCR_Invalid, // not memory
		MCR_Physical, // main system memory
		MCR_GPU, // memory directly a GPU (graphics card, etc)
		MCR_GPUSystem, // system memory directly accessible by a GPU
		MCR_TexturePool, // presized texture pools
		MCR_StreamingPool, // amount of texture pool available for streaming.
		MCR_UsedStreamingPool, // amount of texture pool used for streaming.
		MCR_GPUDefragPool, // presized pool of memory that can be defragmented.
		MCR_SamplePlatformSpecifcMemoryRegion, 
		MCR_PhysicalLLM, // total physical memory displayed in the LLM stats (on consoles CPU + GPU)
		MCR_MAX
	};

	/**
	 * Windows representation of a shared memory region
	 */
	struct FWindowsSharedMemoryRegion : public FSharedMemoryRegion
	{
		/** Returns the handle to file mapping object. */
		Windows::HANDLE GetMapping() const { return Mapping; }

		FWindowsSharedMemoryRegion(const FString& InName, uint32 InAccessMode, void* InAddress, SIZE_T InSize, Windows::HANDLE InMapping)
			:	FSharedMemoryRegion(InName, InAccessMode, InAddress, InSize)
			,	Mapping(InMapping)
		{}

	protected:

		/** Handle of a file mapping object */
		Windows::HANDLE				Mapping;
	};

	//~ Begin FGenericPlatformMemory Interface
	static CORE_API void Init();
	static uint32 GetBackMemoryPoolSize()
	{
		/**
		* Value determined by series of tests on Fortnite with limited process memory.
		* 26MB sufficed to report all test crashes, using 32MB to have some slack.
		* If this pool is too large, use the following values to determine proper size:
		* 2MB pool allowed to report 78% of crashes.
		* 6MB pool allowed to report 90% of crashes.
		*/
		return 32 * 1024 * 1024;
	}

	static CORE_API class FMalloc* BaseAllocator();
	static CORE_API FPlatformMemoryStats GetStats();
	static CORE_API void GetStatsForMallocProfiler( FGenericMemoryStats& out_Stats );
	static CORE_API const FPlatformMemoryConstants& GetConstants();
	static CORE_API bool PageProtect(void* const Ptr, const SIZE_T Size, const bool bCanRead, const bool bCanWrite);
	static CORE_API void* BinnedAllocFromOS( SIZE_T Size );
	static CORE_API void BinnedFreeToOS( void* Ptr, SIZE_T Size );
	static CORE_API void MiMallocInit();

	class FPlatformVirtualMemoryBlock : public FBasicVirtualMemoryBlock
	{
	public:

		FPlatformVirtualMemoryBlock()
		{
		}

		FPlatformVirtualMemoryBlock(void *InPtr, uint32 InVMSizeDivVirtualSizeAlignment)
			: FBasicVirtualMemoryBlock(InPtr, InVMSizeDivVirtualSizeAlignment)
		{
		}
		FPlatformVirtualMemoryBlock(const FPlatformVirtualMemoryBlock& Other) = default;
		FPlatformVirtualMemoryBlock& operator=(const FPlatformVirtualMemoryBlock& Other) = default;

		CORE_API void Commit(size_t InOffset, size_t InSize);
		CORE_API void Decommit(size_t InOffset, size_t InSize);
		CORE_API void FreeVirtual();

		FORCEINLINE void CommitByPtr(void *InPtr, size_t InSize)
		{
			Commit(size_t(((uint8*)InPtr) - ((uint8*)Ptr)), InSize);
		}

		FORCEINLINE void DecommitByPtr(void *InPtr, size_t InSize)
		{
			Decommit(size_t(((uint8*)InPtr) - ((uint8*)Ptr)), InSize);
		}

		FORCEINLINE void Commit()
		{
			Commit(0, GetActualSize());
		}

		FORCEINLINE void Decommit()
		{
			Decommit(0, GetActualSize());
		}

		FORCEINLINE size_t GetActualSize() const
		{
			return VMSizeDivVirtualSizeAlignment * GetVirtualSizeAlignment();
		}

		static CORE_API FPlatformVirtualMemoryBlock AllocateVirtual(size_t Size, size_t InAlignment = FPlatformVirtualMemoryBlock::GetVirtualSizeAlignment());
		static CORE_API size_t GetCommitAlignment();
		static CORE_API size_t GetVirtualSizeAlignment();
	};

	static CORE_API FSharedMemoryRegion* MapNamedSharedMemoryRegion(const FString& InName, bool bCreate, uint32 AccessMode, SIZE_T Size, const void* pSecurityAttributes = nullptr);
	static CORE_API bool UnmapNamedSharedMemoryRegion(FSharedMemoryRegion * MemoryRegion);
	static CORE_API bool GetLLMAllocFunctions(void*(*&OutAllocFunction)(size_t), void(*&OutFreeFunction)(void*, size_t), int32& OutAlignment);
protected:
	friend struct FGenericStatsUpdater;

	static CORE_API void InternalUpdateStats( const FPlatformMemoryStats& MemoryStats );
	//~ End FGenericPlatformMemory Interface
};


typedef FWindowsPlatformMemory FPlatformMemory;
