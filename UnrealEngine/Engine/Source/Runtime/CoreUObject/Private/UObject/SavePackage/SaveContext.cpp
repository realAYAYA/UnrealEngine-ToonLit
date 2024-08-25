// Copyright Epic Games, Inc. All Rights Reserved.

#include "SaveContext.h"

#include "Algo/Find.h"
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

bool FSaveContext::IsUnsaveable(TObjectPtr<UObject> InObject, bool bEmitWarning) const
{
	TObjectPtr<UObject> Culprit;
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
		if (InObject.GetPackage() == GetPackage())
		{
			UE_LOG(LogSavePackage, Warning, TEXT("%s has a deprecated or abstract class outer %s, so it will not be saved"),
				*InObject.GetFullName(), *Culprit.GetFullName());
		}
	}
	return true;
}

ESaveableStatus FSaveContext::GetSaveableStatus(TObjectPtr<UObject> InObject, TObjectPtr<UObject>* OutCulprit, ESaveableStatus* OutCulpritStatus) const
{
	TObjectPtr<UObject> Obj = InObject;
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
		Obj = Obj.GetOuter();
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

ESaveableStatus FSaveContext::GetSaveableStatusNoOuter(TObjectPtr<UObject> Obj) const
{
	// pending kill object are unsaveable
	if (Obj.IsResolved() && !IsValidChecked(Obj))
	{
		return ESaveableStatus::PendingKill;
	}

	// transient object are considered unsaveable if non native
	if (Obj.IsResolved() && Obj->HasAnyFlags(RF_Transient) && !Obj->IsNative())
	{
		return ESaveableStatus::Transient;
	}

	UClass* Class = Obj.GetClass();
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

FSavePackageResultStruct FSaveContext::GetFinalResult()
{
	if (Result != ESavePackageResult::Success)
	{
		return Result;
	}

	ESavePackageResult FinalResult = IsStubRequested() ? ESavePackageResult::GenerateStub : ESavePackageResult::Success;
	FSavePackageResultStruct ResultData(FinalResult, TotalPackageSizeUncompressed,
		SerializedPackageFlags, IsCompareLinker() ? MoveTemp(GetHarvestedRealm().Linker) : nullptr);

	ResultData.SavedAssets = MoveTemp(SavedAssets);
	UClass* PackageClass = UPackage::StaticClass();
	for (TObjectPtr<UObject> Import : GetImports())
	{
		if (Import.IsA(PackageClass))
		{
			ResultData.ImportPackages.Add(Import.GetFName());
		}
	}
	TSet<FName>& SoftPackageReferenceList = GetSoftPackageReferenceList();
	ResultData.SoftPackageReferences = SoftPackageReferenceList.Array();

	return ResultData;
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
		// if the asset type itself is a class (ie. BP) use that to check for auto optional
		UClass* AssetType = Cast<UClass>(Asset);
		AssetType = AssetType ? AssetType : Asset->GetClass();
		bool bAllowedClass = Algo::FindByPredicate(AutomaticOptionalInclusionAssetTypeList, [AssetType](const UClass* InAssetClass)
			{
				return AssetType->IsChildOf(InAssetClass);
			}) != nullptr;
		bIsSaveAutoOptional = IsCooking() && IsSaveOptional() && bAllowedClass;
	}
}

EObjectMark FSaveContext::GetExcludedObjectMarksForGameRealm(const ITargetPlatform* TargetPlatform)
{
	if (TargetPlatform)
	{
		return UE::SavePackageUtilities::GetExcludedObjectMarksForTargetPlatform(TargetPlatform);
	}
	else
	{
		return static_cast<EObjectMark>(OBJECTMARK_NotForTargetPlatform | OBJECTMARK_EditorOnly);
	}
}

const TCHAR* LexToString(ESaveableStatus Status)
{
	static_assert(static_cast<int32>(ESaveableStatus::__Count) == 9);
	switch (Status)
	{
	case ESaveableStatus::Success: return TEXT("is saveable");
	case ESaveableStatus::PendingKill: return TEXT("is pendingkill");
	case ESaveableStatus::Transient: return TEXT("is transient");
	case ESaveableStatus::AbstractClass: return TEXT("has a Class with CLASS_Abstract");
	case ESaveableStatus::DeprecatedClass: return TEXT("has a Class with CLASS_Deprecated");
	case ESaveableStatus::NewerVersionExistsClass: return TEXT("has a Class with CLASS_NewerVersionExists");
	case ESaveableStatus::OuterUnsaveable: return TEXT("has an unsaveable Outer");
	case ESaveableStatus::ClassUnsaveable: return TEXT("has an unsaveable Class");
	case ESaveableStatus::ExcludedByPlatform: return TEXT("is excluded by TargetPlatform");
	default: return TEXT("Unknown");
	}
}