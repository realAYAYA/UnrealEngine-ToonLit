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
#include "InterchangeResultsContainer.h"
#include "Nodes/InterchangeFactoryBaseNode.h"
#include "Stats/Stats.h"
#include "Templates/SharedPointer.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Object.h"
#include "UObject/UObjectHash.h"
#include "UObject/ObjectMacros.h"
#include "UObject/WeakObjectPtrTemplates.h"

void UE::Interchange::FTaskPreAsyncCompletion::DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	TRACE_CPUPROFILER_EVENT_SCOPE("UE::Interchange::FTaskPreAsyncCompletion::DoTask")
#if INTERCHANGE_TRACE_ASYNCHRONOUS_TASK_ENABLED
	INTERCHANGE_TRACE_ASYNCHRONOUS_TASK(PreAsyncCompletion)
#endif
	FGCScopeGuard GCScopeGuard;

	TSharedPtr<FImportAsyncHelper, ESPMode::ThreadSafe> AsyncHelper = WeakAsyncHelper.Pin();
	check(AsyncHelper.IsValid());

	//No need anymore of the translators sources
	AsyncHelper->ReleaseTranslatorsSource();
}

void UE::Interchange::FTaskPreCompletion::DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	TRACE_CPUPROFILER_EVENT_SCOPE("UE::Interchange::FTaskPreCompletion::DoTask")
#if INTERCHANGE_TRACE_ASYNCHRONOUS_TASK_ENABLED
	INTERCHANGE_TRACE_ASYNCHRONOUS_TASK(PreCompletion)
#endif
	TSharedPtr<FImportAsyncHelper, ESPMode::ThreadSafe> AsyncHelper = WeakAsyncHelper.Pin();
	check(AsyncHelper.IsValid());

	UInterchangeResultsContainer* Results = AsyncHelper->AssetImportResult->GetResults();

	auto ForEachImportedObjectForEachSource = [&AsyncHelper, &Results](TMap<int32, TArray<FImportAsyncHelper::FImportedObjectInfo>>& ImportedObjectsPerSourceIndex, bool bIsAsset)
	{
		for (TPair<int32, TArray<FImportAsyncHelper::FImportedObjectInfo>>& ObjectInfosPerSourceIndexPair : ImportedObjectsPerSourceIndex)
		{
			//Verify if the task was cancel
			if (AsyncHelper->bCancel)
			{
				for (const FImportAsyncHelper::FImportedObjectInfo& ObjectInfo : ObjectInfosPerSourceIndexPair.Value)
				{
					//Cancel factories so they can do proper cleanup
					if (ObjectInfo.Factory)
					{
						ObjectInfo.Factory->Cancel();
					}
				}
				break;
			}

			const int32 SourceIndex = ObjectInfosPerSourceIndexPair.Key;
			const bool bCallPostImportGameThreadCallback = ensure(AsyncHelper->SourceDatas.IsValidIndex(SourceIndex));

			for (const FImportAsyncHelper::FImportedObjectInfo& ObjectInfo : ObjectInfosPerSourceIndexPair.Value)
			{
				UObject* ImportedObject = ObjectInfo.ImportedObject;
				//In case Some factory code cannot run outside of the main thread we offer this callback to finish the work before calling post edit change (building the asset)
				if (bCallPostImportGameThreadCallback && ObjectInfo.Factory)
				{
					UInterchangeFactoryBase::FImportPreCompletedCallbackParams Arguments;
					Arguments.ImportedObject = ImportedObject;
					Arguments.SourceData = AsyncHelper->SourceDatas[SourceIndex];
					Arguments.FactoryNode = ObjectInfo.FactoryNode;
					// Should we assert if there is no factory node?
					Arguments.NodeUniqueID = ObjectInfo.FactoryNode ? ObjectInfo.FactoryNode->GetUniqueID() : FString();
					Arguments.NodeContainer = AsyncHelper->BaseNodeContainers[SourceIndex].Get();
					Arguments.Pipelines = AsyncHelper->Pipelines;
					Arguments.OriginalPipelines = AsyncHelper->OriginalPipelines;
					Arguments.bIsReimport = ObjectInfo.bIsReimport;
					ObjectInfo.Factory->PreImportPreCompletedCallback(Arguments);
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
						Actor->RegisterAllComponents();
					}
					else if (UActorComponent* Component = Cast<UActorComponent>(ImportedObject))
					{
						Component->RegisterComponent();
					}
				}

#if WITH_EDITOR
				ImportedObject->PostEditChange();
#endif

				//Post import broadcast
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

				//In case Some factory code cannot run outside of the main thread we offer this callback to finish the work after calling post edit change (building the asset)
				//Its possible the build of the asset to be asynchronous, the factory must handle is own asset correctly
				if (bCallPostImportGameThreadCallback && ObjectInfo.Factory)
				{
					UInterchangeFactoryBase::FImportPreCompletedCallbackParams Arguments;
					Arguments.ImportedObject = ImportedObject;
					Arguments.SourceData = AsyncHelper->SourceDatas[SourceIndex];
					Arguments.FactoryNode = ObjectInfo.FactoryNode;
					Arguments.NodeUniqueID = ObjectInfo.FactoryNode ? ObjectInfo.FactoryNode->GetUniqueID() : FString();
					Arguments.NodeContainer = AsyncHelper->BaseNodeContainers[SourceIndex].Get();
					Arguments.Pipelines = AsyncHelper->Pipelines;
					Arguments.OriginalPipelines = AsyncHelper->OriginalPipelines;
					Arguments.bIsReimport = ObjectInfo.bIsReimport;
					ObjectInfo.Factory->PostImportPreCompletedCallback(Arguments);
				}
			}
		}
	};

	bool bIsAsset = true;
	ForEachImportedObjectForEachSource(AsyncHelper->ImportedAssetsPerSourceIndex, bIsAsset);

	bIsAsset = false;
	ForEachImportedObjectForEachSource(AsyncHelper->ImportedSceneObjectsPerSourceIndex, bIsAsset);
}


