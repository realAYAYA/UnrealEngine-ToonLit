// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/AnimNextGraph_EditorData.h"

#include "ControlRigDefines.h"
#include "Graph/AnimNextGraph.h"
#include "Graph/AnimNextGraph_EdGraphSchema.h"
#include "UncookedOnlyUtils.h"
#include "Graph/GraphExecuteContext.h"
#include "Rigs/RigHierarchyPose.h"
#include "RigVMModel/RigVMFunctionLibrary.h"
#include "RigVMModel/RigVMNotifications.h"
#include "RigVMModel/Nodes/RigVMCollapseNode.h"
#include "Curves/CurveFloat.h"
#include "UObject/ObjectSaveContext.h"
#include "Kismet2/BlueprintEditorUtils.h"

UAnimNextGraph_EditorData::UAnimNextGraph_EditorData(const FObjectInitializer& ObjectInitializer)
{
	RigVMClient.Reset();
	RigVMClient.SetSchemaClass(UAnimNextGraph_Schema::StaticClass());
	RigVMClient.SetOuterClientHost(this, GET_MEMBER_NAME_CHECKED(UAnimNextGraph_EditorData, RigVMClient));
	{
		TGuardValue<bool> DisableClientNotifs(RigVMClient.bSuspendNotifications, true);
		RigVMClient.AddModel(TEXT("RigVMGraph"), false, &ObjectInitializer);
		RigVMClient.GetOrCreateFunctionLibrary(false, &ObjectInitializer);
	}
	RigVMClient.SetExecuteContextStruct(FAnimNextGraphExecuteContext::StaticStruct());
	
	auto MakeEdGraph = [this, &ObjectInitializer](FName InName) -> UAnimNextGraph_EdGraph*
	{
		UAnimNextGraph_EdGraph* EdGraph = ObjectInitializer.CreateDefaultSubobject<UAnimNextGraph_EdGraph>(this, InName);
		EdGraph->Schema = UAnimNextGraph_EdGraphSchema::StaticClass();
		EdGraph->bAllowRenaming = false;
		EdGraph->bEditable = true;
		EdGraph->bAllowDeletion = false;
		EdGraph->bIsFunctionDefinition = false;
		EdGraph->Initialize(this);

		return EdGraph;
	};

	RootGraph = MakeEdGraph(TEXT("RootEdGraph"));
	FunctionLibraryEdGraph = MakeEdGraph(TEXT("RigVMFunctionLibraryEdGraph"));
}

void UAnimNextGraph_EditorData::Serialize(FArchive& Ar)
{
	RigVMClient.SetOuterClientHost(this, GET_MEMBER_NAME_CHECKED(UAnimNextGraph_EditorData, RigVMClient));

	UObject::Serialize(Ar);
}

void UAnimNextGraph_EditorData::Initialize(bool bRecompileVM)
{
	UAnimNextGraph* AnimNextGraph = GetTypedOuter<UAnimNextGraph>();

	if (RigVMClient.GetController(0) == nullptr)
	{
		check(RigVMClient.Num() == 1);
		check(RigVMClient.GetFunctionLibrary());
		
		RigVMClient.GetOrCreateController(RigVMClient.GetDefaultModel());
		RigVMClient.GetOrCreateController(RigVMClient.GetFunctionLibrary());

		// Init function library controllers
		for(URigVMLibraryNode* LibraryNode : RigVMClient.GetFunctionLibrary()->GetFunctions())
		{
			RigVMClient.GetOrCreateController(LibraryNode->GetContainedGraph());
		}
		
		if(bRecompileVM)
		{
			UE::AnimNext::UncookedOnly::FUtils::Compile(AnimNextGraph);
		}
	}

	RootGraph->Initialize(this);
	FunctionLibraryEdGraph->Initialize(this);
	if(EntryPointGraph)
	{
		EntryPointGraph->Initialize(this);
	}
}

