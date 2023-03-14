// Copyright Epic Games, Inc. All Rights Reserved.

#include "DatasmithInterchangeScripting.h"

#include "InterchangeDatasmithPipeline.h"
#include "InterchangeDatasmithTranslator.h"

#include "DatasmithImportOptions.h"
#include "DatasmithTranslatorManager.h"

#include "Async/Async.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/FileManager.h"
#include "Logging/LogMacros.h"
#include "Misc/App.h"
#include "Misc/Paths.h"

DECLARE_LOG_CATEGORY_EXTERN(LogDatasmithInterchangeImportLibrary, Log, All)
DEFINE_LOG_CATEGORY(LogDatasmithInterchangeImportLibrary)

#define LOCTEXT_NAMESPACE "DatasmithInterchangeImportLibrary"

FDatasmithInterchangeImportContext::FDatasmithInterchangeImportContext()
	: AssetPath(TEXT("/Game/DatasmithInterchangeImport"))
{
}

UDatasmithInterchangeImportResult::~UDatasmithInterchangeImportResult()
{
	const FString ConfigFilePath = UInterchangeDatasmithTranslator::BuildConfigFilePath(FilePath);
	if (FPaths::FileExists(ConfigFilePath))
	{
		IFileManager::Get().Delete(*ConfigFilePath);
	}
}

bool UDatasmithInterchangeImportResult::Build(const FString& InFilePath, FImportResultPtr InAssetResult, FImportResultPtr InSceneResult, bool bAsync, USceneComponent* Anchor)
{
	using namespace UE::Interchange;

	FilePath = InFilePath;
	AssetResult = InAssetResult;
	SceneResult = InSceneResult;

	bIsValid = AssetResult.IsValid() && AssetResult->IsValid() && SceneResult.IsValid(); // Status of SceneResult is not set yet.
	
	if (bIsValid)
	{
		// Note: All the logic below assume that this is valid at the end of the import
		if (bAsync)
		{
			// TODO: The two results might be done by the time this code is executed
			SceneResult->OnDone(
				[this, Anchor](UE::Interchange::FImportResult& Result)
				{
					ImportedObjects.Append(Result.GetImportedObjects());
					if (AssetResult->GetStatus() == FImportResult::EStatus::Done)
					{
						UE_LOG(LogDatasmithInterchangeImportLibrary, Warning, TEXT("Scene import completed"));
						OnImportComplete(Anchor, true);
					}
				}
			);

			AssetResult->OnDone(
				[this, Anchor](UE::Interchange::FImportResult& Result)
				{
					ImportedObjects.Append(Result.GetImportedObjects());
					if (SceneResult->GetStatus() == FImportResult::EStatus::Done)
					{
						UE_LOG(LogDatasmithInterchangeImportLibrary, Warning, TEXT("Asset import completed"));
						OnImportComplete(Anchor, true);
					}
				}
			);
		}
		else
		{
			// Wait for the import to complete
			AssetResult->WaitUntilDone();
			SceneResult->WaitUntilDone();

			ImportedObjects.Append(AssetResult->GetImportedObjects());
			ImportedObjects.Append(SceneResult->GetImportedObjects());

			OnImportComplete(Anchor, true);
		}
	}

	return bIsValid;
}

void UDatasmithInterchangeImportResult::OnImportComplete(USceneComponent* Anchor, bool bAsync)
{
	auto ApplyAnchor = [Anchor](const TArray< UObject* >& ResultObjects)
	{
		if (Anchor)
		{
#if WITH_EDITOR
			UE_LOG(LogDatasmithInterchangeImportLibrary, Warning, TEXT("Applying anchor..."));
			int32 RootLessCount = 0;
#endif
			for (UObject* Object : ResultObjects)
			{
				if (AActor* Actor = Cast<AActor>(Object))
				{
					if (!Actor->GetAttachParentActor() && Actor->GetRootComponent())
					{
#if WITH_EDITOR
						if (RootLessCount > 0)
						{
							UE_LOG(LogDatasmithInterchangeImportLibrary, Warning, TEXT("Actor %s is rootless. It should not..."), *Actor->GetActorLabel());
						}
						RootLessCount++;
#endif
						Anchor->GetOwner()->UnregisterAllComponents(true);
						Actor->UnregisterAllComponents(true);

						USceneComponent* RootComponent = Actor->GetRootComponent();
						if (RootComponent->Mobility == EComponentMobility::Static && Anchor->Mobility != EComponentMobility::Static)
						{
							RootComponent->SetMobility(EComponentMobility::Movable);
						}

						Actor->GetRootComponent()->AttachToComponent(Anchor, FAttachmentTransformRules::KeepRelativeTransform);

						Actor->ReregisterAllComponents();
						Anchor->GetOwner()->ReregisterAllComponents();
					}
					//break;
				}
			}
		}
	};

	// Trigger notification on next tick on game thread
	AsyncTask(ENamedThreads::GameThread, [this, ApplyAnchor] {
			ApplyAnchor(this->ImportedObjects);
			this->OnImportEnded.Broadcast();
		});
}

