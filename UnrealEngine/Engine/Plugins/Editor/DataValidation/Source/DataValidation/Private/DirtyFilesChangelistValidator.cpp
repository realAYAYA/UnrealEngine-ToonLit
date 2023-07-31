// Copyright Epic Games, Inc. All Rights Reserved.

#include "DirtyFilesChangelistValidator.h"

#include "Algo/Transform.h"
#include "DataValidationChangelist.h"
#include "FileHelpers.h"
#include "ISourceControlChangelistState.h"
#include "ISourceControlModule.h"
#include "ISourceControlProvider.h"
#include "Misc/Paths.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DirtyFilesChangelistValidator)

#define LOCTEXT_NAMESPACE "DirtyFilesChangelistValidation"

FString UDirtyFilesChangelistValidator::GetPackagePath(const UPackage* InPackage)
{
	if (InPackage == nullptr)
	{
		return TEXT("");
	}

	const FString LocalFullPath(InPackage->GetLoadedPath().GetLocalFullPath());

	if (LocalFullPath.IsEmpty())
	{
		return TEXT("");
	}

	return FPaths::ConvertRelativePathToFull(LocalFullPath);
}

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
	TSet<FString> DirtyPackagesPath;
	FEditorFileUtils::GetDirtyPackages(DirtyPackages, FEditorFileUtils::FShouldIgnorePackage::Default);

	Algo::Transform(DirtyPackages, DirtyPackagesPath, UDirtyFilesChangelistValidator::GetPackagePath);

	// Check if any of changelist's files is unsaved
	for (const FSourceControlStateRef& FileState : FileStates)
	{
		if (DirtyPackagesPath.Contains(FileState->GetFilename()))
		{
			AssetFails(InAsset, LOCTEXT("DirtyFilesFound", "This changelist contains unsaved modifications. Please save to proceed."), ValidationErrors);
			return EDataValidationResult::NotValidated;
		}
	}

	AssetPasses(InAsset);
	return EDataValidationResult::Valid;
}

#undef LOCTEXT_NAMESPACE

