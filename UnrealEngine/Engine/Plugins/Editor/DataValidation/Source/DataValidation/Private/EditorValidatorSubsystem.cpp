// Copyright Epic Games, Inc. All Rights Reserved.

#include "EditorValidatorSubsystem.h"

#include "Algo/AnyOf.h"
#include "Algo/RemoveIf.h"
#include "AssetCompilingManager.h"
#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetDataToken.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/BlueprintSupport.h"
#include "DataValidationChangelist.h"
#include "EditorValidatorHelpers.h"
#include "DirectoryWatcherModule.h"
#include "Editor.h"
#include "EditorUtilityBlueprint.h"
#include "EditorValidatorBase.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/Level.h"
#include "IDirectoryWatcher.h"
#include "ISourceControlModule.h"
#include "Logging/MessageLog.h"
#include "Misc/App.h"
#include "Misc/DataValidation.h"
#include "Misc/MessageDialog.h"
#include "Misc/PackageName.h"
#include "Misc/ScopedSlowTask.h"
#include "Misc/ScopeExit.h"
#include "UObject/ICookInfo.h"
#include "UObject/TopLevelAssetPath.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(EditorValidatorSubsystem)

#define LOCTEXT_NAMESPACE "EditorValidationSubsystem"

DEFINE_LOG_CATEGORY(LogContentValidation);

FValidateAssetsSettings::FValidateAssetsSettings()
	: ShowMessageLogSeverity(EMessageSeverity::Warning)
	, MessageLogPageTitle(LOCTEXT("DataValidation.MessagePageTitle", "Data Validation"))
{
}

FValidateAssetsSettings::~FValidateAssetsSettings()
{
}

UDataValidationSettings::UDataValidationSettings()
{

}

UEditorValidatorSubsystem::UEditorValidatorSubsystem()
	: UEditorSubsystem()
{
	bAllowBlueprintValidators = true;
}

bool UEditorValidatorSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (GetClass() == UEditorValidatorSubsystem::StaticClass())
	{
		TArray<UClass*> ChildClasses;
		GetDerivedClasses(UEditorValidatorSubsystem::StaticClass(), ChildClasses, true);
		for (UClass* Child : ChildClasses)
		{
			if (Child->GetDefaultObject<UEditorSubsystem>()->ShouldCreateSubsystem(Outer))
			{
				// Do not create this class because one of our child classes wants to be created
				return false;
			}
		}
	}
	return true;
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

bool UEditorValidatorSubsystem::ShouldValidateAsset(
	const FAssetData& 				Asset,
	const FValidateAssetsSettings& 	Settings,
	FDataValidationContext& 		InContext) const
{
	if (Asset.HasAnyPackageFlags(PKG_Cooked))
	{
		return false;
	}

	FNameBuilder AssetPackageNameBuilder(Asset.PackageName);
	FStringView AssetPackageNameView = AssetPackageNameBuilder.ToView();

	if (FPackageName::IsTempPackage(AssetPackageNameView))
	{
		return false;
	}

	if (FPackageName::IsVersePackage(AssetPackageNameView))
	{
		return false;
	}

	if (IsPathExcludedFromValidation(AssetPackageNameView))
	{
		return false;
	}

	return true;
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

			AddValidator(BPAssetData);
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
		UClass* Class = InValidator->GetClass();
		if (Cast<UBlueprintGeneratedClass>(Class))
		{
			if(UObject* ClassGenerator = Class->ClassGeneratedBy)
			{
				Validators.Add(FTopLevelAssetPath(ClassGenerator), InValidator);
			}
		}
		else
		{ 
			Validators.Add(InValidator->GetClass()->GetClassPathName(), InValidator);
		}
	}
}

void UEditorValidatorSubsystem::AddValidator(const FAssetData& InValidatorAssetData)
{
	if (InValidatorAssetData.IsValid())
	{
		Validators.Add(FTopLevelAssetPath(InValidatorAssetData.PackageName, InValidatorAssetData.AssetName), nullptr);
		bNeedLoadingOfValidators = true;
	}
}

void UEditorValidatorSubsystem::RemoveValidator(UEditorValidatorBase* InValidator)
{
	if (InValidator)
	{
		UClass* Class = InValidator->GetClass();
		if (Cast<UBlueprintGeneratedClass>(Class))
		{
			if (UObject* ClassGenerator = Class->ClassGeneratedBy)
			{
				Validators.Remove(FTopLevelAssetPath(ClassGenerator));
			}
		}
		else
		{
			Validators.Remove(InValidator->GetClass()->GetClassPathName());
		}
	}
}

void UEditorValidatorSubsystem::CleanupValidators()
{
	Validators.Empty();
}