UDatasmithInterchangeImportResult* UDatasmithInterchangeScripting::LoadFile(const FString& FilePath, const FDatasmithInterchangeImportContext& Context)
{
	using FInterchangeResults = TTuple<UE::Interchange::FAssetImportResultRef, UE::Interchange::FSceneImportResultRef>;

	UDatasmithInterchangeImportResult* Result = nullptr;

	UE::Interchange::FScopedSourceData ScopedSourceData(FilePath);

	UInterchangeManager& InterchangeManager = UInterchangeManager::GetInterchangeManager();

	if (!InterchangeManager.CanTranslateSourceData(ScopedSourceData.GetSourceData()))
	{
		return Result;
	}

	// Save tessellation options' settings, so they can be used by the Datasmith translator
	{
		const FString ConfigFilePath = UInterchangeDatasmithTranslator::BuildConfigFilePath(FilePath);

		for (UObject* Option : Context.ImportOptions)
		{
			if (Option)
			{
				Option->SaveConfig(CPF_Config, *ConfigFilePath);
			}
		}
	}

	UInterchangeDatasmithPipeline* DatasmithPipeline = NewObject< UInterchangeDatasmithPipeline >();

	FImportAssetParameters ImportAssetParameters;
	ImportAssetParameters.bIsAutomated = false;

	ImportAssetParameters.OverridePipelines.Add(DatasmithPipeline);

	FString AssetPath = FApp::IsGame() ? FPaths::Combine(GetTransientPackage()->GetPathName(), FGuid::NewGuid().ToString()) : Context.AssetPath;

	FInterchangeResults ImportResults = InterchangeManager.ImportSceneAsync(AssetPath, ScopedSourceData.GetSourceData(), ImportAssetParameters);

	Result = NewObject<UDatasmithInterchangeImportResult>();

	if (!Result->Build(FilePath, ImportResults.Get<0>(), ImportResults.Get<1>(), Context.bAsync, Context.Anchor))
	{
		return nullptr;
	}

	return Result;
}

void UDatasmithInterchangeScripting::GetDatasmithFormats(FString& Extensions, FString& FileTypes)
{
	const TArray<FString> Formats = FDatasmithTranslatorManager::Get().GetSupportedFormats();

	Extensions = TEXT("");

	TMap<FString, FString> FileTypeMap;

	for (TArray<FString>::TConstIterator FormatIter(Formats); FormatIter; ++FormatIter)
	{
		const FString& CurFormat = *FormatIter;

		// Parse the format into its extension and description parts
		TArray<FString> FormatComponents;
		CurFormat.ParseIntoArray(FormatComponents, TEXT(";"), false);

		for (int32 ComponentIndex = 0; ComponentIndex < FormatComponents.Num(); ComponentIndex += 2)
		{
			const FString CurExtension = FString::Printf(TEXT("*.%s"), *FormatComponents[ComponentIndex]);
			if (CurExtension.Equals(TEXT(".gltf"), ESearchCase::IgnoreCase) || CurExtension.Equals(TEXT(".glb"), ESearchCase::IgnoreCase))
			{
				continue;
			}

			const FString& CurDescription = FormatComponents[ComponentIndex + 1];

			if (Extensions.Len() > 0)
			{
				Extensions += TEXT(";");
			}

			Extensions += CurExtension;

			if (CurDescription.Len() > 0)
			{
				FString& FileTypeExtensions = FileTypeMap.FindOrAdd(CurDescription);

				if (FileTypeExtensions.Len() > 0)
				{
					FileTypeExtensions += TEXT("; ");
				}

				FileTypeExtensions += CurExtension;

			}
		}
	}

	FileTypes = FString::Printf(TEXT("All Files (%s)|%s"), *Extensions, *Extensions);

	for (const TPair<FString, FString>& Entry : FileTypeMap)
	{
		FileTypes += FString::Printf(TEXT("|%s (%s)|%s"), *Entry.Key, *Entry.Value, *Entry.Value);
	}

}

void UDatasmithInterchangeScripting::GetDatasmithOptionsForFile(const FString& FilePath, TArray<UObject*>& Options)
{
	FDatasmithSceneSource Source;
	Source.SetSourceFile(FilePath);

	if (TSharedPtr<IDatasmithTranslator> Translator = FDatasmithTranslatorManager::Get().SelectFirstCompatible(Source))
	{
		TArray<TObjectPtr<UDatasmithOptionsBase>> OptionPtrs;
		Translator->GetSceneImportOptions(OptionPtrs);

		Options.Reserve(Options.Num() + OptionPtrs.Num());

		for (const TObjectPtr<UDatasmithOptionsBase>& Option : OptionPtrs)
		{
			if (Option)
			{
				Options.Add(Option);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE