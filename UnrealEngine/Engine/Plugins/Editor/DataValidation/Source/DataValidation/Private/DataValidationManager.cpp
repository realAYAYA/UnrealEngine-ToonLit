// Copyright Epic Games, Inc. All Rights Reserved.

#include "DataValidationManager.h"

#include "Modules/ModuleManager.h"
#include "MessageLogModule.h"
#include "Logging/MessageLog.h"
#include "Misc/ScopedSlowTask.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"

#include "CoreGlobals.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DataValidationManager)

#define LOCTEXT_NAMESPACE "DataValidationManager"

UDEPRECATED_DataValidationManager* GDataValidationManager = nullptr;

/**
 * UDEPRECATED_DataValidationManager
 */

UDEPRECATED_DataValidationManager::UDEPRECATED_DataValidationManager(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DataValidationManagerClassName = FSoftClassPath(TEXT("/Script/DataValidation.DataValidationManager"));
	bValidateOnSave = true;
}

UDEPRECATED_DataValidationManager* UDEPRECATED_DataValidationManager::Get()
{
	if (GDataValidationManager == nullptr)
	{
		FSoftClassPath DataValidationManagerClassName = (UDEPRECATED_DataValidationManager::StaticClass()->GetDefaultObject<UDEPRECATED_DataValidationManager>())->DataValidationManagerClassName;

		UClass* SingletonClass = DataValidationManagerClassName.TryLoadClass<UObject>();
		checkf(SingletonClass != nullptr, TEXT("Data validation config value DataValidationManagerClassName is not a valid class name."));

		GDataValidationManager = NewObject<UDEPRECATED_DataValidationManager>(GetTransientPackage(), SingletonClass, NAME_None);
		checkf(GDataValidationManager != nullptr, TEXT("Data validation config value DataValidationManagerClassName is not a subclass of UDataValidationManager."))

		GDataValidationManager->AddToRoot();
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		GDataValidationManager->Initialize();
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

	return GDataValidationManager;
}

void UDEPRECATED_DataValidationManager::Initialize()
{
	FMessageLogInitializationOptions InitOptions;
	InitOptions.bShowFilters = true;

	FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
	MessageLogModule.RegisterLogListing("DataValidation", LOCTEXT("DataValidation", "Data Validation"), InitOptions);
}

UDEPRECATED_DataValidationManager::~UDEPRECATED_DataValidationManager()
{
}

EDataValidationResult UDEPRECATED_DataValidationManager::IsObjectValid(UObject* InObject, TArray<FText>& ValidationErrors) const
{
	if (ensure(InObject))
	{
		return InObject->IsDataValid(ValidationErrors);
	}

	return EDataValidationResult::NotValidated;
}

EDataValidationResult UDEPRECATED_DataValidationManager::IsAssetValid(FAssetData& AssetData, TArray<FText>& ValidationErrors) const
{
	if (AssetData.IsValid())
	{
		UObject* Obj = AssetData.GetAsset();
		if (Obj)
		{
			PRAGMA_DISABLE_DEPRECATION_WARNINGS
			return IsObjectValid(Obj, ValidationErrors);
			PRAGMA_ENABLE_DEPRECATION_WARNINGS
		}
	}

	return EDataValidationResult::Invalid;
}

int32 UDEPRECATED_DataValidationManager::ValidateAssets(TArray<FAssetData> AssetDataList, bool bSkipExcludedDirectories, bool bShowIfNoFailures) const
{
	FScopedSlowTask SlowTask(1.0f, LOCTEXT("ValidatingDataTask", "Validating Data..."));
	SlowTask.Visibility = bShowIfNoFailures ? ESlowTaskVisibility::ForceVisible : ESlowTaskVisibility::Invisible;
	if (bShowIfNoFailures)
	{
		SlowTask.MakeDialogDelayed(.1f);
	}

	FMessageLog DataValidationLog("DataValidation");

	int32 NumAdded = 0;

	int32 NumFilesChecked = 0;
	int32 NumValidFiles = 0;
	int32 NumInvalidFiles = 0;
	int32 NumFilesSkipped = 0;
	int32 NumFilesUnableToValidate = 0;

	int32 NumFilesToValidate = AssetDataList.Num();

	// Now add to map or update as needed
	for (FAssetData& Data : AssetDataList)
	{
		SlowTask.EnterProgressFrame(1.0f / NumFilesToValidate, FText::Format(LOCTEXT("ValidatingFilename", "Validating {0}"), FText::FromString(Data.GetFullName())));

		// Check exclusion path
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		if (bSkipExcludedDirectories && IsPathExcludedFromValidation(Data.PackageName.ToString()))
		{
			++NumFilesSkipped;
			continue;
		}
		PRAGMA_ENABLE_DEPRECATION_WARNINGS

		TArray<FText> ValidationErrors;
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		EDataValidationResult Result = IsAssetValid(Data, ValidationErrors);
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
		++NumFilesChecked;

		for (const FText& ErrorMsg : ValidationErrors)
		{
			DataValidationLog.Error()->AddToken(FTextToken::Create(ErrorMsg));
		}

		if (Result == EDataValidationResult::Valid)
		{
			++NumValidFiles;
		}
		else
		{
			if (Result == EDataValidationResult::Invalid)
			{
				DataValidationLog.Info()->AddToken(FAssetNameToken::Create(Data.PackageName.ToString()))
					->AddToken(FTextToken::Create(LOCTEXT("InvalidDataResult", "contains invalid data.")));
				++NumInvalidFiles;
			}
			else if (Result == EDataValidationResult::NotValidated)
			{
				if (bShowIfNoFailures)
				{
					DataValidationLog.Info()->AddToken(FAssetNameToken::Create(Data.PackageName.ToString()))
						->AddToken(FTextToken::Create(LOCTEXT("NotValidatedDataResult", "has no data data validation.")));
				}
				++NumFilesUnableToValidate;
			}
		}
	}

	const bool bFailed = (NumInvalidFiles > 0);

	if (bFailed || bShowIfNoFailures)
	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("Result"), bFailed ? LOCTEXT("Failed", "FAILED") : LOCTEXT("Succeeded", "SUCCEEDED"));
		Arguments.Add(TEXT("NumChecked"), NumFilesChecked);
		Arguments.Add(TEXT("NumValid"), NumValidFiles);
		Arguments.Add(TEXT("NumInvalid"), NumInvalidFiles);
		Arguments.Add(TEXT("NumSkipped"), NumFilesSkipped);
		Arguments.Add(TEXT("NumUnableToValidate"), NumFilesUnableToValidate);

		TSharedRef<FTokenizedMessage> ValidationLog = bFailed ? DataValidationLog.Error() : DataValidationLog.Info();
		ValidationLog->AddToken(FTextToken::Create(FText::Format(LOCTEXT("SuccessOrFailure", "Data validation {Result}."), Arguments)));
		ValidationLog->AddToken(FTextToken::Create(FText::Format(LOCTEXT("ResultsSummary", "Files Checked: {NumChecked}, Passed: {NumValid}, Failed: {NumInvalid}, Skipped: {NumSkipped}, Unable to validate: {NumUnableToValidate}"), Arguments)));

		DataValidationLog.Open(EMessageSeverity::Info, true);
	}

	return NumInvalidFiles;
}