void UEditorValidatorSubsystem::ForEachEnabledValidator(TFunctionRef<bool(UEditorValidatorBase* Validator)> Callback) const
{
	LoadValidators();

	for (const TPair<FTopLevelAssetPath, TObjectPtr<UEditorValidatorBase>>& ValidatorPair : Validators)
	{
		if (UEditorValidatorBase* Validator = ValidatorPair.Value;
			Validator && Validator->IsEnabled())
		{
			if (!Callback(Validator))
			{
				break;
			}
		}
	}
}

EDataValidationResult UEditorValidatorSubsystem::IsObjectValid(
	UObject* InObject,
	TArray<FText>& ValidationErrors,
	TArray<FText>& ValidationWarnings,
	const EDataValidationUsecase InValidationUsecase) const
{
	FDataValidationContext Context(false, InValidationUsecase, {}); // No associated objects in this context
	EDataValidationResult Result = IsObjectValidWithContext(InObject, Context);
	Context.SplitIssues(ValidationWarnings, ValidationErrors);
	return Result;
}

EDataValidationResult UEditorValidatorSubsystem::IsAssetValid(
	const FAssetData& AssetData,
	TArray<FText>& ValidationErrors,
	TArray<FText>& ValidationWarnings,
	const EDataValidationUsecase InValidationUsecase) const
{
	if (AssetData.IsValid())
	{
		UObject* Obj = AssetData.GetAsset({ ULevel::LoadAllExternalObjectsTag });
		if (Obj)
		{
			FDataValidationContext Context(false, InValidationUsecase, {}); // No associated objects in this context
			EDataValidationResult Result = ValidateObjectInternal(AssetData, Obj, Context);
			Context.SplitIssues(ValidationWarnings, ValidationErrors);
			return Result;
		}
		return EDataValidationResult::NotValidated;
	}

	return EDataValidationResult::Invalid;
}

EDataValidationResult UEditorValidatorSubsystem::IsObjectValidWithContext(
	UObject* InObject,
	FDataValidationContext& InContext) const
{
	EDataValidationResult Result = EDataValidationResult::NotValidated;
	
	if (ensure(InObject))
	{
		IAssetRegistry& AssetRegistry = IAssetRegistry::GetChecked();

		FAssetData AssetData = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(InObject), true);
		if (!AssetData.IsValid())
		{
			// Construct dynamically with potentially fewer tags 
			AssetData = FAssetData(InObject);
		}
		
		return ValidateObjectInternal(AssetData, InObject, InContext);
	}

	return Result;
}

EDataValidationResult UEditorValidatorSubsystem::IsAssetValidWithContext(
	const FAssetData& AssetData,
	FDataValidationContext& InContext) const
{
	if (AssetData.IsValid())
	{
		UObject* Obj = AssetData.GetAsset({ ULevel::LoadAllExternalObjectsTag });
		if (Obj)
		{
			return ValidateObjectInternal(AssetData, Obj, InContext);
		}
		return EDataValidationResult::NotValidated;
	}

	return EDataValidationResult::Invalid;
}

EDataValidationResult UEditorValidatorSubsystem::ValidateObjectInternal(
	const FAssetData& InAssetData,
	UObject* InObject,
	FDataValidationContext& InContext) const
{
	EDataValidationResult Result = EDataValidationResult::NotValidated;
	
	if (ensure(InObject) && ensure(InAssetData.IsValid()))
	{
		// First check the class level validation
		Result = const_cast<const UObject*>(InObject)->IsDataValid(InContext);

		// If the asset is still valid or there wasn't a class-level validation, keep validating with custom validators
		if (Result == EDataValidationResult::Invalid)
		{
			return Result;
		}
		
		ForEachEnabledValidator([InObject, &InAssetData, &InContext, &Result](UEditorValidatorBase* Validator)
		{
			GInitRunaway(); // Reset runaway counter, as ValidateLoadedAsset may be implemented in a BP and could overflow the runaway count due to being called in a loop
			EDataValidationResult NewResult = Validator->ValidateLoadedAsset(InAssetData, InObject, InContext);
			Result = CombineDataValidationResults(Result, NewResult);
			return true;
		});
	}

	return Result;
}

int32 UEditorValidatorSubsystem::ValidateAssetsWithSettings(
	const TArray<FAssetData>& AssetDataList,
	FValidateAssetsSettings& InSettings,
	FValidateAssetsResults& OutResults) const
{
	FMessageLog DataValidationLog(UE::DataValidation::MessageLogName);
	ValidateAssetsInternal(DataValidationLog, TSet<FAssetData>{AssetDataList}, InSettings, OutResults);
	
	if (EMessageSeverity::Type* Severity = InSettings.ShowMessageLogSeverity.GetPtrOrNull())
	{
		DataValidationLog.Open(*Severity, false);
	}

	return OutResults.NumWarnings + OutResults.NumInvalid;
}