void UE::Interchange::FTaskCompletion::DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	TRACE_CPUPROFILER_EVENT_SCOPE("UE::Interchange::FTaskCompletion::DoTask")
#if INTERCHANGE_TRACE_ASYNCHRONOUS_TASK_ENABLED
	INTERCHANGE_TRACE_ASYNCHRONOUS_TASK(Completion)
#endif
	TSharedPtr<FImportAsyncHelper, ESPMode::ThreadSafe> AsyncHelper = WeakAsyncHelper.Pin();
	check(AsyncHelper.IsValid());

	AsyncHelper->SendAnalyticImportEndData();
	//No need anymore of the translators sources
	AsyncHelper->ReleaseTranslatorsSource();

	//Broadcast OnAssetPostImport/OnAssetPostReimport for each imported asset
	for(TPair<int32, TArray<FImportAsyncHelper::FImportedObjectInfo>>& AssetInfosPerSourceIndexPair : AsyncHelper->ImportedAssetsPerSourceIndex)
	{
		//Verify if the task was cancel
		if (AsyncHelper->bCancel)
		{
			break;
		}
		const int32 SourceIndex = AssetInfosPerSourceIndexPair.Key;
		for (const FImportAsyncHelper::FImportedObjectInfo& AssetInfo : AssetInfosPerSourceIndexPair.Value)
		{
			UObject* Asset = AssetInfo.ImportedObject;
			if (AsyncHelper->TaskData.ReimportObject)
			{
				InterchangeManager->OnAssetPostReimport.Broadcast(Asset);
			}
			//We broadcast this event for both import and reimport.
			InterchangeManager->OnAssetPostImport.Broadcast(Asset);
		}
	}

	if (AsyncHelper->bCancel)
	{
		//If task is canceled, delete all created assets by this task
		for (TPair<int32, TArray<FImportAsyncHelper::FImportedObjectInfo>>& AssetInfosPerSourceIndexPair : AsyncHelper->ImportedAssetsPerSourceIndex)
		{
			const int32 SourceIndex = AssetInfosPerSourceIndexPair.Key;
			for (const FImportAsyncHelper::FImportedObjectInfo& AssetInfo : AssetInfosPerSourceIndexPair.Value)
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
		}

		//If task is canceled, remove all actors from their world
		for (TPair<int32, TArray<FImportAsyncHelper::FImportedObjectInfo>>& SceneObjectInfosPerSourceIndexPair : AsyncHelper->ImportedSceneObjectsPerSourceIndex)
		{
			const int32 SourceIndex = SceneObjectInfosPerSourceIndexPair.Key;
			for (const FImportAsyncHelper::FImportedObjectInfo& SceneObjectInfo : SceneObjectInfosPerSourceIndexPair.Value)
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
		}
	}

	AsyncHelper->AssetImportResult->SetDone();
	AsyncHelper->SceneImportResult->SetDone();

	//Release the async helper
	AsyncHelper = nullptr;
	InterchangeManager->ReleaseAsyncHelper(WeakAsyncHelper);
}
