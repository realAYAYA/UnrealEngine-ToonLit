// Copyright Epic Games, Inc. All Rights Reserved.

#include "UObject/ObjectPtr.h"

#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE

FString FObjectPtr::GetPathName() const
{
	if (IsResolved())
	{
		return Get()->GetPathName();
	}
	else
	{
		FObjectRef ObjectRef = MakeObjectRef(Handle);
		return ObjectRef.GetPathName();
	}
}

FName FObjectPtr::GetFName() const
{
	if (IsResolved())
	{
		UObject* ResolvedObject = Get();
		return ResolvedObject ? ResolvedObject->GetFName() : NAME_None;
	}
	else
	{
		FObjectRef ObjectRef = MakeObjectRef(Handle);
		return ObjectRef.GetFName();
	}
}

FString FObjectPtr::GetFullName(EObjectFullNameFlags Flags) const
{
	if (IsResolved())
	{
		// UObjectBaseUtility::GetFullName is safe to call on null objects.
		return Get()->GetFullName(nullptr, Flags);
	}
	else
	{
		FObjectRef ObjectRef = MakeObjectRef(Handle);
		return ObjectRef.GetFullName(Flags);
	}
}

#endif