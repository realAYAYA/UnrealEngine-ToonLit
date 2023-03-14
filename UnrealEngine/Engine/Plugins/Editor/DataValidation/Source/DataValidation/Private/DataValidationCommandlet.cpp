// Copyright Epic Games, Inc. All Rights Reserved.

#include "DataValidationCommandlet.h"
#include "DataValidationModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetRegistry/AssetRegistryHelpers.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "EditorValidatorSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DataValidationCommandlet)


DEFINE_LOG_CATEGORY_STATIC(LogDataValidation, Warning, All);

// Commandlet for validating data
int32 UDataValidationCommandlet::Main(const FString& FullCommandLine)
{
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
		AssetRegistryModule.Get().GetAssetsByClass(FTopLevelAssetPath(AssetTypeString), AssetDataList, bSearchSubClasses);
	}
	else
	{
		AssetRegistryModule.Get().GetAllAssets(AssetDataList);
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

