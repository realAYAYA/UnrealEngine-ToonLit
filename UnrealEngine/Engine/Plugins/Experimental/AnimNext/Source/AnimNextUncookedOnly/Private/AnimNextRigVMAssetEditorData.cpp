// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextRigVMAssetEditorData.h"

#include "AnimNextRigVMAsset.h"
#include "AnimNextRigVMAssetEntry.h"
#include "ControlRigDefines.h"
#include "ExternalPackageHelper.h"
#include "IAnimNextRigVMGraphInterface.h"
#include "IAnimNextRigVMParameterInterface.h"
#include "UncookedOnlyUtils.h"
#include "Misc/TransactionObjectEvent.h"
#include "Param/AnimNextTag.h"
#include "RigVMModel/RigVMFunctionLibrary.h"
#include "RigVMModel/RigVMNotifications.h"
#include "RigVMModel/Nodes/RigVMCollapseNode.h"
#include "UObject/AssetRegistryTagsContext.h"

void UAnimNextRigVMAssetEditorData::BroadcastModified()
{
	RecompileVM();

	if(!bSuspendEditorDataNotifications)
	{
		ModifiedDelegate.Broadcast(this);
	}
}

void UAnimNextRigVMAssetEditorData::ReportError(const TCHAR* InMessage) const
{
	FScriptExceptionHandler::Get().HandleException(ELogVerbosity::Error, InMessage, TEXT(""));
}

void UAnimNextRigVMAssetEditorData::ReconstructAllNodes()
{
	// Avoid refreshing EdGraph nodes during cook
	if (GIsCookerLoadingPackage)
	{
		return;
	}
	
	if (GetRigVMClient()->GetDefaultModel() == nullptr)
	{
		return;
	}

	TArray<URigVMEdGraphNode*> AllNodes;
	GetAllNodesOfClass(AllNodes);

	for (URigVMEdGraphNode* Node : AllNodes)
	{
		Node->SetFlags(RF_Transient);
	}

	for(URigVMEdGraphNode* Node : AllNodes)
	{
		Node->ReconstructNode();
	}

	for (URigVMEdGraphNode* Node : AllNodes)
	{
		Node->ClearFlags(RF_Transient);
	}
}

void UAnimNextRigVMAssetEditorData::Serialize(FArchive& Ar)
{
	RigVMClient.SetOuterClientHost(this, GET_MEMBER_NAME_CHECKED(UAnimNextRigVMAssetEditorData, RigVMClient));

	Super::Serialize(Ar);
}

void UAnimNextRigVMAssetEditorData::Initialize(bool bRecompileVM)
{
	RigVMClient.bDefaultModelCanBeRemoved = true;
	RigVMClient.SetControllerClass(GetControllerClass());
	RigVMClient.SetSchemaClass(GetRigVMSchemaClass());
	RigVMClient.SetExecuteContextStruct(GetExecuteContextStruct());
	RigVMClient.SetOuterClientHost(this, GET_MEMBER_NAME_CHECKED(UAnimNextRigVMAssetEditorData, RigVMClient));
	{
		TGuardValue<bool> DisableClientNotifs(RigVMClient.bSuspendNotifications, true);
		RigVMClient.GetOrCreateFunctionLibrary(false);
	}

	if (RigVMClient.GetController(0) == nullptr)
	{
		if(RigVMClient.GetDefaultModel())
		{
			RigVMClient.GetOrCreateController(RigVMClient.GetDefaultModel());
		}

		check(RigVMClient.GetFunctionLibrary());
		RigVMClient.GetOrCreateController(RigVMClient.GetFunctionLibrary());

		// Init function library controllers
		for(URigVMLibraryNode* LibraryNode : RigVMClient.GetFunctionLibrary()->GetFunctions())
		{
			RigVMClient.GetOrCreateController(LibraryNode->GetContainedGraph());
		}
		
		if(bRecompileVM)
		{
			RecompileVM();
		}
	}

	for(UAnimNextRigVMAssetEntry* Entry : Entries)
	{
		Entry->Initialize(this);
	}
}

