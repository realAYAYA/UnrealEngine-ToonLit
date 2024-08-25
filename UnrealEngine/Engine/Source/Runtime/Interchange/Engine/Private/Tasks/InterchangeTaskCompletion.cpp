// Copyright Epic Games, Inc. All Rights Reserved.
#include "InterchangeTaskCompletion.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Async/TaskGraphInterfaces.h"
#include "CoreMinimal.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "InterchangeEngineLogPrivate.h"
#include "InterchangeFactoryBase.h"
#include "InterchangeManager.h"
#include "InterchangePipelineBase.h"
#include "InterchangeResultsContainer.h"
#include "Nodes/InterchangeFactoryBaseNode.h"
#include "Stats/Stats.h"
#include "Templates/SharedPointer.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Object.h"
#include "UObject/UObjectHash.h"
#include "UObject/ObjectMacros.h"
#include "UObject/WeakObjectPtrTemplates.h"

void UE::Interchange::FTaskPreCompletion::DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UE::Interchange::FTaskPreCompletion::DoTask)
#if INTERCHANGE_TRACE_ASYNCHRONOUS_TASK_ENABLED
	INTERCHANGE_TRACE_ASYNCHRONOUS_TASK(PreCompletion)
#endif
	
	LLM_SCOPE_BYNAME(TEXT("Interchange"));

	TSharedPtr<FImportAsyncHelper, ESPMode::ThreadSafe> AsyncHelper = WeakAsyncHelper.Pin();
	check(AsyncHelper.IsValid());

	UInterchangeResultsContainer* Results = AsyncHelper->AssetImportResult->GetResults();

	bool bIsAsset = true;

	auto IterationCallback = [&AsyncHelper, &Results, &bIsAsset](int32 SourceIndex, const TArray<FImportAsyncHelper::FImportedObjectInfo>& ImportedObjects)
	{
		LLM_SCOPE_BYNAME(TEXT("Interchange"));
		//Verify if the task was cancel
		if (AsyncHelper->bCancel)
		{
			for (const FImportAsyncHelper::FImportedObjectInfo& ObjectInfo : ImportedObjects)
			{
				//Cancel factories so they can do proper cleanup
				if (ObjectInfo.Factory)
				{
					ObjectInfo.Factory->Cancel();
				}
			}
			//Skip if cancel
			return;
		}

		const bool bCallPostImportGameThreadCallback = ensure(AsyncHelper->SourceDatas.IsValidIndex(SourceIndex));

		UInterchangeFactoryBase::FSetupObjectParams Arguments;
		Arguments.SourceData = AsyncHelper->SourceDatas[SourceIndex];
		Arguments.NodeContainer = AsyncHelper->BaseNodeContainers[SourceIndex].Get();
		Arguments.Pipelines = AsyncHelper->Pipelines;
		Arguments.OriginalPipelines = AsyncHelper->OriginalPipelines;
		Arguments.Translator = AsyncHelper->Translators[SourceIndex];

		//First iteration to call SetupObject_GameThread and pipeline ExecutePostFactoryPipeline
		for (const FImportAsyncHelper::FImportedObjectInfo& ObjectInfo : ImportedObjects)
		{
			UObject* ImportedObject = ObjectInfo.ImportedObject;
			//In case Some factory code cannot run outside of the main thread we offer this callback to finish the work before calling post edit change (building the asset)
			if (bCallPostImportGameThreadCallback && ObjectInfo.Factory)
			{
				Arguments.ImportedObject = ImportedObject;
				// Should we assert if there is no factory node?
				Arguments.FactoryNode = ObjectInfo.FactoryNode;
				Arguments.NodeUniqueID = ObjectInfo.FactoryNode ? ObjectInfo.FactoryNode->GetUniqueID() : FString();
				Arguments.bIsReimport = ObjectInfo.bIsReimport;
				ObjectInfo.Factory->SetupObject_GameThread(Arguments);
			}

			if (ImportedObject == nullptr || !IsValid(ImportedObject))
			{
				continue;
			}

			UInterchangeResultSuccess* Message = Results->Add<UInterchangeResultSuccess>();
			Message->SourceAssetName = AsyncHelper->SourceDatas[SourceIndex]->GetFilename();
			Message->DestinationAssetName = ImportedObject->GetPathName();
			Message->AssetType = ImportedObject->GetClass();

			//Clear any async flag from the created asset and all its subobjects
			const EInternalObjectFlags AsyncFlags = EInternalObjectFlags::Async | EInternalObjectFlags::AsyncLoading;
			ImportedObject->ClearInternalFlags(AsyncFlags);

			TArray<UObject*> ImportedSubobjects;
			const bool bIncludeNestedObjects = true;
			GetObjectsWithOuter(ImportedObject, ImportedSubobjects, bIncludeNestedObjects);
			for (UObject* ImportedSubobject : ImportedSubobjects)
			{
				ImportedSubobject->ClearInternalFlags(AsyncFlags);
			}

			//Make sure the package is dirty
			ImportedObject->MarkPackageDirty();

			if (!bIsAsset)
			{
				if (AActor* Actor = Cast<AActor>(ImportedObject))
				{
#if WITH_EDITOR
					Message->AssetFriendlyName = Actor->GetActorLabel();
#endif
					Actor->RegisterAllComponents();
				}
				else if (UActorComponent* Component = Cast<UActorComponent>(ImportedObject))
				{
					Component->RegisterComponent();
				}
			}

			for (UInterchangePipelineBase* PipelineBase : AsyncHelper->Pipelines)
			{
				PipelineBase->ScriptedExecutePostFactoryPipeline(AsyncHelper->BaseNodeContainers[SourceIndex].Get()
					, ObjectInfo.FactoryNode ? ObjectInfo.FactoryNode->GetUniqueID() : FString()
					, ImportedObject
					, ObjectInfo.bIsReimport);
			}
		}

#if WITH_EDITOR
		//Second iteration to call PostEditChange
		for (const FImportAsyncHelper::FImportedObjectInfo& ObjectInfo : ImportedObjects)
		{
			UObject* ImportedObject = ObjectInfo.ImportedObject;
			if (ImportedObject == nullptr || !IsValid(ImportedObject))
			{
				continue;
			}
			ImportedObject->PostEditChange();
		}
#endif //WITH_EDITOR

		//Third iteration to register the assets
		for (const FImportAsyncHelper::FImportedObjectInfo& ObjectInfo : ImportedObjects)
		{
			UObject* ImportedObject = ObjectInfo.ImportedObject;
			if (ImportedObject == nullptr || !IsValid(ImportedObject))
			{
				continue;
			}
			//Register the assets
			if (bIsAsset)
			{
				AsyncHelper->AssetImportResult->AddImportedObject(ImportedObject);

				if (!AsyncHelper->TaskData.ReimportObject)
				{
					//Notify the asset registry, only when we have created the asset
					FAssetRegistryModule::AssetCreated(ImportedObject);
				}
			}
			else
			{
				AsyncHelper->SceneImportResult->AddImportedObject(ImportedObject);
			}
		}
	};

	//Asset import
	bIsAsset = true;
	AsyncHelper->IterateImportedAssetsPerSourceIndex(IterationCallback);

	//Scene import
	bIsAsset = false;
	AsyncHelper->IterateImportedSceneObjectsPerSourceIndex(IterationCallback);
}


