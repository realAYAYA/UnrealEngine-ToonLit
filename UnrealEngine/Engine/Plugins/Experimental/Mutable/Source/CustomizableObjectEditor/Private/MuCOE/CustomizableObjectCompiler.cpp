// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/CustomizableObjectCompiler.h"

#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "ClothConfig.h"
#include "HAL/RunnableThread.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "Materials/MaterialInterface.h"
#include "MessageLogModule.h"
#include "MuCO/CustomizableObjectPrivate.h"
#include "MuCO/CustomizableObjectInstance.h"
#include "MuCO/CustomizableObjectSystem.h"
#include "MuCO/ICustomizableObjectModule.h"
#include "MuCOE/CustomizableObjectCompileRunnable.h"
#include "MuCOE/CustomizableObjectEditorLogger.h"
#include "MuCOE/GenerateMutableSource/GenerateMutableSourceImage.h"
#include "MuCOE/GraphTraversal.h"
#include "MuCOE/ICustomizableObjectPopulationModule.h"
#include "MuCOE/Nodes/CustomizableObjectNodeObjectGroup.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTable.h"
#include "MuCOE/CustomizableObjectEditorModule.h"
#include "UObject/ICookInfo.h"
#include "UObject/UObjectIterator.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Interfaces/ITargetPlatform.h"
#include "Misc/App.h"
#include "MuCO/CustomizableObjectPrivate.h"
#include "MuCO/CustomizableObjectSystemPrivate.h"
#include "MuR/Model.h"
#include "MuR/ModelPrivate.h"
#include "MuCOE/CustomizableObjectVersionBridge.h"

class UTexture2D;

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"

#define UE_MUTABLE_COMPILE_REGION		TEXT("Mutable Compile")
#define UE_MUTABLE_PRELOAD_REGION		TEXT("Mutable Preload")
#define UE_MUTABLE_SAVEDD_REGION		TEXT("Mutable SaveDD")

UCustomizableObjectNodeObject* GetRootNode(UCustomizableObject* Object, bool &bOutMultipleBaseObjectsFound);



FCustomizableObjectCompiler::FCustomizableObjectCompiler() : FCustomizableObjectCompilerBase()
	, CompilationLaunchPending(false)
	, PreloadingReferencerAssets(false)
	, CurrentGAsyncLoadingTimeLimit(-1.0f)
	, CompletedUnrealToMutableTask(0)
	, MaxConvertToMutableTextureTime(0.2f)
{
	CustomizableObjectNumBoneInfluences = ICustomizableObjectModule::Get().GetNumBoneInfluences();
}



bool FCustomizableObjectCompiler::Tick()
{
	if (CompileTask)
	{
		CompileTask->Tick();
	}
	
	bool bUpdated = false;

	if (CompileTask.IsValid() && CompileTask->IsCompleted())
	{
		UE_LOG(LogMutable, Verbose, TEXT("PROFILE: [ %16.8f ] Finishing Compilation task for Object %s."), FPlatformTime::Seconds(), *CurrentObject->GetName());

		FinishCompilation();

		if (SaveDDTask.IsValid())
		{
			SaveCODerivedData();
		}
		else
		{
			bUpdated = true;
			SetCompilationState(ECustomizableObjectCompilationState::Completed);

			RemoveCompileNotification();

			NotifyCompilationErrors();
		}

		UE_LOG(LogMutable, Verbose, TEXT("PROFILE: [ %16.8f ] Finished Compilation task."), FPlatformTime::Seconds());
		UE_LOG(LogMutable, Verbose, TEXT("PROFILE: -----------------------------------------------------------"));

		CleanCachedReferencers();
		UpdateArrayGCProtect();

		CompilationLogsContainer.ClearMessagesArray();
	}

	if (SaveDDTask.IsValid() && SaveDDTask->IsCompleted())
	{
		SetCompilationState(ECustomizableObjectCompilationState::Completed);

		bUpdated = true;

		FinishSavingDerivedData();
	
		CurrentObject->GetPrivate()->PostCompile();

		UE_LOG(LogMutable, Verbose, TEXT("PROFILE: [ %16.8f ] Finished Saving Derived Data task."), FPlatformTime::Seconds());
		UE_LOG(LogMutable, Verbose, TEXT("PROFILE: -----------------------------------------------------------"));
	}

	if (CompilationLaunchPending)
	{
		CompilationLaunchPending = false;
		LaunchMutableCompile();
	}

	return bUpdated;
}


bool FCustomizableObjectCompiler::IsRootObject(const UCustomizableObject* Object) const
{
	// Look for the base object node
	UCustomizableObjectNodeObject* Root = nullptr;
	TArray<UCustomizableObjectNodeObject*> ObjectNodes;
	if (!Object->Source || !Object->Source->Nodes.Num())
	{
		// Conservative approach.
		return true;
	}

	Object->Source->GetNodesOfClass<UCustomizableObjectNodeObject>(ObjectNodes);

	for (TArray<UCustomizableObjectNodeObject*>::TIterator It(ObjectNodes); It; ++It)
	{
		if ((*It)->bIsBase)
		{
			Root = *It;
		}
	}

	return Root && !Root->ParentObject;
}


void FCustomizableObjectCompiler::PreloadingReferencerAssetsCallback(UCustomizableObject* Object, FCustomizableObjectCompiler* CustomizableObjectCompiler, const FCompilationOptions Options, bool bAsync)
{
	UE_LOG(LogMutable, Verbose, TEXT("PROFILE: [ %16.8f ] Preload asynchronously assets end."), FPlatformTime::Seconds());

	if (CustomizableObjectCompiler->GetCurrentGAsyncLoadingTimeLimit() != -1.0f)
	{
		static IConsoleVariable* CVAsyncLoadingTimeLimit = IConsoleManager::Get().FindConsoleVariable(TEXT("s.AsyncLoadingTimeLimit"));
		if (CVAsyncLoadingTimeLimit)
		{
			CVAsyncLoadingTimeLimit->Set(CustomizableObjectCompiler->GetCurrentGAsyncLoadingTimeLimit());
		}
		CustomizableObjectCompiler->SetCurrentGAsyncLoadingTimeLimit(-1.0f);
	}

	CustomizableObjectCompiler->UpdateArrayGCProtect();
	CustomizableObjectCompiler->SetPreloadingReferencerAssets(false);
	UCustomizableObjectSystem::GetInstance()->UnlockObject(Object);

	TRACE_END_REGION(UE_MUTABLE_PRELOAD_REGION);

	CustomizableObjectCompiler->CompileInternal(Object, Options, bAsync);
}