void UAnimNextRigVMAssetEditorData::PostLoad()
{
	Super::PostLoad();

	Initialize(/*bRecompileVM*/false);

	RefreshAllModels(ERigVMLoadType::PostLoad);

	PostLoadExternalPackages();

	// delay compilation until the package has been loaded
	FCoreUObjectDelegates::OnEndLoadPackage.AddUObject(this, &UAnimNextRigVMAssetEditorData::HandlePackageDone);
}

void UAnimNextRigVMAssetEditorData::PostLoadExternalPackages()
{
	FExternalPackageHelper::LoadObjectsFromExternalPackages<UAnimNextRigVMAssetEntry>(this, [this](UAnimNextRigVMAssetEntry* InLoadedEntry)
	{
		check(IsValid(InLoadedEntry));
		InLoadedEntry->Initialize(this);
		Entries.Add(InLoadedEntry);
	});
}

void UAnimNextRigVMAssetEditorData::PostTransacted(const FTransactionObjectEvent& TransactionEvent)
{
	Super::PostTransacted(TransactionEvent);

	if (TransactionEvent.GetEventType() == ETransactionObjectEventType::UndoRedo)
	{
		BroadcastModified();
	}
}

void UAnimNextRigVMAssetEditorData::PostDuplicate(EDuplicateMode::Type DuplicateMode)
{
	Initialize(/*bRecompileVM*/true);
}

void UAnimNextRigVMAssetEditorData::GetAssetRegistryTags(FAssetRegistryTagsContext Context) const
{
	Super::GetAssetRegistryTags(Context);

	FString TagValue;
	FAnimNextParameterProviderAssetRegistryExports::StaticStruct()->ExportText(TagValue, &CachedExports, nullptr, nullptr, PPF_None, nullptr);
	Context.AddTag(FAssetRegistryTag(UE::AnimNext::ExportsAnimNextAssetRegistryTag, TagValue, FAssetRegistryTag::TT_Hidden));
}

void UAnimNextRigVMAssetEditorData::HandlePackageDone(const FEndLoadPackageContext& Context)
{
	if (!Context.LoadedPackages.Contains(GetPackage()))
	{
		return;
	}
	HandlePackageDone();
}

void UAnimNextRigVMAssetEditorData::HandlePackageDone()
{
	FCoreUObjectDelegates::OnEndLoadPackage.RemoveAll(this);

	RecompileVM();

	ReconstructAllNodes();
}

