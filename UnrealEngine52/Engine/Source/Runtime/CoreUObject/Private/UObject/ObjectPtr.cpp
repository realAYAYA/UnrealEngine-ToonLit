// Copyright Epic Games, Inc. All Rights Reserved.

#include "UObject/ObjectPtr.h"

#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE

FString FObjectPtr::GetPathName() const
{
	FObjectHandle LocalHandle = Handle;
	if (IsObjectHandleResolved(LocalHandle) && !IsObjectHandleNull(LocalHandle))
	{
		return Get()->GetPathName();
	}
	else
	{
		FObjectRef ObjectRef = UE::CoreUObject::Private::MakeObjectRef(UE::CoreUObject::Private::ReadObjectHandlePackedObjectRefNoCheck(LocalHandle));
		return ObjectRef.GetPathName();
	}
}

FName FObjectPtr::GetFName() const
{
	FObjectHandle LocalHandle = Handle;
	if (IsObjectHandleResolved(LocalHandle) && !IsObjectHandleNull(LocalHandle))
	{
		return Get()->GetFName();
	}
	else
	{
		FObjectRef ObjectRef = UE::CoreUObject::Private::MakeObjectRef(UE::CoreUObject::Private::ReadObjectHandlePackedObjectRefNoCheck(LocalHandle));
		return ObjectRef.GetFName();
	}
}

FString FObjectPtr::GetFullName(EObjectFullNameFlags Flags) const
{
	FObjectHandle LocalHandle = Handle;
	if (IsObjectHandleResolved(LocalHandle) && !IsObjectHandleNull(LocalHandle))
	{
		return Get()->GetFullName(nullptr, Flags);
	}
	else
	{
		FObjectRef ObjectRef = UE::CoreUObject::Private::MakeObjectRef(UE::CoreUObject::Private::ReadObjectHandlePackedObjectRefNoCheck(LocalHandle));
		return ObjectRef.GetFullName(Flags);
	}
}

#endif