EDataValidationResult UEditorValidatorSubsystem::ValidateAssetsInternal(
	FMessageLog& 					DataValidationLog,
	TSet<FAssetData>				AssetDataList,
	const FValidateAssetsSettings& 	InSettings,
	FValidateAssetsResults& 		OutResults) const
{
	IAssetRegistry& AssetRegistry = IAssetRegistry::GetChecked();

	FScopedSlowTask SlowTask(AssetDataList.Num(), LOCTEXT("DataValidation.ValidateAssetsTask", "Validating Assets"));
	SlowTask.MakeDialog();
	
	UE_LOG(LogContentValidation, Display, TEXT("Starting to validate %d assets"), AssetDataList.Num());
	UE_LOG(LogContentValidation, Log, TEXT("Enabled validators:"));
	ForEachEnabledValidator([](UEditorValidatorBase* Validator)
	{
		UE_LOG(LogContentValidation, Log, TEXT("\t%s"), *Validator->GetClass()->GetClassPathName().ToString());
		return true;
	});
	
	// Broadcast the Editor event before we start validating. This lets other systems (such as Sequencer) restore the state
	// of the level to what is actually saved on disk before performing validation.
	if (FEditorDelegates::OnPreAssetValidation.IsBound())
	{
		FEditorDelegates::OnPreAssetValidation.Broadcast();
	}
	
	// Filter external objects out from the asset data list to be validated indirectly via their outers 
	TMap<FAssetData, TArray<FAssetData>> AssetsToExternalObjects;
	for (auto It = AssetDataList.CreateIterator(); It; ++It)
	{
		if (!It->GetOptionalOuterPathName().IsNone())
		{
			FSoftObjectPath Path = It->ToSoftObjectPath();
			FAssetData OuterAsset = AssetRegistry.GetAssetByObjectPath(Path.GetWithoutSubPath(), true);
			if (OuterAsset.IsValid())
			{
				AssetsToExternalObjects.FindOrAdd(OuterAsset).Add(*It);
			}
			It.RemoveCurrent();
		}
	}
	
	// Add any packages which contain those external objects to be validated
	{
		FDataValidationContext ValidationContext(false, InSettings.ValidationUsecase, {});
		for (const TPair<FAssetData, TArray<FAssetData>>& Pair : AssetsToExternalObjects)
		{
			if (ShouldValidateAsset(Pair.Key, InSettings, ValidationContext))
			{
				AssetDataList.Add(Pair.Key);
			}
		}
		UE::DataValidation::AddAssetValidationMessages(DataValidationLog, ValidationContext);
		DataValidationLog.Flush();
	}

	// Dont let other async compilation warnings be attributed incorrectly to the package that is loading.
	WaitForAssetCompilationIfNecessary(InSettings.ValidationUsecase);

	OutResults.NumRequested = AssetDataList.Num();
	
	EDataValidationResult Result = EDataValidationResult::NotValidated;

	// Now add to map or update as needed
	for (const FAssetData& Data : AssetDataList)
	{
		ensure(Data.IsValid());

		SlowTask.EnterProgressFrame(1.0f, FText::Format(LOCTEXT("DataValidation.ValidatingFilename", "Validating {0}"), FText::FromString(Data.GetFullName())));
		
		if (OutResults.NumChecked >= InSettings.MaxAssetsToValidate)
		{
			OutResults.bAssetLimitReached = true;
			DataValidationLog.Info(FText::Format(LOCTEXT("DataValidation.MaxAssetCountReached", "MaxAssetsToValidate count {0} reached."), InSettings.MaxAssetsToValidate));
			break;
		}

		if (Data.HasAnyPackageFlags(PKG_Cooked))
		{
			++OutResults.NumSkipped;
			continue;
		}

		// Check exclusion path
		if (InSettings.bSkipExcludedDirectories && IsPathExcludedFromValidation(Data.PackageName.ToString()))
		{
			++OutResults.NumSkipped;
			continue;
		}

		const bool bLoadAsset = false;
		if (!InSettings.bLoadAssetsForValidation && !Data.FastGetAsset(bLoadAsset))
		{
			++OutResults.NumSkipped;
			continue;
		}
		
		DataValidationLog.Info()
			->AddToken(FAssetDataToken::Create(Data))
			->AddToken(FTextToken::Create(LOCTEXT("Data.ValidatingAsset", "Validating asset")));
		UE_LOG(LogContentValidation, Display, TEXT("Validating asset %s"), *Data.ToSoftObjectPath().ToString());
		
		UObject* LoadedAsset = Data.FastGetAsset(false);
		const bool bAlreadyLoaded = LoadedAsset != nullptr;

		TConstArrayView<FAssetData> ValidationExternalObjects;
		if (const TArray<FAssetData>* ValidationExternalObjectsPtr = AssetsToExternalObjects.Find(Data))
		{
			ValidationExternalObjects = *ValidationExternalObjectsPtr;
		}

		FDataValidationContext ValidationContext(bAlreadyLoaded, InSettings.ValidationUsecase, ValidationExternalObjects);
		EDataValidationResult AssetResult = EDataValidationResult::NotValidated;
		if (!LoadedAsset)
		{
			UE_LOG(LogContentValidation, Log, TEXT("Loading asset %s for validation"), *Data.ToSoftObjectPath().ToString());
			UE::DataValidation::FScopedLogMessageGatherer LogGatherer(InSettings.bCaptureAssetLoadLogs);
			LoadedAsset = Data.GetAsset(); // Do not load external objects, validators should load external objects that they want via GetAssociatedExternalObjects in the validation context 

			WaitForAssetCompilationIfNecessary(InSettings.ValidationUsecase);
			
			// Associate any load errors with this asset in the message log
			TArray<FString> Warnings;
			TArray<FString> Errors;
			LogGatherer.Stop(Warnings, Errors);
			if (Warnings.Num())
			{
				TStringBuilder<2048> Buffer;
				Buffer.Join(Warnings, LINE_TERMINATOR);
				ValidationContext.AddMessage(EMessageSeverity::Warning)
					->AddToken(FAssetDataToken::Create(Data))
					->AddText(LOCTEXT("DataValidation.LoadWarnings", "Warnings loading asset {0}"), FText::FromStringView(Buffer.ToView()));
			}
			if(Errors.Num())
			{
				TStringBuilder<2048> Buffer;
				Buffer.Join(Errors, LINE_TERMINATOR);
				ValidationContext.AddMessage(EMessageSeverity::Error)
					->AddToken(FAssetDataToken::Create(Data))
					->AddText(LOCTEXT("DataValidation.LoadErrors", "Errors loading asset {0}"), FText::FromStringView(Buffer.ToView()));
				AssetResult = EDataValidationResult::Invalid;
			}
		}

		if (LoadedAsset)
		{
			UE::DataValidation::FScopedLogMessageGatherer LogGatherer(InSettings.bCaptureLogsDuringValidation);
			AssetResult = IsObjectValidWithContext(LoadedAsset, ValidationContext);
			
			// Associate any UE_LOG errors with this asset in the message log
			TArray<FString> Warnings;
			TArray<FString> Errors;
			LogGatherer.Stop(Warnings, Errors);
			if (Warnings.Num())
			{
				TStringBuilder<2048> Buffer;
				Buffer.Join(Warnings, LINE_TERMINATOR);
				ValidationContext.AddMessage(EMessageSeverity::Error)
					->AddToken(FAssetDataToken::Create(Data))
					->AddText(LOCTEXT("DataValidation.DuringValidationWarnings", "Warnings logged while validating asset {0}"), FText::FromStringView(Buffer.ToView()));
				AssetResult = EDataValidationResult::Invalid;
			}
			if(Errors.Num())
			{
				TStringBuilder<2048> Buffer;
				Buffer.Join(Errors, LINE_TERMINATOR);
				ValidationContext.AddMessage(EMessageSeverity::Error)
					->AddToken(FAssetDataToken::Create(Data))
					->AddText(LOCTEXT("DataValidation.DuringValidationErrors", "Errors logged while validating asset {0}"), FText::FromStringView(Buffer.ToView()));
				AssetResult = EDataValidationResult::Invalid;
			}
		}
		else if (InSettings.bLoadAssetsForValidation)
		{
			ValidationContext.AddMessage(EMessageSeverity::Error)
				->AddToken(FAssetDataToken::Create(Data))
				->AddToken(FTextToken::Create(LOCTEXT("DataValidation.LoadFailed", "Failed to load object")));
			AssetResult = EDataValidationResult::Invalid;
		}
		else 
		{
			ValidationContext.AddMessage(EMessageSeverity::Error)
				->AddToken(FAssetDataToken::Create(Data))
				->AddToken(FTextToken::Create(LOCTEXT("DataValidation.CannotValidateNotLoaded", "Cannot validate unloaded asset")));
			AssetResult = EDataValidationResult::Invalid;
		}

		++OutResults.NumChecked;
		
		// Don't add more messages to ValidationContext after this point because we will no longer add them to the message log 
		UE::DataValidation::AddAssetValidationMessages(Data, DataValidationLog, ValidationContext);

		bool bAnyWarnings = Algo::AnyOf(ValidationContext.GetIssues(), [](const FDataValidationContext::FIssue& Issue){ return Issue.Severity == EMessageSeverity::Warning; });
		if (bAnyWarnings)
		{
			++OutResults.NumWarnings;
		}

		if (InSettings.bShowIfNoFailures)
		{
			switch (AssetResult)
			{
				case EDataValidationResult::Valid:
					if (bAnyWarnings)
					{
						DataValidationLog.Info()->AddToken(FAssetDataToken::Create(Data))
							->AddToken(FTextToken::Create(LOCTEXT("DataValidation.ContainsWarningsResult", "contains valid data, but has warnings.")));
					}
					else 
					{
						DataValidationLog.Info()->AddToken(FAssetDataToken::Create(Data))
							->AddToken(FTextToken::Create(LOCTEXT("DataValidation.ValidResult", "contains valid data.")));
					}
					break;
				case EDataValidationResult::NotValidated:
					DataValidationLog.Info()->AddToken(FAssetDataToken::Create(Data))
						->AddToken(FTextToken::Create(LOCTEXT("DataValidation.NotValidatedDataResult", "has no data validation.")));
					break;
			}
		}	

		switch (AssetResult)
		{
			case EDataValidationResult::Valid:
				++OutResults.NumValid;
				break;
			case EDataValidationResult::Invalid:
				++OutResults.NumInvalid;
				break;
			case EDataValidationResult::NotValidated:
				++OutResults.NumUnableToValidate;
				break;
		}

		if (InSettings.bCollectPerAssetDetails)
		{
			FValidateAssetsDetails& Details = OutResults.AssetsDetails.Emplace(Data.GetObjectPathString());
			Details.PackageName = Data.PackageName;
			Details.AssetName = Data.AssetName;
			Details.Result = AssetResult;
			ValidationContext.SplitIssues(Details.ValidationWarnings, Details.ValidationErrors);

			Details.ExternalObjects.Reserve(ValidationExternalObjects.Num());
			for (const FAssetData& ExtData : ValidationExternalObjects)
			{
				FValidateAssetsExternalObject& ExtDetails = Details.ExternalObjects.Emplace_GetRef();
				ExtDetails.PackageName = ExtData.PackageName;
				ExtDetails.AssetName = ExtData.AssetName;
			}
		}
		
		DataValidationLog.Flush();
		
		Result = CombineDataValidationResults(Result, AssetResult);
	}

	// Broadcast now that we're complete so other systems can go back to their previous state.
	if (FEditorDelegates::OnPostAssetValidation.IsBound())
	{
		FEditorDelegates::OnPostAssetValidation.Broadcast();
	}
	
	return Result;
}