void UAnimNextRigVMAssetEditorData::RefreshAllModels(ERigVMLoadType InLoadType)
{
	const bool bIsPostLoad = InLoadType == ERigVMLoadType::PostLoad;

	TGuardValue<bool> IsCompilingGuard(bIsCompiling, true);
	TGuardValue<bool> ClientIgnoreModificationsGuard(RigVMClient.bIgnoreModelNotifications, true);

	TArray<URigVMGraph*> GraphsToDetach = RigVMClient.GetAllModels(true, false);
	TMap<const URigVMGraph*, TArray<URigVMController::FLinkedPath>> LinkedPaths;
	
	if (ensure(IsInGameThread()))
	{
		for (const URigVMGraph* GraphToDetach : GraphsToDetach)
		{
			URigVMController* Controller = RigVMClient.GetOrCreateController(GraphToDetach);
			// temporarily disable default value validation during load time, serialized values should always be accepted
			TGuardValue<bool> PerGraphDisablePinDefaultValueValidation(Controller->bValidatePinDefaults, false);
			FRigVMControllerNotifGuard NotifGuard(Controller, true);
			LinkedPaths.Add(GraphToDetach, Controller->GetLinkedPaths());
			Controller->FastBreakLinkedPaths(LinkedPaths.FindChecked(GraphToDetach));
			TArray<URigVMNode*> Nodes = GraphToDetach->GetNodes();
			for (URigVMNode* Node : Nodes)
			{
				Controller->RepopulatePinsOnNode(Node, true, true);
			}
		}
		//SetupPinRedirectorsForBackwardsCompatibility();
	}

	for (const URigVMGraph* GraphToDetach : GraphsToDetach)
	{
		URigVMController* Controller = RigVMClient.GetOrCreateController(GraphToDetach);
		// at this stage, allow all links to be reattached,
		// RecomputeAllTemplateFilteredPermutations() later should break any invalid links
		FRigVMControllerNotifGuard NotifGuard(Controller, true);
		Controller->RestoreLinkedPaths(LinkedPaths.FindChecked(GraphToDetach));
	}

	if (bIsPostLoad)
	{
		//PatchTemplateNodesWithPreferredPermutation();
	}

	TArray<URigVMGraph*> GraphsToClean = RigVMClient.GetAllModels(true, true);

	// Sort from leaf graphs to root
	TArray<URigVMGraph*> SortedGraphsToClean;
	SortedGraphsToClean.Reserve(GraphsToClean.Num());
	while (SortedGraphsToClean.Num() < GraphsToClean.Num())
	{
		bool bGraphAdded = false;
		for (URigVMGraph* Graph : GraphsToClean)
		{
			if (SortedGraphsToClean.Contains(Graph))
			{
				continue;
			}

			TArray<URigVMGraph*> ContainedGraphs;
			for (URigVMNode* Node : Graph->GetNodes())
			{
				if (URigVMLibraryNode* LibraryNode = Cast<URigVMLibraryNode>(Node))
				{
					if (URigVMFunctionReferenceNode* FunctionReferenceNode = Cast<URigVMFunctionReferenceNode>(LibraryNode))
					{
						if (FunctionReferenceNode->GetReferencedFunctionHeader().LibraryPointer.LibraryNode.GetLongPackageName() != GetPackage()->GetPathName())
						{
							continue;
						}
						if (URigVMLibraryNode* ReferencedNode = FunctionReferenceNode->LoadReferencedNode())
						{
							ContainedGraphs.Add(ReferencedNode->GetContainedGraph());
							continue;
						}
					}

					if (URigVMGraph* ContainedGraph = LibraryNode->GetContainedGraph())
					{
						ContainedGraphs.Add(ContainedGraph);
					}
				}
			}

			bool bAllContained = true;
			for (URigVMGraph* Contained : ContainedGraphs)
			{
				if (!SortedGraphsToClean.Contains(Contained))
				{
					bAllContained = false;
					break;
				}
			}
			if (bAllContained)
			{
				SortedGraphsToClean.Add(Graph);
				bGraphAdded = true;
			}
		}
		ensure(bGraphAdded);
	}

	for (int32 GraphIndex = 0; GraphIndex < SortedGraphsToClean.Num(); GraphIndex++)
	{
		URigVMGraph* GraphToClean = SortedGraphsToClean[GraphIndex];
		URigVMController* Controller = RigVMClient.GetOrCreateController(GraphToClean);
		//TGuardValue<bool> GuardEditGraph(GraphToClean->bEditable, true);
		FRigVMControllerNotifGuard NotifGuard(Controller, true);

		for (URigVMNode* ModelNode : GraphToClean->GetNodes())
		{
			Controller->RemoveUnusedOrphanedPins(ModelNode);
		}
	}
}

void UAnimNextRigVMAssetEditorData::OnRigVMRegistryChanged()
{
	RefreshAllModels(ERigVMLoadType::PostLoad);
	//RebuildGraphFromModel(); // TODO zzz : How we do this on AnimNext ?
}

void UAnimNextRigVMAssetEditorData::RequestRigVMInit()
{
	// TODO zzz : How we do this on AnimNext ?
}

URigVMGraph* UAnimNextRigVMAssetEditorData::GetModel(const UEdGraph* InEdGraph) const
{
	return RigVMClient.GetModel(InEdGraph);
}

URigVMGraph* UAnimNextRigVMAssetEditorData::GetModel(const FString& InNodePath) const
{
	return RigVMClient.GetModel(InNodePath);
}

URigVMGraph* UAnimNextRigVMAssetEditorData::GetDefaultModel() const 
{
	return RigVMClient.GetDefaultModel();
}

TArray<URigVMGraph*> UAnimNextRigVMAssetEditorData::GetAllModels() const
{
	return RigVMClient.GetAllModels(true, true);
}

URigVMFunctionLibrary* UAnimNextRigVMAssetEditorData::GetLocalFunctionLibrary() const
{
	return RigVMClient.GetFunctionLibrary();
}

URigVMGraph* UAnimNextRigVMAssetEditorData::AddModel(FString InName, bool bSetupUndoRedo, bool bPrintPythonCommand)
{
	TGuardValue<bool> EnablePythonPrint(bSuspendPythonMessagesForRigVMClient, !bPrintPythonCommand);
	return RigVMClient.AddModel(InName, bSetupUndoRedo, bPrintPythonCommand);
}

