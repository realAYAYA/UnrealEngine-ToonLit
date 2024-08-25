// Copyright Epic Games, Inc. All Rights Reserved.

#include "UObject/ObjectPtr.h"
#include "UObject/Class.h"

#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE

namespace  UE::CoreUObject::Private
{
	FPackedObjectRef GetOuter(FPackedObjectRef);
	FPackedObjectRef GetPackage(FPackedObjectRef);
}

FString FObjectPtr::GetPathName() const
{
	FObjectHandle LocalHandle = Handle;
	if (IsObjectHandleResolved(LocalHandle) && !IsObjectHandleNull(LocalHandle))
	{
		return ResolveObjectHandle(LocalHandle)->GetPathName();
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
		return ResolveObjectHandle(LocalHandle)->GetFName();
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
		return ResolveObjectHandle(LocalHandle)->GetFullName(nullptr, Flags);
	}
	else
	{
		FObjectRef ObjectRef = UE::CoreUObject::Private::MakeObjectRef(UE::CoreUObject::Private::ReadObjectHandlePackedObjectRefNoCheck(LocalHandle));
		return ObjectRef.GetFullName(Flags);
	}
}

FObjectPtr FObjectPtr::GetOuter() const
{
	FObjectHandle LocalHandle = Handle;
	if (IsObjectHandleResolved(LocalHandle) && !IsObjectHandleNull(LocalHandle))
	{
		return FObjectPtr(ResolveObjectHandle(LocalHandle)->GetOuter());
	}
	UE::CoreUObject::Private::FPackedObjectRef PackedRef = UE::CoreUObject::Private::GetOuter(ReadObjectHandlePackedObjectRefNoCheck(LocalHandle));
	FObjectPtr Ptr({ PackedRef.EncodedRef });
	return Ptr;
}

FObjectPtr FObjectPtr::GetPackage() const
{
	FObjectHandle LocalHandle = Handle;
	if (IsObjectHandleResolved(LocalHandle) && !IsObjectHandleNull(LocalHandle))
	{
		return FObjectPtr(ResolveObjectHandle(LocalHandle)->GetPackage());
	}
	UE::CoreUObject::Private::FPackedObjectRef PackedRef = UE::CoreUObject::Private::GetPackage(ReadObjectHandlePackedObjectRefNoCheck(LocalHandle));
	FObjectPtr Ptr({ PackedRef.EncodedRef });
	return Ptr;
}
#endif

bool FObjectPtr::IsA(const UClass* SomeBase) const
{
	checkfSlow(SomeBase, TEXT("IsA(NULL) cannot yield meaningful results"));

	if (const UClass* ThisClass = GetClass())
	{
		return ThisClass->IsChildOf(SomeBase);
	}

	return false;
}