void UEditorValidatorSubsystem::LogAssetValidationSummary(FMessageLog& DataValidationLog, const FValidateAssetsSettings& InSettings, const FValidateAssetsResults& Results) const
{
	const bool bFailed = (Results.NumInvalid > 0);
	const bool bAtLeastOneWarning = (Results.NumWarnings > 0);

	if (bFailed || bAtLeastOneWarning || InSettings.bShowIfNoFailures)
	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("Result"), bFailed ? LOCTEXT("Failed", "FAILED") : LOCTEXT("Succeeded", "SUCCEEDED"));
		Arguments.Add(TEXT("NumChecked"), Results.NumChecked);
		Arguments.Add(TEXT("NumValid"), Results.NumValid);
		Arguments.Add(TEXT("NumInvalid"), Results.NumInvalid);
		Arguments.Add(TEXT("NumSkipped"), Results.NumSkipped);
		Arguments.Add(TEXT("NumUnableToValidate"), Results.NumUnableToValidate);

		DataValidationLog.Info()->AddToken(FTextToken::Create(FText::Format(LOCTEXT("DataValidation.SuccessOrFailure", "Data validation {Result}."), Arguments)));
		DataValidationLog.Info()->AddToken(FTextToken::Create(FText::Format(LOCTEXT("DataValidation.ResultsSummary", "Files Checked: {NumChecked}, Passed: {NumValid}, Failed: {NumInvalid}, Skipped: {NumSkipped}, Unable to validate: {NumUnableToValidate}"), Arguments)));

		DataValidationLog.Open(EMessageSeverity::Info, true);
	}
}