bool UAnimNextRigVMAssetEditorData::RemoveModel(FString InName, bool bSetupUndoRedo, bool bPrintPythonCommand)
{
	TGuardValue<bool> EnablePythonPrint(bSuspendPythonMessagesForRigVMClient, !bPrintPythonCommand);
	return RigVMClient.RemoveModel(InName, bSetupUndoRedo, bPrintPythonCommand);
}

FRigVMGetFocusedGraph& UAnimNextRigVMAssetEditorData::OnGetFocusedGraph()
{
	return RigVMClient.OnGetFocusedGraph();
}

const FRigVMGetFocusedGraph& UAnimNextRigVMAssetEditorData::OnGetFocusedGraph() const
{
	return RigVMClient.OnGetFocusedGraph();
}

URigVMGraph* UAnimNextRigVMAssetEditorData::GetFocusedModel() const
{
	return RigVMClient.GetFocusedModel();
}

URigVMController* UAnimNextRigVMAssetEditorData::GetController(const URigVMGraph* InGraph) const
{
	return RigVMClient.GetController(InGraph);
};

URigVMController* UAnimNextRigVMAssetEditorData::GetControllerByName(const FString InGraphName) const
{
	return RigVMClient.GetControllerByName(InGraphName);
};

URigVMController* UAnimNextRigVMAssetEditorData::GetOrCreateController(URigVMGraph* InGraph)
{
	return RigVMClient.GetOrCreateController(InGraph);
};

URigVMController* UAnimNextRigVMAssetEditorData::GetController(const UEdGraph* InEdGraph) const
{
	return RigVMClient.GetController(InEdGraph);
};

URigVMController* UAnimNextRigVMAssetEditorData::GetOrCreateController(const UEdGraph* InEdGraph)
{
	return RigVMClient.GetOrCreateController(InEdGraph);
};

TArray<FString> UAnimNextRigVMAssetEditorData::GeneratePythonCommands(const FString InNewBlueprintName)
{
	return TArray<FString>();
}

FRigVMClient* UAnimNextRigVMAssetEditorData::GetRigVMClient()
{
	return &RigVMClient;
}

const FRigVMClient* UAnimNextRigVMAssetEditorData::GetRigVMClient() const
{
	return &RigVMClient;
}

IRigVMGraphFunctionHost* UAnimNextRigVMAssetEditorData::GetRigVMGraphFunctionHost()
{
	return this;
}

const IRigVMGraphFunctionHost* UAnimNextRigVMAssetEditorData::GetRigVMGraphFunctionHost() const
{
	return this;
}

void UAnimNextRigVMAssetEditorData::HandleRigVMGraphAdded(const FRigVMClient* InClient, const FString& InNodePath)
{
	if(URigVMGraph* RigVMGraph = InClient->GetModel(InNodePath))
	{
		RigVMGraph->SetExecuteContextStruct(GetExecuteContextStruct());

		if(!HasAnyFlags(RF_ClassDefaultObject | RF_NeedInitialization | RF_NeedLoad | RF_NeedPostLoad) &&
			GetOuter() != GetTransientPackage())
		{
			CreateEdGraph(RigVMGraph, true);
			RequestAutoVMRecompilation();
		}
		
#if WITH_EDITOR
		if(!bSuspendPythonMessagesForRigVMClient)
		{
			const FString AssetName = InClient->GetSchema()->GetSanitizedName(GetName(), true, false);
			RigVMPythonUtils::Print(AssetName, FString::Printf(TEXT("asset.add_graph('%s')"), *RigVMGraph->GetName()));
		}
#endif
	}
}

void UAnimNextRigVMAssetEditorData::HandleRigVMGraphRemoved(const FRigVMClient* InClient, const FString& InNodePath)
{
	if(URigVMGraph* RigVMGraph = InClient->GetModel(InNodePath))
	{
		RemoveEdGraph(RigVMGraph);
		RecompileVM();

#if WITH_EDITOR
		if(!bSuspendPythonMessagesForRigVMClient)
		{
			const FString AssetName = InClient->GetSchema()->GetSanitizedName(GetName(), true, false);
			RigVMPythonUtils::Print(AssetName, FString::Printf(TEXT("asset.add_graph('%s')"), *RigVMGraph->GetName()));
		}
#endif
	}
}

