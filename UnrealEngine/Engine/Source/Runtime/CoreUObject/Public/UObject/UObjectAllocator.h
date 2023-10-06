// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UnObjAllocator.h: Unreal object allocation
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "HAL/PlatformMath.h"

class UObjectBase;

class FUObjectAllocator
{
public:

	/**
	 * Constructor, initializes to no permanent object pool
	 */
	FUObjectAllocator() :
	  PermanentObjectPoolSize(0),
	  PermanentObjectPool(NULL),
	  PermanentObjectPoolTail(NULL),
		PermanentObjectPoolExceededTail(NULL)
	{
	}

	/**
	 * Allocates and initializes the permanent object pool
	 *
	 * @param InPermanentObjectPoolSize size of permanent object pool
	 */
	COREUOBJECT_API void AllocatePermanentObjectPool(int32 InPermanentObjectPoolSize);

	/**
	 * Prints a debugf message to allow tuning
	 */
	COREUOBJECT_API void BootMessage();

	UE_DEPRECATED(5.1, "Use the more efficient FPermanentObjectPoolExtents instead")
	FORCEINLINE bool ResidesInPermanentPool(const UObjectBase *Object) const
	{
		return ((const uint8*)Object >= PermanentObjectPool) && ((const uint8*)Object < PermanentObjectPoolTail);
	}

	/**
	 * Allocates a UObjectBase from the free store or the permanent object pool
	 *
	 * @param Size size of uobject to allocate
	 * @param Alignment alignment of uobject to allocate
	 * @param bAllowPermanent if true, allow allocation in the permanent object pool, if it fits
	 * @return newly allocated UObjectBase (not really a UObjectBase yet, no constructor like thing has been called).
	 */
	COREUOBJECT_API UObjectBase* AllocateUObject(int32 Size, int32 Alignment, bool bAllowPermanent);

	/**
	 * Returns a UObjectBase to the free store, unless it is in the permanent object pool
	 *
	 * @param Object object to free
	 */
	COREUOBJECT_API void FreeUObject(UObjectBase *Object) const;

private:
	friend class FPermanentObjectPoolExtents;

	/** Size in bytes of pool for objects disregarded for GC.								*/
	int32							PermanentObjectPoolSize;
	/** Begin of pool for objects disregarded for GC.										*/
	uint8*						PermanentObjectPool;
	/** Current position in pool for objects disregarded for GC.							*/
	uint8*						PermanentObjectPoolTail;
	/** Tail that exceeded the size of the permanent object pool, >= PermanentObjectPoolTail.		*/
	uint8*						PermanentObjectPoolExceededTail;
};

/** Global UObjectBase allocator							*/
extern COREUOBJECT_API FUObjectAllocator GUObjectAllocator;

/** Helps check if an object is part of permanent object pool */
class FPermanentObjectPoolExtents
{
public:
	FORCEINLINE FPermanentObjectPoolExtents(const FUObjectAllocator& ObjectAllocator = GUObjectAllocator)
		: Address(reinterpret_cast<uint64>(ObjectAllocator.PermanentObjectPool))
		, Size(static_cast<uint64>(ObjectAllocator.PermanentObjectPoolSize))
	{}
	
	FORCEINLINE bool Contains(const UObjectBase* Object) const
	{
		return reinterpret_cast<uint64>(Object) - Address < Size;
	}

private:
	const uint64 Address;
	const uint64 Size;
};