void UEditorValidatorSubsystem::ValidateOnSave(TArray<FAssetData> AssetDataList, bool bProceduralSave) const
{
	// Only validate if enabled and not auto saving
	if (!GetDefault<UDataValidationSettings>()->bValidateOnSave || GEditor->IsAutosaving())
	{
		return;
	}

	if (bProceduralSave)
	{
		return;
	}
	
	FValidateAssetsSettings Settings;
	{
		FDataValidationContext Context {false, EDataValidationUsecase::Save, {}};
		AssetDataList.SetNum(Algo::RemoveIf(AssetDataList, [this, &Settings, &Context](const FAssetData& Asset) { 
			return !ShouldValidateAsset(Asset, Settings, Context); 
		}));
	}

	FMessageLog DataValidationLog(UE::DataValidation::MessageLogName);
	FText SavedAsset = AssetDataList.Num() == 1 ? FText::FromName(AssetDataList[0].AssetName) : LOCTEXT("MultipleErrors", "multiple assets");
	DataValidationLog.NewPage(FText::Format(LOCTEXT("DataValidationLogPage", "Asset Save: {0}"), SavedAsset));

	FValidateAssetsResults Results;

	Settings.bSkipExcludedDirectories = true;
	Settings.bShowIfNoFailures = false;
	Settings.ValidationUsecase = EDataValidationUsecase::Save;
	Settings.bLoadAssetsForValidation = false;
	Settings.MessageLogPageTitle = LOCTEXT("DataValidation.ValidateOnSaveMessageLogPage", "Validate Saved Assets");

	if (ValidateAssetsWithSettings(AssetDataList, Settings, Results) > 0)
	{
		const FText ErrorMessageNotification = FText::Format(
			LOCTEXT("ValidationFailureNotification", "Validation failed when saving {0}, check Data Validation log"), SavedAsset);
		DataValidationLog.Notify(ErrorMessageNotification, EMessageSeverity::Warning, /*bForce=*/ true);
	}
}