void FCustomizableObjectCompiler::Compile(UCustomizableObject& Object, const FCompilationOptions& InOptions, bool bAsync)
{
	TRACE_BEGIN_REGION(UE_MUTABLE_COMPILE_REGION);

	if (!UCustomizableObjectSystem::IsActive())
	{
		UE_LOG(LogMutable, Warning, TEXT("Failed to compile Customizable Object [%s]. Mutable is disabled. To enable it set the CVar Mutable.Enabled to true."), *Object.GetName());
		SetCompilationState(ECustomizableObjectCompilationState::Failed);

		return;
	}

	if (Object.VersionBridge && !Object.VersionBridge->GetClass()->ImplementsInterface(UCustomizableObjectVersionBridgeInterface::StaticClass()))
	{
		UE_LOG(LogMutable, Warning, TEXT("In Customizable Object [%s], the VersionBridge asset [%s] does not implement the required UCustomizableObjectVersionBridgeInterface."), 
			*Object.GetName(), *Object.VersionBridge.GetName());
		SetCompilationState(ECustomizableObjectCompilationState::Failed);

		return;
	}

	UCustomizableObjectSystem* System = UCustomizableObjectSystem::GetInstanceChecked();

	UE_LOG(LogMutable, Verbose, TEXT("PROFILE: [ %16.8f ] Preload asynchronously assets start."), FPlatformTime::Seconds());

	FString Message = FString::Printf(TEXT("Customizable Object %s is already being compiled or updated. Please wait a few seconds and try again."), *Object.GetName());
	FNotificationInfo Info(LOCTEXT("CustomizableObjectBeingCompilerOrUpdated", "Customizable Object compile and/or update still in process. Please wait a few seconds and try again."));
	Info.bFireAndForget = true;
	Info.bUseThrobber = true;
	Info.FadeOutDuration = 1.0f;
	Info.ExpireDuration = 1.0f;

	if (PreloadingReferencerAssets || CompilationLaunchPending || (Object.GetPrivate()->IsLocked()))
	{
		UE_LOG(LogMutable, Warning, TEXT("%s"), *Message);
		FSlateNotificationManager::Get().AddNotification(Info);

		return;
	}

	// Lock object during asynchronous asset loading to avoid instance/mip updates and reentrant compilations
	bool LockResult = System->LockObject(&Object);

	if (!LockResult)
	{		
		UE_LOG(LogMutable, Warning, TEXT("%s"), *Message);
		FSlateNotificationManager::Get().AddNotification(Info);

		return;
	}

	// Now that we know for sure that the CO is locked and there are no pending updates of instances using the CO,
	// destroy any live update instances, as they become invalid when recompiling the CO
	for (TObjectIterator<UCustomizableObjectInstance> It; It; ++It)
	{
		UCustomizableObjectInstance* Instance = *It;
		if (IsValid(Instance) &&
			Instance->GetCustomizableObject() == &Object)
		{
			Instance->DestroyLiveUpdateInstance();
		}
	}

	CurrentObject = &Object;

	PreloadingReferencerAssets = true;
	TRACE_BEGIN_REGION(UE_MUTABLE_PRELOAD_REGION);

	CleanCachedReferencers();
	UpdateArrayGCProtect();
	TArray<FName> ArrayReferenceNames;
	AddCachedReferencers(*Object.GetOuter()->GetPathName(), ArrayReferenceNames);

	TArray<FSoftObjectPath> ArrayAssetToStream;

	for (FAssetData& Element : ArrayAssetData)
	{
		ArrayAssetToStream.Add(Element.GetSoftObjectPath());
	}

	MaxConvertToMutableTextureTime = ComputeAsyncLoadingTimeLimit() * 0.001f;

	bool bAssetsLoaded = true;

	// Customizations are marked as editoronly on load and are not packaged into the runtime game by default.
 	// The ones that need to be kept will be copied into SoftObjectPath on the object during save.
	FCookLoadScope CookLoadScope(ECookLoadType::EditorOnly);
	if (ArrayAssetToStream.Num() > 0)
	{
		FStreamableManager& Streamable = System->GetPrivate()->StreamableManager;

		if (bAsync) 
		{
			AddCompileNotification(LOCTEXT("LoadingReferencerAssets", "Loading assets"));

			static IConsoleVariable* CVAsyncLoadingTimeLimit = IConsoleManager::Get().FindConsoleVariable(TEXT("s.AsyncLoadingTimeLimit"));
			if (CVAsyncLoadingTimeLimit)
			{
				CurrentGAsyncLoadingTimeLimit = CVAsyncLoadingTimeLimit->GetFloat();
				CVAsyncLoadingTimeLimit->Set(MaxConvertToMutableTextureTime * 1000.0f);
			}

			AsynchronousStreamableHandlePtr = Streamable.RequestAsyncLoad(ArrayAssetToStream, FStreamableDelegate::CreateStatic(PreloadingReferencerAssetsCallback, &Object, this, InOptions, bAsync));
			bAssetsLoaded = false;
		}
		else
		{
			Streamable.RequestSyncLoad(ArrayAssetToStream);
			bAssetsLoaded = true;
		}
	}

	if (bAssetsLoaded)
	{
		PreloadingReferencerAssetsCallback(&Object, this, InOptions, bAsync);
	}
}


void FCustomizableObjectCompiler::AddReferencedObjects(FReferenceCollector& Collector)
{
	// While compilation takes place, no COs involved can be garbage-collected
	const int32 MaxIndex = ArrayGCProtect.Num();

	for (int32 i = 0; i < MaxIndex; ++i)
	{
		Collector.AddReferencedObject(ArrayGCProtect[i]);
	}

	if (CurrentObject)
	{
		Collector.AddReferencedObject(CurrentObject);
	}
}


void FCustomizableObjectCompiler::UpdateArrayGCProtect()
{
	check(IsInGameThread());

	const int32 MaxIndex = ArrayAssetData.Num();
	ArrayGCProtect.SetNum(MaxIndex);

	for (int i = 0; i < MaxIndex; ++i)
	{
		ArrayGCProtect[i] = Cast<UCustomizableObject>(ArrayAssetData[i].GetAsset());
	}
}


void FCustomizableObjectCompiler::ProcessChildObjectsRecursively(UCustomizableObject* ParentObject, FMutableGraphGenerationContext& GenerationContext)
{
	TArray<FName> ArrayReferenceNames;
	AddCachedReferencers(*ParentObject->GetOuter()->GetPathName(), ArrayReferenceNames);
	UpdateArrayGCProtect();

	bool bMultipleBaseObjectsFound = false;

	for (const FName& ReferenceName : ArrayReferenceNames)
	{
		if (ArrayAlreadyProcessedChild.Contains(ReferenceName))
		{
			continue;
		}

		const FAssetData* AssetData = GetCachedAssetData(ReferenceName.ToString());

		UCustomizableObject* ChildObject = AssetData ? Cast<UCustomizableObject>(AssetData->GetAsset()) : nullptr;
		if (!ChildObject || ChildObject->HasAnyFlags(RF_Transient))
		{
			ArrayAlreadyProcessedChild.Add(ReferenceName);
			continue;
		}

		UCustomizableObjectNodeObject* Root = GetRootNode(ChildObject, bMultipleBaseObjectsFound);
		if (Root->ParentObject != ParentObject)
		{
			continue;
		}

		if (ChildObject->VersionStruct.IsValid())
		{
			if (!GenerationContext.Object->VersionBridge)
			{
				UE_LOG(LogMutable, Warning, TEXT("The child Customizable Object [%s] defines its VersionStruct Property but its root CustomizableObject doesn't define the VersionBridge property. There's no way to verify the VersionStruct has to be included in this compilation, so the child CustomizableObject will be omitted."), 
					*ChildObject->GetName());
				continue;
			}

			ICustomizableObjectVersionBridgeInterface* CustomizableObjectVersionBridgeInterface = Cast<ICustomizableObjectVersionBridgeInterface>(GenerationContext.Object->VersionBridge);

			if (CustomizableObjectVersionBridgeInterface)
			{
				if (!CustomizableObjectVersionBridgeInterface->IsVersionStructIncludedInCurrentRelease(ChildObject->VersionStruct))
				{
					continue;
				}
			}
			else
			{
				// This should never happen as the ICustomizableObjectVersionBridgeInterface was already checked at the start of the compilation
				ensure(false);
			}
		}

		ArrayAlreadyProcessedChild.Add(ReferenceName);

		if (!bMultipleBaseObjectsFound)
		{
			if (const FGroupNodeIdsTempData* GroupGuid = GenerationContext.DuplicatedGroupNodeIds.FindPair(ParentObject, FGroupNodeIdsTempData(Root->ParentObjectGroupId)))
			{
				Root->ParentObjectGroupId = GroupGuid->NewGroupNodeId;
			}

			GenerationContext.GroupIdToExternalNodeMap.Add(Root->ParentObjectGroupId, Root);

			TArray<UCustomizableObjectNodeObjectGroup*> GroupNodes;
			ChildObject->Source->GetNodesOfClass<UCustomizableObjectNodeObjectGroup>(GroupNodes);

			if (GroupNodes.Num() > 0) // Only grafs with group nodes should have child grafs
			{
				for (int32 i = 0; i < GroupNodes.Num(); ++i)
				{
					const FGuid NodeId = GenerationContext.GetNodeIdUnique(GroupNodes[i]);
					if (NodeId != GroupNodes[i]->NodeGuid)
					{
						GenerationContext.DuplicatedGroupNodeIds.Add(ChildObject, FGroupNodeIdsTempData(GroupNodes[i]->NodeGuid, NodeId));
						GroupNodes[i]->NodeGuid = NodeId;
					}
				}

				ProcessChildObjectsRecursively(ChildObject, GenerationContext);
			}
		}
	}
}


