// Copyright Epic Games, Inc. All Rights Reserved.

#include "SaveContext.h"

#include "Misc/ConfigCacheIni.h"
#include "Serialization/PackageWriter.h"
#include "UObject/UObjectGlobals.h"



TArray<ESaveRealm> FSaveContext::GetHarvestedRealmsToSave()
{
	TArray<ESaveRealm> HarvestedContextsToSave;
	if (IsCooking())
	{
		HarvestedContextsToSave.Add(ESaveRealm::Game);
		if (IsSaveOptional())
		{
			HarvestedContextsToSave.Add(ESaveRealm::Optional);
		}
	}
	else
	{
		HarvestedContextsToSave.Add(ESaveRealm::Editor);
	}
	return HarvestedContextsToSave;
}

void FSaveContext::MarkUnsaveable(UObject* InObject)
{
	if (IsUnsaveable(InObject))
	{
		// TODO: We should not be modifying objects during the save. Besides interfering with the objects outside of the save,
		// it also prevents us from gathering reasons about why an object was marked unsaveable.
		InObject->SetFlags(RF_Transient);
	}

	// if this is the class default object, make sure it's not
	// marked transient for any reason, as we need it to be saved
	// to disk (unless it's associated with a transient generated class)
#if WITH_EDITORONLY_DATA
	ensureAlways(!InObject->HasAllFlags(RF_ClassDefaultObject | RF_Transient) || (InObject->GetClass()->ClassGeneratedBy != nullptr && InObject->GetClass()->HasAnyFlags(RF_Transient)));
#endif
}

bool FSaveContext::IsUnsaveable(UObject* InObject, bool bEmitWarning) const
{
	UObject* Culprit;
	ESaveableStatus CulpritStatus;
	ESaveableStatus Status = GetSaveableStatus(InObject, &Culprit, &CulpritStatus);
	if (Status == ESaveableStatus::Success)
	{
		return false;
	}
	if (Status == ESaveableStatus::OuterUnsaveable && bEmitWarning &&
		(CulpritStatus == ESaveableStatus::AbstractClass || CulpritStatus == ESaveableStatus::DeprecatedClass || CulpritStatus == ESaveableStatus::NewerVersionExistsClass))
	{
		check(Culprit);
		// Only warn if the base object is fine but the outer is invalid. If an object is itself unsaveable, the old behavior is to ignore it
		if (InObject->GetOutermost() == GetPackage())
		{
			UE_LOG(LogSavePackage, Warning, TEXT("%s has a deprecated or abstract class outer %s, so it will not be saved"),
				*InObject->GetFullName(), *Culprit->GetFullName());
		}
	}
	return true;
}

FSaveContext::ESaveableStatus FSaveContext::GetSaveableStatus(UObject* InObject, UObject** OutCulprit, ESaveableStatus* OutCulpritStatus) const
{
	UObject* Obj = InObject;
	while (Obj)
	{
		ESaveableStatus Status = GetSaveableStatusNoOuter(Obj);
		if (Status != ESaveableStatus::Success)
		{
			if (OutCulprit)
			{
				*OutCulprit = Obj;
			}
			if (OutCulpritStatus)
			{
				*OutCulpritStatus = Status;
			}
			return Obj == InObject ? Status : ESaveableStatus::OuterUnsaveable;
		}
		Obj = Obj->GetOuter();
	}
	if (OutCulprit)
	{
		*OutCulprit = InObject;
	}
	if (OutCulpritStatus)
	{
		*OutCulpritStatus = ESaveableStatus::Success;
	}
	return ESaveableStatus::Success;
}

FSaveContext::ESaveableStatus FSaveContext::GetSaveableStatusNoOuter(UObject* Obj) const
{
	// pending kill object are unsaveable
	if (!IsValidChecked(Obj))
	{
		return ESaveableStatus::PendingKill;
	}

	// transient object are considered unsaveable if non native
	if (Obj->HasAnyFlags(RF_Transient) && !Obj->IsNative())
	{
		return ESaveableStatus::Transient;
	}

	UClass* Class = Obj->GetClass();
	// if the object class is abstract, has been marked as deprecated, there is a newer version that exist, or the class is marked transient, then the object is unsaveable
	// @note: Although object instances of a transient class should definitely be unsaveable, it results in discrepancies with the old save algorithm and currently load problems
	if (Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists /*| CLASS_Transient*/)
		&& !Obj->HasAnyFlags(RF_ClassDefaultObject))
	{
		// There used to be a check for reference if the class had the CLASS_HasInstancedReference,
		// but we don't need it because those references are outer-ed to the object being flagged as unsaveable, making them unsaveable as well without having to look for them
		return Class->HasAnyClassFlags(CLASS_Abstract) ? ESaveableStatus::AbstractClass :
			Class->HasAnyClassFlags(CLASS_Deprecated) ? ESaveableStatus::DeprecatedClass :
			ESaveableStatus::NewerVersionExistsClass;
	}

	return ESaveableStatus::Success;
}

namespace
{
	TArray<UClass*> AutomaticOptionalInclusionAssetTypeList;
}

void FSaveContext::SetupHarvestingRealms()
{
	// Create the different harvesting realms
	HarvestedRealms.AddDefaulted((uint32)ESaveRealm::RealmCount);

	// if cooking the default harvesting context is Game, otherwise it's the editor context
	CurrentHarvestingRealm = IsCooking() ? ESaveRealm::Game : ESaveRealm::Editor;

	// Generate the automatic optional context inclusion asset list
	static bool bAssetListGenerated = [](TArray<UClass*>& OutAssetList)
	{
		TArray<FString> AssetList;
		GConfig->GetArray(TEXT("CookSettings"), TEXT("AutomaticOptionalInclusionAssetType"), AssetList, GEditorIni);
		for (const FString& AssetType : AssetList)
		{
			if (UClass* AssetClass = FindObject<UClass>(nullptr, *AssetType, true))
			{
				OutAssetList.Add(AssetClass);
			}
			else
			{
				UE_LOG(LogSavePackage, Warning, TEXT("The asset type '%s' was not found while building the allowlist for automatic optional data inclusion list."), *AssetType);
			}
		}
		return true;
	}(AutomaticOptionalInclusionAssetTypeList);

	if (bAssetListGenerated && Asset)
	{
		bIsSaveAutoOptional = IsCooking() && IsSaveOptional() && AutomaticOptionalInclusionAssetTypeList.Contains(Asset->GetClass());
	}
}

const TCHAR* LexToString(FSaveContext::ESaveableStatus Status)
{
	static_assert(static_cast<int32>(FSaveContext::ESaveableStatus::__Count) == 7);
	switch (Status)
	{
	case FSaveContext::ESaveableStatus::Success: return TEXT("is saveable");
	case FSaveContext::ESaveableStatus::PendingKill: return TEXT("is pendingkill");
	case FSaveContext::ESaveableStatus::Transient: return TEXT("is transient");
	case FSaveContext::ESaveableStatus::AbstractClass: return TEXT("has a Class with CLASS_Abstract");
	case FSaveContext::ESaveableStatus::DeprecatedClass: return TEXT("has a Class with CLASS_Deprecated");
	case FSaveContext::ESaveableStatus::NewerVersionExistsClass: return TEXT("has a Class with CLASS_NewerVersionExists");
	case FSaveContext::ESaveableStatus::OuterUnsaveable: return TEXT("has an unsaveable Outer");
	default: return TEXT("Unknown");
	}
}