// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/TransactionObjectEvent.h"
#include "UObject/Object.h"
#include "UObject/Package.h"
#include "UObject/Class.h"

void FTransactionObjectId::SetObject(const UObject* Object)
{
	auto GetPathNameHelper = [](const UObject* ObjectToGet) -> FName
	{
		FNameBuilder ObjectPathNameBuilder;
		ObjectToGet->GetPathName(nullptr, ObjectPathNameBuilder);
		return FName(ObjectPathNameBuilder);
	};

	ObjectPackageName = Object->GetPackage()->GetFName();
	ObjectName = Object->GetFName();
	ObjectPathName = GetPathNameHelper(Object);
	ObjectOuterPathName = Object->GetOuter() ? GetPathNameHelper(Object->GetOuter()) : FName();
	ObjectExternalPackageName = Object->GetExternalPackage() ? Object->GetExternalPackage()->GetFName() : FName();
	ObjectClassPathName = GetPathNameHelper(Object->GetClass());
}
