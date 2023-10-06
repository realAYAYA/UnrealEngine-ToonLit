// Copyright Epic Games, Inc. All Rights Reserved.

#include "DirtyFilesChangelistValidator.h"

#include "DataValidationChangelist.h"
#include "FileHelpers.h"
#include "ISourceControlModule.h"
#include "Misc/Paths.h"
#include "Containers/Map.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DirtyFilesChangelistValidator)

#define LOCTEXT_NAMESPACE "DirtyFilesChangelistValidation"

bool UDirtyFilesChangelistValidator::CanValidateAsset_Implementation(UObject* InAsset) const
{
	return (InAsset != nullptr) && (UDataValidationChangelist::StaticClass() == InAsset->GetClass());
}

EDataValidationResult UDirtyFilesChangelistValidator::ValidateLoadedAsset_Implementation(UObject* InAsset, TArray<FText>& ValidationErrors)
{
	check(UDataValidationChangelist::StaticClass() == InAsset->GetClass());

	UDataValidationChangelist* DataValidationChangelist = Cast<UDataValidationChangelist>(InAsset);

	if ((DataValidationChangelist == nullptr) || (!DataValidationChangelist->Changelist.IsValid()))
	{
		return EDataValidationResult::Valid;
	}

	// Retrieve files contained in the changelist
	ISourceControlProvider& Provider = ISourceControlModule::Get().GetProvider();
	FSourceControlChangelistStatePtr ChangelistStatePtr = Provider.GetState(DataValidationChangelist->Changelist.ToSharedRef(), EStateCacheUsage::Use);
	const TArray<FSourceControlStateRef>& FileStates = ChangelistStatePtr->GetFilesStates();
	
	// Retrieve current unsaved packages
	TArray<UPackage*> DirtyPackages;
	TMap<FString, const UPackage*> DirtyPackagesPath;
	FEditorFileUtils::GetDirtyPackages(DirtyPackages, FEditorFileUtils::FShouldIgnorePackage::Default);

	auto Predicate = [](const UPackage* InPackage) -> bool
	{
		if (InPackage == nullptr)
		{
			return false;
		}
		
		const FString LocalFullPath(InPackage->GetLoadedPath().GetLocalFullPath());

		if (LocalFullPath.IsEmpty())
		{
			return false;
		}

		return true;
	};

	auto Transform = [](const UPackage* InPackage) -> TTuple<FString, const UPackage*>
	{
		check(InPackage);
		const FString LocalFullPath(InPackage->GetLoadedPath().GetLocalFullPath());
		check(!LocalFullPath.IsEmpty());
		
		return TTuple<FString, const UPackage*>(FPaths::ConvertRelativePathToFull(LocalFullPath), InPackage);
	};
		
	Algo::TransformIf(DirtyPackages, DirtyPackagesPath, Predicate, Transform);

	// Check if any of changelist's files is unsaved
	for (const FSourceControlStateRef& FileState : FileStates)
	{
		if (const UPackage** PackagePtr = DirtyPackagesPath.Find(FileState->GetFilename()))
		{
			const UPackage* Package = *PackagePtr;
			check(Package);
			FText CurrentError = FText::Format(LOCTEXT("DirtyFilesFound.Changelist.Error", "This changelist contains an unsaved asset {0}. Please save to proceed."), FText::FromString(UDataValidationChangelist::GetPrettyPackageName(Package->GetFName())));
			AssetFails(InAsset, CurrentError, ValidationErrors);
		}
	}

	if (GetValidationResult() != EDataValidationResult::Invalid)
	{
		AssetPasses(InAsset);
	}

	return GetValidationResult();
}

#undef LOCTEXT_NAMESPACE