void UEditorValidatorSubsystem::ValidateSavedPackage(FName PackageName, bool bProceduralSave)
{
	// Only validate if enabled and not auto saving
	if (!GetDefault<UDataValidationSettings>()->bValidateOnSave || GEditor->IsAutosaving())
	{
		return;
	}

	// For performance reasons, don't validate when making a procedural save by default. Assumption is we validated when saving previously. 
	if (bProceduralSave)
	{
		return;
	}

	if (SavedPackagesToValidate.Num() == 0)
	{
		GEditor->GetTimerManager()->SetTimerForNextTick(this, &UEditorValidatorSubsystem::ValidateAllSavedPackages);
	}

	SavedPackagesToValidate.AddUnique(PackageName);
}

bool UEditorValidatorSubsystem::IsPathExcludedFromValidation(FStringView Path) const
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

	IAssetRegistry& AssetRegistry = IAssetRegistry::GetChecked();

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

void UEditorValidatorSubsystem::ValidateChangelistPreSubmit(
	FSourceControlChangelistPtr InChangelist,
	EDataValidationResult& 		OutResult,
	TArray<FText>& 				OutValidationErrors,
	TArray<FText>& 				OutValidationWarnings) const
{
	check(InChangelist.IsValid());

	// Create temporary changelist object to do most of the heavy lifting
	UDataValidationChangelist* Changelist = NewObject<UDataValidationChangelist>();
	Changelist->Initialize(InChangelist);

	FValidateAssetsSettings Settings;
	Settings.ValidationUsecase = EDataValidationUsecase::PreSubmit;
	Settings.bLoadAssetsForValidation = GetDefault<UDataValidationSettings>()->bLoadAssetsWhenValidatingChangelists;
	Settings.MessageLogPageTitle = FText::Format(LOCTEXT("DataValidation.ValidateChangelistMessageLogPage", "Validate Changelist {0}"), FText::FromString(InChangelist->GetIdentifier()));
	FValidateAssetsResults Results;
	OutResult = ValidateChangelist(Changelist, Settings, Results);
	
	if (const FValidateAssetsDetails* Details = Results.AssetsDetails.Find(Changelist->GetPathName()))
	{
		OutValidationWarnings = Details->ValidationWarnings;
		OutValidationErrors = Details->ValidationErrors;
		
		for (const TSharedRef<FTokenizedMessage>& Message : Details->ValidationMessages)
		{
			switch (Message->GetSeverity())
			{
				case EMessageSeverity::Error:
					OutValidationErrors.Add(Message->ToText());
					break;
				case EMessageSeverity::Warning:
				case EMessageSeverity::PerformanceWarning:
					OutValidationWarnings.Add(Message->ToText());
					break;
			}
		}
	}
}

EDataValidationResult UEditorValidatorSubsystem::ValidateChangelist(
	UDataValidationChangelist* InChangelist,
	const FValidateAssetsSettings& InSettings,
	FValidateAssetsResults& OutResults) const
{
	return ValidateChangelistsInternal(MakeArrayView(&InChangelist, 1), InSettings, OutResults);	
}

EDataValidationResult UEditorValidatorSubsystem::ValidateChangelists(
	const TArray<UDataValidationChangelist*> InChangelists,
	const FValidateAssetsSettings& InSettings,
	FValidateAssetsResults& OutResults) const
{
	return ValidateChangelistsInternal(MakeArrayView(InChangelists), InSettings, OutResults);	
}

