// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	WeakObjectPtr.cpp: Weak pointer to UObject
=============================================================================*/

#include "UObject/WeakObjectPtr.h"
#include "UObject/Object.h"

DEFINE_LOG_CATEGORY_STATIC(LogWeakObjectPtr, Log, All);

/*-----------------------------------------------------------------------------------------------------------
	Base serial number management.
-------------------------------------------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------------------------------------
	FWeakObjectPtr
-------------------------------------------------------------------------------------------------------------*/

/**  
 * Copy from an object pointer
 * @param Object object to create a weak pointer to
**/
void FWeakObjectPtr::operator=(const class UObject *Object)
{
	if (Object // && UObjectInitialized() we might need this at some point, but it is a speed hit we would prefer to avoid
		)
	{
		ObjectIndex = GUObjectArray.ObjectToIndex((UObjectBase*)Object);
		ObjectSerialNumber = GUObjectArray.AllocateSerialNumber(ObjectIndex);
		checkSlow(SerialNumbersMatch());
	}
	else
	{
		Reset();
	}
}

bool FWeakObjectPtr::IsValid(bool bEvenIfGarbage, bool bThreadsafeTest) const
{
	// This is the external function, so we just pass through to the internal inlined method.
	return Internal_IsValid(bEvenIfGarbage, bThreadsafeTest);
}

bool FWeakObjectPtr::IsValid() const
{
	// Using literals here allows the optimizer to remove branches later down the chain.
	return Internal_IsValid(false, false);
}

bool FWeakObjectPtr::IsStale(bool bIncludingGarbage, bool bThreadsafeTest) const
{
	using namespace UE::Core::Private;

	if (ObjectSerialNumber == 0)
	{
#if UE_WEAKOBJECTPTR_ZEROINIT_FIX
		checkSlow(ObjectIndex == InvalidWeakObjectIndex); // otherwise this is a corrupted weak pointer
#else
		checkSlow(ObjectIndex == 0 || ObjectIndex == -1); // otherwise this is a corrupted weak pointer
#endif
		return false;
	}

	if (ObjectIndex < 0)
	{
		return true;
	}
	FUObjectItem* ObjectItem = GUObjectArray.IndexToObject(ObjectIndex);
	if (!ObjectItem)
	{
		return true;
	}
	if (!SerialNumbersMatch(ObjectItem))
	{
		return true;
	}
	if (bThreadsafeTest)
	{
		return false;
	}
	return GUObjectArray.IsStale(ObjectItem, bIncludingGarbage);
}

UObject* FWeakObjectPtr::Get(/*bool bEvenIfGarbage = false*/) const
{
	// Using a literal here allows the optimizer to remove branches later down the chain.
	return Internal_Get(false);
}

UObject* FWeakObjectPtr::Get(bool bEvenIfGarbage) const
{
	return Internal_Get(bEvenIfGarbage);
}

UObject* FWeakObjectPtr::GetEvenIfUnreachable() const
{
	UObject* Result = nullptr;
	if (Internal_IsValid(true, true))
	{
		FUObjectItem* ObjectItem = GUObjectArray.IndexToObject(GetObjectIndex_Private(), true);
		Result = static_cast<UObject*>(ObjectItem->Object);
	}
	return Result;
}

void FWeakObjectPtr::Serialize(FArchive& Ar)
{
	FArchiveUObject::SerializeWeakObjectPtr(Ar, *this);
}