void FCustomizableObjectCompiler::DisplayParameterWarning(FMutableGraphGenerationContext& GenerationContext)
{
	for (const TPair<FString, TArray<const UObject*>>& It : GenerationContext.ParameterNamesMap)
	{
		if (It.Key == "")
		{
			FText MessageWarning = LOCTEXT("NodeWithNoName", ". There is at least one node with no name.");
			CompilerLog(MessageWarning, It.Value, EMessageSeverity::Warning, true);
		}
		else if (It.Value.Num() > 1)
		{
			FText MessageWarning = FText::Format(LOCTEXT("NodeWithRepeatedName", ". Several nodes have repeated name \"{0}\""), FText::FromString(It.Key));
			CompilerLog(MessageWarning, It.Value, EMessageSeverity::Warning, true);
		}
	}
}


void FCustomizableObjectCompiler::DisplayDuplicatedNodeIdsWarning(FMutableGraphGenerationContext & GenerationContext)
{
	for (const TPair<FGuid, TArray<const UObject*>>& It : GenerationContext.NodeIdsMap)
	{
		if (It.Value.Num() > 1)
		{
			FText MessageWarning = LOCTEXT("NodeWithRepeatedIds", ". Several nodes have repeated NodeIds, reconstruct the nodes.");
			CompilerLog(MessageWarning, It.Value, EMessageSeverity::Warning, true);
		}
	}
}


void FCustomizableObjectCompiler::DisplayUnnamedNodeObjectWarning(FMutableGraphGenerationContext& GenerationContext)
{
	FText Message = LOCTEXT("Unnamed Node Object", "Unnamed Node Object");
	for (const UCustomizableObjectNode* It : GenerationContext.NoNameNodeObjectArray)
	{
		CompilerLog(Message, It, EMessageSeverity::Warning, true);
	}
}


//void FCustomizableObjectCompiler::DisplayDiscardedPhysicsAssetSingleWarning(FMutableGraphGenerationContext& GenerationContext)
//{
//	if (GenerationContext.DiscartedPhysicsAssetMap.Num())
//	{
//		FString PhysicsAssetName;
//		for (int32 ComponentIndex = 0; ComponentIndex < GenerationContext.NumMeshComponentsInRoot; ++ComponentIndex)
//		{
//			USkeletalMesh* RefSkeletalMesh = GenerationContext.ComponentInfos.IsValidIndex(ComponentIndex) ? GenerationContext.ComponentInfos[ComponentIndex].RefSkeletalMesh : nullptr;
//			if (RefSkeletalMesh && GenerationContext.DiscartedPhysicsAssetMap.Find(RefSkeletalMesh->GetPhysicsAsset()))
//			{
//				PhysicsAssetName = RefSkeletalMesh->GetPhysicsAsset()->GetName();
//				break;
//			}
//		}
//		
//		if(PhysicsAssetName.IsEmpty())
//		{
//			PhysicsAssetName = (*GenerationContext.DiscartedPhysicsAssetMap.FindKey(0))->GetName();
//		}
//
//		FText Message = FText::Format( LOCTEXT("Discarted PhysicsAssets", "{0} and {1} other PhysicsAssets have been discarted because one or more Bodies have no corresponding bones in the SkeletalMesh.\nFor more infromation on the assets that need attention check the Output log."),
//										FText::FromString(PhysicsAssetName), FText::AsNumber(GenerationContext.DiscartedPhysicsAssetMap.Num()));
//		
//		CompilerLog(Message, nullptr, EMessageSeverity::Warning, false);
//	}
//}


void FCustomizableObjectCompiler::DisplayOrphanNodesWarning(FMutableGraphGenerationContext& GenerationContext)
{
	for (const TPair<FGeneratedKey, FGeneratedData>& It : GenerationContext.Generated)
	{
		if (const UCustomizableObjectNode* Node = Cast<UCustomizableObjectNode>(It.Value.Source))
		{
			if (Node->GetAllOrphanPins().Num() > 0)
			{
				CompilerLog(LOCTEXT("OrphanPinsWarningCompiler", "Node contains deprecated pins"), Node, EMessageSeverity::Warning, false);
			}
		}
	}
}