EDataValidationResult UEditorValidatorSubsystem::ValidateChangelistsInternal(
	TConstArrayView<UDataValidationChangelist*>	Changelists,
	const FValidateAssetsSettings& 				Settings,
	FValidateAssetsResults& 					OutResults) const
{
	FScopedSlowTask SlowTask(Changelists.Num(), LOCTEXT("DataValidation.ValidatingChangelistTask", "Validating Changelists"));
	SlowTask.Visibility = ESlowTaskVisibility::Invisible;
	SlowTask.MakeDialog();

	IAssetRegistry& AssetRegistry = IAssetRegistry::GetChecked();

	if (AssetRegistry.IsLoadingAssets())
	{
		UE_CLOG(FApp::IsUnattended(), LogContentValidation, Fatal, TEXT("Unable to perform unattended content validation while asset registry scan is in progress. Callers just wait for asset registry scan to complete."));
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("DataValidation.UnableToValidate_PendingAssetRegistry", "Unable to validate changelist while asset registry scan is in progress. Wait until asset discovery is complete."));
		return EDataValidationResult::NotValidated;
	}

	// Choose a specific message log page for this output, flushing in case other recursive calls also write to this log 
	{	
		FMessageLog DataValidationLog(UE::DataValidation::MessageLogName);
		DataValidationLog.SetCurrentPage(Settings.MessageLogPageTitle);
	}

	FMessageLog DataValidationLog(UE::DataValidation::MessageLogName);

	for (UDataValidationChangelist* CL : Changelists)
	{
		CL->AddToRoot();
	}
	
	ON_SCOPE_EXIT {
		for (UDataValidationChangelist* CL : Changelists)
		{
			CL->RemoveFromRoot();		
		}
	};

	EDataValidationResult Result = EDataValidationResult::NotValidated;
	for (UDataValidationChangelist* Changelist : Changelists)
	{
		FText ValidationMessage = FText::Format(LOCTEXT("DataValidation.ValidatingChangelistMessage", "Validating changelist {0}"), Changelist->Description);
		DataValidationLog.Info(ValidationMessage);
		SlowTask.EnterProgressFrame(1.0f, ValidationMessage);

		FValidateAssetsDetails& Details = OutResults.AssetsDetails.FindOrAdd(Changelist->GetPathName());
		{
			FDataValidationContext ValidationContext(false, Settings.ValidationUsecase, {}); // No associated objects for changelist
			Details.Result = IsObjectValidWithContext(Changelist, ValidationContext);
			UE::DataValidation::AddAssetValidationMessages(DataValidationLog, ValidationContext);
		}
		Result = CombineDataValidationResults(Result, Details.Result);
		DataValidationLog.Flush();
	}

	TSet<FAssetData> AssetsToValidate;	
	for (UDataValidationChangelist* Changelist : Changelists)
	{
		FDataValidationContext ValidationContext(false, Settings.ValidationUsecase, {}); // No associated objects for changelist
		GatherAssetsToValidateFromChangelist(Changelist, Settings, AssetsToValidate, ValidationContext);
		UE::DataValidation::AddAssetValidationMessages(DataValidationLog, ValidationContext);
		DataValidationLog.Flush();
	}

	// Filter out assets that we don't want to validate
	{
		FDataValidationContext ValidationContext(false, Settings.ValidationUsecase, {}); 
		for (auto It = AssetsToValidate.CreateIterator(); It; ++It)
		{
			if (!ShouldValidateAsset(*It, Settings, ValidationContext))
			{
				UE_LOG(LogContentValidation, Log, TEXT("Excluding asset %s from validation"), *It->GetSoftObjectPath().ToString());
				It.RemoveCurrent();
			}
		}
		UE::DataValidation::AddAssetValidationMessages(DataValidationLog, ValidationContext);
		DataValidationLog.Flush();
	}

	// Validate assets from all changelists
	EDataValidationResult AssetResult = ValidateAssetsInternal(DataValidationLog, MoveTemp(AssetsToValidate), Settings, OutResults);
	Result = CombineDataValidationResults(Result, AssetResult);
	LogAssetValidationSummary(DataValidationLog, Settings, OutResults);

	return Result;
}