void UAnimNextRigVMAssetEditorData::HandleConfigureRigVMController(const FRigVMClient* InClient, URigVMController* InControllerToConfigure)
{
	InControllerToConfigure->OnModified().AddUObject(this, &UAnimNextRigVMAssetEditorData::HandleModifiedEvent);

	TWeakObjectPtr<UAnimNextRigVMAssetEditorData> WeakThis(this);

	// this delegate is used by the controller to retrieve the current bytecode of the VM
	InControllerToConfigure->GetCurrentByteCodeDelegate.BindLambda([WeakThis]() -> const FRigVMByteCode*
	{
		if (WeakThis.IsValid())
		{
			if(UAnimNextRigVMAsset* Asset = WeakThis->GetTypedOuter<UAnimNextRigVMAsset>())
			{
				if (Asset->VM)
				{
					return &Asset->VM->GetByteCode();
				}
			}
		}
		return nullptr;

	});

#if WITH_EDITOR
	InControllerToConfigure->SetupDefaultUnitNodeDelegates(TDelegate<FName(FRigVMExternalVariable, FString)>::CreateLambda(
		[](FRigVMExternalVariable InVariableToCreate, FString InDefaultValue) -> FName
		{
			return NAME_None;
		}
	));
#endif
}

UObject* UAnimNextRigVMAssetEditorData::GetEditorObjectForRigVMGraph(URigVMGraph* InVMGraph) const
{
	if(InVMGraph)
	{
		for(UAnimNextRigVMAssetEntry* Entry : Entries)
		{
			if(IAnimNextRigVMGraphInterface* GraphInterface = Cast<IAnimNextRigVMGraphInterface>(Entry))
			{
				URigVMEdGraph* EdGraph = GraphInterface->GetEdGraph();
				if(EdGraph->ModelNodePath == InVMGraph->GetNodePath())
				{
					return EdGraph;
				}
			}
		}
	}
	return nullptr;
}

URigVMGraph* UAnimNextRigVMAssetEditorData::GetRigVMGraphForEditorObject(UObject* InObject) const
{
	if(const URigVMEdGraph* Graph = Cast<URigVMEdGraph>(InObject))
	{
		if (Graph->bIsFunctionDefinition)
		{
			if (URigVMLibraryNode* LibraryNode = RigVMClient.GetFunctionLibrary()->FindFunction(*Graph->ModelNodePath))
			{
				return LibraryNode->GetContainedGraph();
			}
		}
		else
		{
			return RigVMClient.GetModel(Graph->ModelNodePath);
		}
	}

	return nullptr;
}

FRigVMGraphFunctionStore* UAnimNextRigVMAssetEditorData::GetRigVMGraphFunctionStore()
{
	return &GraphFunctionStore;
}

const FRigVMGraphFunctionStore* UAnimNextRigVMAssetEditorData::GetRigVMGraphFunctionStore() const
{
	return &GraphFunctionStore;
}

void UAnimNextRigVMAssetEditorData::RecompileVMIfRequired()
{
	if (bVMRecompilationRequired)
	{
		RecompileVM();
	}
}

void UAnimNextRigVMAssetEditorData::RequestAutoVMRecompilation()
{
	bVMRecompilationRequired = true;
	if (bAutoRecompileVM && VMRecompilationBracket == 0)
	{
		RecompileVMIfRequired();
	}
}

void UAnimNextRigVMAssetEditorData::SetAutoVMRecompile(bool bAutoRecompile)
{
	bAutoRecompileVM = bAutoRecompile;
}

bool UAnimNextRigVMAssetEditorData::GetAutoVMRecompile() const
{
	return bAutoRecompileVM;
}

void UAnimNextRigVMAssetEditorData::IncrementVMRecompileBracket()
{
	VMRecompilationBracket++;
}

void UAnimNextRigVMAssetEditorData::DecrementVMRecompileBracket()
{
	if (VMRecompilationBracket == 1)
	{
		if (bAutoRecompileVM)
		{
			RecompileVMIfRequired();
		}
		VMRecompilationBracket = 0;
	}
	else if (VMRecompilationBracket > 0)
	{
		VMRecompilationBracket--;
	}
}