mu::NodeObjectPtr FCustomizableObjectCompiler::GenerateMutableRoot( 
	UCustomizableObject* Object, 
	FMutableGraphGenerationContext& GenerationContext, 
	FText& ErrorMsg, 
	bool& bOutIsRootObject)
{
	check(Object);
	
	if (!Object->Source)
	{
		ErrorMsg = LOCTEXT("NoSource", "Object with no valid graph found. Object not build.");

		if (IsRunningCookCommandlet() || IsRunningCookOnTheFly())
		{
			UE_LOG(LogMutable, Warning, TEXT("Compilation failed! Missing EDITORONLY data for Customizable Object [%s]. The object might have been loaded outside the Cooking context."), *Object->GetName());
		}

		return nullptr;
	}

	bool bMultipleBaseObjectsFound;
	bOutIsRootObject = false;
	UCustomizableObjectNodeObject* Root = GetRootNode(Object, bMultipleBaseObjectsFound);

	if (bMultipleBaseObjectsFound)
	{
		ErrorMsg = LOCTEXT("MutlipleBase","Multiple base object nodes found. Only one will be used.");
		return nullptr;
	}

	if (!Root)
	{
		ErrorMsg = LOCTEXT("NoBase","No base object node found. Object not built.");
		return nullptr;
	}

	bOutIsRootObject = Root->ParentObject == nullptr;

	UCustomizableObjectNodeObject* ActualRoot = Root;
	UCustomizableObject* ActualRootObject = Object;

	ArrayAlreadyProcessedChild.Empty();

	if (Root->ObjectName.IsEmpty())
	{
		GenerationContext.NoNameNodeObjectArray.AddUnique(Root);
	}

	if ((Object->MeshCompileType == EMutableCompileMeshType::Full) || Options.bIsCooking)
	{
		if (Root->ParentObject!=nullptr && Options.bIsCooking)
		{
			// This happens while packaging.
			return nullptr;
		}

		// We cannot load while saving. This should only happen in cooking and all assets should have been preloaded.
		if (!GIsSavingPackage)
		{
			UE_LOG(LogMutable, Verbose, TEXT("PROFILE: [ %16.8f ] Begin search for children."), FPlatformTime::Seconds());

			TArray<UCustomizableObject*> VisitedObjects;
			ActualRootObject = Root->ParentObject ? GetFullGraphRootObject(Root, VisitedObjects) : Object;

			if (Root->ParentObject != nullptr)
			{
				VisitedObjects.Empty();
				ActualRoot = GetFullGraphRootNodeObject(ActualRoot, VisitedObjects);
			}

			// The object doesn't reference a root object but is a root object, look for all the objects that reference it and get their root nodes
			FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
			ProcessChildObjectsRecursively(ActualRootObject, GenerationContext);
			UE_LOG(LogMutable, Verbose, TEXT("PROFILE: [ %16.8f ] End search for children."), FPlatformTime::Seconds());
		}
	}
	else
	{
		// Local, local with children and working set modes: add parents until whole CO graph root
		TArray<UCustomizableObjectNodeObject*> ArrayNodeObject;
		TArray<UCustomizableObject*> ArrayCustomizableObject;
		
		if (!GetParentsUntilRoot(Object, ArrayNodeObject, ArrayCustomizableObject))
		{
			CompilerLog(LOCTEXT("SkeletalMeshCycleFound", "Error! Cycle detected in the Customizable Object hierarchy."), Root);
			return nullptr;
		}

		if ((Object->MeshCompileType == EMutableCompileMeshType::AddWorkingSetNoChildren) ||
			(Object->MeshCompileType == EMutableCompileMeshType::AddWorkingSetAndChildren))
		{
			const int32 MaxIndex = Object->WorkingSet.Num();
			for (int32 i = 0; i < MaxIndex; ++i)
			{
				if (UCustomizableObject* WorkingSetObject = Object->WorkingSet[i].LoadSynchronous())
				{
					ArrayCustomizableObject.Reset();

					if (!GetParentsUntilRoot(WorkingSetObject, ArrayNodeObject, ArrayCustomizableObject))
					{
						CompilerLog(LOCTEXT("NoReferenceMesh", "Error! Cycle detected in the Customizable Object hierarchy."), Root);
						return nullptr;
					}
				}
			}
		}

		if ((Object->MeshCompileType == EMutableCompileMeshType::LocalAndChildren) ||
			(Object->MeshCompileType == EMutableCompileMeshType::AddWorkingSetAndChildren))
		{
			TArray<UCustomizableObjectNodeObjectGroup*> GroupNodes;
			Object->Source->GetNodesOfClass<UCustomizableObjectNodeObjectGroup>(GroupNodes);

			if (GroupNodes.Num() > 0) // Only graphs with group nodes should have child graphs
			{
				FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
				ProcessChildObjectsRecursively(Object, GenerationContext);
			}
		}

		for (int32 i = 0; i < ArrayNodeObject.Num(); ++i)
		{
			if (GenerationContext.GroupIdToExternalNodeMap.FindKey(ArrayNodeObject[i]) == nullptr)
			{
				GenerationContext.GroupIdToExternalNodeMap.Add(ArrayNodeObject[i]->ParentObjectGroupId, ArrayNodeObject[i]);
			}
		}

		TArray<UCustomizableObject*> VisitedObjects;

		if (Root->ParentObject != nullptr)
		{
			ActualRoot = GetFullGraphRootNodeObject(ActualRoot, VisitedObjects);
		}

		VisitedObjects.Empty();
		ActualRootObject = Root->ParentObject ? GetFullGraphRootObject(Root, VisitedObjects) : Object;

	}

	// Ensure that the CO has a valid AutoLODStrategy on the ActualRoot.
	if (ActualRoot->AutoLODStrategy == ECustomizableObjectAutomaticLODStrategy::Inherited)
	{
		CompilerLog(LOCTEXT("RootInheritsFromParent", "Error! Base CustomizableObject's LOD Strategy can't be set to 'Inherit from parent object'"), ActualRoot);
		return nullptr;
	}

	// Make sure we have a valid Reference SkeletalMesh and Skeleton for each component
	for (int32 ComponentIndex = 0; ComponentIndex < ActualRoot->NumMeshComponents; ++ComponentIndex)
	{
		USkeletalMesh* RefSkeletalMesh = ActualRootObject->GetRefSkeletalMesh(ComponentIndex);
		if (!RefSkeletalMesh)
		{
			CompilerLog(LOCTEXT("NoReferenceMeshObjectTab", "Error! Missing reference mesh in the Object Properties Tab"), ActualRoot);
			return nullptr;
		}

		USkeleton* RefSkeleton = RefSkeletalMesh->GetSkeleton();
		if(!RefSkeleton)
		{
			FText Msg = FText::Format(LOCTEXT("NoReferenceSkeleton", "Error! Missing skeleton in the reference mesh [{0}]"),
				FText::FromString(GenerationContext.CustomizableObjectWithCycle->GetPathName()));

			CompilerLog(Msg, ActualRoot);
			return nullptr;
		}

		// Add a new entry to the list of Component Infos 
		GenerationContext.ComponentInfos.Add(RefSkeletalMesh);

		// Make sure the Skeleton from the reference mesh is added to the list of referenced Skeletons.
		GenerationContext.ReferencedSkeletons.Add(RefSkeleton);
	}

	Object->ReferenceSkeletalMeshes = ActualRootObject->ReferenceSkeletalMeshes;

	GenerationContext.AddParticipatingObject(Object->ReferenceSkeletalMeshes);

    GenerationContext.RealTimeMorphTargetsOverrides = ActualRoot->RealTimeMorphSelectionOverrides;
    GenerationContext.RealTimeMorphTargetsOverrides.Reset();

	// Generate the object expression
	UE_LOG(LogMutable, Verbose, TEXT("PROFILE: [ %16.8f ] GenerateMutableSource start."), FPlatformTime::Seconds());
	mu::NodeObjectPtr MutableRoot = GenerateMutableSource(ActualRoot->OutputPin(), GenerationContext, !bOutIsRootObject);
	UE_LOG(LogMutable, Verbose, TEXT("PROFILE: [ %16.8f ] GenerateMutableSource end."), FPlatformTime::Seconds());

    ActualRoot->RealTimeMorphSelectionOverrides = GenerationContext.RealTimeMorphTargetsOverrides;
	GenerationContext.GenerateClippingCOInternalTags();

	GenerationContext.GenerateSharedSurfacesUniqueIds();

	// Generate ReferenceSkeletalMeshes data;
	PopulateReferenceSkeletalMeshesData(GenerationContext);
	
	//for (const USkeletalMesh* RefSkeletalMesh : Object->ReferenceSkeletalMeshes)
	//{
	//	GenerationContext.CheckPhysicsAssetInSkeletalMesh(RefSkeletalMesh);
	//}

	DisplayParameterWarning( GenerationContext );
	DisplayUnnamedNodeObjectWarning( GenerationContext );
	DisplayDuplicatedNodeIdsWarning( GenerationContext );
	//DisplayDiscardedPhysicsAssetSingleWarning( GenerationContext );
	DisplayOrphanNodesWarning( GenerationContext );

	if (GenerationContext.CustomizableObjectWithCycle)
	{
		ErrorMsg = FText::Format(LOCTEXT("CycleDetected","Cycle detected in graph of CustomizableObject {0}. Object not built."),
			FText::FromString(GenerationContext.CustomizableObjectWithCycle->GetPathName()));

		return nullptr;
	}

	return MutableRoot;
}


void FCustomizableObjectCompiler::AddCachedReferencers(const FName& PathName, TArray<FName>& ArrayReferenceNames)
{
	ArrayReferenceNames.Empty();
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	AssetRegistryModule.Get().GetReferencers(PathName, ArrayReferenceNames, UE::AssetRegistry::EDependencyCategory::Package, UE::AssetRegistry::EDependencyQuery::Hard);

	// Required to make compilations deterministic within editor runs.
	ArrayReferenceNames.Sort([](const FName& A, const FName& B)
	{
		return A.LexicalLess(B);
	});
	
	FARFilter Filter;
	for (const FName& ReferenceName : ArrayReferenceNames)
	{
		if (!IsCachedInAssetData(ReferenceName.ToString()) && !ReferenceName.ToString().StartsWith(TEXT("/TempAutosave")))
		{
			Filter.PackageNames.Add(ReferenceName);
		}
	}

	Filter.bIncludeOnlyOnDiskAssets = false;

	TArray<FAssetData> ArrayAssetDataTemp;
	AssetRegistryModule.Get().GetAssets(Filter, ArrayAssetDataTemp);

	// Store only those which have static class type Customizable Object, to avoid loading not needed elements
	const int32 MaxIndex = ArrayAssetDataTemp.Num();
	for (int32 i = 0; i < MaxIndex; ++i)
	{
		if (ArrayAssetDataTemp[i].GetClass() == UCustomizableObject::StaticClass())
		{
			ArrayAssetData.Add(ArrayAssetDataTemp[i]);
		}
	}
}


void FCustomizableObjectCompiler::LaunchMutableCompile()
{
	AddCompileNotification(LOCTEXT("CustomizableObjectCompileInProgress", "Compiling"));

	// Even for async build, we spawn a thread, so that we can set a large stack. 
	// Thread names need to be unique, apparently.
	static int ThreadCount = 0;
	FString ThreadName = FString::Printf(TEXT("MutableCompile-%03d"), ++ThreadCount);
	CompileThread = MakeShareable(FRunnableThread::Create(CompileTask.Get(), *ThreadName, 16 * 1024 * 1024, TPri_Normal));
}


void FCustomizableObjectCompiler::SaveCODerivedData()
{
	if (!SaveDDTask.IsValid())
	{
		return;
	}

	AddCompileNotification(LOCTEXT("SavingCustomizableObjectDerivedData", "Saving Data"));

	// Even for async saving derived data.
	static int SDDThreadCount = 0;
	FString ThreadName = FString::Printf(TEXT("MutableSDD-%03d"), ++SDDThreadCount);
	SaveDDThread = MakeShareable(FRunnableThread::Create(SaveDDTask.Get(), *ThreadName));
}


