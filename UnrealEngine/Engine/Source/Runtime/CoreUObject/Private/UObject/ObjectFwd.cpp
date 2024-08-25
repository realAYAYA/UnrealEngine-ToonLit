// Copyright Epic Games, Inc. All Rights Reserved.

#include "UObject/ObjectFwd.h"
#include "UObject/Object.h"

namespace UE::CoreUObject::Private
{
UClass* GetClass(UObject* Obj)
{
	return Obj->GetClass();
}

FName GetFName(const UObject* Obj)
{
	return Obj->GetFName();
}

UObject* GetOuter(const UObject* Obj)
{
	return Obj->GetOuter();
}

FString GetPathName( const UObject* Obj, const UObject* StopOuter )
{
	return Obj->GetPathName(StopOuter);
}

UPackage* GetPackage(const UObject* Obj)
{
	return Obj->GetPackage();
}

FString GetFullName( const UObject* Obj, const UObject* StopOuter, EObjectFullNameFlags Flags)
{
	return Obj->GetFullName(StopOuter, Flags);
}

bool HasAnyFlags(const UObject* Obj, int32 FlagsToCheck)
{
	return Obj->HasAnyFlags(EObjectFlags(FlagsToCheck));
}
}