void UAnimNextRigVMAssetEditorData::HandleModifiedEvent(ERigVMGraphNotifType InNotifType, URigVMGraph* InGraph, UObject* InSubject)
{
	bool bNotifForOthersPending = true;

	switch(InNotifType)
	{
	case ERigVMGraphNotifType::InteractionBracketOpened:
		{
			IncrementVMRecompileBracket();
			break;
		}
	case ERigVMGraphNotifType::InteractionBracketClosed:
	case ERigVMGraphNotifType::InteractionBracketCanceled:
		{
			DecrementVMRecompileBracket();
			break;
		}
	case ERigVMGraphNotifType::NodeAdded:
		{
			if (URigVMCollapseNode* CollapseNode = Cast<URigVMCollapseNode>(InSubject))
			{
				CreateEdGraphForCollapseNode(CollapseNode);
				break;
			}
		}
		// Fall through to the next case
	case ERigVMGraphNotifType::LinkAdded:
	case ERigVMGraphNotifType::LinkRemoved:
	case ERigVMGraphNotifType::PinArraySizeChanged:
	case ERigVMGraphNotifType::PinDirectionChanged:
		{
			RequestAutoVMRecompilation();
			break;
		}

	case ERigVMGraphNotifType::PinDefaultValueChanged:
		{
			if (InGraph->GetRuntimeAST().IsValid())
			{
				URigVMPin* RootPin = CastChecked<URigVMPin>(InSubject)->GetRootPin();
				FRigVMASTProxy RootPinProxy = FRigVMASTProxy::MakeFromUObject(RootPin);
				const FRigVMExprAST* Expression = InGraph->GetRuntimeAST()->GetExprForSubject(RootPinProxy);
				if (Expression == nullptr)
				{
					InGraph->ClearAST();
				}
				else if (Expression->NumParents() > 1)
				{
					InGraph->ClearAST();
				}
			}

			RequestAutoVMRecompilation();	// We need to rebuild our metadata when a default value changes
			break;
		}
	}
	
	// if the notification still has to be sent...
	if (bNotifForOthersPending && !bSuspendModelNotificationsForOthers)
	{
		if (RigVMGraphModifiedEvent.IsBound())
		{
			RigVMGraphModifiedEvent.Broadcast(InNotifType, InGraph, InSubject);
		}
	}
}

TArray<UEdGraph*> UAnimNextRigVMAssetEditorData::GetAllEdGraphs() const
{
	TArray<UEdGraph*> Graphs;
	for(UAnimNextRigVMAssetEntry* Entry : Entries)
	{
		if(IAnimNextRigVMGraphInterface* GraphInterface = Cast<IAnimNextRigVMGraphInterface>(Entry))
		{
			Graphs.Add(GraphInterface->GetEdGraph());
		}
	}
	return Graphs;
}

UAnimNextRigVMAssetEntry* UAnimNextRigVMAssetLibrary::FindEntry(UAnimNextRigVMAsset* InAsset, FName InName)
{
	return UE::AnimNext::UncookedOnly::FUtils::GetEditorData(InAsset)->FindEntry(InName);
}

UAnimNextRigVMAssetEntry* UAnimNextRigVMAssetEditorData::FindEntry(FName InName) const
{
	if(InName == NAME_None)
	{
		ReportError(TEXT("UAnimNextRigVMAssetEditorData::FindEntry: Invalid name supplied."));
		return nullptr;
	}

	const TObjectPtr<UAnimNextRigVMAssetEntry>* FoundEntry = Entries.FindByPredicate([InName](const UAnimNextRigVMAssetEntry* InEntry)
	{
		return InEntry->GetEntryName() == InName;
	});

	return FoundEntry != nullptr ? *FoundEntry : nullptr;
}

bool UAnimNextRigVMAssetLibrary::RemoveEntry(UAnimNextRigVMAsset* InAsset, UAnimNextRigVMAssetEntry* InEntry,  bool bSetupUndoRedo, bool bPrintPythonCommand)
{
	return UE::AnimNext::UncookedOnly::FUtils::GetEditorData(InAsset)->RemoveEntry(InEntry, bSetupUndoRedo, bPrintPythonCommand);
}