void FCustomizableObjectCompiler::CleanCachedReferencers()
{
	ArrayAssetData.Empty();
}


bool FCustomizableObjectCompiler::IsCachedInAssetData(const FString& PackageName)
{
	const int32 MaxIndex = ArrayAssetData.Num();

	for (int32 i = 0; i < MaxIndex; ++i)
	{
		if (ArrayAssetData[i].PackageName.ToString() == PackageName)
		{
			return true;
		}
	}

	return false;
}


FAssetData* FCustomizableObjectCompiler::GetCachedAssetData(const FString& PackageName)
{
	const int32 MaxIndex = ArrayAssetData.Num();

	for (int32 i = 0; i < MaxIndex; ++i)
	{
		if (ArrayAssetData[i].PackageName.ToString() == PackageName)
		{
			return &ArrayAssetData[i];
		}
	}

	return nullptr;
}


float FCustomizableObjectCompiler::ComputeAsyncLoadingTimeLimit()
{
	float DeltaTimeSeconds = FApp::GetDeltaTime();

	if (DeltaTimeSeconds == 0.0f)
	{
		return 100.0f;
	}

	// AsyncLoadingTimeLimit will depend on the speed of the computer, using delta time as reference
	float Value = 2.0f / DeltaTimeSeconds;
	Value = FMath::Clamp(Value, 75.0f, 250.0f);
	return Value;
}

void FCustomizableObjectCompiler::GetCompilationMessages(TArray<FText>& OutWarningMessages, TArray<FText>& OutErrorMessages) const
{
	CompilationLogsContainer.GetMessages(OutWarningMessages,OutErrorMessages);
}


void FCustomizableObjectCompiler::SetCompilationState(ECustomizableObjectCompilationState InState)
{
	State = InState;
	
	if (CurrentObject)
	{
		CurrentObject->GetPrivate()->CompilationState = InState;
	}
}


