// Copyright Epic Games, Inc. All Rights Reserved.
#include "InterchangeTaskCreateSceneObjects.h"

#include "CoreMinimal.h"

#include "Async/TaskGraphInterfaces.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "InterchangeEngineLogPrivate.h"
#include "InterchangeFactoryBase.h"
#include "InterchangeImportCommon.h"
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
	TRACE_CPUPROFILER_EVENT_SCOPE(UE::Interchange::FTaskCreateSceneObjects::DoTask)
#if INTERCHANGE_TRACE_ASYNCHRONOUS_TASK_ENABLED
	INTERCHANGE_TRACE_ASYNCHRONOUS_TASK(SpawnActor)
#endif
	using namespace UE::Interchange;

	TOptional<FGCScopeGuard> GCScopeGuard;
	if (!IsInGameThread())
	{
		GCScopeGuard.Emplace();
	}

	TSharedPtr<FImportAsyncHelper> AsyncHelper = WeakAsyncHelper.Pin();
	check(WeakAsyncHelper.IsValid());

	//Verify if the task was canceled
	if (AsyncHelper->bCancel)
	{
		return;
	}

	UObject* ReimportObject = AsyncHelper->TaskData.ReimportObject;
	ULevel* ImportLevel = AsyncHelper->TaskData.ImportLevel ? AsyncHelper->TaskData.ImportLevel : GWorld->GetCurrentLevel();
	UWorld* ImportWorld = ImportLevel->GetWorld();
	const FString WorldPath = ImportWorld->GetOutermost()->GetPathName();
	const FString WorldName = ImportWorld->GetName();
	const FString NodePrefix = ImportLevel->GetName() + TEXT(".");

	for (UInterchangeFactoryBaseNode* FactoryNode : FactoryNodes)
	{
		if (!FactoryNode)
		{
			continue;
		}

		UInterchangeFactoryBase* Factory = NewObject<UInterchangeFactoryBase>(GetTransientPackage(), FactoryClass);
		Factory->SetResultsContainer(AsyncHelper->AssetImportResult->GetResults());
		AsyncHelper->AddCreatedFactory(FactoryNode->GetUniqueID(), Factory);

		FString SceneNodeName = FactoryNode->GetAssetName();
		SanitizeObjectName(SceneNodeName);

		UInterchangeFactoryBase::FImportSceneObjectsParams CreateSceneObjectsParams;
		CreateSceneObjectsParams.ObjectName = SceneNodeName;
		CreateSceneObjectsParams.FactoryNode = FactoryNode;
		CreateSceneObjectsParams.Level = ImportLevel;
		CreateSceneObjectsParams.ReimportObject = FFactoryCommon::GetObjectToReimport(ReimportObject, *FactoryNode, WorldPath, WorldName, NodePrefix + SceneNodeName);
		CreateSceneObjectsParams.ReimportFactoryNode = FFactoryCommon::GetFactoryNode(ReimportObject, WorldPath, WorldName, NodePrefix + SceneNodeName);

		if (AsyncHelper->BaseNodeContainers.IsValidIndex(SourceIndex))
		{
			CreateSceneObjectsParams.NodeContainer = AsyncHelper->BaseNodeContainers[SourceIndex].Get();
			CreateSceneObjectsParams.SourceData = AsyncHelper->SourceDatas[SourceIndex];
		}

		UObject* SceneObject = Factory->ImportSceneObject_GameThread(CreateSceneObjectsParams);
		if (SceneObject)
		{
			const FImportAsyncHelper::FImportedObjectInfo* ImportedInfoPtr = AsyncHelper->FindImportedSceneObjects(SourceIndex, [SceneObject](const FImportAsyncHelper::FImportedObjectInfo& CurInfo)
				{
					return CurInfo.ImportedObject == SceneObject;
				});

			if (!ImportedInfoPtr)
			{
				UE::Interchange::FImportAsyncHelper::FImportedObjectInfo& ObjectInfo = AsyncHelper->AddDefaultImportedSceneObjectGetRef(SourceIndex);
				ObjectInfo.ImportedObject = SceneObject;
				ObjectInfo.Factory = Factory;
				ObjectInfo.FactoryNode = FactoryNode;
			}

			FactoryNode->SetCustomReferenceObject(FSoftObjectPath(SceneObject));
		}
	}
}
