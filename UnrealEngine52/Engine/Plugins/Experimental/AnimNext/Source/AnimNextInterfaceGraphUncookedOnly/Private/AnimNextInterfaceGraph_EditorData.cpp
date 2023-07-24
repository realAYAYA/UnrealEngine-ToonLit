// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextInterfaceGraph_EditorData.h"

#include "ControlRigDefines.h"
#include "AnimNextInterfaceGraph.h"
#include "AnimNextInterfaceGraph_EdGraphSchema.h"
#include "AnimNextInterfaceUncookedOnlyUtils.h"
#include "AnimNextInterfaceExecuteContext.h"
#include "Rigs/RigHierarchyPose.h"
#include "RigVMModel/RigVMFunctionLibrary.h"
#include "RigVMModel/RigVMNotifications.h"
#include "RigVMModel/Nodes/RigVMCollapseNode.h"
#include "Curves/CurveFloat.h"
#include "UObject/ObjectSaveContext.h"
#include "Kismet2/BlueprintEditorUtils.h"

UAnimNextInterfaceGraph_EditorData::UAnimNextInterfaceGraph_EditorData(const FObjectInitializer& ObjectInitializer)
{
	RigVMClient.Reset();
	RigVMClient.SetOuterClientHost(this, GET_MEMBER_NAME_CHECKED(UAnimNextInterfaceGraph_EditorData, RigVMClient));
	{
		TGuardValue<bool> DisableClientNotifs(RigVMClient.bSuspendNotifications, true);
		RigVMClient.AddModel(TEXT("RigVMGraph"), false, &ObjectInitializer);
		RigVMClient.GetOrCreateFunctionLibrary(false, &ObjectInitializer);
	}
	RigVMClient.SetExecuteContextStruct(FAnimNextInterfaceExecuteContext::StaticStruct());
	
	auto MakeEdGraph = [this, &ObjectInitializer](FName InName) -> UAnimNextInterfaceGraph_EdGraph*
	{
		UAnimNextInterfaceGraph_EdGraph* EdGraph = ObjectInitializer.CreateDefaultSubobject<UAnimNextInterfaceGraph_EdGraph>(this, InName);
		EdGraph->Schema = UAnimNextInterfaceGraph_EdGraphSchema::StaticClass();
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

void UAnimNextInterfaceGraph_EditorData::Serialize(FArchive& Ar)
{
	RigVMClient.SetOuterClientHost(this, GET_MEMBER_NAME_CHECKED(UAnimNextInterfaceGraph_EditorData, RigVMClient));

	UObject::Serialize(Ar);

	if(Ar.IsLoading())
	{
		if(RigVMGraph_DEPRECATED || RigVMFunctionLibrary_DEPRECATED)
		{
			TGuardValue<bool> DisableClientNotifs(RigVMClient.bSuspendNotifications, true);
			RigVMClient.SetFromDeprecatedData(RigVMGraph_DEPRECATED, RigVMFunctionLibrary_DEPRECATED);
		}
	}
}

void UAnimNextInterfaceGraph_EditorData::Initialize(bool bRecompileVM)
{
	UAnimNextInterfaceGraph* AnimNextInterfaceGraph = GetTypedOuter<UAnimNextInterfaceGraph>();

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
			UE::AnimNext::InterfaceGraphUncookedOnly::FUtils::Compile(AnimNextInterfaceGraph);
		}
	}

	RootGraph->Initialize(this);
	FunctionLibraryEdGraph->Initialize(this);
	if(EntryPointGraph)
	{
		EntryPointGraph->Initialize(this);
	}
}

void UAnimNextInterfaceGraph_EditorData::PostLoad()
{
	Super::PostLoad();
	
	Initialize(/*bRecompileVM*/false);
	RefreshAllModels(EAnimNextInterfaceGraphLoadType::PostLoad);

#if WITH_EDITOR
	if (GIsEditor)
	{
		// delay compilation until the package has been loaded
		FCoreUObjectDelegates::OnEndLoadPackage.AddUObject(this, &UAnimNextInterfaceGraph_EditorData::HandlePackageDone);
	}
#else // !WITH_EDITOR
	RecompileVMIfRequired();
#endif // WITH_EDITOR
}

