// Copyright Epic Games, Inc. All Rights Reserved.

#include "EditorValidatorSubsystem.h"

#include "Editor.h"
#include "AssetToolsModule.h"
#include "ObjectTools.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "IDirectoryWatcher.h"
#include "DirectoryWatcherModule.h"
#include "EditorUtilityBlueprint.h"
#include "Settings/EditorLoadingSavingSettings.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"
#include "CookOnTheSide/CookOnTheFlyServer.h"
#include "Logging/MessageLog.h"
#include "Misc/ScopedSlowTask.h"
#include "Misc/DataValidation.h"
#include "AssetRegistry/AssetData.h"
#include "ISourceControlModule.h"
#include "DataValidationChangelist.h"
#include "DataValidationModule.h"
#include "Engine/Level.h"
#include "Editor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(EditorValidatorSubsystem)

#define LOCTEXT_NAMESPACE "EditorValidationSubsystem"

DEFINE_LOG_CATEGORY(LogContentValidation);


UDataValidationSettings::UDataValidationSettings()
	: bValidateOnSave(true)
{

}

UEditorValidatorSubsystem::UEditorValidatorSubsystem()
	: UEditorSubsystem()
{
	bAllowBlueprintValidators = true;
}

void UEditorValidatorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	if (!AssetRegistryModule.Get().IsLoadingAssets())
	{
		RegisterBlueprintValidators();
	}
	else
	{
		// We are still discovering assets, listen for the completion delegate before building the graph
		if (!AssetRegistryModule.Get().OnFilesLoaded().IsBoundToObject(this))
		{
			AssetRegistryModule.Get().OnFilesLoaded().AddUObject(this, &UEditorValidatorSubsystem::RegisterBlueprintValidators);
		}
	}

	// C++ registration
	TArray<UClass*> ValidatorClasses;
	GetDerivedClasses(UEditorValidatorBase::StaticClass(), ValidatorClasses);
	for (UClass* ValidatorClass : ValidatorClasses)
	{
		if (!ValidatorClass->HasAllClassFlags(CLASS_Abstract))
		{
			UPackage* const ClassPackage = ValidatorClass->GetOuterUPackage();
			if (ClassPackage)
			{
				const FName ModuleName = FPackageName::GetShortFName(ClassPackage->GetFName());
				if (FModuleManager::Get().IsModuleLoaded(ModuleName))
				{
					UEditorValidatorBase* Validator = NewObject<UEditorValidatorBase>(GetTransientPackage(), ValidatorClass);
					AddValidator(Validator);
				}
			}
		}
	}

	// Register to SCC pre-submit callback
	ISourceControlModule::Get().RegisterPreSubmitDataValidation(FSourceControlPreSubmitDataValidationDelegate::CreateUObject(this, &UEditorValidatorSubsystem::ValidateChangelistPreSubmit));
}

// Rename to BP validators
void UEditorValidatorSubsystem::RegisterBlueprintValidators()
{
	if (bAllowBlueprintValidators)
	{
		// Locate all validators (include unloaded)
		const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		TArray<FAssetData> AllBPsAssetData;
		AssetRegistryModule.Get().GetAssetsByClass(UEditorUtilityBlueprint::StaticClass()->GetClassPathName(), AllBPsAssetData, true);

		for (FAssetData& BPAssetData : AllBPsAssetData)
		{
			UClass* ParentClass = nullptr;
			FString ParentClassName;
			if (!BPAssetData.GetTagValue(FBlueprintTags::NativeParentClassPath, ParentClassName))
			{
				BPAssetData.GetTagValue(FBlueprintTags::ParentClassPath, ParentClassName);
			}

			if (!ParentClassName.IsEmpty())
			{
				UObject* Outer = nullptr;
				ResolveName(Outer, ParentClassName, false, false);
				ParentClass = FindObject<UClass>(Outer, *ParentClassName);
				if (!ParentClass || !ParentClass->IsChildOf(UEditorValidatorBase::StaticClass()))
				{
					continue;
				}
			}

			// If this object isn't currently loaded, load it
			UObject* ValidatorObject = BPAssetData.ToSoftObjectPath().ResolveObject();
			if (ValidatorObject == nullptr)
			{
				FSoftObjectPathSerializationScope SerializationScope(NAME_None, NAME_None, ESoftObjectPathCollectType::EditorOnlyCollect, ESoftObjectPathSerializeType::AlwaysSerialize);
				ValidatorObject = BPAssetData.ToSoftObjectPath().TryLoad();
			}
			if (ValidatorObject)
			{
				UEditorUtilityBlueprint* ValidatorBlueprint = Cast<UEditorUtilityBlueprint>(ValidatorObject);
				UEditorValidatorBase* Validator = NewObject<UEditorValidatorBase>(GetTransientPackage(), ValidatorBlueprint->GeneratedClass);
				AddValidator(Validator);
			}
		}
	}
}

void UEditorValidatorSubsystem::Deinitialize()
{
	CleanupValidators();

	// Unregister to SCC pre-submit callback
	ISourceControlModule::Get().UnregisterPreSubmitDataValidation();

	Super::Deinitialize();
}

void UEditorValidatorSubsystem::AddValidator(UEditorValidatorBase* InValidator)
{
	if (InValidator)
	{
		Validators.Add(InValidator->GetClass()->GetPathName(), InValidator);
	}
}

void UEditorValidatorSubsystem::CleanupValidators()
{
	Validators.Empty();
}

EDataValidationResult UEditorValidatorSubsystem::IsObjectValid(UObject* InObject, TArray<FText>& ValidationErrors, TArray<FText>& ValidationWarnings, const EDataValidationUsecase InValidationUsecase) const
{
	EDataValidationResult Result = EDataValidationResult::NotValidated;
	
	if (ensure(InObject))
	{
		// First check the class level validation
		FDataValidationContext Context;
		Result = InObject->IsDataValid(Context);
		Context.SplitIssues(ValidationWarnings, ValidationErrors);

		// If the asset is still valid or there wasn't a class-level validation, keep validating with custom validators
		if (Result != EDataValidationResult::Invalid)
		{
			for (auto ValidatorPair : Validators)
			{
				if (ValidatorPair.Value && ValidatorPair.Value->IsEnabled() && ValidatorPair.Value->CanValidate(InValidationUsecase) && ValidatorPair.Value->CanValidateAsset(InObject))
				{
					ValidatorPair.Value->ResetValidationState();
					EDataValidationResult NewResult = ValidatorPair.Value->ValidateLoadedAsset(InObject, ValidationErrors);

					// Don't accidentally overwrite an invalid result with a valid or not-validated one
					if(Result != EDataValidationResult::Invalid)
					{
						Result = NewResult;
					}

					ValidationWarnings.Append(ValidatorPair.Value->GetAllWarnings());

					ensureMsgf(ValidatorPair.Value->IsValidationStateSet(), TEXT("Validator %s did not include a pass or fail state."), *ValidatorPair.Value->GetClass()->GetName());
				}
			}
		}
	}

	return Result;
}

EDataValidationResult UEditorValidatorSubsystem::IsAssetValid(const FAssetData& AssetData, TArray<FText>& ValidationErrors, TArray<FText>& ValidationWarnings, const EDataValidationUsecase InValidationUsecase) const
{
	if (AssetData.IsValid())
	{
		UObject* Obj = AssetData.GetAsset({ ULevel::LoadAllExternalObjectsTag });
		if (Obj)
		{
			return IsObjectValid(Obj, ValidationErrors, ValidationWarnings, InValidationUsecase);
		}
		return EDataValidationResult::NotValidated;
	}

	return EDataValidationResult::Invalid;
}

int32 UEditorValidatorSubsystem::ValidateAssets(TArray<FAssetData> AssetDataList, bool bSkipExcludedDirectories, bool bShowIfNoFailures) const
{
	FValidateAssetsSettings Settings;
	FValidateAssetsResults Results;

	Settings.bSkipExcludedDirectories = bSkipExcludedDirectories;
	Settings.bShowIfNoFailures = bShowIfNoFailures;
	
	return ValidateAssetsWithSettings(AssetDataList, Settings, Results);
}

