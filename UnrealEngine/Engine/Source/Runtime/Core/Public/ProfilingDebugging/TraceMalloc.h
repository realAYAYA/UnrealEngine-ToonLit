// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "MemoryTrace.h"
#include "HAL/MemoryBase.h"

class FTraceMalloc : public FMalloc
{
public:
	FTraceMalloc(FMalloc* InMalloc);
	virtual ~FTraceMalloc();
	
	virtual void* Malloc(SIZE_T Count, uint32 Alignment) override;
	virtual void* Realloc(void* Original, SIZE_T Count, uint32 Alignment) override;
	virtual void Free(void* Original) override;

	UE_DEPRECATED(5.1, "ShouldTrace is deprecated. Details should be internal now.")
	static bool ShouldTrace();

	virtual void OnMallocInitialized() override
	{
		WrappedMalloc->OnMallocInitialized();
	}

	virtual void OnPreFork() override
	{
		WrappedMalloc->OnPreFork();
	}

	virtual void OnPostFork() override
	{
		WrappedMalloc->OnPostFork();
	}

	FMalloc* WrappedMalloc;
};