#if WITH_EDITOR
void UAnimNextInterfaceGraph_EditorData::HandlePackageDone(const FEndLoadPackageContext& Context)
{
	if (!Context.LoadedPackages.Contains(GetPackage()))
	{
		return;
	}
	HandlePackageDone();
}

void UAnimNextInterfaceGraph_EditorData::HandlePackageDone()
{
	FCoreUObjectDelegates::OnEndLoadPackage.RemoveAll(this);

	RecompileVM();
}

void UAnimNextInterfaceGraph_EditorData::RefreshAllModels(EAnimNextInterfaceGraphLoadType InLoadType)
{
	const bool bIsPostLoad = InLoadType == EAnimNextInterfaceGraphLoadType::PostLoad;

	TGuardValue<bool> IsCompilingGuard(bIsCompiling, true);
	TGuardValue<bool> ClientIgnoreModificationsGuard(RigVMClient.bIgnoreModelNotifications, true);

	TArray<URigVMGraph*> GraphsToDetach = RigVMClient.GetAllModels(true, false);

	if (ensure(IsInGameThread()))
	{
		for (URigVMGraph* GraphToDetach : GraphsToDetach)
		{
			URigVMController* Controller = RigVMClient.GetOrCreateController(GraphToDetach);
			// temporarily disable default value validation during load time, serialized values should always be accepted
			TGuardValue<bool> PerGraphDisablePinDefaultValueValidation(Controller->bValidatePinDefaults, false);
			FRigVMControllerNotifGuard NotifGuard(Controller, true);
			Controller->DetachLinksFromPinObjects();
			TArray<URigVMNode*> Nodes = GraphToDetach->GetNodes();
			for (URigVMNode* Node : Nodes)
			{
				Controller->RepopulatePinsOnNode(Node, true, true);
			}
		}
		//SetupPinRedirectorsForBackwardsCompatibility();
	}

	for (URigVMGraph* GraphToDetach : GraphsToDetach)
	{
		URigVMController* Controller = RigVMClient.GetOrCreateController(GraphToDetach);
		// at this stage, allow all links to be reattached,
		// RecomputeAllTemplateFilteredPermutations() later should break any invalid links
		FRigVMControllerNotifGuard NotifGuard(Controller, true);
		Controller->ReattachLinksToPinObjects(true /* follow redirectors */, nullptr, true, true);
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
		TGuardValue<bool> RecomputeGuard(Controller->bSuspendRecomputingOuterTemplateFilters, true);
		//TGuardValue<bool> GuardEditGraph(GraphToClean->bEditable, true);
		FRigVMControllerNotifGuard NotifGuard(Controller, true);

		for (URigVMNode* ModelNode : GraphToClean->GetNodes())
		{
			Controller->RemoveUnusedOrphanedPins(ModelNode);
		}

		if (bIsPostLoad)
		{
			if (URigVMLibraryNode* LibraryNode = GraphToClean->GetTypedOuter<URigVMLibraryNode>())
			{
				Controller->UpdateLibraryTemplate(LibraryNode, false);
			}
		}
	}
}

#endif // WITH_EDITOR

FRigVMClient* UAnimNextInterfaceGraph_EditorData::GetRigVMClient()
{
	return &RigVMClient;
}

const FRigVMClient* UAnimNextInterfaceGraph_EditorData::GetRigVMClient() const
{
	return &RigVMClient;
}

IRigVMGraphFunctionHost* UAnimNextInterfaceGraph_EditorData::GetRigVMGraphFunctionHost()
{
	return this;
}

const IRigVMGraphFunctionHost* UAnimNextInterfaceGraph_EditorData::GetRigVMGraphFunctionHost() const
{
	return this;
}


void UAnimNextInterfaceGraph_EditorData::HandleRigVMGraphAdded(const FRigVMClient* InClient, const FString& InNodePath)
{
	if(URigVMGraph* RigVMGraph = InClient->GetModel(InNodePath))
	{
		RigVMGraph->SetExecuteContextStruct(FAnimNextInterfaceExecuteContext::StaticStruct());
	}
}