void FCustomizableObjectCompiler::CompileInternal(UCustomizableObject* Object, const FCompilationOptions& InOptions, bool bAsync)
{
	MUTABLE_CPUPROFILER_SCOPE(FCustomizableObjectCompiler::Compile)
	
	SetCompilationState(ECustomizableObjectCompilationState::Failed);

	if (!Object) return;

	if (bAsync && CompileTask.IsValid()) // Don't start compilation if there's a compilation running
	{
		UE_LOG(LogMutable, Log, TEXT("FCustomizableObjectCompiler::Compile An object is already being compiled."));
		return;
	}

	UE_LOG(LogMutable, Log, TEXT("PROFILE: [ %16.8f ] FCustomizableObjectCompiler::Compile start."), FPlatformTime::Seconds());

	CurrentObject = Object;

	// This is redundant but necessary to keep static analysis happy.
	if (!Object || !CurrentObject) return;

	Options = InOptions;
	Options.CustomizableObjectNumBoneInfluences = CustomizableObjectNumBoneInfluences;
	Options.bRealTimeMorphTargetsEnabled = Object->bEnableRealTimeMorphTargets;
	Options.bClothingEnabled = Object->bEnableClothing;
	Options.b16BitBoneWeightsEnabled = Object->bEnable16BitBoneWeights;
	Options.bSkinWeightProfilesEnabled = Object->bEnableAltSkinWeightProfiles;
	Options.bPhysicsAssetMergeEnabled = Object->bEnablePhysicsAssetMerge;
	Options.bAnimBpPhysicsManipulationEnabled = Object->bEnableAnimBpPhysicsAssetsManipualtion;
	Options.ImageTiling = Object->CompileOptions.ImageTiling;

	if (!InOptions.bIsCooking && IsRunningCookCommandlet())
	{
		UE_LOG(LogMutable, Display, TEXT("Editor compilation suspended for Customizable Object [%s]. Can not compile COs when the cook commandlet is running. "), *Object->GetName());
		return;
	}

	if (Object->GetPrivate()->IsLocked() || !UCustomizableObjectSystem::GetInstance()->LockObject(Object))
	{
		UE_LOG(LogMutable, Display, TEXT("Customizable Object is already being compiled or updated %s. Please wait a few seconds and try again."), *Object->GetName());
		return;
	}

	UE_LOG(LogMutable, Display, TEXT("Started Customizable Object Compile %s."), *Object->GetName());

	if (InOptions.bIsCooking && InOptions.TargetPlatform)
	{
		UE_LOG(LogMutable, Display, TEXT("Compiling Customizable Object %s for platform %s."), *Object->GetName(), *InOptions.TargetPlatform->PlatformName());
	}

	if (InOptions.bIsCooking && InOptions.bForceLargeLODBias)
	{
		UE_LOG(LogMutable, Display, TEXT("Compiling Customizable Object with %d LODBias."), InOptions.DebugBias);
	}

	FMutableGraphGenerationContext GenerationContext(Object, this, Options);
	GenerationContext.ParamNamesToSelectedOptions = ParamNamesToSelectedOptions;

	// If we don't have the target platform yet (in editor) we need to get it
	if (!GenerationContext.Options.TargetPlatform)
	{
		// Set the target platform in the context. For now it is the current platform.
		ITargetPlatformManagerModule* TPM = GetTargetPlatformManager();
		check(TPM);

		GenerationContext.Options.TargetPlatform = TPM->GetRunningTargetPlatform();
		check(GenerationContext.Options.TargetPlatform != nullptr);
	}

	// Clear Messages from previous Compilations
	CompilationLogsContainer.ClearMessageCounters();
	CompilationLogsContainer.ClearMessagesArray();

	// Generate the mutable node expression
	FText ErrorMessage;
	bool bIsRootObject = false;
	mu::NodeObjectPtr MutableRoot = GenerateMutableRoot(Object, GenerationContext, ErrorMessage, bIsRootObject);

	if (!MutableRoot)
	{
		if (!ErrorMessage.IsEmpty())
		{
			CompilerLog(ErrorMessage, nullptr);
		}
		else
		{
			CompilerLog(FText::FromString(TEXT("Failed to generate the mutable node graph. Object not built.")), nullptr);
		}

		if (Object->GetPrivate()->IsLocked())
		{
			UCustomizableObjectSystem::GetInstance()->UnlockObject(Object);
		}

		RemoveCompileNotification();
	}
	else
	{
		// Always work with the ModelResources (Editor) when compiling. They'll be copied to the cooked version during PreSave.
		FModelResources& ModelResources = Object->GetPrivate()->GetModelResources(false);
		ModelResources = FModelResources();
		
		ModelResources.ReferenceSkeletalMeshesData = MoveTemp(GenerationContext.ReferenceSkeletalMeshesData);

		ModelResources.Skeletons.Reserve(GenerationContext.ReferencedSkeletons.Num());
		for (const USkeleton* Skeleton : GenerationContext.ReferencedSkeletons)
		{
			ModelResources.Skeletons.Emplace(Skeleton);
		}
		
		ModelResources.Materials.Reserve(GenerationContext.ReferencedMaterials.Num());
		for (const UMaterialInterface* Material : GenerationContext.ReferencedMaterials)
		{
			ModelResources.Materials.Emplace(Material);
		}

		for (const TPair<TSoftObjectPtr<UTexture>, FMutableGraphGenerationContext::FGeneratedReferencedTexture>& Pair : GenerationContext.RuntimeReferencedTextureMap)
		{
			check(Pair.Value.ID == ModelResources.PassThroughTextures.Num());
			ModelResources.PassThroughTextures.Add(Pair.Key);
		}

		ModelResources.PhysicsAssets = MoveTemp(GenerationContext.PhysicsAssets);

		ModelResources.AnimBPs = MoveTemp(GenerationContext.AnimBPAssets);
		ModelResources.AnimBpOverridePhysiscAssetsInfo = MoveTemp(GenerationContext.AnimBpOverridePhysicsAssetsInfo);

		ModelResources.MaterialSlotNames = MoveTemp(GenerationContext.ReferencedMaterialSlotNames);
		ModelResources.BoneNames = MoveTemp(GenerationContext.BoneNames);
		ModelResources.SocketArray = MoveTemp(GenerationContext.SocketArray);

		ModelResources.SkinWeightProfilesInfo = MoveTemp(GenerationContext.SkinWeightProfilesInfo);

		TArray<FGeneratedImageProperties> ImageProperties;
		GenerationContext.ImageProperties.GenerateValueArray(ImageProperties);
		
		// Must sort image properties by ImagePropertiesIndex so that ImageNames point to the right properties.
		ImageProperties.Sort([](const FGeneratedImageProperties& PropsA, const FGeneratedImageProperties& PropsB)
			{ return PropsA.ImagePropertiesIndex < PropsB.ImagePropertiesIndex;	});

		ModelResources.ImageProperties.Empty(ImageProperties.Num());

		for (const FGeneratedImageProperties& ImageProp : ImageProperties)
		{
			ModelResources.ImageProperties.Add({ ImageProp.TextureParameterName,
										ImageProp.Filter,
										ImageProp.SRGB,
										ImageProp.bFlipGreenChannel,
										ImageProp.bIsPassThrough,
										ImageProp.LODBias,
										ImageProp.LODGroup,
										ImageProp.AddressX, ImageProp.AddressY });
		}

		ModelResources.ParameterUIDataMap = MoveTemp(GenerationContext.ParameterUIDataMap);
		ModelResources.StateUIDataMap = MoveTemp(GenerationContext.StateUIDataMap);

		ModelResources.RealTimeMorphTargetNames = MoveTemp(GenerationContext.RealTimeMorphTargetsNames);

		if (GenerationContext.RealTimeMorphTargetPerMeshData.Num() >= TNumericLimits<uint16>::Max())
		{
			UE_LOG(LogMutable, Warning, TEXT("Maximum number of meshes with realtime morph targets reached. Some morphs may not work."));
		}

		// Create the RealTimeMorphsTargets Blocks from the per mesh Morph data.
		uint64 RealTimeMorphDataSize = 0;
		for (const TArray<FMorphTargetVertexData>& VertexDataArray : GenerationContext.RealTimeMorphTargetPerMeshData)
		{
			RealTimeMorphDataSize += VertexDataArray.Num();
		}
		
		ModelResources.RealTimeMorphStreamableBlocks.Empty(32);
		ModelResources.EditorOnlyMorphTargetReconstructionData.Empty(RealTimeMorphDataSize);

		uint64 RealTimeMorphDataOffset = 0;
		for (const TArray<FMorphTargetVertexData>& VertexDataArray : GenerationContext.RealTimeMorphTargetPerMeshData)
		{
			ModelResources.RealTimeMorphStreamableBlocks.Emplace(FMutableStreamableBlock
					{
						uint32(0),
						(uint32)VertexDataArray.Num()*sizeof(FMorphTargetVertexData), 
						RealTimeMorphDataOffset, 
					});

			RealTimeMorphDataOffset += VertexDataArray.Num()*sizeof(FMorphTargetVertexData);
			ModelResources.EditorOnlyMorphTargetReconstructionData.Append(VertexDataArray);
		}
		
		// Clothing	
		Object->ClothMeshToMeshVertData = MoveTemp(GenerationContext.ClothMeshToMeshVertData);
		Object->ContributingClothingAssetsData = MoveTemp(GenerationContext.ContributingClothingAssetsData);
		Object->ClothSharedConfigsData.Empty();

		// A clothing backend, e.g. Chaos cloth, can use 2 config files, one owned by the asset, and another that is shared 
		// among all assets in a SkeletalMesh. When merging different assets in a skeletalmesh we need to make sure only one of 
		// the shared is used. In that case we will keep the first visited of a type and will be stored separated from the asset.
		// TODO: Shared configs, which typically controls the quality of the simulation (iterations, etc), probably should be specified 
		// somewhere else to give more control with which config ends up used. 
		auto IsSharedConfigData = [](const FCustomizableObjectClothConfigData& ConfigData) -> bool
		{
			 const UClass* ConfigClass = FindObject<UClass>(nullptr, *ConfigData.ClassPath);
			 return ConfigClass ? static_cast<bool>(Cast<UClothSharedConfigCommon>(ConfigClass->GetDefaultObject())) : false;
		};
		
		// Find shared configs to be used (One of each type) 
		for (FCustomizableObjectClothingAssetData& ClothingAssetData : Object->ContributingClothingAssetsData)
		{
			 for (FCustomizableObjectClothConfigData& ClothConfigData : ClothingAssetData.ConfigsData)
			 {
				  if (IsSharedConfigData(ClothConfigData))
				  {
					  FCustomizableObjectClothConfigData* FoundConfig = Object->ClothSharedConfigsData.FindByPredicate(
						   [Name = ClothConfigData.ConfigName](const FCustomizableObjectClothConfigData& Other)
						   {
							   return Name == Other.ConfigName;
						   });

					  if (!FoundConfig)
					  {
						   Object->ClothSharedConfigsData.AddDefaulted_GetRef() = ClothConfigData;
					  }
				  }
			 }
		}
		
		// Remove shared configs
		for (FCustomizableObjectClothingAssetData& ClothingAssetData : Object->ContributingClothingAssetsData)
		{
			 ClothingAssetData.ConfigsData.RemoveAllSwap(IsSharedConfigData);
		}

		Object->GetPrivate()->GroupNodeMap = GenerationContext.GroupNodeMap;

		if (GenerationContext.Options.OptimizationLevel == 0)
		{
			// If the optimization level is "none" disable texture streaming, because textures are all referenced
			// unreal assets and progressive generation is not supported.
			Object->GetPrivate()->bDisableTextureStreaming = true;
		}
		else
		{
			Object->GetPrivate()->bDisableTextureStreaming = false;
		}
		
		Object->GetPrivate()->bIsCompiledWithoutOptimization = GenerationContext.Options.OptimizationLevel < UE_MUTABLE_MAX_OPTIMIZATION;

		Object->GetPrivate()->GetAlwaysLoadedExtensionData() = MoveTemp(GenerationContext.AlwaysLoadedExtensionData);

		Object->GetPrivate()->GetStreamedExtensionData().Empty(GenerationContext.StreamedExtensionData.Num());
		for (UCustomizableObjectResourceDataContainer* Container : GenerationContext.StreamedExtensionData)
		{
			Object->GetPrivate()->GetStreamedExtensionData().Emplace(Container);
		}

#if WITH_EDITORONLY_DATA
		Object->GetPrivate()->CustomizableObjectPathMap = GenerationContext.CustomizableObjectPathMap;
#endif

		ModelResources.NumComponents = GenerationContext.NumMeshComponentsInRoot;
		ModelResources.NumLODs = GenerationContext.NumLODsInRoot;
		ModelResources.NumLODsToStream = GenerationContext.bEnableLODStreaming ? GenerationContext.NumMaxLODsToStream : 0;
		ModelResources.FirstLODAvailable = GenerationContext.FirstLODAvailable;

		Object->GetPrivate()->GetStreamedResourceData() = MoveTemp(GenerationContext.StreamedResourceData);

		// Pass-through textures
		TArray<TSoftObjectPtr<UTexture>> NewCompileTimeReferencedTextures;
		for (const TPair<TSoftObjectPtr<UTexture>, FMutableGraphGenerationContext::FGeneratedReferencedTexture>& Pair : GenerationContext.CompileTimeTextureMap)
		{
			check(Pair.Value.ID == NewCompileTimeReferencedTextures.Num());
			NewCompileTimeReferencedTextures.Add(Pair.Key);
		}

		if (!ParamNamesToSelectedOptions.Num())
		{
			// Get possible objects used in the compilation that are not directly referenced.
			// Due to this check being done also in PIE (to detect out of date compilations), it has to be performant. Therefore we are gathering a relaxed set.
			// For example, a referencing Customizable Object may not be used if it is not assigned in any Group Node. In the relaxed set we include those regardless.
			// Notice that, to avoid automatic compilations/warnings, the set of referencing objects set found here must coincide with the set found when loading the
			// model (discard previous compilations) or when showing PIE warnings.
			TArray<FName> ReferencingObjectNames;
			GetReferencingPackages(*Object, ReferencingObjectNames);

			for (const FName& ReferencingObjectName : ReferencingObjectNames)
			{
				const TSoftObjectPtr SoftObjectPtr(ReferencingObjectName.ToString());

				if (const UObject* ReferencingObject = SoftObjectPtr.LoadSynchronous())
				{
					GenerationContext.AddParticipatingObject(*ReferencingObject);					
				}
			}
			
			// Copy final array of participating objects
			Object->GetPrivate()->ParticipatingObjects = MoveTemp(GenerationContext.ParticipatingObjects);
			Object->GetPrivate()->DirtyParticipatingObjects.Empty();
		}

		if (CompileTask.IsValid()) // Don't start compilation if there's a compilation running
		{
			// TODO : warning?
			UCustomizableObjectSystem::GetInstance()->UnlockObject(CurrentObject);
			return;
		}

		// Lock the object to prevent instance updates while compiling. It's ignored and returns false if it's already locked
		// Will unlock in the FinishCompilation call.
		if (!Object->GetPrivate()->IsLocked())
		{
			UCustomizableObjectSystem::GetInstance()->LockObject(Object);
		}

		CompileTask = MakeShareable(new FCustomizableObjectCompileRunnable(MutableRoot));
		CompileTask->Options = Options;
		CompileTask->ReferencedTextures = NewCompileTimeReferencedTextures;
		SetCompilationState(ECustomizableObjectCompilationState::InProgress);

		if (!bAsync)
		{
			CompileTask->Init();
			CompileTask->Run();
			FinishCompilation();

			if (SaveDDTask.IsValid())
			{
				SaveDDTask->Init();
				SaveDDTask->Run();
				CurrentObject->GetPrivate()->GetModel()->GetPrivate()->UnloadRoms();
				FinishSavingDerivedData();
			}

			CleanCachedReferencers();
			UpdateArrayGCProtect();

			CurrentObject->GetPrivate()->PostCompile(); 

			SetCompilationState(ECustomizableObjectCompilationState::Completed);
		}
		else
		{
			// If packaging, convert textures and launch Mutable compile thread
			if (IsRunningCommandlet())
			{
				LaunchMutableCompile();
			}
			else
			{
				// If asynchronous compilation and not packaging, set CompilationLaunchPending to true and FCustomizableObjectCompiler::Tick()
				// will do the remaining steps (convert textures asynchronously and launch the Mutable compile thread)
				CompilationLaunchPending = true;
			}
		}
	}

	for (UCustomizableObjectNode* Node : GenerationContext.GeneratedNodes)
	{
		Node->ResetAttachedErrorData();
	}

	UE_LOG(LogMutable, Log, TEXT("PROFILE: [ %16.8f ] FCustomizableObjectCompiler::Compile end."), FPlatformTime::Seconds());

	// Population Recompilation
	if (MutableRoot)
	{
		//Checking if there is the population plugin
		if (FModuleManager::Get().IsModuleLoaded("CustomizableObjectPopulation"))
		{
			ICustomizableObjectPopulationModule::Get().RecompilePopulations(Object);
		}
	}
}


