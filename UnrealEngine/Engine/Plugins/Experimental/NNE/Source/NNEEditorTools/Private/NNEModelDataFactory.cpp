// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNEModelDataFactory.h"

#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "EditorFramework/AssetImportData.h"
#include "EngineAnalytics.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "NNE.h"
#include "NNEModelData.h"
#include "Subsystems/ImportSubsystem.h"


UNNEModelDataFactory::UNNEModelDataFactory(const FObjectInitializer& ObjectInitializer) : UFactory(ObjectInitializer)
{
	bCreateNew = false;
	bEditorImport = true;
	SupportedClass = UNNEModelData::StaticClass();
	ImportPriority = DefaultImportPriority;
	Formats.Add("onnx;Open Neural Network Exchange Format");
}

UObject* UNNEModelDataFactory::FactoryCreateBinary(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, const TCHAR * Type, const uint8 *& Buffer, const uint8 * BufferEnd, FFeedbackContext* Warn)
{
	GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetPreImport(this, Class, InParent, Name, Type);

	if (!Type || !Buffer || !BufferEnd || BufferEnd - Buffer <= 0)
	{
		GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetPostImport(this, nullptr);
		return nullptr;
	}

	UNNEModelData* ModelData = NewObject<UNNEModelData>(InParent, Class, Name, Flags);
	TConstArrayView<uint8> BufferView = MakeArrayView(Buffer, BufferEnd-Buffer);
	ModelData->Init(Type, BufferView);

	ModelData->AssetImportData->Update(GetCurrentFilename());
	GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetPostImport(this, ModelData);

	if (FEngineAnalytics::IsAvailable())
	{
		TArray<FAnalyticsEventAttribute> Attributes = MakeAnalyticsEventAttributeArray(
			TEXT("PlatformName"), UGameplayStatics::GetPlatformName(),
			TEXT("FactoryName"), TEXT("UNNEModelDataFactory"),
			TEXT("ModelFileSize"), BufferView.Num()
		);
		FEngineAnalytics::GetProvider().RecordEvent(TEXT("NeuralNetworkEngine.FactoryCreateBinary"), Attributes);
	}

	return ModelData;
}

bool UNNEModelDataFactory::FactoryCanImport(const FString & Filename)
{
	return Filename.EndsWith(FString("onnx"));
}

bool UNNEModelDataFactory::CanReimport(UObject* Obj, TArray<FString>& OutFilenames)
{
	UNNEModelData* pModel = Cast<UNNEModelData>(Obj);
	if (pModel)
	{
		pModel->AssetImportData->ExtractFilenames(OutFilenames);
		return true;
	}
	return false;
}

void UNNEModelDataFactory::SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths)
{
	UNNEModelData* pModel = Cast<UNNEModelData>(Obj);
	if (pModel && ensure(NewReimportPaths.Num() == 1))
	{
		pModel->AssetImportData->UpdateFilenameOnly(NewReimportPaths[0]);
	}
}

EReimportResult::Type UNNEModelDataFactory::Reimport(UObject* Obj)
{
	if (!Obj || !Obj->IsA(UNNEModelData::StaticClass()))
	{
		return EReimportResult::Failed;
	}

	UNNEModelData* pModel = Cast<UNNEModelData>(Obj);
	const FString ResolvedSourceFilePath = pModel->AssetImportData->GetFirstFilename();
	if (!ResolvedSourceFilePath.Len())
	{
		UE_LOG(LogNNE, Warning, TEXT("Cannot reimport NNE Model Data: it does not have a path stored."));
		return EReimportResult::Failed;
	}

	// Ensure that the file provided by the path exists
	if (IFileManager::Get().FileSize(*ResolvedSourceFilePath) == INDEX_NONE)
	{
		UE_LOG(LogNNE, Warning, TEXT("Cannot reimport NNE Model Data: source file cannot be found."));
		return EReimportResult::Failed;
	}

	// Keep runtime list to be reapplied upon asset import.
	TArray<FString, TInlineAllocator<10>> TargetRuntimes;
	TargetRuntimes = pModel->GetTargetRuntimes();

	bool OutCanceled = false;

	if (ImportObject(pModel->GetClass(), pModel->GetOuter(), *pModel->GetName(), RF_Public | RF_Standalone, ResolvedSourceFilePath, nullptr, OutCanceled) != nullptr)
	{
		UE_LOG(LogNNE, Log, TEXT("Reimported successfully"));

		pModel->SetTargetRuntimes(TargetRuntimes);
		pModel->AssetImportData->Update(ResolvedSourceFilePath);

		// Try to find the outer package so we can dirty it up
		if (pModel->GetOuter())
		{
			pModel->GetOuter()->MarkPackageDirty();
		}
		else
		{
			pModel->MarkPackageDirty();
		}
	}
	else if (OutCanceled)
	{
		UE_LOG(LogNNE, Warning, TEXT("Reimport canceled"));
		return EReimportResult::Cancelled;
	}
	else
	{
		UE_LOG(LogNNE, Warning, TEXT("Reimport failed"));
		return EReimportResult::Failed;
	}

	return EReimportResult::Succeeded;
}