int32 UEditorValidatorSubsystem::ValidateAssetsWithSettings(const TArray<FAssetData>& AssetDataList, const FValidateAssetsSettings& InSettings, FValidateAssetsResults& OutResults) const
{
	FScopedSlowTask SlowTask(1.0f, LOCTEXT("ValidatingDataTask", "Validating Data..."));
	SlowTask.Visibility = ESlowTaskVisibility::ForceVisible;
	SlowTask.MakeDialogDelayed(.1f);

	// Broadcast the Editor event before we start validating. This lets other systems (such as Sequencer) restore the state
	// of the level to what is actually saved on disk before performing validation.
	if (FEditorDelegates::OnPreAssetValidation.IsBound())
	{
		FEditorDelegates::OnPreAssetValidation.Broadcast();
	}

	FMessageLog DataValidationLog("AssetCheck");

	int32 NumAdded = 0;

	int32 NumFilesChecked = 0;
	int32 NumValidFiles = 0;
	int32 NumInvalidFiles = 0;
	int32 NumFilesSkipped = 0;
	int32 NumFilesUnableToValidate = 0;
	int32 NumFilesWithWarnings = 0;

	int32 NumFilesToValidate = AssetDataList.Num();

	auto AddAssetLogTokens = [&DataValidationLog](EMessageSeverity::Type Severity, const TArray<FText>& TextMessages, FName PackageName)
	{
		const FString PackageNameString = PackageName.ToString();
		for (const FText& Msg : TextMessages)
		{
			const FString AssetLogString = FAssetMsg::GetAssetLogString(*PackageNameString, Msg.ToString());
			FString BeforeAsset;
			FString AfterAsset;
			TSharedRef<FTokenizedMessage> TokenizedMessage = DataValidationLog.Message(Severity);
			if (AssetLogString.Split(PackageNameString, &BeforeAsset, &AfterAsset))
			{
				if (!BeforeAsset.IsEmpty())
				{
					TokenizedMessage->AddToken(FTextToken::Create(FText::FromString(BeforeAsset)));
				}
				TokenizedMessage->AddToken(FAssetNameToken::Create(PackageNameString));
				if (!AfterAsset.IsEmpty())
				{
					TokenizedMessage->AddToken(FTextToken::Create(FText::FromString(AfterAsset)));
				}
			}
			else
			{
				TokenizedMessage->AddToken(FTextToken::Create(FText::FromString(AssetLogString)));
			}
		}
	};

	// Now add to map or update as needed
	for (const FAssetData& Data : AssetDataList)
	{
		FText ValidatingMessage = FText::Format(LOCTEXT("ValidatingFilename", "Validating {0}"), FText::FromString(Data.GetFullName()));
		SlowTask.EnterProgressFrame(1.0f / NumFilesToValidate, ValidatingMessage);

		// Check exclusion path
		if (InSettings.bSkipExcludedDirectories && IsPathExcludedFromValidation(Data.PackageName.ToString()))
		{
			++NumFilesSkipped;
			continue;
		}

		const bool bLoadAsset = false;
		if (!InSettings.bLoadAssetsForValidation && !Data.FastGetAsset(bLoadAsset))
		{
			++NumFilesSkipped;
			continue;
		}

		UE_LOG(LogContentValidation, Display, TEXT("%s"), *ValidatingMessage.ToString());

		TArray<FText> ValidationErrors;
		TArray<FText> ValidationWarnings;
		EDataValidationResult Result = IsAssetValid(Data, ValidationErrors, ValidationWarnings, InSettings.ValidationUsecase);
		++NumFilesChecked;

		AddAssetLogTokens(EMessageSeverity::Error, ValidationErrors, Data.PackageName);

		if (ValidationWarnings.Num() > 0)
		{
			++NumFilesWithWarnings;

			AddAssetLogTokens(EMessageSeverity::Warning, ValidationWarnings, Data.PackageName);
		}

		if (Result == EDataValidationResult::Valid)
		{
			if (ValidationWarnings.Num() > 0)
			{
				DataValidationLog.Info()->AddToken(FAssetNameToken::Create(Data.PackageName.ToString()))
					->AddToken(FTextToken::Create(LOCTEXT("ContainsWarningsResult", "contains valid data, but has warnings.")));
			}
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
				if (InSettings.bShowIfNoFailures)
				{
					DataValidationLog.Info()->AddToken(FAssetNameToken::Create(Data.PackageName.ToString()))
						->AddToken(FTextToken::Create(LOCTEXT("NotValidatedDataResult", "has no data validation.")));
				}
				++NumFilesUnableToValidate;
			}
		}
	}

	const bool bFailed = (NumInvalidFiles > 0);
	const bool bAtLeastOneWarning = (NumFilesWithWarnings > 0);

	OutResults.NumChecked = NumFilesChecked;
	OutResults.NumValid = NumValidFiles;
	OutResults.NumInvalid = NumInvalidFiles;
	OutResults.NumSkipped = NumFilesSkipped;
	OutResults.NumWarnings = NumFilesWithWarnings;
	OutResults.NumUnableToValidate = NumFilesUnableToValidate;

	if (bFailed || bAtLeastOneWarning || InSettings.bShowIfNoFailures)
	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("Result"), bFailed ? LOCTEXT("Failed", "FAILED") : LOCTEXT("Succeeded", "SUCCEEDED"));
		Arguments.Add(TEXT("NumChecked"), NumFilesChecked);
		Arguments.Add(TEXT("NumValid"), NumValidFiles);
		Arguments.Add(TEXT("NumInvalid"), NumInvalidFiles);
		Arguments.Add(TEXT("NumSkipped"), NumFilesSkipped);
		Arguments.Add(TEXT("NumUnableToValidate"), NumFilesUnableToValidate);

		DataValidationLog.Info()->AddToken(FTextToken::Create(FText::Format(LOCTEXT("SuccessOrFailure", "Data validation {Result}."), Arguments)));
		DataValidationLog.Info()->AddToken(FTextToken::Create(FText::Format(LOCTEXT("ResultsSummary", "Files Checked: {NumChecked}, Passed: {NumValid}, Failed: {NumInvalid}, Skipped: {NumSkipped}, Unable to validate: {NumUnableToValidate}"), Arguments)));

		DataValidationLog.Open(EMessageSeverity::Info, true);
	}

	// Broadcast now that we're complete so other systems can go back to their previous state.
	if (FEditorDelegates::OnPostAssetValidation.IsBound())
	{
		FEditorDelegates::OnPostAssetValidation.Broadcast();
	}

	return NumInvalidFiles + NumFilesWithWarnings;
}

void UEditorValidatorSubsystem::ValidateOnSave(TArray<FAssetData> AssetDataList) const
{
	ValidateOnSave(AssetDataList, false /* bProceduralSave */);
}

void UEditorValidatorSubsystem::ValidateOnSave(TArray<FAssetData> AssetDataList, bool bProceduralSave) const
{
	// Only validate if enabled and not auto saving
	if (!GetDefault<UDataValidationSettings>()->bValidateOnSave || GEditor->IsAutosaving())
	{
		return;
	}

	if ((!bValidateAssetsWhileSavingForCook && bProceduralSave))
	{
		return;
	}

	FMessageLog DataValidationLog("AssetCheck");
	FText SavedAsset = AssetDataList.Num() == 1 ? FText::FromName(AssetDataList[0].AssetName) : LOCTEXT("MultipleErrors", "multiple assets");
	DataValidationLog.NewPage(FText::Format(LOCTEXT("DataValidationLogPage", "Asset Save: {0}"), SavedAsset));

	FValidateAssetsSettings Settings;
	FValidateAssetsResults Results;

	Settings.bSkipExcludedDirectories = true;
	Settings.bShowIfNoFailures = false;
	Settings.ValidationUsecase = EDataValidationUsecase::Save;
	Settings.bLoadAssetsForValidation = false;

	if (ValidateAssetsWithSettings(AssetDataList, Settings, Results) > 0)
	{
		const FText ErrorMessageNotification = FText::Format(
			LOCTEXT("ValidationFailureNotification", "Validation failed when saving {0}, check Data Validation log"), SavedAsset);
		DataValidationLog.Notify(ErrorMessageNotification, EMessageSeverity::Warning, /*bForce=*/ true);
	}
}

void UEditorValidatorSubsystem::ValidateSavedPackage(FName PackageName)
{
	ValidateSavedPackage(PackageName, false /* bProceduralSave */);
}

void UEditorValidatorSubsystem::ValidateSavedPackage(FName PackageName, bool bProceduralSave)
{
	// Only validate if enabled and not auto saving
	if (!GetDefault<UDataValidationSettings>()->bValidateOnSave || GEditor->IsAutosaving())
	{
		return;
	}

	// For performance reasons, don't validate when making a procedural save by default. Assumption is we validated when saving previously. 
	if ((!bValidateAssetsWhileSavingForCook && bProceduralSave))
	{
		return;
	}

	if (SavedPackagesToValidate.Num() == 0)
	{
		GEditor->GetTimerManager()->SetTimerForNextTick(this, &UEditorValidatorSubsystem::ValidateAllSavedPackages);
	}

	SavedPackagesToValidate.AddUnique(PackageName);
}

bool UEditorValidatorSubsystem::IsPathExcludedFromValidation(const FString& Path) const
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

void UEditorValidatorSubsystem::ValidateAllSavedPackages()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UEditorValidatorSubsystem::ValidateAllSavedPackages);

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	// Prior to validation, make sure Asset Registry is updated. This is done by ticking the DirectoryWatcher module, which 
	// is responsible of scanning modified asset files.
	if( !FApp::IsProjectNameEmpty() )
	{
		static FName DirectoryWatcherName("DirectoryWatcher");
		FDirectoryWatcherModule& DirectoryWatcherModule = FModuleManager::Get().LoadModuleChecked<FDirectoryWatcherModule>(DirectoryWatcherName);
		DirectoryWatcherModule.Get()->Tick(1.f);
	}
	// We need to query the in-memory data as the disk cache may not be accurate
	FARFilter Filter;
	Filter.PackageNames = SavedPackagesToValidate;
	Filter.bIncludeOnlyOnDiskAssets = false;

	TArray<FAssetData> Assets;
	AssetRegistry.GetAssets(Filter, Assets);

	bool bProceduralSave = false; // The optional suppression for ProceduralSaves was checked before adding to SavedPackagesToValidate
	ValidateOnSave(MoveTemp(Assets), bProceduralSave);

	SavedPackagesToValidate.Empty();
}

void UEditorValidatorSubsystem::ValidateChangelistPreSubmit(FSourceControlChangelistPtr InChangelist, EDataValidationResult& OutResult, TArray<FText>& OutValidationErrors, TArray<FText>& OutValidationWarnings) const
{
	FScopedSlowTask SlowTask(1.0f, LOCTEXT("ValidatingChangelistTask", "Validating changelist..."));
	SlowTask.Visibility = ESlowTaskVisibility::Invisible;
	SlowTask.MakeDialogDelayed(.1f);

	check(InChangelist);

	// Create temporary changelist object to do most of the heavy lifting
	UDataValidationChangelist* Changelist = NewObject<UDataValidationChangelist>();
	Changelist->Initialize(InChangelist);
	Changelist->AddToRoot();

	SlowTask.EnterProgressFrame(1.0f, LOCTEXT("ValidatingChangelist", "Validating changelist to submit"));
	OutResult = IsObjectValid(Changelist, OutValidationErrors, OutValidationWarnings, EDataValidationUsecase::PreSubmit);

	Changelist->RemoveFromRoot();
}

#undef LOCTEXT_NAMESPACE