void UE::Interchange::FTaskCompletion::DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UE::Interchange::FTaskCompletion::DoTask)
#if INTERCHANGE_TRACE_ASYNCHRONOUS_TASK_ENABLED
	INTERCHANGE_TRACE_ASYNCHRONOUS_TASK(Completion)
#endif

	LLM_SCOPE_BYNAME(TEXT("Interchange"));

	TSharedPtr<FImportAsyncHelper, ESPMode::ThreadSafe> AsyncHelper = WeakAsyncHelper.Pin();
	check(AsyncHelper.IsValid());

	AsyncHelper->SendAnalyticImportEndData();
	//No need anymore of the translators sources
	AsyncHelper->ReleaseTranslatorsSource();

	if (!AsyncHelper->bCancel)
	{
		//Broadcast OnAssetPostImport/OnAssetPostReimport for each imported asset
		AsyncHelper->IterateImportedAssetsPerSourceIndex([AsyncHelper](int32 SourceIndex, const TArray<FImportAsyncHelper::FImportedObjectInfo>& AssetInfos)
			{
				for (const FImportAsyncHelper::FImportedObjectInfo& AssetInfo : AssetInfos)
				{
					UObject* Asset = AssetInfo.ImportedObject;
					if (AsyncHelper->TaskData.ReimportObject && AsyncHelper->TaskData.ReimportObject == Asset)
					{
						UInterchangeManager::GetInterchangeManager().OnAssetPostReimport.Broadcast(Asset);
					}
					//We broadcast this event for both import and reimport.
					UInterchangeManager::GetInterchangeManager().OnAssetPostImport.Broadcast(Asset);
				}

				UE_LOG(LogInterchangeEngine, Display, TEXT("Interchange import completed [%s]"), *AsyncHelper->SourceDatas[SourceIndex]->ToDisplayString());
			});
	}
	else
	{
		//If task is canceled, delete all created assets by this task
		AsyncHelper->IterateImportedAssetsPerSourceIndex([](int32 SourceIndex, const TArray<FImportAsyncHelper::FImportedObjectInfo>& AssetInfos)
			{
				for (const FImportAsyncHelper::FImportedObjectInfo& AssetInfo : AssetInfos)
				{
					UObject* Asset = AssetInfo.ImportedObject;
					if (Asset)
					{
						//Make any created asset go away
						Asset->ClearFlags(RF_Standalone | RF_Public | RF_Transactional);
						Asset->ClearInternalFlags(EInternalObjectFlags::Async);
						Asset->SetFlags(RF_Transient);
						Asset->MarkAsGarbage();
					}
				}
			});

		//If task is canceled, remove all actors from their world
		AsyncHelper->IterateImportedSceneObjectsPerSourceIndex([AsyncHelper](int32 SourceIndex, const TArray<FImportAsyncHelper::FImportedObjectInfo>& AssetInfos)
			{
				for (const FImportAsyncHelper::FImportedObjectInfo& SceneObjectInfo : AssetInfos)
				{
					if (AActor* Actor = Cast<AActor>(SceneObjectInfo.ImportedObject))
					{
						if (UWorld* ActorWorld = Actor->GetWorld())
						{
							const bool bModifyLevel = false; //This isn't undoable
							ActorWorld->RemoveActor(Actor, bModifyLevel);
						}
					}
				}

				UE_LOG(LogInterchangeEngine, Display, TEXT("Interchange import cancelled [%s]"), *AsyncHelper->SourceDatas[SourceIndex]->ToDisplayString());
			});
	}

	AsyncHelper->AssetImportResult->SetDone();
	AsyncHelper->SceneImportResult->SetDone();

	//Release the async helper
	AsyncHelper = nullptr;
	InterchangeManager->ReleaseAsyncHelper(WeakAsyncHelper);
}
