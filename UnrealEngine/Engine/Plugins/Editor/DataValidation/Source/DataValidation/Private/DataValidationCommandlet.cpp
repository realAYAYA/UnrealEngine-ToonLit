// Copyright Epic Games, Inc. All Rights Reserved.

#include "DataValidationCommandlet.h"

#include "AssetRegistry/AssetRegistryHelpers.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Blueprint/BlueprintSupport.h"
#include "DataValidationModule.h"
#include "Editor.h"
#include "EditorUtilityBlueprint.h"
#include "EditorValidatorBase.h"
#include "EditorValidatorSubsystem.h"
#include "Misc/PackageName.h"
#include "Misc/PathViews.h"
#include "Misc/Paths.h"
#include "UObject/UnrealType.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DataValidationCommandlet)


DEFINE_LOG_CATEGORY_STATIC(LogDataValidation, Warning, All);

// Commandlet for validating data
int32 UDataValidationCommandlet::Main(const FString& FullCommandLine)
{
	// This commandlet won't work properly when use outside of an editor executable
	check(GEditor);

	UE_LOG(LogDataValidation, Log, TEXT("--------------------------------------------------------------------------------------------"));
	UE_LOG(LogDataValidation, Log, TEXT("Running DataValidation Commandlet"));

	// validate data
	if (!ValidateData(FullCommandLine))
	{
		UE_LOG(LogDataValidation, Warning, TEXT("Errors occurred while validating data"));
		return 2; // return something other than 1 for error since the engine will return 1 if any other system (possibly unrelated) logged errors during execution.
	}

	UE_LOG(LogDataValidation, Log, TEXT("Successfully finished running DataValidation Commandlet"));
	UE_LOG(LogDataValidation, Log, TEXT("--------------------------------------------------------------------------------------------"));
	return 0;
}

//static
bool UDataValidationCommandlet::ValidateData(const FString& FullCommandLine)
{
	TArray<FString> Tokens;
	TArray<FString> Switches;
	TMap<FString, FString> Params;
	ParseCommandLine(*FullCommandLine, Tokens, Switches, Params);

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(AssetRegistryConstants::ModuleName);
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
	AssetRegistry.SearchAllAssets(true /* bSynchronousSearch */);

	bool bProjectOnly = !Switches.Contains(TEXT("includeengine"));

	TArray<FAssetData> AssetDataList;
	FString AssetTypeString;
	if (FParse::Value(*FullCommandLine, TEXT("AssetType="), AssetTypeString) && !AssetTypeString.IsEmpty())
	{
		if (FPackageName::IsShortPackageName(AssetTypeString))
		{
			UClass* Class = FindFirstObject<UClass>(*AssetTypeString, EFindFirstObjectOptions::EnsureIfAmbiguous);
			if (Class)
			{
				AssetTypeString = Class->GetPathName();
			}
			else
			{
				UE_LOG(LogDataValidation, Error, TEXT("Unable to resolve class path name given short name: \"%s\""), *AssetTypeString);
				return false;
			}
		}
		const bool bSearchSubClasses = true;
		AssetRegistry.GetAssetsByClass(FTopLevelAssetPath(AssetTypeString), AssetDataList, bSearchSubClasses);
	}
	else
	{
		AssetRegistry.GetAllAssets(AssetDataList);
	}

	if (bProjectOnly)
	{
		FString EngineDir = FPaths::ConvertRelativePathToFull(FPaths::EngineDir());
		AssetDataList.RemoveAll([&EngineDir](const FAssetData& AssetData)
			{
				// Remove /Engine and any plugins from /Engine, but keep /Game and any plugins under /Game.
				FString FileName;
				FString PackageName;
				AssetData.PackageName.ToString(PackageName);
				if (!FPackageName::TryConvertLongPackageNameToFilename(PackageName, FileName))
				{
					// We don't recognize this packagepath, so keep it
					return false;
				}
				// ConvertLongPackageNameToFilename can return ../../Plugins for some plugins instead of
				// ../../../Engine/Plugins. We should fix that in FPackageName to always return the normalized
				// filename. For now, workaround it by converting to absolute paths.
				FileName = FPaths::ConvertRelativePathToFull(MoveTemp(FileName));
				return FPathViews::IsParentPathOf(EngineDir, FileName);
			});
	}


	if (!GEditor->IsInitialized())
	{
		// Check if we have some BP validator that were created using an editor utility
		const FTopLevelAssetPath EditorUtilityClassPath = UEditorUtilityBlueprint::StaticClass()->GetClassPathName();
		const FString EditorValidatorBaseClassExportPath = FObjectPropertyBase::GetExportPath(UEditorValidatorBase::StaticClass());
		const bool bHasAnEditorUtilityDataValidator = AssetDataList.ContainsByPredicate([EditorUtilityClassPath, &EditorValidatorBaseClassExportPath](const FAssetData& AssetData)
			{
				if (AssetData.AssetClassPath == EditorUtilityClassPath)
				{
					if (AssetData.TagsAndValues.ContainsKeyValue(FBlueprintTags::NativeParentClassPath, EditorValidatorBaseClassExportPath))
					{
						return true;
					}
				}

				return false;
			});

		if (bHasAnEditorUtilityDataValidator)
		{
			// Those Editor Utilities Validator might have an dependency to an editor module that is loaded during the editor initialization.
			GEditor->LoadDefaultEditorModules();
		}
	}


	UEditorValidatorSubsystem* EditorValidationSubsystem = GEditor->GetEditorSubsystem<UEditorValidatorSubsystem>();
	check(EditorValidationSubsystem);

	FValidateAssetsSettings Settings;
	FValidateAssetsResults Results;

	Settings.bSkipExcludedDirectories = true;
	Settings.bShowIfNoFailures = true;
	Settings.ValidationUsecase = EDataValidationUsecase::Commandlet;

	EditorValidationSubsystem->ValidateAssetsWithSettings(AssetDataList, Settings, Results);

	return true;
}

