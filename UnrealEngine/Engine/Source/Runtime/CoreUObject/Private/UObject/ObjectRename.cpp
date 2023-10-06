// Copyright Epic Games, Inc. All Rights Reserved.

#include "UObject/ObjectRename.h"

#include "Misc/StringBuilder.h"
#include "UObject/Object.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobalsInternal.h"

namespace UE::Object
{
	void RenameLeakedPackage(UPackage* Package)
	{
		FCoreUObjectInternalDelegates::GetOnLeakedPackageRenameDelegate().Broadcast(Package);
		FName NewName = MakeUniqueObjectName(nullptr, UPackage::StaticClass(), Package->GetFName());
		TStringBuilder<FName::StringBufferSize> NewNameString;
		NewNameString << NewName;
		UE_LOG(LogObj, Log, TEXT("Renaming leaked package %s to %s"), *Package->GetName(), *NewNameString);
		Package->Rename(*NewNameString, nullptr, REN_ForceNoResetLoaders | REN_DontCreateRedirectors | REN_NonTransactional);
	}
}
