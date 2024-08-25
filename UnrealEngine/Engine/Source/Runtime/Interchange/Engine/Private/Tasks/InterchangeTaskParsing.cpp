// Copyright Epic Games, Inc. All Rights Reserved.
#include "InterchangeTaskParsing.h"

#include "Async/TaskGraphInterfaces.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InterchangeEngineLogPrivate.h"
#include "InterchangeFactoryBase.h"
#include "InterchangeManager.h"
#include "InterchangeSourceData.h"
#include "InterchangeTaskCompletion.h"
#include "InterchangeTaskCreateSceneObjects.h"
#include "InterchangeTaskImportObject.h"
#include "InterchangeTaskPipeline.h"
#include "InterchangeTranslatorBase.h"
#include "Misc/Paths.h"
#include "Nodes/InterchangeBaseNode.h"
#include "Nodes/InterchangeBaseNodeContainer.h"
#include "PackageUtils/PackageUtils.h"
#include "Stats/Stats.h"
#include "Templates/SharedPointer.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/WeakObjectPtrTemplates.h"

/**
 * For the Dependency sort to work the predicate must be transitive ( A > B > C implying A > C).
 * That means we must take into account the whole dependency chain, not just the immediate dependencies.
 * 
 * This is a helper struct to quickly create the dependencies chain of a node using a cache.
 */
struct FNodeDependencyCache
{
	const TSet<FString>& GetAccumulatedDependencies(const UInterchangeBaseNodeContainer* NodeContainer, const FString& NodeID)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FNodeDependencyCache::GetAccumulatedDependencies)
		TSet<FString> NodeStack;
		return GetAccumulatedDependencies(NodeContainer, NodeID, NodeStack);
	}

private:

	const TSet<FString>& GetAccumulatedDependencies(const UInterchangeBaseNodeContainer* NodeContainer, const FString& NodeID, TSet<FString>& NodeStack)
	{
		if (const TSet<FString>* DependenciesPtr = CachedDependencies.Find(NodeID))
		{
			return *DependenciesPtr;
		}

		TSet<FString> Dependencies;
		AccumulateDependencies(NodeContainer, NodeID, Dependencies, NodeStack);
		return CachedDependencies.Add(NodeID, MoveTemp(Dependencies));
	}

	void AccumulateDependencies(const UInterchangeBaseNodeContainer* NodeContainer, const FString& NodeID, TSet<FString>& OutDependenciesSet, TSet<FString>& NodeStack)
	{
		const UInterchangeFactoryBaseNode* FactoryNode = NodeContainer->GetFactoryNode(NodeID);
		if (!FactoryNode)
		{
			return;
		}

		bool bAlreadyInSet = false;
		NodeStack.Add(NodeID, &bAlreadyInSet);
		if (ensureMsgf(!bAlreadyInSet, TEXT("FNodeDependencyCache::AccumulateDependencies - Node \"%s\" is in a circular dependency, assets may not be imported properly."), *NodeID))
		{
			TArray<FString> FactoryDependencies;
			FactoryNode->GetFactoryDependencies(FactoryDependencies);
			OutDependenciesSet.Reserve(OutDependenciesSet.Num() + FactoryDependencies.Num());
			for (const FString& DependencyID : FactoryDependencies)
			{
				bAlreadyInSet = false;
				OutDependenciesSet.Add(DependencyID, &bAlreadyInSet);
				// Avoid infinite recursion.
				if (!bAlreadyInSet)
				{
					OutDependenciesSet.Append(GetAccumulatedDependencies(NodeContainer, DependencyID, NodeStack));
				}
			}
			NodeStack.Remove(NodeID);
		}
	}

	TMap<FString, TSet<FString>> CachedDependencies;
};


void UE::Interchange::FTaskParsing::DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UE::Interchange::FTaskParsing::DoTask)
#if INTERCHANGE_TRACE_ASYNCHRONOUS_TASK_ENABLED
	INTERCHANGE_TRACE_ASYNCHRONOUS_TASK(ParsingGraph)