void UEditorValidatorSubsystem::GatherAssetsToValidateFromChangelist(
	UDataValidationChangelist* 		InChangelist,
	const FValidateAssetsSettings& 	Settings,
	TSet<FAssetData>& 				OutAssets,
	FDataValidationContext& 		InContext) const
{
	IAssetRegistry& AssetRegistry = IAssetRegistry::GetChecked();
	
	for (const FName& PackageName : InChangelist->ModifiedPackageNames)
	{
		TArray<FAssetData> NewAssets;
		AssetRegistry.GetAssetsByPackageName(PackageName, NewAssets, true);	
		OutAssets.Append(NewAssets);
	}
	
	// Gather assets requested by plugin/project validators 
	ForEachEnabledValidator([this, InChangelist, &Settings, &InContext, &OutAssets](UEditorValidatorBase* Validator)
	{
		TArray<FAssetData> NewAssets = Validator->GetAssetsToValidateFromChangelist(InChangelist, InContext);
		for (const FAssetData& Asset : NewAssets)
		{
			// It's not strictly necessary to filter assets here but it makes logging simpler
			if (ShouldValidateAsset(Asset, Settings, InContext))
			{
				UE_LOG(LogContentValidation, Verbose, TEXT("Asset validator %s adding %s to be validated."), *Validator->GetPathName(), *Asset.GetSoftObjectPath().ToString());
				OutAssets.Add(Asset);
			}
		}
		return true;
	});
	
	if (Settings.bValidateReferencersOfDeletedAssets)
	{
		TSet<FAssetData> AdditionalAssetsToValidate;
		for (FName DeletedPackageName : InChangelist->DeletedPackageNames)
		{
			InContext.AddMessage(EMessageSeverity::Info, 
				FText::Format(LOCTEXT("DataValidation.AddDeletedPackageReferencers", "Adding referencers of deleted package {0} to be validated"), FText::FromName(DeletedPackageName)));

			TArray<FName> PackageReferencers;
			AssetRegistry.GetReferencers(DeletedPackageName, PackageReferencers, UE::AssetRegistry::EDependencyCategory::Package);
			for (const FName& Referencer : PackageReferencers)
			{
				UE_LOG(LogContentValidation, Log, TEXT("Adding %s to to validated as it is a referencer of deleted asset %s"), *Referencer.ToString(), *DeletedPackageName.ToString());
				TArray<FAssetData> NewAssets;
				AssetRegistry.GetAssetsByPackageName(Referencer, NewAssets, true);
				OutAssets.Append(NewAssets);
			}
		}
	}
}

void UEditorValidatorSubsystem::LoadValidators() const
{
	const_cast<UEditorValidatorSubsystem*>(this)->LoadValidators();
}

void UEditorValidatorSubsystem::LoadValidators() 
{
	if (bNeedLoadingOfValidators)
	{
		for (TPair<FTopLevelAssetPath, TObjectPtr<UEditorValidatorBase>>& ValidatorPair : Validators)
		{
			if (!ValidatorPair.Value)
			{
				FSoftObjectPath ValidatorObjectSoftPath(ValidatorPair.Key);
				UObject* ValidatorObject = ValidatorObjectSoftPath.ResolveObject();

				// If this object isn't currently loaded, load it
				if (ValidatorObject == nullptr)
				{
					FCookLoadScope EditorOnlyLoadScope(ECookLoadType::EditorOnly);
					FSoftObjectPathSerializationScope SerializationScope(NAME_None, NAME_None, ESoftObjectPathCollectType::EditorOnlyCollect, ESoftObjectPathSerializeType::AlwaysSerialize);
					ValidatorObject = ValidatorObjectSoftPath.TryLoad();
				}

				if (ValidatorObject)
				{
					if (UEditorUtilityBlueprint* ValidatorBlueprint = Cast<UEditorUtilityBlueprint>(ValidatorObject))
					{
						UEditorValidatorBase* Validator = NewObject<UEditorValidatorBase>(GetTransientPackage(), ValidatorBlueprint->GeneratedClass);
						ValidatorPair.Value = Validator;
					}
				}
			}
		}

		bNeedLoadingOfValidators = false;
	}
}

TArray<FAssetData> UEditorValidatorSubsystem::GetAssetsResolvingRedirectors(FARFilter& InFilter)
{
	IAssetRegistry& AssetRegistry = IAssetRegistry::GetChecked();

	TArray<FAssetData> Found;
	AssetRegistry.GetAssets(InFilter, Found);
	
	TArray<FAssetData> Redirectors;	
	for (int32 i=Found.Num()-1; i >= 0; --i)
	{
		if (Found[i].IsRedirector())
		{
			Redirectors.Add(Found[i]);
			Found.RemoveAt(i);
		}
	}
	
	for (const FAssetData& RedirectorAsset : Redirectors)
	{
		FSoftObjectPath Path = AssetRegistry.GetRedirectedObjectPath(RedirectorAsset.GetSoftObjectPath());		
		if (!Path.IsNull())
		{
			FAssetData Destination = AssetRegistry.GetAssetByObjectPath(Path, true);
			if (Destination.IsValid())
			{
				Found.Add(Destination);
			}
		}
	}
	return Found;
}

void UEditorValidatorSubsystem::WaitForAssetCompilationIfNecessary(EDataValidationUsecase InUsecase) const
{
	if (InUsecase == EDataValidationUsecase::Save)
	{
		return;
	}

	if (FAssetCompilingManager::Get().GetNumRemainingAssets())
	{
		FScopedSlowTask CompileAssetsSlowTask(0.f, LOCTEXT("DataValidation.CompilingAssetsBeforeCheckingContentTask", "Finishing asset compilations before checking content..."));
		CompileAssetsSlowTask.MakeDialog();
		FAssetCompilingManager::Get().FinishAllCompilation();
	}
}

#undef LOCTEXT_NAMESPACE