mu::NodePtr FCustomizableObjectCompiler::Export(UCustomizableObject* Object, const FCompilationOptions& InCompilerOptions, 
	TArray<TSoftObjectPtr<UTexture>>& OutRuntimeReferencedTextures,
	TArray<TSoftObjectPtr<UTexture>>& OutCompilerReferencedTextures )
{
	UE_LOG(LogMutable, Log, TEXT("Started Customizable Object Export %s."), *Object->GetName());

	FNotificationInfo Info(LOCTEXT("CustomizableObjectExportInProgress", "Exported Customizable Object"));
	Info.bFireAndForget = true;
	Info.bUseThrobber = true;
	Info.FadeOutDuration = 1.0f;
	Info.ExpireDuration = 1.0f;
	FSlateNotificationManager::Get().AddNotification(Info);

	FCompilationOptions CompilerOptions = InCompilerOptions;
	CompilerOptions.CustomizableObjectNumBoneInfluences = CustomizableObjectNumBoneInfluences;
		
	FMutableGraphGenerationContext GenerationContext(Object, this, CompilerOptions);
	GenerationContext.ParamNamesToSelectedOptions = ParamNamesToSelectedOptions;

	// Generate the mutable node expression
	FText ErrorMsg;
	bool bIsRootObject = false;
	mu::NodeObjectPtr MutableRoot = GenerateMutableRoot(Object, GenerationContext, ErrorMsg, bIsRootObject);

	if (!MutableRoot)
	{
		if (!ErrorMsg.IsEmpty())
		{
			FCustomizableObjectCompiler::CompilerLog(ErrorMsg, nullptr);
		}
		else
		{
			FCustomizableObjectCompiler::CompilerLog(LOCTEXT("FailedToGenerate","Failed to generate the mutable node graph. Object not built."), nullptr);
		}
		return nullptr;
	}

	// Pass out the references textures
	OutRuntimeReferencedTextures.Empty();
	for (const TPair<TSoftObjectPtr<UTexture>, FMutableGraphGenerationContext::FGeneratedReferencedTexture>& Pair : GenerationContext.RuntimeReferencedTextureMap)
	{
		check(Pair.Value.ID == OutRuntimeReferencedTextures.Num());
		OutRuntimeReferencedTextures.Add(Pair.Key);
	}

	OutCompilerReferencedTextures.Empty();
	for (const TPair<TSoftObjectPtr<UTexture>, FMutableGraphGenerationContext::FGeneratedReferencedTexture>& Pair : GenerationContext.CompileTimeTextureMap)
	{
		check(Pair.Value.ID == OutCompilerReferencedTextures.Num());
		OutCompilerReferencedTextures.Add(Pair.Key);
	}

	return MutableRoot;
}


void FCustomizableObjectCompiler::FinishCompilation()
{
	check(CompileTask.IsValid());

	UpdateCompilerLogData();
	TSharedPtr<mu::Model, ESPMode::ThreadSafe> Model = CompileTask->Model;

	// Generate a map that using the resource id tells the offset and size of the resource inside the bulk data
	// At this point it is assumed that all data goes into a single file.
	if (Model)
	{
		// Always work with the ModelResources (Editor) when compiling. They'll be copied to the cooked version during PreSave.
		FModelResources& ModelResources = CurrentObject->GetPrivate()->GetModelResources(false);
		
		const int32 NumStreamingFiles = Model->GetRomCount();
		ModelResources.HashToStreamableBlock.Empty(NumStreamingFiles);

		uint64 Offset = 0;
		for (int32 FileIndex = 0; FileIndex < NumStreamingFiles; ++FileIndex)
		{
			const uint32 ResourceId = Model->GetRomId(FileIndex);
			const uint32 ResourceSize = Model->GetRomSize(FileIndex);

			ModelResources.HashToStreamableBlock.Add(ResourceId, FMutableStreamableBlock{0, ResourceSize, Offset });
			Offset += ResourceSize;
		}
	}

	// Generate ParameterProperties and IntParameterLookUpTable
	CurrentObject->GetPrivate()->UpdateParameterPropertiesFromModel(Model);

	CurrentObject->GetPrivate()->SetModel(Model, GenerateIdentifier(*CurrentObject));
	
	// Reset all instances, as the parameters may need to be rebuilt.
	for (TObjectIterator<UCustomizableObjectInstance> It; It; ++It)
	{
		UCustomizableObjectInstance* Instance = *It;
		if (IsValid(Instance) &&
			Instance->GetCustomizableObject() == CurrentObject)
		{
			Instance->SetObject(CurrentObject);
		}
	}

	// Order matters
	CompileThread.Reset();
	CompileTask.Reset();

	TRACE_END_REGION(UE_MUTABLE_COMPILE_REGION);

	if (!Options.bDontUpdateStreamedDataAndCache)
	{
		TRACE_BEGIN_REGION(UE_MUTABLE_SAVEDD_REGION);

		SaveDDTask = MakeShareable(new FCustomizableObjectSaveDDRunnable(CurrentObject, Options));
	}
	else
	{
		CurrentObject->GetPrivate()->GetModel()->GetPrivate()->UnloadRoms();

		// when skipping the SaveDerivedData task unlock the object so that instances can be updated
		UCustomizableObjectSystem::GetInstance()->UnlockObject(CurrentObject);
	}

	UE_LOG(LogMutable, Display, TEXT("Finished Customizable Object Compile %s."), *CurrentObject->GetName());
}

