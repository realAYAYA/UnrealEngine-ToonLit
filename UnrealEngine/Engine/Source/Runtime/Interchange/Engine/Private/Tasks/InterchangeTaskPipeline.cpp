// Copyright Epic Games, Inc. All Rights Reserved.
#include "InterchangeTaskPipeline.h"

#include "Async/TaskGraphInterfaces.h"
#include "CoreMinimal.h"
#include "InterchangeEngineLogPrivate.h"
#include "InterchangeManager.h"
#include "InterchangePipelineBase.h"
#include "Nodes/InterchangeBaseNodeContainer.h"
#include "Nodes/InterchangeFactoryBaseNode.h"
#include "Stats/Stats.h"
#include "Templates/SharedPointer.h"
#include "UObject/GarbageCollection.h"
#include "UObject/WeakObjectPtrTemplates.h"



void UE::Interchange::FTaskPipelinePreImport::DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	TRACE_CPUPROFILER_EVENT_SCOPE("UE::Interchange::FTaskPipelinePreImport::DoTask")
#if INTERCHANGE_TRACE_ASYNCHRONOUS_TASK_ENABLED
	INTERCHANGE_TRACE_ASYNCHRONOUS_TASK(PipelinePreImport)
#endif

	TOptional<FGCScopeGuard> GCScopeGuard;
	if (!IsInGameThread())
	{
		GCScopeGuard.Emplace();
	}

	TSharedPtr<FImportAsyncHelper, ESPMode::ThreadSafe> AsyncHelper = WeakAsyncHelper.Pin();
	check(AsyncHelper.IsValid());

	if (UInterchangePipelineBase* Pipeline = PipelineBase.Get())
	{
		Pipeline->SetResultsContainer(AsyncHelper->AssetImportResult->GetResults());

		for (int32 GraphIndex = 0; GraphIndex < AsyncHelper->BaseNodeContainers.Num(); ++GraphIndex)
		{
			//Verify if the task was cancel
			if (AsyncHelper->bCancel)
			{
				return;
			}

			if (ensure(AsyncHelper->BaseNodeContainers[GraphIndex].IsValid()))
			{
				Pipeline->ScriptedExecutePreImportPipeline(AsyncHelper->BaseNodeContainers[GraphIndex].Get(), AsyncHelper->SourceDatas);
			}
		}
	}
}

void UE::Interchange::FTaskPipelinePostImport::DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	TRACE_CPUPROFILER_EVENT_SCOPE("UE::Interchange::FTaskPipelinePostImport::DoTask")
#if INTERCHANGE_TRACE_ASYNCHRONOUS_TASK_ENABLED
	INTERCHANGE_TRACE_ASYNCHRONOUS_TASK(PipelinePostImport)
#endif

	TOptional<FGCScopeGuard> GCScopeGuard;
	if (!IsInGameThread())
	{
		GCScopeGuard.Emplace();
	}

	TSharedPtr<FImportAsyncHelper, ESPMode::ThreadSafe> AsyncHelper = WeakAsyncHelper.Pin();
	if (!ensure(AsyncHelper.IsValid()) || AsyncHelper->bCancel)
	{
		return;
	}

	if (!ensure(AsyncHelper->Pipelines.IsValidIndex(PipelineIndex)) || !ensure(AsyncHelper->BaseNodeContainers.IsValidIndex(SourceIndex)))
	{
		return;
	}
	UInterchangePipelineBase* PipelineBase = AsyncHelper->Pipelines[PipelineIndex];
	TArray<FString> NodeUniqueIDs;
	TArray<UObject*> ImportedObjects;
	TArray<bool> IsAssetsReimported;

	auto FillImportedObjectsFromSource =
		[&NodeUniqueIDs, &ImportedObjects, &IsAssetsReimported, this](FCriticalSection& CriticalSection, const TMap<int32, TArray<UE::Interchange::FImportAsyncHelper::FImportedObjectInfo>>& ImportedInfosPerSource)
		{
			FScopeLock Lock(&CriticalSection);
			if (ImportedInfosPerSource.Contains(SourceIndex))
			{
				const TArray<UE::Interchange::FImportAsyncHelper::FImportedObjectInfo>& ImportedInfos = ImportedInfosPerSource.FindChecked(SourceIndex);
				NodeUniqueIDs.Reserve(ImportedInfos.Num());
				ImportedObjects.Reserve(ImportedInfos.Num());
				IsAssetsReimported.Reserve(ImportedInfos.Num());
				for (const UE::Interchange::FImportAsyncHelper::FImportedObjectInfo& ImportedInfo : ImportedInfos)
				{
					NodeUniqueIDs.Add(ImportedInfo.FactoryNode->GetUniqueID());
					ImportedObjects.Add(ImportedInfo.ImportedObject);
					IsAssetsReimported.Add(ImportedInfo.bIsReimport);
				}
			}
		};

	FillImportedObjectsFromSource(AsyncHelper->ImportedAssetsPerSourceIndexLock, AsyncHelper->ImportedAssetsPerSourceIndex);
	FillImportedObjectsFromSource(AsyncHelper->ImportedSceneObjectsPerSourceIndexLock, AsyncHelper->ImportedSceneObjectsPerSourceIndex);

	if (!ensure(NodeUniqueIDs.Num() == ImportedObjects.Num()))
	{
		//We do not execute the script if we cannot give proper parameter
		return;
	}

	//Get the Container from the async helper
	UInterchangeBaseNodeContainer* NodeContainer = AsyncHelper->BaseNodeContainers[SourceIndex].Get();
	if (!ensure(NodeContainer))
	{
		return;
	}
	UInterchangePipelineBase* Pipeline = AsyncHelper->Pipelines[PipelineIndex];

	//Call the pipeline outside of the lock, we do this in case the pipeline take a long time. We call it for each asset created by this import
	for (int32 ObjectIndex = 0; ObjectIndex < ImportedObjects.Num(); ++ObjectIndex)
	{
		Pipeline->ScriptedExecutePostImportPipeline(NodeContainer, NodeUniqueIDs[ObjectIndex], ImportedObjects[ObjectIndex], IsAssetsReimported[ObjectIndex]);
	}
}
