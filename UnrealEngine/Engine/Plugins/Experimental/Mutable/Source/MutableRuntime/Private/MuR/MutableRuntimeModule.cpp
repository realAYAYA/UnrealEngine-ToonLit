// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuR/MutableRuntimeModule.h"

#include "HAL/PlatformMath.h"
#include "HAL/UnrealMemory.h"
#include "Logging/LogMacros.h"
#include "Misc/AssertionMacros.h"
#include "Modules/ModuleManager.h"
#include "MuR/MutableMemory.h"
#include "MuR/Platform.h"

IMPLEMENT_MODULE(FMutableRuntimeModule, MutableRuntime);

DEFINE_LOG_CATEGORY(LogMutableCore);

#if USE_STAT_MUTABLE_MEMORY
DECLARE_MEMORY_STAT(TEXT("Mutable Memory Used"), STAT_MemoryMutableTotalAllocationSize, STATGROUP_Memory);
#endif


// Uncomment the following line to have allocation/deallocation messages printed to the output
//#define MUTABLE_PRINT_ALLOCATIONS

#ifdef MUTABLE_PRINT_ALLOCATIONS

static int64 NumAllocations = 0;

static void* CustomMalloc(std::size_t Size_t, uint32_t alignment)
{
	NumAllocations++;
	UE_LOG(LogTemp, Warning, TEXT("NumAllocations=%d, Allocated %d bytes"), NumAllocations, Size_t);

	return malloc(Size_t);
}


static void CustomFree(void* mem)
{
	NumAllocations--;
	UE_LOG(LogTemp, Warning, TEXT("NumAllocations=%d, Deallocated"), NumAllocations);

	free(mem);
}

#else //MUTABLE_PRINT_ALLOCATIONS



static void* CustomMalloc(std::size_t Size_t, uint32_t alignment)
{
	void* Result = FMemory::Malloc(Size_t, alignment);

#if USE_STAT_MUTABLE_MEMORY
	SIZE_T ActualSize = FMemory::GetAllocSize(Result);
	INC_DWORD_STAT_BY(STAT_MemoryMutableTotalAllocationSize, ActualSize);
#endif
	// This should always be fatal: mutable cannot handle out-of-memory errors yet.
	if (Size_t > 0 && !Result)
	{
		LowLevelFatalError(TEXT("Not enough memory for mutable request %ul."), Size_t);
		check(false);
	}

	return Result;
}


static void CustomFree(void* mem)
{
#if USE_STAT_MUTABLE_MEMORY
	DEC_DWORD_STAT_BY(STAT_MemoryMutableTotalAllocationSize, FMemory::GetAllocSize(mem));
#endif
	return FMemory::Free(mem);
}

#endif //MUTABLE_PRINT_ALLOCATIONS


void FMutableRuntimeModule::StartupModule()
{
	void* (*customMalloc)(size_t, uint32_t) = nullptr;
	void(*customFree)(void*) = nullptr;

#if USE_UNREAL_ALLOC_IN_MUTABLE
	customMalloc = CustomMalloc;
	customFree = CustomFree;
#endif

	mu::Initialize(customMalloc, customFree);
}


void FMutableRuntimeModule::ShutdownModule()
{
	// Finalize the mutable runtime in this module
	mu::Finalize();
}