void FCustomizableObjectCompiler::FinishSavingDerivedData()
{
	MUTABLE_CPUPROFILER_SCOPE(FinishSavingDerivedData)

	check(SaveDDTask.IsValid());

	if (Options.bIsCooking)
	{
		CurrentObject->GetPrivate()->CachePlatformData(
				SaveDDTask->GetTargetPlatform(), 
				SaveDDTask->Bytes, 
				SaveDDTask->BulkDataBytes,
				SaveDDTask->MorphDataBytes);
	}

	// Order matters
	SaveDDThread.Reset();
	SaveDDTask.Reset();

	CurrentObject->GetPrivate()->GetModel()->GetPrivate()->UnloadRoms();
	
	// Unlock the object so that instances can be updated
	UCustomizableObjectSystem::GetInstance()->UnlockObject(CurrentObject);

	RemoveCompileNotification();

	NotifyCompilationErrors();

	TRACE_END_REGION(UE_MUTABLE_SAVEDD_REGION);

}


void FCustomizableObjectCompiler::ForceFinishCompilation()
{
	if (CompileTask.IsValid())
	{
		// Compilation needs game thread tasks every now and then. Wait for compilation to finish while
		// giving execution time for these tasks.
		// TODO: interruptible compilations?
		while (!CompileTask->IsCompleted())
		{
			FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
		}

		CleanCachedReferencers();
		UpdateArrayGCProtect();

		UCustomizableObjectSystem* System = UCustomizableObjectSystem::GetInstance();

		// Check the system has not started BeginDestroy, it can crash during a GC during a compilation forced shutdown without this check
		if (!System->HasAnyFlags(EObjectFlags::RF_BeginDestroyed))
		{
			System->UnlockObject(CurrentObject);
		}
	}

	else if (SaveDDTask.IsValid())
	{
		SaveDDThread->WaitForCompletion();

		UCustomizableObjectSystem* System = UCustomizableObjectSystem::GetInstance();

		if (!System->HasAnyFlags(EObjectFlags::RF_BeginDestroyed))
		{
			System->UnlockObject(CurrentObject);
		}
	}

	RemoveCompileNotification();
}


void FCustomizableObjectCompiler::ForceFinishBeforeStartCompilation(UCustomizableObject* Object)
{
	CleanCachedReferencers();
	UpdateArrayGCProtect();

	if (Object && Object->GetPrivate()->IsLocked())
	{
		UCustomizableObjectSystem::GetInstance()->UnlockObject(Object);
	}

	RemoveCompileNotification();
}


void FCustomizableObjectCompiler::AddCompileNotification(const FText& CompilationStep) const
{
	const FText Text = CurrentObject ? FText::FromString(FString::Printf(TEXT("Compiling %s"), *CurrentObject->GetName())) : LOCTEXT("CustomizableObjectCompileInProgressNotification", "Compiling Customizable Object");
	
	FCustomizableObjectEditorLogger::CreateLog(Text)
	.SubText(CompilationStep)
	.Category(ELoggerCategory::Compilation)
	.Notification(!Options.bSilentCompilation)
	.CustomNotification()
	.FixNotification()
	.Log();
}


void FCustomizableObjectCompiler::RemoveCompileNotification()
{
	FCustomizableObjectEditorLogger::DismissNotification(ELoggerCategory::Compilation);
}


void FCustomizableObjectCompiler::NotifyCompilationErrors() const
{
	const uint32 NumWarnings = CompilationLogsContainer.GetWarningCount(false);
	const uint32 NumErrors = CompilationLogsContainer.GetErrorCount();
	const uint32 NumIgnoreds = CompilationLogsContainer.GetIgnoredCount();
	const bool NoWarningsOrErrors = !(NumWarnings || NumErrors);

	const EMessageSeverity::Type Severity = [&]
	{
		if (NumErrors)
		{
			return EMessageSeverity::Error;
		}
		else if (NumWarnings)
		{
			return EMessageSeverity::Warning;
		}
		else
		{
			return EMessageSeverity::Info;
		}
	}();

	const FText Prefix = FText::FromString(CurrentObject ? CurrentObject->GetName() : "Customizable Object");

	const FText Message = NoWarningsOrErrors ?
		FText::Format(LOCTEXT("CompilationFinishedSuccessfully", "{0} finished compiling."), Prefix) :
		NumIgnoreds > 0 ?
		FText::Format(LOCTEXT("CompilationFinished_WithIgnoreds", "{0} finished compiling with {1} {1}|plural(one=warning,other=warnings), {2} {2}|plural(one=error,other=errors) and {3} more similar warnings."), Prefix, NumWarnings, NumErrors, NumIgnoreds)
		:
		FText::Format(LOCTEXT("CompilationFinished_WithoutIgnoreds", "{0} finished compiling with {1} {1}|plural(one=warning,other=warnings) and {2} {2}|plural(one=error,other=errors)."), Prefix, NumWarnings, NumErrors);
	
	FCustomizableObjectEditorLogger::CreateLog(Message)
	.Category(ELoggerCategory::Compilation)
	.Severity(Severity)
	.Notification(!Options.bSilentCompilation || !NoWarningsOrErrors)
	.CustomNotification()
	.Log();
}


void FCustomizableObjectCompiler::CompilerLog(const FText& Message, const TArray<const UObject*>& Context, const EMessageSeverity::Type MessageSeverity, const bool bAddBaseObjectInfo, const ELoggerSpamBin SpamBin)
{
	if (CompilationLogsContainer.AddMessage(Message, Context, MessageSeverity, SpamBin)) // Cache the message for later reference
	{
		FCustomizableObjectEditorLogger::CreateLog(Message)
			.Severity(MessageSeverity)
			.Context(Context)
			.BaseObject(bAddBaseObjectInfo)
			.SpamBin(SpamBin)
			.Log();
	}
}


void FCustomizableObjectCompiler::CompilerLog(const FText& Message, const UObject* Context, const EMessageSeverity::Type MessageSeverity, const bool bAddBaseObjectInfo, const ELoggerSpamBin SpamBin)
{
	TArray<const UObject*> ContextArray;
	if (Context)
	{
		ContextArray.Add(Context);
	}
	CompilerLog(Message, ContextArray, MessageSeverity, bAddBaseObjectInfo, SpamBin);
}


void FCustomizableObjectCompiler::UpdateCompilerLogData()
{
	FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
	MessageLogModule.RegisterLogListing(FName("Mutable"), LOCTEXT("MutableLog", "Mutable"));
	const TArray<FCustomizableObjectCompileRunnable::FError>& ArrayCompileErrors = CompileTask->GetArrayErrors();

	const FText ObjectName = CurrentObject ? FText::FromString(CurrentObject->GetName()) : LOCTEXT("Unknown Object", "Unknown Object");

	for (const FCustomizableObjectCompileRunnable::FError& CompileError : ArrayCompileErrors)
	{
		const UObject* Object = static_cast<const UObject*>(CompileError.Context); // Context are always UObjects

		if (const UCustomizableObjectNode* Node = Cast<UCustomizableObjectNode>(Object))
		{
			if (CompileError.AttachedData)
			{
				UCustomizableObjectNode::FAttachedErrorDataView ErrorDataView;
				ErrorDataView.UnassignedUVs = { CompileError.AttachedData->UnassignedUVs.GetData(),
												CompileError.AttachedData->UnassignedUVs.Num() };

				const_cast<UCustomizableObjectNode*>(Node)->AddAttachedErrorData(ErrorDataView);
			}			
		}

		FText FullMsg = FText::Format(LOCTEXT("MutableMessage", "{0} : {1}"), ObjectName, CompileError.Message);
		CompilerLog(FullMsg, Object, CompileError.Severity, true, CompileError.SpamBin);
	}
}


void FCustomizableObjectCompiler::AddCompileOnlySelectedOption(const FString& ParamName, const FString& OptionValue)
{
	ParamNamesToSelectedOptions.Add(ParamName, OptionValue);
}


void FCustomizableObjectCompiler::ClearAllCompileOnlySelectedOption()
{
	ParamNamesToSelectedOptions.Empty();
}

#undef LOCTEXT_NAMESPACE
