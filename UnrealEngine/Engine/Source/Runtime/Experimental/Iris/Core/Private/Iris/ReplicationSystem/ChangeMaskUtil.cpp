// Copyright Epic Games, Inc. All Rights Reserved.

#include "Iris/ReplicationSystem/ChangeMaskUtil.h"
#include "HAL/UnrealMemory.h"
#include "Misc/MemStack.h"

namespace UE::Net::Private
{

void* FGlobalChangeMaskAllocator::Alloc(uint32 Size, uint32 Alignment)
{
	return FMemory::Malloc(Size, Alignment);
}

void FGlobalChangeMaskAllocator::Free(void* Pointer)
{
	return FMemory::Free(Pointer);
}

FMemStackChangeMaskAllocator::FMemStackChangeMaskAllocator(FMemStackBase* InMemStack)
: MemStack(InMemStack)
{
}

void* FMemStackChangeMaskAllocator::Alloc(uint32 Size, uint32 Alignment)
{ 
	return MemStack->Alloc(Size, Alignment);
}

void FMemStackChangeMaskAllocator::Free(void* Pointer)
{
}

}