void UAnimNextInterfaceGraph_EditorData::HandleConfigureRigVMController(const FRigVMClient* InClient,
                                                                    URigVMController* InControllerToConfigure)
{
	InControllerToConfigure->OnModified().AddUObject(this, &UAnimNextInterfaceGraph_EditorData::HandleModifiedEvent);

	InControllerToConfigure->UnfoldStructDelegate.BindLambda([](const UStruct* InStruct) -> bool
	{
		if (InStruct == TBaseStructure<FQuat>::Get())
		{
			return false;
		}
		if (InStruct == FRuntimeFloatCurve::StaticStruct())
		{
			return false;
		}
		if (InStruct == FRigPose::StaticStruct())
		{
			return false;
		}
		return true;
	});

	TWeakObjectPtr<UAnimNextInterfaceGraph_EditorData> WeakThis(this);

	// this delegate is used by the controller to determine variable validity
	// during a bind process. the controller itself doesn't own the variables,
	// so we need a delegate to request them from the owning blueprint
	InControllerToConfigure->GetExternalVariablesDelegate.BindLambda([](URigVMGraph* InGraph) -> TArray<FRigVMExternalVariable>
	{
		if (InGraph)
		{
			if(UAnimNextInterfaceGraph_EditorData* EditorData = InGraph->GetTypedOuter<UAnimNextInterfaceGraph_EditorData>())
			{
				if (UAnimNextInterfaceGraph* Graph = EditorData->GetTypedOuter<UAnimNextInterfaceGraph>())
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
			if(UAnimNextInterfaceGraph* Graph = WeakThis->GetTypedOuter<UAnimNextInterfaceGraph>())
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

UObject* UAnimNextInterfaceGraph_EditorData::GetEditorObjectForRigVMGraph(URigVMGraph* InVMGraph) const
{
	if(InVMGraph)
	{
		TArray<UAnimNextInterfaceGraph_EdGraph*> AllGraphs = {RootGraph, EntryPointGraph, FunctionLibraryEdGraph};
		for(UAnimNextInterfaceGraph_EdGraph* EdGraph : AllGraphs)
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

FRigVMGraphFunctionStore* UAnimNextInterfaceGraph_EditorData::GetRigVMGraphFunctionStore()
{
	return &GraphFunctionStore;
}

const FRigVMGraphFunctionStore* UAnimNextInterfaceGraph_EditorData::GetRigVMGraphFunctionStore() const
{
	return &GraphFunctionStore;
}


void UAnimNextInterfaceGraph_EditorData::RecompileVM()
{
	UE::AnimNext::InterfaceGraphUncookedOnly::FUtils::Compile(GetTypedOuter<UAnimNextInterfaceGraph>());
}

void UAnimNextInterfaceGraph_EditorData::RecompileVMIfRequired()
{
	if (bVMRecompilationRequired)
	{
		RecompileVM();
	}
}

void UAnimNextInterfaceGraph_EditorData::RequestAutoVMRecompilation()
{
	bVMRecompilationRequired = true;
	if (bAutoRecompileVM)
	{
		RecompileVMIfRequired();
	}
}

void UAnimNextInterfaceGraph_EditorData::HandleModifiedEvent(ERigVMGraphNotifType InNotifType, URigVMGraph* InGraph, UObject* InSubject)
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
			UpdateGraphReturnType();
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

URigVMGraph* UAnimNextInterfaceGraph_EditorData::GetVMGraphForEdGraph(const UEdGraph* InGraph) const
{
	if (InGraph == RootGraph)
	{
		return RigVMClient.GetDefaultModel();
	}
	else
	{
		const UAnimNextInterfaceGraph_EdGraph* Graph = Cast<UAnimNextInterfaceGraph_EdGraph>(InGraph);
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

void UAnimNextInterfaceGraph_EditorData::CreateEdGraphForCollapseNode(URigVMCollapseNode* InNode)
{
	if (InNode->GetGraph()->IsA<URigVMFunctionLibrary>())
	{
		if (URigVMGraph* ContainedGraph = InNode->GetContainedGraph())
		{
			if (EntryPointGraph == nullptr)
			{
				// create a sub graph
				UAnimNextInterfaceGraph_EdGraph* RigFunctionGraph = NewObject<UAnimNextInterfaceGraph_EdGraph>(this, *InNode->GetName(), RF_Transactional);
				RigFunctionGraph->Schema = UAnimNextInterfaceGraph_EdGraphSchema::StaticClass();
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

FEdGraphPinType UAnimNextInterfaceGraph_EditorData::FindGraphReturnPinType() const
{
	static const TCHAR* EndExecStr = TEXT("AnimNextInterfaceEndExec");
	static const FName ResultName = TEXT(".Result");

	const UAnimNextInterfaceGraph_EdGraph* AnimNextInterfaceEdGraph = RootGraph.Get();
	if (AnimNextInterfaceEdGraph != nullptr)
	{
		for (const TObjectPtr<UEdGraphNode>& Node : AnimNextInterfaceEdGraph->Nodes)
		{
			if (IsNodeExecConnected(Node))
			{
				const FString& NodeName = Node->GetName();
				if (NodeName.Contains(EndExecStr))
				{
					for (const UEdGraphPin* Pin : Node->Pins)
					{
						const FString PinName = Pin->PinName.ToString();

						if (PinName.Contains(ResultName.ToString()) && PinName.Contains(EndExecStr))
						{
							return Pin->PinType;
						}
					}
				}
			}
		}
	}

	return FEdGraphPinType();
}

bool UAnimNextInterfaceGraph_EditorData::IsNodeExecConnected(const UEdGraphNode* Node) const
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

void UAnimNextInterfaceGraph_EditorData::UpdateGraphReturnType()
{
	if (UAnimNextInterfaceGraph* Graph = GetTypedOuter<UAnimNextInterfaceGraph>())
	{
		FEdGraphPinType GraphReturnPinType = FindGraphReturnPinType();
		const FName PinTypeName = GetPinTypeName(GraphReturnPinType);
		if (Graph->ReturnTypeName != PinTypeName || Graph->ReturnTypeStruct != GraphReturnPinType.PinSubCategoryObject)
		{
			Graph->Modify();
			Graph->ReturnTypeName = PinTypeName;
			Graph->ReturnTypeStruct = TObjectPtr<UScriptStruct>(Cast<UScriptStruct>(GraphReturnPinType.PinSubCategoryObject));
		}
	}
}

FName UAnimNextInterfaceGraph_EditorData::GetPinTypeName(const FEdGraphPinType& EdGraphPinType)
{
	FName DataType = EdGraphPinType.PinCategory;

	if (EdGraphPinType.PinCategory == UEdGraphSchema_K2::PC_Real)
	{
		if (EdGraphPinType.PinSubCategory == UEdGraphSchema_K2::PC_Float)
		{
			DataType = TNameOf<float>::GetName();
		}
		else if (EdGraphPinType.PinSubCategory == UEdGraphSchema_K2::PC_Double)
		{
			DataType = TNameOf<double>::GetName();
		}
		else
		{
			ensure(false);
		}
	}
	else if (DataType != NAME_None)
	{
		if (DataType == UEdGraphSchema_K2::PC_Struct)
		{
			UObject* DataTypeObject = nullptr;
			DataType = NAME_None;
			if (UScriptStruct* DataStruct = Cast<UScriptStruct>(EdGraphPinType.PinSubCategoryObject))
			{
				DataTypeObject = DataStruct;
				DataType = *DataStruct->GetStructCPPName();
			}
		}

		if (DataType == TEXT("int"))
		{
			DataType = TNameOf<int32>::GetName();
		}
		else if (DataType == TEXT("name"))
		{
			DataType = TNameOf<FName>::GetName();
		}
		else if (DataType == TEXT("string"))
		{
			DataType = TNameOf<FString>::GetName();;
		}
	}

	return DataType;
}
