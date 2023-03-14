// Copyright Epic Games, Inc. All Rights Reserved.
#include "InterchangeTaskCreateSceneObjects.h"

#include "CoreMinimal.h"

#include "Async/TaskGraphInterfaces.h"
#include "Engine/World.h"
#include "InterchangeEngineLogPrivate.h"
#include "InterchangeFactoryBase.h"
#include "InterchangeManager.h"
#include "InterchangeSourceData.h"
#include "InterchangeTranslatorBase.h"
#include "Misc/Paths.h"
#include "Nodes/InterchangeFactoryBaseNode.h"
#include "PackageUtils/PackageUtils.h"
#include "Stats/Stats.h"
#include "Templates/SharedPointer.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/WeakObjectPtrTemplates.h"

UE::Interchange::FTaskCreateSceneObjects::FTaskCreateSceneObjects(const FString& InPackageBasePath, const int32 InSourceIndex, TWeakPtr<FImportAsyncHelper> InAsyncHelper, TArrayView<UInterchangeFactoryBaseNode*> InFactoryNodes, const UClass* InFactoryClass)
	: PackageBasePath(InPackageBasePath)
	, SourceIndex(InSourceIndex)
	, WeakAsyncHelper(InAsyncHelper)
	, FactoryNodes(InFactoryNodes)
	, FactoryClass(InFactoryClass)
{
	check(FactoryClass);
}

void UE::Interchange::FTaskCreateSceneObjects::DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	TRACE_CPUPROFILER_EVENT_SCOPE("UE::Interchange::FTaskCreateSceneObjects::DoTask")
#if INTERCHANGE_TRACE_ASYNCHRONOUS_TASK_ENABLED
	INTERCHANGE_TRACE_ASYNCHRONOUS_TASK(SpawnActor)
#endif

	TOptional<FGCScopeGuard> GCScopeGuard;
	if (!IsInGameThread())
	{
		GCScopeGuard.Emplace();
	}

	TSharedPtr<UE::Interchange::FImportAsyncHelper> AsyncHelper = WeakAsyncHelper.Pin();
	check(WeakAsyncHelper.IsValid());

	//Verify if the task was canceled
	if (AsyncHelper->bCancel)
	{
		return;
	}

	for (UInterchangeFactoryBaseNode* FactoryNode : FactoryNodes)
	{
		UInterchangeFactoryBase* Factory = NewObject<UInterchangeFactoryBase>(GetTransientPackage(), FactoryClass);
		Factory->SetResultsContainer(AsyncHelper->AssetImportResult->GetResults());
		{
			FScopeLock Lock(&AsyncHelper->CreatedFactoriesLock);
			AsyncHelper->CreatedFactories.Add(FactoryNode->GetUniqueID(), Factory);
		}

		FString NodeDisplayName = FactoryNode->GetDisplayLabel();
		SanitizeObjectName(NodeDisplayName);

		UInterchangeFactoryBase::FCreateSceneObjectsParams CreateSceneObjectsParams;
		CreateSceneObjectsParams.ObjectName = NodeDisplayName;
		CreateSceneObjectsParams.FactoryNode = FactoryNode;
		CreateSceneObjectsParams.Level = GWorld->GetCurrentLevel();

		if (AsyncHelper->BaseNodeContainers.IsValidIndex(SourceIndex))
		{
			CreateSceneObjectsParams.NodeContainer = AsyncHelper->BaseNodeContainers[SourceIndex].Get();
		}

		UObject* SceneObject = Factory->CreateSceneObject(CreateSceneObjectsParams);
		if (SceneObject)
		{
			FScopeLock Lock(&AsyncHelper->ImportedSceneObjectsPerSourceIndexLock);
			TArray<UE::Interchange::FImportAsyncHelper::FImportedObjectInfo>& ImportedInfos = AsyncHelper->ImportedSceneObjectsPerSourceIndex.FindOrAdd(SourceIndex);
			UE::Interchange::FImportAsyncHelper::FImportedObjectInfo* ImportedInfoPtr = ImportedInfos.FindByPredicate([SceneObject](const UE::Interchange::FImportAsyncHelper::FImportedObjectInfo& CurInfo)
				{
					return CurInfo.ImportedObject == SceneObject;
				});

			if (!ImportedInfoPtr)
			{
				UE::Interchange::FImportAsyncHelper::FImportedObjectInfo& ObjectInfo = ImportedInfos.AddDefaulted_GetRef();
				ObjectInfo.ImportedObject = SceneObject;
				ObjectInfo.Factory = Factory;
				ObjectInfo.FactoryNode = FactoryNode;
			}

			FactoryNode->SetCustomReferenceObject(FSoftObjectPath(SceneObject));
		}
	}
}