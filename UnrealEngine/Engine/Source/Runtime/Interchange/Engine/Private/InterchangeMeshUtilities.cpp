// Copyright Epic Games, Inc. All Rights Reserved.

#include "InterchangeMeshUtilities.h"

#include "Async/Future.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkinnedAssetAsyncCompileUtils.h"
#include "InterchangeAssetImportData.h"
#include "InterchangeFactoryBase.h"
#include "InterchangeEngineLogPrivate.h"
#include "InterchangeManager.h"
#include "InterchangeProjectSettings.h"
#include "InterchangePythonPipelineBase.h"
#include "InterchangeSourceData.h"
#include "Logging/LogMacros.h"
#include "UObject/Class.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

#if WITH_EDITOR
#include "LODUtilities.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(InterchangeMeshUtilities)

TFuture<bool> UInterchangeMeshUtilities::ImportCustomLod(UObject* MeshObject, const int32 LodIndex)
{
	TSharedPtr<TPromise<bool>> Promise = MakeShared<TPromise<bool>>();
	if (!MeshObject)
	{
		UE_LOG(LogInterchangeEngine, Warning, TEXT("FInterchangeMeshUtilities::ImportCustomLod: The MeshObject parameter cannot be null."));
		Promise->SetValue(false);
		return Promise->GetFuture();
	}
	if (!IsInGameThread())
	{
		UE_LOG(LogInterchangeEngine, Warning, TEXT("FInterchangeMeshUtilities::ImportCustomLod: Cannot ask the user for a file path outside of the game thread."));
		Promise->SetValue(false);
		return Promise->GetFuture();
	}
	UInterchangeManager& InterchangeManager = UInterchangeManager::GetInterchangeManager();

	//Ask the user for a file path
	const UInterchangeProjectSettings* InterchangeProjectSettings = GetDefault<UInterchangeProjectSettings>();
	UInterchangeFilePickerBase* FilePicker = nullptr;

	//In runtime we do not have any pipeline configurator
#if WITH_EDITORONLY_DATA
	TSoftClassPtr <UInterchangeFilePickerBase> FilePickerClass = InterchangeProjectSettings->FilePickerClass;
	if (FilePickerClass.IsValid())
	{
		UClass* FilePickerClassLoaded = FilePickerClass.LoadSynchronous();
		if (FilePickerClassLoaded)
		{
			FilePicker = NewObject<UInterchangeFilePickerBase>(GetTransientPackage(), FilePickerClassLoaded, NAME_None, RF_NoFlags);
		}
	}
#endif
	if(FilePicker)
	{
		FInterchangeFilePickerParameters Parameters;
		Parameters.bAllowMultipleFiles = false;
		Parameters.Title = FText::Format(NSLOCTEXT("Interchange", "ImportCustomLodAsync_FilePickerTitle", "Choose a file to import a custom LOD for LOD{0}"), FText::AsNumber(LodIndex));
		TArray<FString> Filenames;
		if (FilePicker->ScriptedFilePickerForTranslatorAssetType(EInterchangeTranslatorAssetType::Meshes, Parameters, Filenames))
		{
			//We set bAllowMultipleFile to false, we should have only one result
			if (ensure(Filenames.Num() == 1))
			{
				const UInterchangeSourceData* SourceData = InterchangeManager.CreateSourceData(Filenames[0]);
				return InternalImportCustomLod(Promise, MeshObject, LodIndex, SourceData);
			}
		}
	}

	Promise->SetValue(false);
	return Promise->GetFuture();
}

TFuture<bool> UInterchangeMeshUtilities::ImportCustomLod(UObject* MeshObject, const int32 LodIndex, const UInterchangeSourceData* SourceData)
{
	TSharedPtr<TPromise<bool>> Promise = MakeShared<TPromise<bool>>();
	
	return InternalImportCustomLod(Promise, MeshObject, LodIndex, SourceData);
}