void UDEPRECATED_DataValidationManager::ValidateOnSave(TArray<FAssetData> AssetDataList) const
{
	// Only validate if enabled and not auto saving
	if (!bValidateOnSave || GEditor->IsAutosaving())
	{
		return;
	}

	FMessageLog DataValidationLog("DataValidation");
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	if (ValidateAssets(AssetDataList, true, false) > 0)
	{
		const FText ErrorMessageNotification = FText::Format(
			LOCTEXT("ValidationFailureNotification", "Validation failed when saving {0}, check Data Validation log"),
			AssetDataList.Num() == 1 ? FText::FromName(AssetDataList[0].AssetName) : LOCTEXT("MultipleErrors", "multiple assets"));
		DataValidationLog.Notify(ErrorMessageNotification, EMessageSeverity::Warning, /*bForce=*/ true);
	}
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

void UDEPRECATED_DataValidationManager::ValidateSavedPackage(FName PackageName)
{
	// Only validate if enabled and not auto saving
	if (!bValidateOnSave || GEditor->IsAutosaving())
	{
		return;
	}

	SavedPackagesToValidate.AddUnique(PackageName);

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	GEditor->GetTimerManager()->SetTimerForNextTick(this, &UDEPRECATED_DataValidationManager::ValidateAllSavedPackages);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

bool UDEPRECATED_DataValidationManager::IsPathExcludedFromValidation(const FString& Path) const
{
	for (const FDirectoryPath& ExcludedPath : ExcludedDirectories)
	{
		if (Path.Contains(ExcludedPath.Path))
		{
			return true;
		}
	}

	return false;
}

void UDEPRECATED_DataValidationManager::ValidateAllSavedPackages()
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	// Prior to validation, make sure Asset Registry is updated.
	// DirectoryWatcher is responsible of scanning modified asset files, but validation can be called before.
	if (SavedPackagesToValidate.Num())
	{
		TArray<FString> FilesToScan;
		FilesToScan.Reserve(SavedPackagesToValidate.Num());
		for (FName PackageName : SavedPackagesToValidate)
		{
			FString PackageFilename;
			if (FPackageName::FindPackageFileWithoutExtension(FPackageName::LongPackageNameToFilename(PackageName.ToString()), PackageFilename))
			{
				FilesToScan.Add(PackageFilename);
			}
		}
		if (FilesToScan.Num())
		{
			AssetRegistry.ScanModifiedAssetFiles(FilesToScan);
		}
	}

	TArray<FAssetData> Assets;
	for (FName PackageName : SavedPackagesToValidate)
	{
		// We need to query the in-memory data as the disk cache may not be accurate
		AssetRegistry.GetAssetsByPackageName(PackageName, Assets);
	}

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	ValidateOnSave(Assets);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	SavedPackagesToValidate.Empty();
}

#undef LOCTEXT_NAMESPACE