bool UAnimNextRigVMAssetEditorData::RemoveEntry(UAnimNextRigVMAssetEntry* InEntry, bool bSetupUndoRedo, bool bPrintPythonCommand)
{
	if(InEntry == nullptr)
	{
		ReportError(TEXT("UAnimNextRigVMAssetEditorData::RemoveEntry: Invalid entry supplied."));
		return false;
	}
	
	TObjectPtr<UAnimNextRigVMAssetEntry>* EntryToRemovePtr = Entries.FindByKey(InEntry);
	if(EntryToRemovePtr == nullptr)
	{
		ReportError(TEXT("UAnimNextRigVMAssetEditorData::RemoveEntry: Asset does not contain the supplied entry."));
		return false;
	}

	if(bSetupUndoRedo)
	{
		Modify();
	}

	// Remove from internal array
	UAnimNextRigVMAssetEntry* EntryToRemove = *EntryToRemovePtr;
	Entries.Remove(EntryToRemove);

	if (bSetupUndoRedo)
	{
		EntryToRemove->Modify();
	}

	bool bResult = true;
	if(const IAnimNextRigVMGraphInterface* GraphInterface = Cast<IAnimNextRigVMGraphInterface>(EntryToRemove))
	{
		// Remove any graphs
		if(URigVMGraph* RigVMGraph = GraphInterface->GetRigVMGraph())
		{
			TGuardValue<bool> EnablePythonPrint(bSuspendPythonMessagesForRigVMClient, !bPrintPythonCommand);
			TGuardValue<bool> DisableAutoCompile(bAutoRecompileVM, false);
			bResult = RigVMClient.RemoveModel(RigVMGraph->GetNodePath(), bSetupUndoRedo);
		}
	}

	// This will cause any external package to be removed when saved
	EntryToRemove->MarkAsGarbage();

	BroadcastModified();

	return bResult;
}

bool UAnimNextRigVMAssetLibrary::RemoveEntries(UAnimNextRigVMAsset* InAsset, const TArray<UAnimNextRigVMAssetEntry*>& InEntries,  bool bSetupUndoRedo, bool bPrintPythonCommand)
{
	return UE::AnimNext::UncookedOnly::FUtils::GetEditorData(InAsset)->RemoveEntries(InEntries, bSetupUndoRedo, bPrintPythonCommand);
}

bool UAnimNextRigVMAssetEditorData::RemoveEntries(const TArray<UAnimNextRigVMAssetEntry*>& InEntries, bool bSetupUndoRedo, bool bPrintPythonCommand)
{
	bool bResult = false;
	{
		TGuardValue<bool> DisableEditorDataNotifications(bSuspendEditorDataNotifications, true);
		TGuardValue<bool> DisableAutoCompile(bAutoRecompileVM, false);
		for(UAnimNextRigVMAssetEntry* Entry : InEntries)
		{
			bResult |= RemoveEntry(Entry, bSetupUndoRedo, bPrintPythonCommand);
		}
	}

	BroadcastModified();

	return bResult;
}

UObject* UAnimNextRigVMAssetEditorData::CreateNewSubEntry(UAnimNextRigVMAssetEditorData* InEditorData, TSubclassOf<UObject> InClass)
{
	UObject* NewEntry = NewObject<UObject>(InEditorData, InClass.Get(), NAME_None, RF_Transactional);
	// If we are a transient asset, dont use external packages
	UAnimNextRigVMAsset* Asset = UE::AnimNext::UncookedOnly::FUtils::GetAsset(InEditorData);
	check(Asset);
	if(!Asset->HasAnyFlags(RF_Transient))
	{
		FExternalPackageHelper::SetPackagingMode(NewEntry, InEditorData, true, false, PKG_None);
	}
	return NewEntry;
}

UAnimNextRigVMAssetEntry* UAnimNextRigVMAssetEditorData::FindEntryForRigVMGraph(URigVMGraph* InRigVMGraph) const
{
	for(UAnimNextRigVMAssetEntry* Entry : Entries)
	{
		if(IAnimNextRigVMGraphInterface* GraphInterface = Cast<IAnimNextRigVMGraphInterface>(Entry))
		{
			if(GraphInterface->GetRigVMGraph() == InRigVMGraph)
			{
				return Entry;
			}
		}
	}

	return nullptr;
}