TFuture<bool> UInterchangeMeshUtilities::InternalImportCustomLod(TSharedPtr<TPromise<bool>> Promise, UObject* MeshObject, const int32 LodIndex, const UInterchangeSourceData* SourceData)
{
#if WITH_EDITOR
	UInterchangeManager& InterchangeManager = UInterchangeManager::GetInterchangeManager();

	UInterchangeAssetImportData* InterchangeAssetImportData = nullptr;
	USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(MeshObject);
	UStaticMesh* StaticMesh = Cast<UStaticMesh>(MeshObject);
	EInterchangePipelineContext ImportType = EInterchangePipelineContext::AssetCustomLODImport;
	bool bInvalidLodIndex = false;
	if (SkeletalMesh)
	{
		InterchangeAssetImportData = Cast<UInterchangeAssetImportData>(SkeletalMesh->GetAssetImportData());
		if (SkeletalMesh->GetLODNum() > LodIndex && InterchangeAssetImportData)
		{
			ImportType = EInterchangePipelineContext::AssetCustomLODReimport;
		}
		if (LodIndex > SkeletalMesh->GetLODNum())
		{
			bInvalidLodIndex = true;
		}
	}
	else if (StaticMesh)
	{
		InterchangeAssetImportData = Cast<UInterchangeAssetImportData>(StaticMesh->GetAssetImportData());
		if (StaticMesh->GetNumSourceModels() > LodIndex && InterchangeAssetImportData)
		{
			ImportType = EInterchangePipelineContext::AssetCustomLODReimport;
		}
		if (LodIndex > StaticMesh->GetNumSourceModels())
		{
			bInvalidLodIndex = true;
		}
	}
	else
	{
		//We support Import custom LOD only for skeletalmesh and staticmesh
		Promise->SetValue(false);
		return Promise->GetFuture();
	}

	if (bInvalidLodIndex)
	{
		UE_LOG(LogInterchangeEngine, Warning, TEXT("FInterchangeMeshUtilities::InternalImportCustomLod: Invalid mesh LOD index %d, no prior LOD index exists."), LodIndex);
		Promise->SetValue(false);
		return Promise->GetFuture();
	}

	FImportAssetParameters ImportAssetParameters;
	ImportAssetParameters.bIsAutomated = true;
	if (InterchangeAssetImportData)
	{
		TArray<UObject*> Pipelines = InterchangeAssetImportData->GetPipelines();
		for (UObject* SelectedPipeline : Pipelines)
		{
			UInterchangePipelineBase* GeneratedPipeline = nullptr;
			if (UInterchangePythonPipelineAsset* PythonPipelineAsset = Cast<UInterchangePythonPipelineAsset>(SelectedPipeline))
			{
				GeneratedPipeline = Cast<UInterchangePipelineBase>(StaticDuplicateObject(PythonPipelineAsset->GeneratedPipeline, GetTransientPackage()));
			}
			else
			{
				GeneratedPipeline = Cast<UInterchangePipelineBase>(StaticDuplicateObject(SelectedPipeline, GetTransientPackage()));
			}
			if (ensure(GeneratedPipeline))
			{
				GeneratedPipeline->AdjustSettingsForContext(ImportType, nullptr);
				ImportAssetParameters.OverridePipelines.Add(GeneratedPipeline);
			}
		}
	}

	FString ImportAssetPath = TEXT("/Engine/TempEditor/Interchange/") + FGuid::NewGuid().ToString(EGuidFormats::Base36Encoded);
	UE::Interchange::FAssetImportResultRef AssetImportResult = InterchangeManager.ImportAssetAsync(ImportAssetPath, SourceData, ImportAssetParameters);
	FString SourceDataFilename = SourceData->GetFilename();
	if (SkeletalMesh)
	{
		AssetImportResult->OnDone([Promise, SkeletalMesh, LodIndex, SourceDataFilename](UE::Interchange::FImportResult& ImportResult)
			{
				USkeletalMesh* SourceSkeletalMesh = Cast< USkeletalMesh >(ImportResult.GetFirstAssetOfClass(USkeletalMesh::StaticClass()));

				if(SourceSkeletalMesh)
				{
					//Make sure we can modify the skeletalmesh properties
					FSkinnedAssetAsyncBuildScope AsyncBuildScope(SkeletalMesh);
					Promise->SetValue(FLODUtilities::SetCustomLOD(SkeletalMesh, SourceSkeletalMesh, LodIndex, SourceDataFilename));
					SourceSkeletalMesh->ClearFlags(RF_Standalone);
					SourceSkeletalMesh->ClearInternalFlags(EInternalObjectFlags::Async);
				}
				else
				{
					Promise->SetValue(false);
				}
				
			});
	}
	else if (StaticMesh)
	{
		AssetImportResult->OnDone([Promise, StaticMesh, LodIndex, SourceDataFilename](UE::Interchange::FImportResult& ImportResult)
			{
				UStaticMesh* SourceStaticMesh = Cast< UStaticMesh >(ImportResult.GetFirstAssetOfClass(UStaticMesh::StaticClass()));
				if(SourceStaticMesh)
				{
					Promise->SetValue(StaticMesh->SetCustomLOD(SourceStaticMesh, LodIndex, SourceDataFilename));
					SourceStaticMesh->ClearFlags(RF_Standalone);
					SourceStaticMesh->ClearInternalFlags(EInternalObjectFlags::Async);
				}
				else
				{
					Promise->SetValue(false);
				}
			});
	}

	return Promise->GetFuture();
#else
	Promise->SetValue(false);
	return Promise->GetFuture();
#endif
}