void UAnimNextGraph_EditorData::PostLoad()
{
	Super::PostLoad();
	
	Initialize(/*bRecompileVM*/false);
	RefreshAllModels(EAnimNextGraphLoadType::PostLoad);

#if WITH_EDITOR
	if (GIsEditor)
	{
		// delay compilation until the package has been loaded
		FCoreUObjectDelegates::OnEndLoadPackage.AddUObject(this, &UAnimNextGraph_EditorData::HandlePackageDone);
	}
#else // !WITH_EDITOR
	RecompileVMIfRequired();
#endif // WITH_EDITOR
}

#if WITH_EDITOR
void UAnimNextGraph_EditorData::HandlePackageDone(const FEndLoadPackageContext& Context)
{
	if (!Context.LoadedPackages.Contains(GetPackage()))
	{
		return;
	}
	HandlePackageDone();
}

void UAnimNextGraph_EditorData::HandlePackageDone()
{
	FCoreUObjectDelegates::OnEndLoadPackage.RemoveAll(this);

	RecompileVM();
}

void UAnimNextGraph_EditorData::RefreshAllModels(EAnimNextGraphLoadType InLoadType)
{
	const bool bIsPostLoad = InLoadType == EAnimNextGraphLoadType::PostLoad;

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

#endif // WITH_EDITOR

FRigVMClient* UAnimNextGraph_EditorData::GetRigVMClient()
{
	return &RigVMClient;
}

const FRigVMClient* UAnimNextGraph_EditorData::GetRigVMClient() const
{
	return &RigVMClient;
}

IRigVMGraphFunctionHost* UAnimNextGraph_EditorData::GetRigVMGraphFunctionHost()
{
	return this;
}

const IRigVMGraphFunctionHost* UAnimNextGraph_EditorData::GetRigVMGraphFunctionHost() const
{
	return this;
}


void UAnimNextGraph_EditorData::HandleRigVMGraphAdded(const FRigVMClient* InClient, const FString& InNodePath)
{
	if(URigVMGraph* RigVMGraph = InClient->GetModel(InNodePath))
	{
		RigVMGraph->SetExecuteContextStruct(FAnimNextGraphExecuteContext::StaticStruct());
	}
}

void UAnimNextGraph_EditorData::HandleConfigureRigVMController(const FRigVMClient* InClient,
                                                                    URigVMController* InControllerToConfigure)
{
	InControllerToConfigure->OnModified().AddUObject(this, &UAnimNextGraph_EditorData::HandleModifiedEvent);

	TWeakObjectPtr<UAnimNextGraph_EditorData> WeakThis(this);

	// this delegate is used by the controller to determine variable validity
	// during a bind process. the controller itself doesn't own the variables,
	// so we need a delegate to request them from the owning blueprint
	InControllerToConfigure->GetExternalVariablesDelegate.BindLambda([](URigVMGraph* InGraph) -> TArray<FRigVMExternalVariable>
	{
		if (InGraph)
		{
			if(UAnimNextGraph_EditorData* EditorData = InGraph->GetTypedOuter<UAnimNextGraph_EditorData>())
			{
				if (UAnimNextGraph* Graph = EditorData->GetTypedOuter<UAnimNextGraph>())
				{
					return Graph->GetRigVMExternalVariables();
				}
			}
		}
		return TArray<FRigVMExternalVariable>();
	});

	// this delegate is used by the controller to retrieve the current bytecode of the VM
	InControllerToConfigure->GetCurrentByteCodeDelegate.BindLambda([WeakThis]() -> const FRigVMByteCode*
	{
		if (WeakThis.IsValid())
		{
			if(UAnimNextGraph* Graph = WeakThis->GetTypedOuter<UAnimNextGraph>())
			{
				if (Graph->RigVM)
				{
					return &Graph->RigVM->GetByteCode();
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

UObject* UAnimNextGraph_EditorData::GetEditorObjectForRigVMGraph(URigVMGraph* InVMGraph) const
{
	if(InVMGraph)
	{
		TArray<UAnimNextGraph_EdGraph*> AllGraphs = {RootGraph, EntryPointGraph, FunctionLibraryEdGraph};
		for(UAnimNextGraph_EdGraph* EdGraph : AllGraphs)
		{
			if(EdGraph)
			{
				if(EdGraph->ModelNodePath == InVMGraph->GetNodePath())
				{
					return EdGraph;
				}
			}
		}
	}
	return nullptr;
}

FRigVMGraphFunctionStore* UAnimNextGraph_EditorData::GetRigVMGraphFunctionStore()
{
	return &GraphFunctionStore;
}

const FRigVMGraphFunctionStore* UAnimNextGraph_EditorData::GetRigVMGraphFunctionStore() const
{
	return &GraphFunctionStore;
}


void UAnimNextGraph_EditorData::RecompileVM()
{
	UE::AnimNext::UncookedOnly::FUtils::Compile(GetTypedOuter<UAnimNextGraph>());
}

void UAnimNextGraph_EditorData::RecompileVMIfRequired()
{
	if (bVMRecompilationRequired)
	{
		RecompileVM();
	}
}

void UAnimNextGraph_EditorData::RequestAutoVMRecompilation()
{
	bVMRecompilationRequired = true;
	if (bAutoRecompileVM)
	{
		RecompileVMIfRequired();
	}
}

void UAnimNextGraph_EditorData::HandleModifiedEvent(ERigVMGraphNotifType InNotifType, URigVMGraph* InGraph, UObject* InSubject)
{
	bool bNotifForOthersPending = true;

	switch(InNotifType)
	{
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
			RecompileVM();
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
					break;
				}
				else if (Expression->NumParents() > 1)
				{
					InGraph->ClearAST();
					break;
				}
			}
			break;
		}
	}
	
	// if the notification still has to be sent...
	if (bNotifForOthersPending && !bSuspendModelNotificationsForOthers)
	{
		if (ModifiedEvent.IsBound())
		{
			ModifiedEvent.Broadcast(InNotifType, InGraph, InSubject);
		}
	}
}

URigVMGraph* UAnimNextGraph_EditorData::GetVMGraphForEdGraph(const UEdGraph* InGraph) const
{
	if (InGraph == RootGraph)
	{
		return RigVMClient.GetDefaultModel();
	}
	else
	{
		const UAnimNextGraph_EdGraph* Graph = Cast<UAnimNextGraph_EdGraph>(InGraph);
		check(Graph);

		if (Graph->bIsFunctionDefinition)
		{
			if (URigVMLibraryNode* LibraryNode = RigVMClient.GetFunctionLibrary()->FindFunction(*Graph->ModelNodePath))
			{
				return LibraryNode->GetContainedGraph();
			}
		}
	}

	return nullptr;
}

void UAnimNextGraph_EditorData::CreateEdGraphForCollapseNode(URigVMCollapseNode* InNode)
{
	if (InNode->GetGraph()->IsA<URigVMFunctionLibrary>())
	{
		if (URigVMGraph* ContainedGraph = InNode->GetContainedGraph())
		{
			if (EntryPointGraph == nullptr)
			{
				// create a sub graph
				UAnimNextGraph_EdGraph* RigFunctionGraph = NewObject<UAnimNextGraph_EdGraph>(this, *InNode->GetName(), RF_Transactional);
				RigFunctionGraph->Schema = UAnimNextGraph_EdGraphSchema::StaticClass();
				RigFunctionGraph->bAllowRenaming = true;
				RigFunctionGraph->bEditable = true;
				RigFunctionGraph->bAllowDeletion = true;
				RigFunctionGraph->ModelNodePath = ContainedGraph->GetNodePath();
				RigFunctionGraph->bIsFunctionDefinition = true;

				EntryPointGraph = RigFunctionGraph;
				RigFunctionGraph->Initialize(this);

				RigVMClient.GetOrCreateController(ContainedGraph)->ResendAllNotifications();
			}
		}
	}
}

bool UAnimNextGraph_EditorData::IsNodeExecConnected(const UEdGraphNode* Node) const
{
	static const TCHAR* ExecuteContextStr = TEXT(".ExecuteContext");

	for (const UEdGraphPin* Pin : Node->Pins)
	{
		const FString PinName = Pin->PinName.ToString();

		if (PinName.Contains(ExecuteContextStr) && Pin->LinkedTo.Num() > 0)
		{
			return true;
		}
	}

	return false;
}