#endif
	LLM_SCOPE_BYNAME(TEXT("Interchange"));

	FGCScopeGuard GCScopeGuard;

	TSharedPtr<FImportAsyncHelper, ESPMode::ThreadSafe> AsyncHelper = WeakAsyncHelper.Pin();
	check(AsyncHelper.IsValid());

	//Parse each graph and prepare import task data, we will then be able to create all the task with the correct dependencies
	struct FTaskData
	{
		FString UniqueID;
		int32 SourceIndex = INDEX_NONE;
		bool bIsSceneNode = false;
		TArray<FString> Dependencies;
		FGraphEventRef GraphEventRef;
		FGraphEventArray Prerequisites;
		const UClass* FactoryClass;

		TArray<UInterchangeFactoryBaseNode*, TInlineAllocator<1>> Nodes; // For scenes, we can group multiple nodes into a single task as they are usually very light
	};

	TArray<FTaskData> TaskDatas;

	//Avoid creating asset if the asynchronous import is canceled, just create the completion task
	if (!AsyncHelper->bCancel)
	{
		for (int32 SourceIndex = 0; SourceIndex < AsyncHelper->SourceDatas.Num(); ++SourceIndex)
		{
			TArray<FTaskData> SourceTaskDatas;

			if (!AsyncHelper->BaseNodeContainers.IsValidIndex(SourceIndex))
			{
				continue;
			}

			UInterchangeBaseNodeContainer* BaseNodeContainer = AsyncHelper->BaseNodeContainers[SourceIndex].Get();
			if (!BaseNodeContainer)
			{
				continue;
			}

			//Translation and pipelines are not executed, compute the children cache for translated and factory nodes
			BaseNodeContainer->ComputeChildrenCache();

			const bool bCanImportSceneNode = AsyncHelper->TaskData.ImportType == EImportType::ImportType_Scene;
			BaseNodeContainer->IterateNodesOfType<UInterchangeFactoryBaseNode>([&](const FString& NodeUID, UInterchangeFactoryBaseNode* FactoryNode)
			{
				if (!FactoryNode->IsEnabled())
				{
					//Do not call factory for a disabled node
					return;
				}

				UClass* ObjectClass = FactoryNode->GetObjectClass();
				if (ObjectClass != nullptr)
				{
					const UClass* RegisteredFactoryClass = InterchangeManager->GetRegisteredFactoryClass(ObjectClass);

					const bool bIsSceneNode = FactoryNode->GetObjectClass()->IsChildOf<AActor>() || FactoryNode->GetObjectClass()->IsChildOf<UActorComponent>();

					if (!RegisteredFactoryClass || (bIsSceneNode && !bCanImportSceneNode))
					{
						//nothing we can import from this element
						return;
					}

					FTaskData& NodeTaskData = SourceTaskDatas.AddDefaulted_GetRef();
					NodeTaskData.UniqueID = FactoryNode->GetUniqueID();
					NodeTaskData.SourceIndex = SourceIndex;
					NodeTaskData.bIsSceneNode = bIsSceneNode;
					NodeTaskData.Nodes.Add(FactoryNode);
					FactoryNode->GetFactoryDependencies(NodeTaskData.Dependencies);
					NodeTaskData.FactoryClass = RegisteredFactoryClass;
				}
			});

			{
				FNodeDependencyCache DependencyCache;

				//Sort per dependencies
				auto SortByDependencies =
					[&BaseNodeContainer, &DependencyCache](const FTaskData& A, const FTaskData& B)
				{
					const TSet<FString>& BDependencies = DependencyCache.GetAccumulatedDependencies(BaseNodeContainer, B.UniqueID);
					//if A is a dependency of B then return true to do A before B
					if (BDependencies.Contains(A.UniqueID))
					{
						return true;
					}
					// Cache number of B's dependencies as reference on TSet can become stale
					const int32 BDependenciesNum = BDependencies.Num();

					const TSet<FString>& ADependencies = DependencyCache.GetAccumulatedDependencies(BaseNodeContainer, A.UniqueID);
					if (ADependencies.Contains(B.UniqueID))
					{
						return false;
					}

					return ADependencies.Num() <= BDependenciesNum;
				};

				// Nodes cannot depend on a node from another source, so it's faster to sort the dependencies per-source and then append those to the TaskData arrays.
				SourceTaskDatas.Sort(SortByDependencies);
			}

			TaskDatas.Append(MoveTemp(SourceTaskDatas));
		}
	}

	auto CreateTasksForEachTaskData = [](TArray<FTaskData>& TaskDatas, TFunction<FGraphEventRef(FTaskData&)> CreateTasksFunc) -> FGraphEventArray
	{
		FGraphEventArray GraphEvents;

		for (int32 TaskIndex = 0; TaskIndex < TaskDatas.Num(); ++TaskIndex)
		{
			FTaskData& TaskData = TaskDatas[TaskIndex];

			if (TaskData.Dependencies.Num() > 0)
			{
				//Search the previous node to find the dependence
				for (int32 DepTaskIndex = 0; DepTaskIndex < TaskIndex; ++DepTaskIndex)
				{
					if (TaskData.Dependencies.Contains(TaskDatas[DepTaskIndex].UniqueID))
					{
						//Add has prerequisite
						TaskData.Prerequisites.Add(TaskDatas[DepTaskIndex].GraphEventRef);
					}
				}
			}

			TaskData.GraphEventRef = CreateTasksFunc(TaskData);
			GraphEvents.Add(TaskData.GraphEventRef);
		}

		return GraphEvents;
	};

	struct FTaskParsingRenameInfo
	{
		UInterchangeFactoryBaseNode* FactoryNode = nullptr;
		int32 SourceIndex = INDEX_NONE;
		FString OriginalName = FString();
		FString NewName = FString();
	};

	TMap<FString, FTaskParsingRenameInfo> RenameAssets;
	TSet<FString> CreatedTasksAssetNames; // Tracks for which asset name we have created a task so that we don't have 2 tasks for the same asset name
	TFunction<FGraphEventRef(FTaskData&)> CreateTasksFromData = [this, &AsyncHelper, &RenameAssets, &CreatedTasksAssetNames](FTaskData& TaskData)
	{
		LLM_SCOPE_BYNAME(TEXT("Interchange"));
		check(TaskData.Nodes.Num() == 1); //We expect 1 node per asset task

		const int32 SourceIndex = TaskData.SourceIndex;
		const UClass* const FactoryClass = TaskData.FactoryClass;
		UInterchangeFactoryBaseNode* FactoryNode = TaskData.Nodes[0];

		if (TaskData.bIsSceneNode)
		{
			return AsyncHelper->SceneTasks.Add_GetRef(
				TGraphTask<FTaskCreateSceneObjects>::CreateTask(&(TaskData.Prerequisites))
				.ConstructAndDispatchWhenReady(AsyncHelper->ContentBasePath, SourceIndex, WeakAsyncHelper, TaskData.Nodes, FactoryClass));
		}
		else
		{
			FString PackageSubPath;
			FactoryNode->GetCustomSubPath(PackageSubPath);

			FString AssetFullPath = FPaths::Combine(AsyncHelper->ContentBasePath, PackageSubPath, FactoryNode->GetAssetName());

			//Make sure there is no duplicate name full path
			uint32 NameIndex = 1;
			FString NewName = AssetFullPath;
			bool NameClash = true;
			
			while (NameClash)
			{
				if (CreatedTasksAssetNames.Contains(NewName))
				{
					const FString NameIndexString = FString::FromInt(NameIndex++);
					NewName = AssetFullPath + NameIndexString;
					FTaskParsingRenameInfo& RenameInfo = RenameAssets.FindOrAdd(AssetFullPath);
					RenameInfo.FactoryNode = FactoryNode;
					RenameInfo.OriginalName = AssetFullPath;
					RenameInfo.NewName = NewName;
					RenameInfo.SourceIndex = SourceIndex;
					FactoryNode->SetDisplayLabel(FactoryNode->GetDisplayLabel() + NameIndexString);
					if (FactoryNode->HasAttribute(UE::Interchange::FBaseNodeStaticData::AssetNameKey()))
					{
						//TextureFactorNodes automatically set up the AssetNames which then are used for asset creation:
						FactoryNode->SetAssetName(FactoryNode->GetDisplayLabel());
					}
				}
				else
				{
					AssetFullPath = NewName;
					NameClash = false;
				}
			}

			if (ensureMsgf(!CreatedTasksAssetNames.Contains(AssetFullPath),
				TEXT("Found multiple task data with the same asset name (%s). Only one will be executed."), *AssetFullPath))
			{
				LLM_SCOPE_BYNAME(TEXT("Interchange"));
				FGraphEventArray ImportObjectTasksPrerequistes;
				int32 BeginImportObjectTaskIndex = AsyncHelper->BeginImportObjectTasks.Add(
					TGraphTask<FTaskImportObject_GameThread>::CreateTask(&(TaskData.Prerequisites)).ConstructAndDispatchWhenReady(AsyncHelper->ContentBasePath, SourceIndex, WeakAsyncHelper, FactoryNode, FactoryClass)
				);
				ImportObjectTasksPrerequistes.Add(AsyncHelper->BeginImportObjectTasks[BeginImportObjectTaskIndex]);

				int32 ImportObjectTaskIndex = AsyncHelper->ImportObjectTasks.Add(
					TGraphTask<FTaskImportObject_Async>::CreateTask(&(ImportObjectTasksPrerequistes)).ConstructAndDispatchWhenReady(AsyncHelper->ContentBasePath, SourceIndex, WeakAsyncHelper, FactoryNode)
				);

				FGraphEventArray FinalizeImportObjectTasksPrerequistes;
				FinalizeImportObjectTasksPrerequistes.Add(AsyncHelper->ImportObjectTasks[ImportObjectTaskIndex]);

				int32 FinalizeCreateTaskIndex = AsyncHelper->FinalizeImportObjectTasks.Add(
					TGraphTask<FTaskImportObjectFinalize_GameThread>::CreateTask(&(FinalizeImportObjectTasksPrerequistes)).ConstructAndDispatchWhenReady(AsyncHelper->ContentBasePath, SourceIndex, WeakAsyncHelper, FactoryNode)
				);

				CreatedTasksAssetNames.Add(AssetFullPath);

				return AsyncHelper->FinalizeImportObjectTasks[FinalizeCreateTaskIndex];
			}
			else
			{
				FGraphEventRef EmptyGraphEvent = FGraphEvent::CreateGraphEvent();
				EmptyGraphEvent->DispatchSubsequents();
				return EmptyGraphEvent;
			}
		}
	};

	//If we find nothing to import, we notify the user
	if (TaskDatas.Num() == 0)
	{
		for(TObjectPtr<UInterchangeTranslatorBase> Translator : AsyncHelper->Translators)
		{
			//We need a valid translator to add a result message
			if (!Translator || AsyncHelper->bCancel)
			{
				continue;
			}
			UInterchangeResultError_Generic* ErrorResult = NewObject<UInterchangeResultError_Generic>(GetTransientPackage(), UInterchangeResultError_Generic::StaticClass());
			ErrorResult->Text = NSLOCTEXT("InterchangeTaskParsing", "TranslatorNoAssetImported", "There was no data to import in the provided source data.");
			ErrorResult->SourceAssetName = Translator->GetSourceData()->GetFilename();
			Translator->AddMessage(ErrorResult);
		}
	}

	FGraphEventArray CompletionPrerequistes;
	const int32 PoolWorkerThreadCount = FTaskGraphInterface::Get().GetNumWorkerThreads() / 2;
	const int32 MaxNumWorker = FMath::Max(PoolWorkerThreadCount, 1);
	FGraphEventArray GroupPrerequistes;
	for (int32 TaskIndex = 0; TaskIndex < TaskDatas.Num(); ++TaskIndex)
	{
		FTaskData& TaskData = TaskDatas[TaskIndex];

		if (TaskData.Dependencies.Num() > 0)
		{
			//Search the previous node to find the dependence
			for (int32 DepTaskIndex = 0; DepTaskIndex < TaskIndex; ++DepTaskIndex)
			{
				if (TaskData.Dependencies.Contains(TaskDatas[DepTaskIndex].UniqueID))
				{
					//Add has prerequisite
					TaskData.Prerequisites.Add(TaskDatas[DepTaskIndex].GraphEventRef);
				}
			}
		}

		//Append the group prerequisite to the task data prerequisite if the group is full
		//This allow to chain the group dependencies to control the number of task
		if (GroupPrerequistes.Num() >= MaxNumWorker)
		{
			TaskData.Prerequisites.Append(GroupPrerequistes);
			GroupPrerequistes.Reset();
		}
		TaskData.GraphEventRef = CreateTasksFromData(TaskData);
		GroupPrerequistes.Add(TaskData.GraphEventRef);
	}
	CompletionPrerequistes.Append(GroupPrerequistes);

	if (!RenameAssets.IsEmpty())
	{
		TMap<TWeakObjectPtr<UInterchangeTranslatorBase>, FString> TranslatorMessageMap;
		for (const TPair<FString, FTaskParsingRenameInfo>& RenameAssetKvp : RenameAssets)
		{
			const FTaskParsingRenameInfo& RenameInfo = RenameAssetKvp.Value;
			FString& Message = TranslatorMessageMap.FindOrAdd(AsyncHelper->Translators[RenameInfo.SourceIndex]);
			Message += FText::Format(NSLOCTEXT("InterchangeTaskParsingDoTask", "RenamedAssetMessagePerAsset", "\n OriginalName:[{0}] NewName:[{1}]")
				, FText::FromString(RenameInfo.OriginalName)
				, FText::FromString(RenameInfo.NewName)).ToString();
		}
		for (const TPair<TWeakObjectPtr<UInterchangeTranslatorBase>, FString>& MessagePerTranslator : TranslatorMessageMap)
		{
			if (GIsAutomationTesting)
			{
				
				UInterchangeResultDisplay_Generic* DisplayResult = NewObject<UInterchangeResultDisplay_Generic>(GetTransientPackage(), UInterchangeResultDisplay_Generic::StaticClass());
				FString Message = NSLOCTEXT("InterchangeTaskParsingDoTask", "RenamedAssetsMessageHeader", "Renamed Assets:").ToString();
				Message += MessagePerTranslator.Value;
				DisplayResult->Text = FText::FromString(Message);
				MessagePerTranslator.Key->AddMessage(DisplayResult);
			}
			else
			{
				UInterchangeResultWarning_Generic* WarningResult = NewObject<UInterchangeResultWarning_Generic>(GetTransientPackage(), UInterchangeResultWarning_Generic::StaticClass());
				FString Message = NSLOCTEXT("InterchangeTaskParsingDoTask", "RenamedAssetsMessageHeader", "Renamed Assets:").ToString();
				Message += MessagePerTranslator.Value;
				WarningResult->Text = FText::FromString(Message);
				MessagePerTranslator.Key->AddMessage(WarningResult);
			}
		}
	}

	//Add an async task for pre completion
	FGraphEventArray PreCompletionPrerequistes;
	AsyncHelper->PreCompletionTask = TGraphTask<FTaskPreCompletion>::CreateTask(&CompletionPrerequistes).ConstructAndDispatchWhenReady(InterchangeManager, WeakAsyncHelper);
	PreCompletionPrerequistes.Add(AsyncHelper->PreCompletionTask);

	FGraphEventArray AssetCompilationPrerequistes;
	//Start the wait for asset compilation task on a async task
	for (int32 SourceIndex = 0; SourceIndex < AsyncHelper->SourceDatas.Num(); ++SourceIndex)
	{
		AsyncHelper->WaitAssetCompilationTask = TGraphTask<FTaskWaitAssetCompilation>::CreateTask(&PreCompletionPrerequistes).ConstructAndDispatchWhenReady(SourceIndex, WeakAsyncHelper);
		AssetCompilationPrerequistes.Add(AsyncHelper->WaitAssetCompilationTask);
	}

	//Start the Post pipeline task
	for (int32 SourceIndex = 0; SourceIndex < AsyncHelper->SourceDatas.Num(); ++SourceIndex)
	{
		int32 GraphTaskIndex = AsyncHelper->PostImportTasks.Add(
			TGraphTask<FTaskPostImport>::CreateTask(&(AssetCompilationPrerequistes)).ConstructAndDispatchWhenReady(SourceIndex, WeakAsyncHelper)
		);
		//Ensure we run the pipeline in the same order we create the task, since the pipeline modifies the node container, its important that its not processed in parallel, Adding the one we start to the prerequisites
		//is the way to go here
		AssetCompilationPrerequistes.Add(AsyncHelper->PostImportTasks[GraphTaskIndex]);
	}

	AsyncHelper->CompletionTask = TGraphTask<FTaskCompletion>::CreateTask(&AssetCompilationPrerequistes).ConstructAndDispatchWhenReady(InterchangeManager, WeakAsyncHelper);
}
