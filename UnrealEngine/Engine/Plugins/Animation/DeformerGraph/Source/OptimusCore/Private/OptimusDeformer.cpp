// Copyright Epic Games, Inc. All Rights Reserved.

#include "OptimusDeformer.h"

#include "Actions/OptimusNodeActions.h"
#include "Actions/OptimusNodeGraphActions.h"
#include "Actions/OptimusResourceActions.h"
#include "Actions/OptimusVariableActions.h"
#include "DataInterfaces/OptimusDataInterfaceGraph.h"
#include "DataInterfaces/OptimusDataInterfaceRawBuffer.h"
#include "IOptimusComputeKernelProvider.h"
#include "IOptimusDataInterfaceProvider.h"
#include "IOptimusValueProvider.h"
#include "OptimusActionStack.h"
#include "OptimusComputeGraph.h"
#include "OptimusDataTypeRegistry.h"
#include "OptimusDeformerInstance.h"
#include "OptimusCoreModule.h"
#include "OptimusFunctionNodeGraph.h"
#include "OptimusHelpers.h"
#include "OptimusKernelSource.h"
#include "OptimusNode.h"
#include "OptimusNodeGraph.h"
#include "OptimusNodePin.h"
#include "OptimusObjectVersion.h"
#include "OptimusResourceDescription.h"
#include "OptimusVariableDescription.h"

#include "Components/MeshComponent.h"
#include "ComputeFramework/ComputeKernel.h"
#include "Containers/Queue.h"
#include "Misc/UObjectToken.h"
#include "RenderingThread.h"
#include "UObject/Package.h"

// FIXME: We should not be accessing nodes directly.
#include "OptimusValueContainer.h"
#include "Actions/OptimusComponentBindingActions.h"
#include "ComponentSources/OptimusSkeletalMeshComponentSource.h"
#include "Nodes/OptimusNode_ComponentSource.h"
#include "Nodes/OptimusNode_ConstantValue.h"
#include "Nodes/OptimusNode_DataInterface.h"
#include "Nodes/OptimusNode_GetVariable.h"
#include "Nodes/OptimusNode_ResourceAccessorBase.h"

#define PRINT_COMPILED_OUTPUT 1

#define LOCTEXT_NAMESPACE "OptimusDeformer"

static const FName DefaultResourceName("Resource");
static const FName DefaultVariableName("Variable");



UOptimusDeformer::UOptimusDeformer()
{
	UOptimusNodeGraph *UpdateGraph = CreateDefaultSubobject<UOptimusNodeGraph>(UOptimusNodeGraph::UpdateGraphName);
	UpdateGraph->SetGraphType(EOptimusNodeGraphType::Update);
	Graphs.Add(UpdateGraph);

	Bindings = CreateDefaultSubobject<UOptimusComponentSourceBindingContainer>(TEXT("@Bindings"));
	Variables = CreateDefaultSubobject<UOptimusVariableContainer>(TEXT("@Variables"));
	Resources = CreateDefaultSubobject<UOptimusResourceContainer>(TEXT("@Resources"));

	FOptimusDataTypeRegistry::Get().GetOnDataTypeChanged().AddUObject(this, &UOptimusDeformer::OnDataTypeChanged);
}


UOptimusActionStack* UOptimusDeformer::GetActionStack()
{
	if (ActionStack == nullptr)
	{
		ActionStack = NewObject<UOptimusActionStack>(this, TEXT("@ActionStack"));
	}
	return ActionStack;
}


UOptimusNodeGraph* UOptimusDeformer::AddSetupGraph()
{
	FOptimusNodeGraphAction_AddGraph* AddGraphAction = 
		new FOptimusNodeGraphAction_AddGraph(this, EOptimusNodeGraphType::Setup, UOptimusNodeGraph::SetupGraphName, 0);

	if (GetActionStack()->RunAction(AddGraphAction))
	{
		return AddGraphAction->GetGraph(this);
	}
	else
	{
		return nullptr;
	}
}


UOptimusNodeGraph* UOptimusDeformer::AddTriggerGraph(const FString &InName)
{
	if (!UOptimusNodeGraph::IsValidUserGraphName(InName))
	{
		return nullptr;
	}

	FOptimusNodeGraphAction_AddGraph* AddGraphAction =
	    new FOptimusNodeGraphAction_AddGraph(this, EOptimusNodeGraphType::ExternalTrigger, *InName, INDEX_NONE);

	if (GetActionStack()->RunAction(AddGraphAction))
	{
		return AddGraphAction->GetGraph(this);
	}
	else
	{
		return nullptr;
	}
}


UOptimusNodeGraph* UOptimusDeformer::GetUpdateGraph() const
{
	for (UOptimusNodeGraph* Graph: Graphs)
	{
		if (Graph->GetGraphType() == EOptimusNodeGraphType::Update)
		{
			return Graph;
		}
	}
	UE_LOG(LogOptimusCore, Fatal, TEXT("No upgrade graph on deformer (%s)."), *GetPathName());
	return nullptr;
}


bool UOptimusDeformer::RemoveGraph(UOptimusNodeGraph* InGraph)
{
    return GetActionStack()->RunAction<FOptimusNodeGraphAction_RemoveGraph>(InGraph);
}


UOptimusVariableDescription* UOptimusDeformer::AddVariable(
	FOptimusDataTypeRef InDataTypeRef, 
	FName InName /*= NAME_None */
	)
{
	if (InName.IsNone())
	{
		InName = DefaultVariableName;
	}

	if (!InDataTypeRef.IsValid())
	{
		// Default to float.
		InDataTypeRef.Set(FOptimusDataTypeRegistry::Get().FindType(*FDoubleProperty::StaticClass()));
	}

	// Is this data type compatible with resources?
	FOptimusDataTypeHandle DataType = InDataTypeRef.Resolve();
	if (!DataType.IsValid() || !EnumHasAnyFlags(DataType->UsageFlags, EOptimusDataTypeUsageFlags::Variable))
	{
		UE_LOG(LogOptimusCore, Error, TEXT("Invalid data type for variables."));
		return nullptr;
	}

	// Ensure the name is unique.
	InName = Optimus::GetUniqueNameForScope(Variables, InName);

	FOptimusVariableAction_AddVariable* AddVariabAction =
	    new FOptimusVariableAction_AddVariable(InDataTypeRef, InName);

	if (GetActionStack()->RunAction(AddVariabAction))
	{
		return AddVariabAction->GetVariable(this);
	}
	else
	{
		return nullptr;
	}
}


bool UOptimusDeformer::RemoveVariable(
	UOptimusVariableDescription* InVariableDesc
	)
{
	if (!ensure(InVariableDesc))
	{
		return false;
	}
	if (InVariableDesc->GetOuter() != Variables)
	{
		UE_LOG(LogOptimusCore, Error, TEXT("Variable not owned by this deformer."));
		return false;
	}

	return GetActionStack()->RunAction<FOptimusVariableAction_RemoveVariable>(InVariableDesc);
}


bool UOptimusDeformer::RenameVariable(	
	UOptimusVariableDescription* InVariableDesc, 
	FName InNewName,
	bool bInForceChange
	)
{
	if (!ensure(InVariableDesc))
	{
		return false;
	}
	if (InVariableDesc->GetOuter() != Variables)
	{
		UE_LOG(LogOptimusCore, Error, TEXT("Variable not owned by this deformer."));
		return false;
	}
	if (InNewName.IsNone())
	{
		UE_LOG(LogOptimusCore, Error, TEXT("Invalid resource name."));
		return false;
	}

	if (!bInForceChange && InNewName == InVariableDesc->VariableName)
	{
		return true;
	}
	
	// Ensure we can rename to that name, update the name if necessary.
	if (InNewName != InVariableDesc->VariableName)
	{
		InNewName = Optimus::GetUniqueNameForScope(Variables, InNewName);
	}
	
	FOptimusCompoundAction* Action = new FOptimusCompoundAction(TEXT("Rename Variable"));
	
	TArray<UOptimusNode*> AllVariableNodes = GetAllNodesOfClass(UOptimusNode_GetVariable::StaticClass());
	for (UOptimusNode* Node: AllVariableNodes)
	{
		const UOptimusNode_GetVariable* VariableNode = Cast<UOptimusNode_GetVariable>(Node);
		if (VariableNode->GetVariableDescription() == InVariableDesc)
		{
			if (ensure(VariableNode->GetPins().Num() == 1))
			{
				Action->AddSubAction<FOptimusNodeAction_SetPinName>(VariableNode->GetPins()[0], InNewName);
			}
		}	
	}

	if (InNewName == InVariableDesc->VariableName)
	{
		Notify(EOptimusGlobalNotifyType::VariableRenamed, InVariableDesc);
	}
	else
	{
		Action->AddSubAction<FOptimusVariableAction_RenameVariable>(InVariableDesc, InNewName);
	}
	return GetActionStack()->RunAction(Action);
}



bool UOptimusDeformer::SetVariableDataType(
	UOptimusVariableDescription* InVariableDesc,
	FOptimusDataTypeRef InDataType,
	bool bInForceChange
	)
{
	if (!ensure(InVariableDesc))
	{
		return false;
	}
	if (InVariableDesc->GetOuter() != Variables)
	{
		UE_LOG(LogOptimusCore, Error, TEXT("Resource not owned by this deformer."));
		return false;
	}
	
	if (!InDataType.IsValid())
	{
		UE_LOG(LogOptimusCore, Error, TEXT("Invalid data type"));
		return false;
	}

	if (!bInForceChange && InDataType == InVariableDesc->DataType)
	{
		return true;
	}
	
	FOptimusCompoundAction* Action = new FOptimusCompoundAction(TEXT("Set Variable Type"));

	TSet<TTuple<UOptimusNodePin* /*OutputPin*/, UOptimusNodePin* /*InputPin*/>> Links;
	
	TArray<UOptimusNode*> AllVariableNodes = GetAllNodesOfClass(UOptimusNode_GetVariable::StaticClass());
	for (UOptimusNode* Node: AllVariableNodes)
	{
		const UOptimusNode_GetVariable* VariableNode = Cast<UOptimusNode_GetVariable>(Node);
		if (VariableNode->GetVariableDescription() == InVariableDesc)
		{
			if (ensure(VariableNode->GetPins().Num() == 1))
			{
				UOptimusNodePin* Pin = VariableNode->GetPins()[0];

				// Update the pin type to match.
				Action->AddSubAction<FOptimusNodeAction_SetPinType>(VariableNode->GetPins()[0], InDataType);

				// Collect _unique_ links (in case there's a resource->resource link, since that would otherwise
				// show up twice).
				const UOptimusNodeGraph* Graph = Pin->GetOwningNode()->GetOwningGraph();

				for (UOptimusNodePin* ConnectedPin: Graph->GetConnectedPins(Pin))
				{
					if (Pin->GetDirection() == EOptimusNodePinDirection::Output)
					{
						Links.Add({Pin, ConnectedPin});
					}
					else
					{
						Links.Add({ConnectedPin, Pin});
					}
				}
			}
		}	
	}

	for (auto [OutputPin, InputPin]: Links)
	{
		Action->AddSubAction<FOptimusNodeGraphAction_RemoveLink>(OutputPin, InputPin);
	}

	if (InVariableDesc->DataType != InDataType)
	{
		Action->AddSubAction<FOptimusVariableAction_SetDataType>(InVariableDesc, InDataType);
	}
	else
	{
		Notify(EOptimusGlobalNotifyType::VariableTypeChanged, InVariableDesc);
	}

	if (!GetActionStack()->RunAction(Action))
	{
		return false;
	}
	
	// Make sure the value data container is still large enough to hold the property value.
	InVariableDesc->ValueData.Reset();
	InVariableDesc->ResetValueDataSize();
	return true;
}


UOptimusVariableDescription* UOptimusDeformer::ResolveVariable(
	FName InVariableName
	) const
{
	for (UOptimusVariableDescription* Variable : GetVariables())
	{
		if (Variable->GetFName() == InVariableName)
		{
			return Variable;
		}
	}
	return nullptr;
}


UOptimusVariableDescription* UOptimusDeformer::CreateVariableDirect(
	FName InName
	)
{
	if (!ensure(!InName.IsNone()))
	{
		return nullptr;
	}
	
	UOptimusVariableDescription* Variable = NewObject<UOptimusVariableDescription>(
		Variables, 
		UOptimusVariableDescription::StaticClass(), 
		InName, 
		RF_Transactional);

	// Make sure to give this variable description a unique GUID. We use this when updating the
	// class.
	Variable->Guid = FGuid::NewGuid();
	
	(void)MarkPackageDirty();

	return Variable;
}


bool UOptimusDeformer::AddVariableDirect(
	UOptimusVariableDescription* InVariableDesc
	)
{
	if (!ensure(InVariableDesc))
	{
		return false;
	}

	if (!ensure(InVariableDesc->GetOuter() == Variables))
	{
		return false;
	}

	Variables->Descriptions.Add(InVariableDesc);

	Notify(EOptimusGlobalNotifyType::VariableAdded, InVariableDesc);

	return true;
}


bool UOptimusDeformer::RemoveVariableDirect(
	UOptimusVariableDescription* InVariableDesc
	)
{
	// Do we actually own this variable?
	const int32 VariableIndex = Variables->Descriptions.IndexOfByKey(InVariableDesc);
	if (VariableIndex == INDEX_NONE)
	{
		return false;
	}

	Variables->Descriptions.RemoveAt(VariableIndex);

	Notify(EOptimusGlobalNotifyType::VariableRemoved, InVariableDesc);

	InVariableDesc->Rename(nullptr, GetTransientPackage());
	InVariableDesc->MarkAsGarbage();

	(void)MarkPackageDirty();

	return true;
}


bool UOptimusDeformer::RenameVariableDirect(
	UOptimusVariableDescription* InVariableDesc, 
	FName InNewName
	)
{
	// Do we actually own this variable?
	if (Variables->Descriptions.IndexOfByKey(InVariableDesc) == INDEX_NONE)
	{
		return false;
	}

	if (InVariableDesc->Rename(*InNewName.ToString(), nullptr, REN_NonTransactional))
	{
		InVariableDesc->VariableName = InNewName;
		Notify(EOptimusGlobalNotifyType::VariableRenamed, InVariableDesc);
		(void)MarkPackageDirty();
		return true;
	}
	
	return false;
}


bool UOptimusDeformer::SetVariableDataTypeDirect(
	UOptimusVariableDescription* InVariableDesc,
	FOptimusDataTypeRef InDataType
	)
{
	// Do we actually own this variable?
	if (Variables->Descriptions.IndexOfByKey(InVariableDesc) == INDEX_NONE)
	{
		return false;
	}
	
	if (InVariableDesc->DataType != InDataType)
	{
		InVariableDesc->DataType = InDataType;
		Notify(EOptimusGlobalNotifyType::VariableTypeChanged, InVariableDesc);
		(void)MarkPackageDirty();
	}

	return true;
}


UOptimusResourceDescription* UOptimusDeformer::AddResource(
	FOptimusDataTypeRef InDataTypeRef,
	FName InName
	)
{
	if (InName.IsNone())
	{
		InName = DefaultResourceName;
	}

	if (!InDataTypeRef.IsValid())
	{
		// Default to float.
		InDataTypeRef.Set(FOptimusDataTypeRegistry::Get().FindType(*FFloatProperty::StaticClass()));
	}

	// Is this data type compatible with resources?
	FOptimusDataTypeHandle DataType = InDataTypeRef.Resolve();
	if (!DataType.IsValid() || !EnumHasAnyFlags(DataType->UsageFlags, EOptimusDataTypeUsageFlags::Resource))
	{
		UE_LOG(LogOptimusCore, Error, TEXT("Invalid data type for resources."));
		return nullptr;
	}

	// Ensure the name is unique.
	InName = Optimus::GetUniqueNameForScope(Resources, InName);

	FOptimusResourceAction_AddResource *AddResourceAction = 	
	    new FOptimusResourceAction_AddResource(InDataTypeRef, InName);

	if (GetActionStack()->RunAction(AddResourceAction))
	{
		return AddResourceAction->GetResource(this);
	}
	else
	{
		return nullptr;
	}
}


bool UOptimusDeformer::RemoveResource(UOptimusResourceDescription* InResourceDesc)
{
	if (!ensure(InResourceDesc))
	{
		return false;
	}
	if (InResourceDesc->GetOuter() != Resources)
	{
		UE_LOG(LogOptimusCore, Error, TEXT("Resource not owned by this deformer."));
		return false;
	}

	FOptimusCompoundAction* Action = new FOptimusCompoundAction(TEXT("Remove Resource"));
	
	TMap<const UOptimusNodeGraph*, TArray<UOptimusNode*>> NodesByGraph;
	
	TArray<UOptimusNode*> AllResourceNodes = GetAllNodesOfClass(UOptimusNode_ResourceAccessorBase::StaticClass());
	for (UOptimusNode* Node: AllResourceNodes)
	{
		UOptimusNode_ResourceAccessorBase* ResourceNode = Cast<UOptimusNode_ResourceAccessorBase>(Node);
		if (ResourceNode->GetResourceDescription() == InResourceDesc)
		{
			NodesByGraph.FindOrAdd(ResourceNode->GetOwningGraph()).Add(ResourceNode);
		}	
	}

	for (const TTuple<const UOptimusNodeGraph*, TArray<UOptimusNode*>>& GraphNodes: NodesByGraph)
	{
		const UOptimusNodeGraph* Graph = GraphNodes.Key;
		Graph->RemoveNodesToAction(Action, GraphNodes.Value);
	}

	Action->AddSubAction<FOptimusResourceAction_RemoveResource>(InResourceDesc);

	return GetActionStack()->RunAction(Action);
}


bool UOptimusDeformer::RenameResource(
	UOptimusResourceDescription* InResourceDesc, 
	FName InNewName,
	bool bInForceChange
	)
{
	if (!ensure(InResourceDesc))
	{
		return false;
	}
	if (InResourceDesc->GetOuter() != Resources)
	{
		UE_LOG(LogOptimusCore, Error, TEXT("Resource not owned by this deformer."));
		return false;
	}
	
	if (InNewName.IsNone())
	{
		UE_LOG(LogOptimusCore, Error, TEXT("Invalid resource name"));
		return false;
	}

	if (!bInForceChange && InNewName == InResourceDesc->ResourceName)
	{
		return true;
	}

	// Ensure we can rename to that name, update the name if necessary.
	if (InNewName != InResourceDesc->ResourceName)
	{
		InNewName = Optimus::GetUniqueNameForScope(Resources, InNewName);
	}

	FOptimusCompoundAction* Action = new FOptimusCompoundAction(TEXT("Rename Resource"));

	TArray<UOptimusNode*> AllResourceNodes = GetAllNodesOfClass(UOptimusNode_ResourceAccessorBase::StaticClass());
	for (UOptimusNode* Node: AllResourceNodes)
	{
		const UOptimusNode_ResourceAccessorBase* ResourceNode = Cast<UOptimusNode_ResourceAccessorBase>(Node);
		if (ResourceNode->GetResourceDescription() == InResourceDesc)
		{
			for (int32 PinIndex = 0; PinIndex < ResourceNode->GetPins().Num(); ++PinIndex)
			{
				Action->AddSubAction<FOptimusNodeAction_SetPinName>(ResourceNode->GetPins()[PinIndex], ResourceNode->GetResourcePinName(PinIndex));
			}
		}	
	}	

	if (InNewName == InResourceDesc->ResourceName)
	{
		// Make sure we update the explorer.
		Notify(EOptimusGlobalNotifyType::ResourceRenamed, InResourceDesc);
	}
	else
	{
		Action->AddSubAction<FOptimusResourceAction_RenameResource>(InResourceDesc, InNewName);
	}
	return GetActionStack()->RunAction(Action);
}


bool UOptimusDeformer::SetResourceDataType(
	UOptimusResourceDescription* InResourceDesc,
	FOptimusDataTypeRef InDataType,
	bool bInForceChange
	)
{
	if (!ensure(InResourceDesc))
	{
		return false;
	}
	if (InResourceDesc->GetOuter() != Resources)
	{
		UE_LOG(LogOptimusCore, Error, TEXT("Resource not owned by this deformer."));
		return false;
	}

	if (!InDataType.IsValid())
	{
		UE_LOG(LogOptimusCore, Error, TEXT("Invalid data type"));
		return false;
	}

	if (!bInForceChange && InDataType == InResourceDesc->DataType)
	{
		return true;
	}
	
	FOptimusCompoundAction* Action = new FOptimusCompoundAction(TEXT("Set Resource Data Type"));

	TSet<TTuple<UOptimusNodePin* /*OutputPin*/, UOptimusNodePin* /*InputPin*/>> Links;
	
	TArray<UOptimusNode*> AllResourceNodes = GetAllNodesOfClass(UOptimusNode_ResourceAccessorBase::StaticClass());
	for (UOptimusNode* Node: AllResourceNodes)
	{
		const UOptimusNode_ResourceAccessorBase* ResourceNode = Cast<UOptimusNode_ResourceAccessorBase>(Node);
		if (ResourceNode->GetResourceDescription() == InResourceDesc)
		{
			for (UOptimusNodePin* Pin : ResourceNode->GetPins())
			{
				// Update the pin type to match.
				Action->AddSubAction<FOptimusNodeAction_SetPinType>(Pin, InDataType);

				// Collect _unique_ links (in case there's a resource->resource link, since that would otherwise show up twice).
				const UOptimusNodeGraph* Graph = Pin->GetOwningNode()->GetOwningGraph();

				for (UOptimusNodePin* ConnectedPin: Graph->GetConnectedPins(Pin))
				{
					if (Pin->GetDirection() == EOptimusNodePinDirection::Output)
					{
						Links.Add({Pin, ConnectedPin});
					}
					else
					{
						Links.Add({ConnectedPin, Pin});
					}
				}
			}
		}	
	}

	for (auto [OutputPin, InputPin]: Links)
	{
		Action->AddSubAction<FOptimusNodeGraphAction_RemoveLink>(OutputPin, InputPin);
	}

	if (InResourceDesc->DataType != InDataType)
	{
		Action->AddSubAction<FOptimusResourceAction_SetDataType>(InResourceDesc, InDataType);
	}

	return GetActionStack()->RunAction(Action);
}

bool UOptimusDeformer::SetResourceDataDomain(
	UOptimusResourceDescription* InResourceDesc,
	const FOptimusDataDomain& InDataDomain,
	bool bInForceChange
	)
{
	if (!ensure(InResourceDesc))
	{
		return false;
	}
	if (InResourceDesc->GetOuter() != Resources)
	{
		UE_LOG(LogOptimusCore, Error, TEXT("Resource not owned by this deformer."));
		return false;
	}

	if (!bInForceChange && InDataDomain == InResourceDesc->DataDomain)
	{
		return true;
	}
	
	FOptimusCompoundAction* Action = new FOptimusCompoundAction(TEXT("Set Resource Data Domain"));

	TSet<TTuple<UOptimusNodePin* /*OutputPin*/, UOptimusNodePin* /*InputPin*/>> Links;
	
	TArray<UOptimusNode*> AllResourceNodes = GetAllNodesOfClass(UOptimusNode_ResourceAccessorBase::StaticClass());
	for (UOptimusNode* Node: AllResourceNodes)
	{
		const UOptimusNode_ResourceAccessorBase* ResourceNode = Cast<UOptimusNode_ResourceAccessorBase>(Node);
		if (ResourceNode->GetResourceDescription() == InResourceDesc)
		{
			for (UOptimusNodePin* Pin : ResourceNode->GetPins())
			{
				// Update the pin type to match.
				Action->AddSubAction<FOptimusNodeAction_SetPinDataDomain>(Pin, InDataDomain);

				// Collect _unique_ links (in case there's a resource->resource link, since that would otherwise
				// show up twice).
				const UOptimusNodeGraph* Graph = Pin->GetOwningNode()->GetOwningGraph();

				for (UOptimusNodePin* ConnectedPin: Graph->GetConnectedPins(Pin))
				{
					if (Pin->GetDirection() == EOptimusNodePinDirection::Output)
					{
						Links.Add({Pin, ConnectedPin});
					}
					else
					{
						Links.Add({ConnectedPin, Pin});
					}
				}
			}
		}	
	}

	for (auto [OutputPin, InputPin]: Links)
	{
		Action->AddSubAction<FOptimusNodeGraphAction_RemoveLink>(OutputPin, InputPin);
	}

	if (InResourceDesc->DataDomain != InDataDomain)
	{
		Action->AddSubAction<FOptimusResourceAction_SetDataDomain>(InResourceDesc, InDataDomain);
	}

	return GetActionStack()->RunAction(Action);
}


UOptimusResourceDescription* UOptimusDeformer::ResolveResource(
	FName InResourceName
	) const
{
	for (UOptimusResourceDescription* Resource : GetResources())
	{
		if (Resource->GetFName() == InResourceName)
		{
			return Resource;
		}
	}
	return nullptr;
}


UOptimusResourceDescription* UOptimusDeformer::CreateResourceDirect(
	FName InName
	)
{
	if (!ensure(!InName.IsNone()))
	{
		return nullptr;
	}
	
	// The resource is actually owned by the "Resources" container to avoid name clashing as
	// much as possible.
	UOptimusResourceDescription* Resource = NewObject<UOptimusResourceDescription>(
		Resources, 
		UOptimusResourceDescription::StaticClass(),
		InName, 
		RF_Transactional);

	return Resource;
}


bool UOptimusDeformer::AddResourceDirect(
	UOptimusResourceDescription* InResourceDesc
	)
{
	if (!ensure(InResourceDesc))
	{
		return false;
	}

	if (!ensure(InResourceDesc->GetOuter() == Resources))
	{
		return false;
	}

	Resources->Descriptions.Add(InResourceDesc);
	Notify(EOptimusGlobalNotifyType::ResourceAdded, InResourceDesc);
	(void)MarkPackageDirty();
	
	return true;
}


bool UOptimusDeformer::RemoveResourceDirect(
	UOptimusResourceDescription* InResourceDesc
	)
{
	// Do we actually own this resource?
	int32 ResourceIndex = Resources->Descriptions.IndexOfByKey(InResourceDesc);
	if (ResourceIndex == INDEX_NONE)
	{
		return false;
	}

	Resources->Descriptions.RemoveAt(ResourceIndex);

	Notify(EOptimusGlobalNotifyType::ResourceRemoved, InResourceDesc);

	InResourceDesc->Rename(nullptr, GetTransientPackage());
	InResourceDesc->MarkAsGarbage();

	(void)MarkPackageDirty();

	return true;
}


bool UOptimusDeformer::RenameResourceDirect(
	UOptimusResourceDescription* InResourceDesc, 
	FName InNewName
	)
{
	// Do we actually own this resource?
	int32 ResourceIndex = Resources->Descriptions.IndexOfByKey(InResourceDesc);
	if (ResourceIndex == INDEX_NONE)
	{
		return false;
	}

	// Rename in a non-transactional manner, since we're handling undo/redo.
	if (InResourceDesc->Rename(*InNewName.ToString(), nullptr, REN_NonTransactional))
	{
		InResourceDesc->ResourceName = InNewName;
		Notify(EOptimusGlobalNotifyType::ResourceRenamed, InResourceDesc);
		(void)MarkPackageDirty();
		return true;
	}

	return false;
}


bool UOptimusDeformer::SetResourceDataTypeDirect(
	UOptimusResourceDescription* InResourceDesc,
	FOptimusDataTypeRef InDataType
	)
{
	// Do we actually own this resource?
	const int32 ResourceIndex = Resources->Descriptions.IndexOfByKey(InResourceDesc);
	if (ResourceIndex == INDEX_NONE)
	{
		return false;
	}
	
	if (InResourceDesc->DataType != InDataType)
	{
		InResourceDesc->DataType = InDataType;
		Notify(EOptimusGlobalNotifyType::ResourceTypeChanged, InResourceDesc);
		(void)MarkPackageDirty();
	}
	
	return true;
}


bool UOptimusDeformer::SetResourceDataDomainDirect(
	UOptimusResourceDescription* InResourceDesc,
	const FOptimusDataDomain& InDataDomain
	)
{
	// Do we actually own this resource?
	const int32 ResourceIndex = Resources->Descriptions.IndexOfByKey(InResourceDesc);
	if (ResourceIndex == INDEX_NONE)
	{
		return false;
	}
	
	if (InResourceDesc->DataDomain != InDataDomain)
	{
		InResourceDesc->DataDomain = InDataDomain;
		Notify(EOptimusGlobalNotifyType::ResourceTypeChanged, InResourceDesc);
		(void)MarkPackageDirty();
	}
	
	return true;
}


// === Component bindings
UOptimusComponentSourceBinding* UOptimusDeformer::AddComponentBinding(
	const UOptimusComponentSource* InComponentSource,
	FName InName
	)
{
	if (!InComponentSource)
	{
		InComponentSource = UOptimusSkeletalMeshComponentSource::StaticClass()->GetDefaultObject<UOptimusComponentSource>();		
	}

	if (InName.IsNone())
	{
		InName = InComponentSource->GetBindingName();
	}

	InName = Optimus::GetUniqueNameForScope(Bindings, InName);

	FOptimusComponentBindingAction_AddBinding *AddComponentBindingAction = 	
		new FOptimusComponentBindingAction_AddBinding(InComponentSource, InName);

	if (!GetActionStack()->RunAction(AddComponentBindingAction))
	{
		return nullptr;
	}
	
	return AddComponentBindingAction->GetComponentBinding(this);
}


UOptimusComponentSourceBinding* UOptimusDeformer::CreateComponentBindingDirect(
	const UOptimusComponentSource* InComponentSource,
	FName InName
	)
{
	if (!ensure(InComponentSource) || !ensure(!InName.IsNone()))
	{
		return nullptr;
	}

	UOptimusComponentSourceBinding* Binding = NewObject<UOptimusComponentSourceBinding>(
		Bindings, 
		UOptimusComponentSourceBinding::StaticClass(), 
		InName, 
		RF_Transactional);

	Binding->ComponentType = InComponentSource->GetClass();
	Binding->BindingName = InName;

	return Binding;
}


bool UOptimusDeformer::AddComponentBindingDirect(
	UOptimusComponentSourceBinding* InComponentBinding
	)
{
	if (!ensure(InComponentBinding))
	{
		return false;
	}
	if (!ensure(InComponentBinding->GetOuter() == Bindings))
	{
		return false;
	}

	if (Bindings->Bindings.IsEmpty())
	{
		InComponentBinding->bIsPrimaryBinding = true;
	}
	
	Bindings->Bindings.Add(InComponentBinding);

	Notify(EOptimusGlobalNotifyType::ComponentBindingAdded, InComponentBinding);

	(void)MarkPackageDirty();

	return true;
}


bool UOptimusDeformer::RemoveComponentBinding(
	UOptimusComponentSourceBinding* InBinding
	)
{
	if (!ensure(InBinding))
	{
		return false;
	}
	if (InBinding->GetOuter() != Bindings)
	{
		UE_LOG(LogOptimusCore, Error, TEXT("Component binding not owned by this deformer."));
		return false;
	}
	if (InBinding->bIsPrimaryBinding)
	{
		UE_LOG(LogOptimusCore, Error, TEXT("The primary binding cannot be removed."));
		return false;
	}

	FOptimusCompoundAction* Action = new FOptimusCompoundAction(TEXT("Remove Binding"));
	
	TMap<const UOptimusNodeGraph*, TArray<UOptimusNode*>> NodesByGraph;
	
	TArray<UOptimusNode*> AllComponentSourceNodes = GetAllNodesOfClass(UOptimusNode_ComponentSource::StaticClass());
	for (UOptimusNode* Node: AllComponentSourceNodes)
	{
		UOptimusNode_ComponentSource* ComponentNode = Cast<UOptimusNode_ComponentSource>(Node);
		if (ComponentNode->GetComponentBinding() == InBinding)
		{
			NodesByGraph.FindOrAdd(ComponentNode->GetOwningGraph()).Add(ComponentNode);
		}	
	}

	for (const TTuple<const UOptimusNodeGraph*, TArray<UOptimusNode*>>& GraphNodes: NodesByGraph)
	{
		const UOptimusNodeGraph* Graph = GraphNodes.Key;
		Graph->RemoveNodesToAction(Action, GraphNodes.Value);
	}

	Action->AddSubAction<FOptimusComponentBindingAction_RemoveBinding>(InBinding);

	return GetActionStack()->RunAction(Action);
}


bool UOptimusDeformer::RemoveComponentBindingDirect(UOptimusComponentSourceBinding* InBinding)
{
	// Do we actually own this binding?
	const int32 BindingIndex = Bindings->Bindings.IndexOfByKey(InBinding);
	if (BindingIndex == INDEX_NONE)
	{
		return false;
	}

	Bindings->Bindings.RemoveAt(BindingIndex);

	Notify(EOptimusGlobalNotifyType::ComponentBindingRemoved, InBinding);

	InBinding->Rename(nullptr, GetTransientPackage());
	InBinding->MarkAsGarbage();

	(void)MarkPackageDirty();
	
	return true;
}


bool UOptimusDeformer::RenameComponentBinding(
	UOptimusComponentSourceBinding* InBinding, 
	FName InNewName,
	bool bInForceChange
	)
{
	if (!ensure(InBinding))
	{
		return false;
	}
	if (InBinding->GetOuter() != Bindings)
	{
		UE_LOG(LogOptimusCore, Error, TEXT("Binding not owned by this deformer."));
		return false;
	}
	
	if (InNewName.IsNone())
	{
		UE_LOG(LogOptimusCore, Error, TEXT("Invalid binding name"));
		return false;
	}

	if (!bInForceChange && InNewName == InBinding->BindingName)
	{
		return true;
	}

	// Ensure we can rename to that name, update the name if necessary.
	if (InNewName != InBinding->BindingName)
	{
		InNewName = Optimus::GetUniqueNameForScope(Resources, InNewName);
	}

	FOptimusCompoundAction* Action = new FOptimusCompoundAction(TEXT("Rename Component Binding"));

	TArray<UOptimusNode*> AllComponentSourceNode = GetAllNodesOfClass(UOptimusNode_ComponentSource::StaticClass());
	for (UOptimusNode* Node: AllComponentSourceNode)
	{
		UOptimusNode_ComponentSource* ComponentSourceNode = Cast<UOptimusNode_ComponentSource>(Node);
		if (ComponentSourceNode->GetComponentBinding() == InBinding)
		{
			Action->AddSubAction<FOptimusNodeAction_RenameNode>(ComponentSourceNode, FText::FromName(InNewName));
		}	
	}	

	if (InNewName == InBinding->BindingName)
	{
		Notify(EOptimusGlobalNotifyType::ComponentBindingRenamed, InBinding);
	}
	else
	{
		Action->AddSubAction<FOptimusComponentBindingAction_RenameBinding>(InBinding, InNewName);
	}
	
	return GetActionStack()->RunAction(Action);
}



bool UOptimusDeformer::RenameComponentBindingDirect(
	UOptimusComponentSourceBinding* InBinding,
	FName InNewName
	)
{
	// Do we actually own this binding?
	if (Bindings->Bindings.IndexOfByKey(InBinding) == INDEX_NONE)
	{
		return false;
	}
	
	if (InBinding->Rename(*InNewName.ToString(), nullptr, REN_NonTransactional))
	{
		InBinding->BindingName = InNewName;
		Notify(EOptimusGlobalNotifyType::ComponentBindingRenamed, InBinding);
		(void)MarkPackageDirty();
		return true;
	}
	
	return false;
}


bool UOptimusDeformer::SetComponentBindingSource(
	UOptimusComponentSourceBinding* InBinding,
	const UOptimusComponentSource* InComponentSource,
	bool bInForceChange
	)
{
	if (!ensure(InBinding))
	{
		return false;
	}
	if (InBinding->GetOuter() != Bindings)
	{
		UE_LOG(LogOptimusCore, Error, TEXT("Binding not owned by this deformer."));
		return false;
	}
	
	if (!InComponentSource)
	{
		UE_LOG(LogOptimusCore, Error, TEXT("Invalid component source"));
		return false;
	}

	if (!bInForceChange && InBinding->ComponentType == InComponentSource->GetClass())
	{
		return true;
	}

	FOptimusCompoundAction* Action = new FOptimusCompoundAction(TEXT("Set Component Binding Source"));
	TSet<TTuple<UOptimusNodePin* /*OutputPin*/, UOptimusNodePin* /*InputPin*/>> Links;
	TArray<UOptimusNode*> AllComponentSourceNode = GetAllNodesOfClass(UOptimusNode_ComponentSource::StaticClass());
	for (UOptimusNode* Node: AllComponentSourceNode)
	{
		const UOptimusNode_ComponentSource* ComponentSourceNode = Cast<UOptimusNode_ComponentSource>(Node);
		if (ComponentSourceNode->GetComponentBinding() == InBinding)
		{
			UOptimusNodePin* ComponentSourcePin = ComponentSourceNode->GetComponentPin();

			const UOptimusNodeGraph* Graph = ComponentSourceNode->GetOwningGraph();

			for (UOptimusNodePin* ConnectedPin: Graph->GetConnectedPins(ComponentSourcePin))
			{
				// Will this connection be invalid once the source is changed?
				if (const UOptimusNode_DataInterface* DataInterfaceNode = Cast<UOptimusNode_DataInterface>(ConnectedPin->GetOwningNode()))
				{
					if (!DataInterfaceNode->IsComponentSourceCompatible(InComponentSource))
					{
						Action->AddSubAction<FOptimusNodeGraphAction_RemoveLink>(ComponentSourcePin, ConnectedPin);
					}
				}
			}

			// Change the pin name _after_ the links are removed, since the link remove action uses the pin path, including
			// the old name to resolve the pin.
			Action->AddSubAction<FOptimusNodeAction_SetPinName>(ComponentSourcePin, InComponentSource->GetBindingName());
		}	
	}
	
	if (InBinding->ComponentType != InComponentSource->GetClass())
	{
		Action->AddSubAction<FOptimusComponentBindingAction_SetComponentSource>(InBinding, InComponentSource);
	}
	else
	{
		Notify(EOptimusGlobalNotifyType::ComponentBindingSourceChanged, InBinding);
	}

	return GetActionStack()->RunAction(Action);
}


bool UOptimusDeformer::SetComponentBindingSourceDirect(
	UOptimusComponentSourceBinding* InBinding,
	const UOptimusComponentSource* InComponentSource
	)
{
	// Do we actually own this binding?
	if (Bindings->Bindings.IndexOfByKey(InBinding) == INDEX_NONE)
	{
		return false;
	}

	if (InBinding->ComponentType != InComponentSource->GetClass())
	{
		InBinding->ComponentType = InComponentSource->GetClass();
		Notify(EOptimusGlobalNotifyType::ComponentBindingSourceChanged, InBinding);
		(void)MarkPackageDirty();
	}
	
	return true;
}


UOptimusComponentSourceBinding* UOptimusDeformer::ResolveComponentBinding(
	FName InBindingName
	) const
{
	for (UOptimusComponentSourceBinding* Binding : GetComponentBindings())
	{
		if (Binding->GetFName() == InBindingName)
		{
			return Binding;
		}
	}
	return nullptr;
}



// Do a breadth-first collection of nodes starting from the seed nodes (terminal data interfaces).
static void CollectNodes(
	const TArray<const UOptimusNode*>& InSeedNodes,
	TArray<FOptimusRoutedConstNode>& OutCollectedNodes
	)
{
	TSet<const UOptimusNode*> VisitedNodes;
	TQueue<FOptimusRoutedConstNode> WorkingSet;

	for (const UOptimusNode* Node: InSeedNodes)
	{
		WorkingSet.Enqueue({Node, FOptimusPinTraversalContext{}});
		VisitedNodes.Add(Node);
		OutCollectedNodes.Add({Node, FOptimusPinTraversalContext{}});
	}

	auto CollectFromInputPins = [&WorkingSet, &VisitedNodes, &OutCollectedNodes](const FOptimusRoutedConstNode& InWorkItem, const UOptimusNodePin* InPin)
	{
		for (const FOptimusRoutedNodePin& ConnectedPin: InPin->GetConnectedPinsWithRouting(InWorkItem.TraversalContext))
		{
			if (ensure(ConnectedPin.NodePin != nullptr))
			{
				const UOptimusNode *NextNode = ConnectedPin.NodePin->GetOwningNode();
				FOptimusRoutedConstNode CollectedNode{NextNode, ConnectedPin.TraversalContext};
				WorkingSet.Enqueue(CollectedNode);
				if (!VisitedNodes.Contains(NextNode))
				{
					VisitedNodes.Add(NextNode);
					OutCollectedNodes.Add(CollectedNode);
				}
				else
				{
					// Push the node to the back because to ensure that it is scheduled  earlier then it's referencing node.
					OutCollectedNodes.RemoveSingle(CollectedNode);
					OutCollectedNodes.Add(CollectedNode);
				}
			}
		}
	};

	FOptimusRoutedConstNode WorkItem;
	while (WorkingSet.Dequeue(WorkItem))
	{
		// Traverse in the direction of input pins (up the graph).
		for (const UOptimusNodePin* Pin: WorkItem.Node->GetPins())
		{
			if (Pin->GetDirection() == EOptimusNodePinDirection::Input)
			{
				if (Pin->IsGroupingPin())
				{
					for (const UOptimusNodePin* SubPin: Pin->GetSubPins())
					{
						CollectFromInputPins(WorkItem, SubPin);
					}
				}
				else
				{
					CollectFromInputPins(WorkItem, Pin);
				}
			}
		}	
	}	
}


bool UOptimusDeformer::Compile()
{
	if (!GetUpdateGraph())
	{
		FOptimusCompilerDiagnostic Diagnostic;
		Diagnostic.Level = EOptimusDiagnosticLevel::Error;
		Diagnostic.Message = LOCTEXT("NoGraphFound", "No update graph found. Compilation aborted.");

		CompileBeginDelegate.Broadcast(this);
		CompileMessageDelegate.Broadcast(Diagnostic);
		CompileEndDelegate.Broadcast(this);
		return false;
	}

	ComputeGraphs.Reset();
	
	CompileBeginDelegate.Broadcast(this);
	
	// Wait for rendering to be done.
	FlushRenderingCommands();

	bool bCompilationFailed = false;
	auto ErrorReporter = [this, &bCompilationFailed](
		EOptimusDiagnosticLevel InDiagnosticLevel, 
		FText InMessage,
		const UObject* InObject)
	{
		FOptimusCompilerDiagnostic Diagnostic;
		Diagnostic.Level = InDiagnosticLevel;
		Diagnostic.Message = InMessage;
		Diagnostic.Object = InObject;
		CompileMessageDelegate.Broadcast(Diagnostic);
		bCompilationFailed = true;
	};

	for (const UOptimusNodeGraph* Graph: Graphs)
	{
		if (UOptimusComputeGraph* ComputeGraph = CompileNodeGraphToComputeGraph(Graph, ErrorReporter))
		{
			FOptimusComputeGraphInfo Info;
			Info.GraphType = Graph->GraphType;
			Info.GraphName = Graph->GetFName();
			Info.ComputeGraph = ComputeGraph;
			ComputeGraphs.Add(Info);
		}
	}
	
	CompileEndDelegate.Broadcast(this);

	if (bCompilationFailed)
	{
		ComputeGraphs.Reset();
		return false;
	}
	
	for (const FOptimusComputeGraphInfo& ComputeGraphInfo: ComputeGraphs)
	{
		ComputeGraphInfo.ComputeGraph->UpdateResources();
	}
	
	return true;
}

TArray<UOptimusNode*> UOptimusDeformer::GetAllNodesOfClass(UClass* InNodeClass) const
{
	if (!ensure(InNodeClass->IsChildOf<UOptimusNode>()))
	{
		return {};
	}

	TArray<UOptimusNodeGraph*> GraphsToSearch = Graphs;
	TArray<UOptimusNode*> NodesFound;
	
	while(!GraphsToSearch.IsEmpty())
	{
		constexpr bool bAllowShrinking = false;
		const UOptimusNodeGraph* CurrentGraph = GraphsToSearch.Pop(bAllowShrinking);

		for (UOptimusNode* Node: CurrentGraph->GetAllNodes())
		{
			if (Node->GetClass()->IsChildOf(InNodeClass))
			{
				NodesFound.Add(Node);
			}
		}

		GraphsToSearch.Append(CurrentGraph->GetGraphs());
	}

	return NodesFound;
}


UOptimusComputeGraph* UOptimusDeformer::CompileNodeGraphToComputeGraph(
	const UOptimusNodeGraph* InNodeGraph,
	TFunction<void(EOptimusDiagnosticLevel, FText, const UObject*)> InErrorReporter
	)
{
	auto AddDiagnostic = [ErrorReporter=InErrorReporter](
		EOptimusDiagnosticLevel InLevel, 
		FText InMessage,
		const UOptimusNode* InNode = nullptr)
	{
		// Only raise the diagnostic level.
		if (InNode && InNode->GetDiagnosticLevel() < InLevel)
		{
			const_cast<UOptimusNode*>(InNode)->SetDiagnosticLevel(InLevel);
		}

		ErrorReporter(InLevel, InMessage, InNode);
	};
	
	// No nodes in the graph, nothing to do.
	if (InNodeGraph->GetAllNodes().IsEmpty())
	{
		return nullptr;
	}

	// Clear the error state of all nodes.
	for (UOptimusNode* Node: InNodeGraph->GetAllNodes())
	{
		Node->SetDiagnosticLevel(EOptimusDiagnosticLevel::None);
	}

	// Terminal nodes are data providers that contain only input pins. Any graph with no
	// written output is a null graph.
	TArray<const UOptimusNode*> TerminalNodes;
	
	for (const UOptimusNode* Node: InNodeGraph->GetAllNodes())
	{
		bool bConnectedInput = false;

		const IOptimusDataInterfaceProvider* DataInterfaceProviderNode = Cast<const IOptimusDataInterfaceProvider>(Node);

		if (DataInterfaceProviderNode)
		{
			for (const UOptimusNodePin* Pin: Node->GetPins())
			{
				// NOTE: No grouping pins on data interfaces (yet).
				if (!ensure(!Pin->IsGroupingPin()))
				{
					continue;
				}
				
				if (Pin->GetDirection() == EOptimusNodePinDirection::Input && Pin->GetConnectedPins().Num())
				{
					bConnectedInput = true;
				}
				if (Pin->GetDirection() == EOptimusNodePinDirection::Output)
				{
					DataInterfaceProviderNode = nullptr;
					break;
				}
			}
		}
		if (DataInterfaceProviderNode && bConnectedInput)
		{
			TerminalNodes.Add(Node);
		}
	}

	if (TerminalNodes.IsEmpty())
	{
		AddDiagnostic(EOptimusDiagnosticLevel::Error, LOCTEXT("NoOutputDataInterfaceFound", "No connected output data interface nodes found. Compilation aborted."));
		return nullptr;
	}

	TArray<FOptimusRoutedConstNode> ConnectedNodes;
	CollectNodes(TerminalNodes, ConnectedNodes);

	// Since we now have the connected nodes in a breadth-first list, reverse the list which
	// will give use the same list but topologically sorted in kernel execution order.
	Algo::Reverse(ConnectedNodes.GetData(), ConnectedNodes.Num());

	// Go through all the nodes and check if their state is valid for compilation.
	bool bValidationFailed = false;
	for (FOptimusRoutedConstNode ConnectedNode: ConnectedNodes)
	{
		TOptional<FText> ErrorMessage = ConnectedNode.Node->ValidateForCompile();
		if (ErrorMessage.IsSet())
		{
			bValidationFailed = true;
			AddDiagnostic(EOptimusDiagnosticLevel::Error, *ErrorMessage, ConnectedNode.Node);
		}
	}
	if (bValidationFailed)
	{
		return nullptr;
	}

	const FName GraphName = MakeUniqueObjectName(this, UOptimusComputeGraph::StaticClass(), InNodeGraph->GetFName());
	UOptimusComputeGraph* ComputeGraph = NewObject<UOptimusComputeGraph>(this, GraphName);
	
	// Find all data interface nodes and create their data interfaces.
	FOptimus_NodeToDataInterfaceMap NodeDataInterfaceMap;

	// Find all resource links from one compute kernel directly to another. The pin here is
	// the output pin from a kernel node that connects to another. We don't map from input pins
	// because a resource output may be used multiple times, but only written into once.
	FOptimus_PinToDataInterfaceMap LinkDataInterfaceMap;

	// Find all value nodes (constant and variable) 
	TArray<const UOptimusNode *> ValueNodes; 

	for (FOptimusRoutedConstNode ConnectedNode: ConnectedNodes)
	{
		if (const IOptimusDataInterfaceProvider* DataInterfaceNode = Cast<const IOptimusDataInterfaceProvider>(ConnectedNode.Node))
		{
			UOptimusComputeDataInterface* DataInterface = DataInterfaceNode->GetDataInterface(this);
			if (!DataInterface)
			{
				AddDiagnostic(EOptimusDiagnosticLevel::Error, LOCTEXT("NoDataInterfaceOnProvider", "No data interface object returned from node. Compilation aborted."));
				return nullptr;
			}

			NodeDataInterfaceMap.Add(ConnectedNode.Node, DataInterface);
		}
		else if (Cast<const IOptimusComputeKernelProvider>(ConnectedNode.Node) != nullptr)
		{
			for (const UOptimusNodePin* Pin: ConnectedNode.Node->GetPins())
			{
				if (Pin->GetDirection() == EOptimusNodePinDirection::Output &&
					ensure(!Pin->GetDataDomain().IsSingleton()) &&
					!LinkDataInterfaceMap.Contains(Pin))
				{
					for (const FOptimusRoutedNodePin& ConnectedPin: Pin->GetConnectedPinsWithRouting(ConnectedNode.TraversalContext))
					{
						// Make sure it connects to another kernel node.
						if (Cast<const IOptimusComputeKernelProvider>(ConnectedPin.NodePin->GetOwningNode()) != nullptr &&
							ensure(Pin->GetDataType().IsValid()))
						{
							UOptimusTransientBufferDataInterface* TransientBufferDI =
								NewObject<UOptimusTransientBufferDataInterface>(this);

							TSet<UOptimusComponentSourceBinding*> ComponentSourceBindings = Pin->GetComponentSourceBindings();
							if (ComponentSourceBindings.Num() != 1)
							{
								AddDiagnostic(EOptimusDiagnosticLevel::Error,
									FText::Format(LOCTEXT("InvalidComponentBindingOnKernelPin", "Missing or multiple component bindings on kernel-to-kernel pin ({0}). Compilation aborted."),
										FText::FromName(Pin->GetUniqueName())),
									ConnectedNode.Node);
								return nullptr;
							}

							TransientBufferDI->ValueType = Pin->GetDataType()->ShaderValueType;
							TransientBufferDI->DataDomain = Pin->GetDataDomain();
							TransientBufferDI->ComponentSourceBinding = *ComponentSourceBindings.CreateConstIterator();
							
							LinkDataInterfaceMap.Add(Pin, TransientBufferDI);
						}
					}
				}
			}
		}
		else if (Cast<const IOptimusValueProvider>(ConnectedNode.Node))
		{
			ValueNodes.AddUnique(ConnectedNode.Node);
		}
	}

	// Create the graph data interface and fill it with the value nodes.
	UOptimusGraphDataInterface* GraphDataInterface = NewObject<UOptimusGraphDataInterface>(this);

	TArray<FOptimusGraphVariableDescription> ValueNodeDescriptions;
	ValueNodeDescriptions.Reserve(ValueNodes.Num());
	for (UOptimusNode const* ValueNode : ValueNodes)
	{
		if (IOptimusValueProvider const* ValueProvider = Cast<const IOptimusValueProvider>(ValueNode))
		{
			FOptimusGraphVariableDescription& ValueNodeDescription = ValueNodeDescriptions.AddDefaulted_GetRef();
			ValueNodeDescription.Name = ValueProvider->GetValueName();
			ValueNodeDescription.ValueType = ValueProvider->GetValueType()->ShaderValueType;

			if (UOptimusNode_ConstantValue const* ConstantNode = Cast<const UOptimusNode_ConstantValue>(ValueNode))
			{
				ValueNodeDescription.Value = ConstantNode->GetShaderValue().ShaderValue;
			}
		}
	}
	GraphDataInterface->Init(ValueNodeDescriptions);

	// Loop through all kernels, create a kernel source, and create a compute kernel for it.
	struct FKernelWithDataBindings
	{
		int32 KernelNodeIndex;
		UComputeKernel *Kernel;
		FOptimus_InterfaceBindingMap InputDataBindings;
		FOptimus_InterfaceBindingMap OutputDataBindings;
	};

	// The component binding for the graph data is the primary binding on the deformer.
	UOptimusComponentSourceBinding* GraphDataComponentBinding = Bindings->Bindings[0];
	
	TArray<FKernelWithDataBindings> BoundKernels;
	for (FOptimusRoutedConstNode ConnectedNode: ConnectedNodes)
	{
		if (const IOptimusComputeKernelProvider *KernelProvider = Cast<const IOptimusComputeKernelProvider>(ConnectedNode.Node))
		{
			FKernelWithDataBindings BoundKernel;

			BoundKernel.KernelNodeIndex = InNodeGraph->Nodes.IndexOfByKey(ConnectedNode.Node);
			BoundKernel.Kernel = NewObject<UComputeKernel>(this);

			FOptimus_ComputeKernelResult KernelSourceResult = KernelProvider->CreateComputeKernel(	
				BoundKernel.Kernel, ConnectedNode.TraversalContext,
				NodeDataInterfaceMap, LinkDataInterfaceMap,
				ValueNodes,
				GraphDataInterface, GraphDataComponentBinding,
				BoundKernel.InputDataBindings, BoundKernel.OutputDataBindings
			);
			if (FText* ErrorMessage = KernelSourceResult.TryGet<FText>())
			{
				AddDiagnostic(EOptimusDiagnosticLevel::Error,
					FText::Format(LOCTEXT("CantCreateKernelWithError", "{0}. Compilation aborted."), *ErrorMessage),
					ConnectedNode.Node);
				return nullptr;
			}

			if (BoundKernel.InputDataBindings.IsEmpty() || BoundKernel.OutputDataBindings.IsEmpty())
			{
				AddDiagnostic(EOptimusDiagnosticLevel::Error,
				LOCTEXT("KernelHasNoBindings", "Kernel has either no input or output bindings. Compilation aborted."),
					ConnectedNode.Node);
				return nullptr;
			}
			
			BoundKernel.Kernel->KernelSource = KernelSourceResult.Get<UOptimusKernelSource*>();

			BoundKernels.Add(BoundKernel);

			ComputeGraph->KernelInvocations.Add(BoundKernel.Kernel);
			ComputeGraph->KernelToNode.Add(ConnectedNode.Node);
		}
	}

	// Create a map from the data interfaces to the component bindings.
	// FIXME: Instead of collecting this during compilation, we should do this when we collect data interfaces instead.
	TMap<const UComputeDataInterface*, int32> DataInterfaceToBindingIndexMap;
	for (int32 KernelIndex = 0; KernelIndex < ComputeGraph->KernelInvocations.Num(); KernelIndex++)
	{
		const FKernelWithDataBindings& BoundKernel = BoundKernels[KernelIndex];

		for (const TPair<int32, FOptimus_InterfaceBinding>& DataBinding: BoundKernel.InputDataBindings)
		{
			const FOptimus_InterfaceBinding& InterfaceBinding = DataBinding.Value;
			const UComputeDataInterface* DataInterface = InterfaceBinding.DataInterface;
			const UOptimusComponentSourceBinding* ComponentBinding = InterfaceBinding.ComponentBinding; 

			if (ensure(ComponentBinding))
			{
				int32 BindingIndex = Bindings->Bindings.IndexOfByKey(ComponentBinding);
				if (ensure(BindingIndex != INDEX_NONE))
				{
					if (DataInterfaceToBindingIndexMap.Contains(DataInterface) &&
						DataInterfaceToBindingIndexMap[DataInterface] != BindingIndex)
					{
						UE_LOG(LogOptimusCore, Error, TEXT("Datainterface found with different component bindings?"));
					}
					DataInterfaceToBindingIndexMap.Add(DataInterface, BindingIndex);
				}
			}
		}
		for (const TPair<int32, FOptimus_InterfaceBinding>& DataBinding: BoundKernel.OutputDataBindings)
		{
			const FOptimus_InterfaceBinding& InterfaceBinding = DataBinding.Value;
			const UComputeDataInterface* DataInterface = InterfaceBinding.DataInterface;
			const UOptimusComponentSourceBinding* ComponentBinding = InterfaceBinding.ComponentBinding; 

			if (ensure(ComponentBinding))
			{
				int32 BindingIndex = Bindings->Bindings.IndexOfByKey(ComponentBinding);
				if (ensure(BindingIndex != INDEX_NONE))
				{
					if (DataInterfaceToBindingIndexMap.Contains(DataInterface) &&
						DataInterfaceToBindingIndexMap[DataInterface] != BindingIndex)
					{
						UE_LOG(LogOptimusCore, Error, TEXT("Datainterface found with different component bindings?"));
					}
					DataInterfaceToBindingIndexMap.Add(DataInterface, BindingIndex);
				}
			}
		}
	}
	
	// Create the binding objects.
	for (const UOptimusComponentSourceBinding* Binding: Bindings->Bindings)
	{
		ComputeGraph->Bindings.Add(Binding->GetComponentSource()->GetComponentClass());
	}

	// Now that we've collected all the pieces, time to line them up.
	ComputeGraph->DataInterfaces.Add(GraphDataInterface);
	ComputeGraph->DataInterfaceToBinding.Add(0);		// Graph data interface always uses the primary binding.

	for (TPair<const UOptimusNode*, UOptimusComputeDataInterface*>& Item : NodeDataInterfaceMap)
	{
		ComputeGraph->DataInterfaces.Add(Item.Value);
		ComputeGraph->DataInterfaceToBinding.Add(DataInterfaceToBindingIndexMap[Item.Value]);
	}
	for (TPair<const UOptimusNodePin *, UOptimusComputeDataInterface *>&Item: LinkDataInterfaceMap)
	{
		ComputeGraph->DataInterfaces.Add(Item.Value);
		ComputeGraph->DataInterfaceToBinding.Add(DataInterfaceToBindingIndexMap[Item.Value]);
	}

	// Create the graph edges.
	for (int32 KernelIndex = 0; KernelIndex < ComputeGraph->KernelInvocations.Num(); KernelIndex++)
	{
		const FKernelWithDataBindings& BoundKernel = BoundKernels[KernelIndex];
		const TArray<FShaderFunctionDefinition>& KernelInputs = BoundKernel.Kernel->KernelSource->ExternalInputs;

		// FIXME: Hoist these two loops into a helper function/lambda.
		for (const TPair<int32, FOptimus_InterfaceBinding>& DataBinding: BoundKernel.InputDataBindings)
		{
			const int32 KernelBindingIndex = DataBinding.Key;
			const FOptimus_InterfaceBinding& InterfaceBinding = DataBinding.Value;
			const UComputeDataInterface* DataInterface = InterfaceBinding.DataInterface;
			const int32 DataInterfaceBindingIndex = InterfaceBinding.DataInterfaceBindingIndex;
			const FString BindingFunctionName = InterfaceBinding.BindingFunctionName;
			const FString BindingFunctionNamespace = InterfaceBinding.BindingFunctionNamespace;

			// FIXME: Collect this beforehand.
			TArray<FShaderFunctionDefinition> DataInterfaceFunctions;
			DataInterface->GetSupportedInputs(DataInterfaceFunctions);
			
			if (ensure(KernelInputs.IsValidIndex(KernelBindingIndex)) &&
				ensure(DataInterfaceFunctions.IsValidIndex(DataInterfaceBindingIndex)))
			{
				FComputeGraphEdge GraphEdge;
				GraphEdge.bKernelInput = true;
				GraphEdge.KernelIndex = KernelIndex;
				GraphEdge.KernelBindingIndex = KernelBindingIndex;
				GraphEdge.DataInterfaceIndex = ComputeGraph->DataInterfaces.IndexOfByKey(DataInterface);
				GraphEdge.DataInterfaceBindingIndex = DataInterfaceBindingIndex;
				GraphEdge.BindingFunctionNameOverride = BindingFunctionName;
				GraphEdge.BindingFunctionNamespace = BindingFunctionNamespace;
				ComputeGraph->GraphEdges.Add(GraphEdge);
			}
		}

		const TArray<FShaderFunctionDefinition>& KernelOutputs = BoundKernels[KernelIndex].Kernel->KernelSource->ExternalOutputs;
		for (const TPair<int32, FOptimus_InterfaceBinding>& DataBinding: BoundKernel.OutputDataBindings)
		{
			const int32 KernelBindingIndex = DataBinding.Key;
			const FOptimus_InterfaceBinding& InterfaceBinding = DataBinding.Value;
			const UComputeDataInterface* DataInterface = InterfaceBinding.DataInterface;
			const int32 DataInterfaceBindingIndex = InterfaceBinding.DataInterfaceBindingIndex;
			const FString BindingFunctionName = InterfaceBinding.BindingFunctionName;
			const FString BindingFunctionNamespace = InterfaceBinding.BindingFunctionNamespace;

			// FIXME: Collect this beforehand.
			TArray<FShaderFunctionDefinition> DataInterfaceFunctions;
			DataInterface->GetSupportedOutputs(DataInterfaceFunctions);
			
			if (ensure(KernelOutputs.IsValidIndex(KernelBindingIndex)) &&
				ensure(DataInterfaceFunctions.IsValidIndex(DataInterfaceBindingIndex)))
			{
				FComputeGraphEdge GraphEdge;
				GraphEdge.bKernelInput = false;
				GraphEdge.KernelIndex = KernelIndex;
				GraphEdge.KernelBindingIndex = KernelBindingIndex;
				GraphEdge.DataInterfaceIndex = ComputeGraph->DataInterfaces.IndexOfByKey(DataInterface);
				GraphEdge.DataInterfaceBindingIndex = DataInterfaceBindingIndex;
				GraphEdge.BindingFunctionNameOverride = BindingFunctionName;
				GraphEdge.BindingFunctionNamespace = BindingFunctionNamespace;
				ComputeGraph->GraphEdges.Add(GraphEdge);
			}
		}
	}

#if PRINT_COMPILED_OUTPUT
	
#endif

	return ComputeGraph;
}

void UOptimusDeformer::OnDataTypeChanged(FName InTypeName)
{
	// Currently only value containers depends on the UDSs,
	UOptimusValueContainerGeneratorClass::RefreshClassForType(GetPackage(), FOptimusDataTypeRegistry::Get().FindType(InTypeName));
	
	for (UOptimusNodeGraph* Graph : Graphs)
	{
		for (UOptimusNode* Node: Graph->Nodes)
		{
			Node->OnDataTypeChanged(InTypeName);
		}
	}

	//TODO: Recreate variables/Resources that uses this type

	// Once we updated the deformer instance, we need to make sure the editor is aware as well
	Notify(EOptimusGlobalNotifyType::DataTypeChanged, nullptr);
}

template<typename Allocator>
static void StringViewSplit(
	TArray<FStringView, Allocator> &OutResult, 
	const FStringView InString,
	const TCHAR* InDelimiter,
	int32 InMaxSplit = INT32_MAX
	)
{
	if (!InDelimiter)
	{
		OutResult.Add(InString);
		return;
	}
	
	const int32 DelimiterLength = FCString::Strlen(InDelimiter); 
	if (DelimiterLength == 0)
	{
		OutResult.Add(InString);
		return;
	}

	InMaxSplit = FMath::Max(0, InMaxSplit);

	int32 StartIndex = 0;
	for (;;)
	{
		const int32 FoundIndex = (InMaxSplit--) ? InString.Find(InDelimiter, StartIndex) : INDEX_NONE;
		if (FoundIndex == INDEX_NONE)
		{
			OutResult.Add(InString.SubStr(StartIndex, INT32_MAX));
			break;
		}

		OutResult.Add(InString.SubStr(StartIndex, FoundIndex - StartIndex));
		StartIndex = FoundIndex + DelimiterLength;
	}
}


UOptimusNodeGraph* UOptimusDeformer::ResolveGraphPath(
	const FStringView InPath,
	FStringView& OutRemainingPath
	) const
{
	TArray<FStringView, TInlineAllocator<4>> Path;
	StringViewSplit<TInlineAllocator<4>>(Path, InPath, TEXT("/"));

	if (Path.Num() == 0)
	{
		return nullptr;
	}

	UOptimusNodeGraph* Graph = nullptr;
	if (Path[0] == UOptimusNodeGraph::LibraryRoot)
	{
		// FIXME: Search the library graphs.
	}
	else
	{
		for (UOptimusNodeGraph* RootGraph : Graphs)
		{
			if (Path[0].Equals(RootGraph->GetName(), ESearchCase::IgnoreCase))
			{
				Graph = RootGraph;
				break;
			}
		}
	}

	if (!Graph)
	{
		return nullptr;
	}

	// See if we need to traverse any sub-graphs
	int32 GraphIndex = 1;
	for (; GraphIndex < Path.Num(); GraphIndex++)
	{
		bool bFoundSubGraph = false;
		for (UOptimusNodeGraph* SubGraph: Graph->GetGraphs())
		{
			if (Path[GraphIndex].Equals(SubGraph->GetName(), ESearchCase::IgnoreCase))
			{
				Graph = SubGraph;
				bFoundSubGraph = true;
				break;
			}
		}
		if (!bFoundSubGraph)
		{
			break;
		}
	}

	if (GraphIndex < Path.Num())
	{
		OutRemainingPath = FStringView(
			Path[GraphIndex].GetData(),
			static_cast<int32>(Path.Last().GetData() - Path[GraphIndex].GetData()) + Path.Last().Len());
	}
	else
	{
		OutRemainingPath.Reset();
	}

	return Graph;
}


UOptimusNode* UOptimusDeformer::ResolveNodePath(
	const FStringView InPath,
	FStringView& OutRemainingPath
	) const
{
	FStringView NodePath;

	UOptimusNodeGraph* Graph = ResolveGraphPath(InPath, NodePath);
	if (!Graph || NodePath.IsEmpty())
	{
		return nullptr;
	}

	// We only want at most 2 elements (single split)
	TArray<FStringView, TInlineAllocator<2>> Path;
	StringViewSplit(Path, NodePath, TEXT("."), 1);
	if (Path.IsEmpty())
	{
		return nullptr;
	}

	const FStringView NodeName = Path[0];
	for (UOptimusNode* Node : Graph->GetAllNodes())
	{
		if (Node != nullptr && NodeName.Equals(Node->GetName(), ESearchCase::IgnoreCase))
		{
			OutRemainingPath = Path.Num() == 2 ? Path[1] : FStringView();
			return Node;
		}
	}

	return nullptr;
}


void UOptimusDeformer::Notify(EOptimusGlobalNotifyType InNotifyType, UObject* InObject) const
{
	switch (InNotifyType)
	{
	case EOptimusGlobalNotifyType::GraphAdded: 
	case EOptimusGlobalNotifyType::GraphRemoved:
	case EOptimusGlobalNotifyType::GraphIndexChanged:
	case EOptimusGlobalNotifyType::GraphRenamed:
		checkSlow(Cast<UOptimusNodeGraph>(InObject) != nullptr);
		break;

	case EOptimusGlobalNotifyType::ComponentBindingAdded:
	case EOptimusGlobalNotifyType::ComponentBindingRemoved:
	case EOptimusGlobalNotifyType::ComponentBindingIndexChanged:
	case EOptimusGlobalNotifyType::ComponentBindingRenamed:
	case EOptimusGlobalNotifyType::ComponentBindingSourceChanged:
		checkSlow(Cast<UOptimusComponentSourceBinding>(InObject) != nullptr);
		break;
		
	case EOptimusGlobalNotifyType::ResourceAdded:
	case EOptimusGlobalNotifyType::ResourceRemoved:
	case EOptimusGlobalNotifyType::ResourceIndexChanged:
	case EOptimusGlobalNotifyType::ResourceRenamed:
	case EOptimusGlobalNotifyType::ResourceTypeChanged:
	case EOptimusGlobalNotifyType::ResourceDomainChanged:
		checkSlow(Cast<UOptimusResourceDescription>(InObject) != nullptr);
		break;

	case EOptimusGlobalNotifyType::VariableAdded:
	case EOptimusGlobalNotifyType::VariableRemoved:
	case EOptimusGlobalNotifyType::VariableIndexChanged:
	case EOptimusGlobalNotifyType::VariableRenamed:
	case EOptimusGlobalNotifyType::VariableTypeChanged:
		checkSlow(Cast<UOptimusVariableDescription>(InObject) != nullptr);
		break;

	case EOptimusGlobalNotifyType::ConstantValueChanged:
		if (UOptimusNode_ConstantValue* ConstantValue = Cast<UOptimusNode_ConstantValue>(InObject))
		{
			ConstantValueUpdateDelegate.Broadcast(ConstantValue->GetValueName(), ConstantValue->GetShaderValue().ShaderValue);
		}
		break;
	default:
		checkfSlow(false, TEXT("Unchecked EOptimusGlobalNotifyType!"));
		break;
	}

	GlobalNotifyDelegate.Broadcast(InNotifyType, InObject);
}

void UOptimusDeformer::SetAllInstancesCanbeActive(bool bInCanBeActive) const
{
	SetAllInstancesCanbeActiveDelegate.Broadcast(bInCanBeActive);
}

void UOptimusDeformer::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	// Mark with a custom version. This has the nice side-benefit of making the asset indexer
	// skip this object if the plugin is not loaded.
	Ar.UsingCustomVersion(FOptimusObjectVersion::GUID);

	// UComputeGraph stored the number of kernels separately, we need to skip over it or the
	// stream is out of sync.
	if (Ar.CustomVer(FOptimusObjectVersion::GUID) < FOptimusObjectVersion::SwitchToMeshDeformerBase)
	{
		int32 NumKernels = 0;
		Ar << NumKernels;
		for (int32 Index = 0; Index < NumKernels; Index++)
		{
			int32 NumResources = 0;
			Ar << NumResources;

			// If this turns out to be not zero in some asset, we have to add in the entirety
			// of FComputeKernelResource::SerializeShaderMap
			check(NumResources == 0); 
		}
	}
}


void UOptimusDeformer::PostLoad()
{
	Super::PostLoad();

	// Fixup any empty array entries.
	Resources->Descriptions.RemoveAllSwap([](const TObjectPtr<UOptimusResourceDescription>& Value) { return Value == nullptr; });
	Variables->Descriptions.RemoveAllSwap([](const TObjectPtr<UOptimusVariableDescription>& Value) { return Value == nullptr; });

	// Fixup any class objects with invalid parents.
	TArray<UObject*> Objects;
	GetObjectsWithOuter(this, Objects, false);

	for (UObject* Object : Objects)
	{
		if (UClass* ClassObject = Cast<UClass>(Object))
		{
			Optimus::RenameObject(ClassObject, nullptr, GetPackage());
		}
	}
	
	if (GetLinkerCustomVersion(FOptimusObjectVersion::GUID) < FOptimusObjectVersion::ReparentResourcesAndVariables)
	{
		// Move any resource or variable descriptor owned by this deformer to their own container.
		// This is to fix a bug where variables/resources were put in their respective container
		// but directly owned by the deformer. This would cause hidden rename issues when trying to
		// rename a variable/graph/resource to the same name.
		for (UObject* ResourceDescription: Resources->Descriptions)
		{
			if (ResourceDescription->GetOuter() != Resources)
			{
				ResourceDescription->Rename(nullptr, Resources);
			}
		}
		for (UObject* VariableDescription: Variables->Descriptions)
		{
			if (VariableDescription->GetOuter() != Variables)
			{
				VariableDescription->Rename(nullptr, Variables);
			}
		}
	}
	if (GetLinkerCustomVersion(FOptimusObjectVersion::GUID) < FOptimusObjectVersion::ComponentProviderSupport)
	{
		if (ensure(Bindings->Bindings.IsEmpty()))
		{
			// Create a default skeletal mesh binding. This is always created for skeletal mesh deformers.
			UOptimusComponentSource* ComponentSource = UOptimusSkeletalMeshComponentSource::StaticClass()->GetDefaultObject<UOptimusComponentSource>();
			UOptimusComponentSourceBinding* Binding = CreateComponentBindingDirect(ComponentSource, ComponentSource->GetBindingName());
			Binding->bIsPrimaryBinding = true;
			Bindings->Bindings.Add(Binding);

			(void)MarkPackageDirty();
		}

		// Fix up any data providers to ensure they have a binding.
		PostLoadFixupMissingComponentBindingsCompat();
	}
	if (GetLinkerCustomVersion(FOptimusObjectVersion::GUID) < FOptimusObjectVersion::SetPrimaryBindingName)
	{
		FName PrimaryBindingName = UOptimusComponentSourceBinding::GetPrimaryBindingName();
		for (UOptimusComponentSourceBinding* Binding : Bindings->Bindings)
		{
			if (Binding->bIsPrimaryBinding)
			{
				Binding->Rename(*PrimaryBindingName.ToString(), nullptr, REN_ForceNoResetLoaders | REN_NonTransactional);
				Binding->BindingName = PrimaryBindingName;
			}
		}
		TArray<UOptimusNode*> AllComponentSourceNode = GetAllNodesOfClass(UOptimusNode_ComponentSource::StaticClass());
		for (UOptimusNode* Node : AllComponentSourceNode)
		{
			if (UOptimusNode_ComponentSource* ComponentSourceNode = Cast<UOptimusNode_ComponentSource>(Node))
			{
				if (ComponentSourceNode->GetComponentBinding()->IsPrimaryBinding())
				{
					ComponentSourceNode->SetDisplayName(FText::FromName(PrimaryBindingName));
				}
			}
		}
	}
	// Fix any resource data domains if the component binding is valid but the domain is not. This will mostly cut links
	// to kernels with mismatched domain info.
	if (GetLinkerCustomVersion(FOptimusObjectVersion::GUID) < FOptimusObjectVersion::DataDomainExpansion)
	{
		PostLoadFixupMismatchedResourceDataDomains();
	}
}


void UOptimusDeformer::PostLoadFixupMissingComponentBindingsCompat()
{
	for (UOptimusNodeGraph* Graph: GetGraphs())
	{
		if (!Optimus::IsExecutionGraphType(Graph->GetGraphType()))
		{
			continue;
		}

		FVector2D::FReal MinimumPosX = std::numeric_limits<FVector2D::FReal>::max(), AccumulatedPosY = 0;

		TMap<UOptimusNode_DataInterface*, UOptimusComponentSourceBinding*> InterfaceBindingMap;  
		
		for (UOptimusNode* Node: Graph->GetAllNodes())
		{
			MinimumPosX = FMath::Min(MinimumPosX, Node->GetGraphPosition().X);
			AccumulatedPosY += Node->GetGraphPosition().Y;

			if (UOptimusNode_DataInterface* DataInterfaceNode = Cast<UOptimusNode_DataInterface>(Node))
			{
				// Do we have a compatible binding?
				if (ensure(DataInterfaceNode->DataInterfaceClass))
				{
					const UOptimusComputeDataInterface* DataInterface = DataInterfaceNode->DataInterfaceClass->GetDefaultObject<UOptimusComputeDataInterface>();
					UOptimusComponentSourceBinding* Binding = FindCompatibleBindingWithInterface(DataInterface);
					if (!Binding)
					{
						if (const UOptimusComponentSource* ComponentSource = UOptimusComponentSource::GetSourceFromDataInterface(DataInterface))
						{
							Binding = AddComponentBinding(ComponentSource);
						}
					}

					if (Binding)
					{
						InterfaceBindingMap.Add(DataInterfaceNode, Binding);

						// Make sure the component input pin has been created.
						DataInterfaceNode->ConditionalPostLoad();
					}
				}
			}
		}

		if (!InterfaceBindingMap.IsEmpty())
		{
			static const FVector2D NodeSize(160.0, 40.0); 
			static const FVector2D NodeMargins(40.0, 20.0); 
			
			// Create component source nodes with the requested binding and connect them to the data interface nodes. 
			TMap<UOptimusComponentSourceBinding*, UOptimusNode_ComponentSource*> BindingToNodeMap;
			for (const TPair<UOptimusNode_DataInterface*, UOptimusComponentSourceBinding*>& Item: InterfaceBindingMap)
			{
				BindingToNodeMap.Add(Item.Value, nullptr);
			}

			MinimumPosX -= NodeSize.X + NodeMargins.X;
			AccumulatedPosY /= Graph->GetAllNodes().Num();
			AccumulatedPosY -= NodeSize.Y * 0.5 + (BindingToNodeMap.Num() - 1) * (NodeSize.Y + NodeMargins.Y);
			
			for (const TPair<UOptimusNode_DataInterface*, UOptimusComponentSourceBinding*>& Item: InterfaceBindingMap)
			{
				UOptimusNode_ComponentSource*& ComponentSourceNodePtr = BindingToNodeMap.FindChecked(Item.Value);
				
				if (ComponentSourceNodePtr == nullptr)
				{
					ComponentSourceNodePtr = Cast<UOptimusNode_ComponentSource>(Graph->AddComponentBindingGetNode(Item.Value, FVector2D(MinimumPosX, AccumulatedPosY)));
					AccumulatedPosY += NodeSize.Y + NodeMargins.Y;
				}

				if (!ensure(ComponentSourceNodePtr))
				{
					continue;
				}
				
				UOptimusNodePin* ComponentSourcePin = ComponentSourceNodePtr->GetComponentPin();

				Graph->AddLink(ComponentSourcePin, Item.Key->GetComponentPin());
			}
		}
	}
	(void)MarkPackageDirty();
}


void UOptimusDeformer::PostLoadFixupMismatchedResourceDataDomains()
{
#if 0
	TArray<UOptimusNode*> AllResourceNodes = GetAllNodesOfClass(UOptimusNode_ResourceAccessorBase::StaticClass());
	
	for (UOptimusResourceDescription* ResourceDescription: Resources->Descriptions)
	{
		if (ResourceDescription->IsValidComponentBinding() && ResourceDescription->DataDomain.IsSingleton())
		{
			
			// SetResourceDataDomain(ResourceDescription, ResourceDescription->GetDataDomainFromComponentBinding(), true);
		}
	}
#endif
}


UOptimusComponentSourceBinding* UOptimusDeformer::FindCompatibleBindingWithInterface(
	const UOptimusComputeDataInterface* InDataInterface) const
{
	if (!ensure(InDataInterface))
	{
		return nullptr;
	}
	
	for (UOptimusComponentSourceBinding* Binding: GetComponentBindings())
	{
		if (!ensure(Binding->GetComponentSource()))
		{
			continue;
		}

		// If the binding comp class is the same or a sub-class of the interface comp class, then
		// they're compatible (e.g. if the interface requires only USceneComponent but the binding class
		// is a USkinnedMeshComponent, then the USkinnedMeshComponent will suffice).
		const UClass* BindingComponentClass = Binding->GetComponentSource()->GetComponentClass();
		const UClass* InterfaceComponentClass = InDataInterface->GetRequiredComponentClass();
		if (BindingComponentClass->IsChildOf(InterfaceComponentClass))
		{
			return Binding;
		}
	}

	return nullptr;
}

void UOptimusDeformer::BeginDestroy()
{
	Super::BeginDestroy();

	FOptimusDataTypeRegistry::Get().GetOnDataTypeChanged().RemoveAll(this);
}

void UOptimusDeformer::PostRename(UObject* OldOuter, const FName OldName)
{
	Super::PostRename(OldOuter, OldName);

	// Whenever the asset is renamed/moved, generated classes parented to the old package
	// are not moved to the new package automatically (see FAssetRenameManager), so we
	// have to manually perform the move/rename, to avoid invalid reference to the old package
	TArray<UClass*> ClassObjects = Optimus::GetClassObjectsInPackage(OldOuter->GetPackage());

	for (UClass* ClassObject : ClassObjects)
	{
		Optimus::RenameObject(ClassObject, nullptr, GetPackage());
	}
}

#if WITH_EDITOR
void UOptimusDeformer::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
}
#endif


UMeshDeformerInstanceSettings* UOptimusDeformer::CreateSettingsInstance(UMeshComponent* InMeshComponent)
{
	const FName SettingsName(GetName() + TEXT("_Settings"));
	const EObjectFlags CreateObjectFlags = InMeshComponent->HasAnyFlags(RF_ArchetypeObject) ? RF_Public : RF_NoFlags; // Make public when stored in a BP.
	UOptimusDeformerInstanceSettings *Settings = NewObject<UOptimusDeformerInstanceSettings>(InMeshComponent, SettingsName, CreateObjectFlags);
	Settings->InitializeSettings(this, InMeshComponent);
	return Settings;
}


UMeshDeformerInstance* UOptimusDeformer::CreateInstance(
	UMeshComponent* InMeshComponent,
	UMeshDeformerInstanceSettings* InSettings
	)
{
	if (InMeshComponent == nullptr)
	{
		return nullptr;
	}

	const FName InstanceName(GetName() + TEXT("_Instance"));
	UOptimusDeformerInstance* Instance = NewObject<UOptimusDeformerInstance>(InMeshComponent, InstanceName);
	Instance->SetMeshComponent(InMeshComponent);
	Instance->SetInstanceSettings(Cast<UOptimusDeformerInstanceSettings>(InSettings));
	Instance->SetupFromDeformer(this);

	// Make sure all the instances know when we finish compiling so they can update their local state to match.
	CompileEndDelegate.AddUObject(Instance, &UOptimusDeformerInstance::SetupFromDeformer);
	ConstantValueUpdateDelegate.AddUObject(Instance, &UOptimusDeformerInstance::SetConstantValueDirect);
	SetAllInstancesCanbeActiveDelegate.AddUObject(Instance, &UOptimusDeformerInstance::SetCanBeActive);

	return Instance;
}


void UOptimusDeformer::SetPreviewMesh(USkeletalMesh* PreviewMesh, bool bMarkAsDirty)
{
	Mesh = PreviewMesh;
	
	// FIXME: Notify upstream so the viewport can react.
}


USkeletalMesh* UOptimusDeformer::GetPreviewMesh() const
{
	return Mesh;
}


IOptimusNodeGraphCollectionOwner* UOptimusDeformer::ResolveCollectionPath(const FString& InPath)
{
	if (InPath.IsEmpty())
	{
		return this;
	}

	return Cast<IOptimusNodeGraphCollectionOwner>(ResolveGraphPath(InPath));
}


UOptimusNodeGraph* UOptimusDeformer::ResolveGraphPath(const FString& InGraphPath)
{
	FStringView PathRemainder;

	UOptimusNodeGraph* Graph = ResolveGraphPath(InGraphPath, PathRemainder);

	// The graph is only valid if the path was fully consumed.
	return PathRemainder.IsEmpty() ? Graph : nullptr;
}


UOptimusNode* UOptimusDeformer::ResolveNodePath(const FString& InNodePath)
{
	FStringView PathRemainder;

	UOptimusNode* Node = ResolveNodePath(InNodePath, PathRemainder);

	// The graph is only valid if the path was fully consumed.
	return PathRemainder.IsEmpty() ? Node : nullptr;
}


UOptimusNodePin* UOptimusDeformer::ResolvePinPath(const FString& InPinPath)
{
	FStringView PinPath;

	UOptimusNode* Node = ResolveNodePath(InPinPath, PinPath);

	return Node ? Node->FindPin(PinPath) : nullptr;
}



UOptimusNodeGraph* UOptimusDeformer::CreateGraph(
	EOptimusNodeGraphType InType, 
	FName InName, 
	TOptional<int32> InInsertBefore
	)
{
	// Update graphs is a singleton and is created by default. Transient graphs are only used
	// when duplicating nodes and should never exist as a part of a collection. 
	if (InType == EOptimusNodeGraphType::Update ||
		InType == EOptimusNodeGraphType::Transient)
	{
		return nullptr;
	}

	UClass* GraphClass = UOptimusNodeGraph::StaticClass();
	
	if (InType == EOptimusNodeGraphType::Setup)
	{
		// Do we already have a setup graph?
		if (Graphs.Num() > 1 && Graphs[0]->GetGraphType() == EOptimusNodeGraphType::Setup)
		{
			return nullptr;
		}

		// The name of the setup graph is fixed.
		InName = UOptimusNodeGraph::SetupGraphName;
	}
	else if (InType == EOptimusNodeGraphType::ExternalTrigger)
	{
		if (!UOptimusNodeGraph::IsValidUserGraphName(InName.ToString()))
		{
			return nullptr;
		}

		// If there's already an object with this name, then attempt to make the name unique.
		InName = Optimus::GetUniqueNameForScope(this, InName);
	}
	else if (InType == EOptimusNodeGraphType::Function)
	{
		// Not fully implemented yet.
		checkNoEntry();
		GraphClass = UOptimusFunctionNodeGraph::StaticClass();
	}

	UOptimusNodeGraph* Graph = NewObject<UOptimusNodeGraph>(this, GraphClass, InName, RF_Transactional);

	Graph->SetGraphType(InType);

	if (InInsertBefore.IsSet())
	{
		if (!AddGraph(Graph, InInsertBefore.GetValue()))
		{
			Graph->Rename(nullptr, GetTransientPackage());
			return nullptr;
		}
	}
	
	return Graph;
}


bool UOptimusDeformer::AddGraph(
	UOptimusNodeGraph* InGraph,
	int32 InInsertBefore
	)
{
	if (InGraph == nullptr || InGraph->GetOuter() != this)
	{
		return false;
	}

	const bool bHaveSetupGraph = (Graphs.Num() > 1 && Graphs[0]->GetGraphType() == EOptimusNodeGraphType::Setup);

	// If INDEX_NONE, insert at the end.
	if (InInsertBefore == INDEX_NONE)
	{
		InInsertBefore = Graphs.Num();
	}
		
	switch (InGraph->GetGraphType())
	{
	case EOptimusNodeGraphType::Update:
		// We cannot replace the update graph.
		return false;
		
	case EOptimusNodeGraphType::Setup:
		// Do we already have a setup graph?
		if (bHaveSetupGraph)
		{
			return false;
		}
		// The setup graph is always first, if present.
		InInsertBefore = 0;
		break;
		
	case EOptimusNodeGraphType::ExternalTrigger:
		// Trigger graphs are always sandwiched between setup and update.
		InInsertBefore = FMath::Clamp(InInsertBefore, bHaveSetupGraph ? 1 : 0, GetUpdateGraphIndex());
		break;

	case EOptimusNodeGraphType::Function:
		// Function graphs always go last.
		InInsertBefore = Graphs.Num();
		break;

	case EOptimusNodeGraphType::SubGraph:
		// We cannot add subgraphs to the root.
		return false;

	case EOptimusNodeGraphType::Transient:
		checkNoEntry();
		return false;
	}
	
	Graphs.Insert(InGraph, InInsertBefore);

	Notify(EOptimusGlobalNotifyType::GraphAdded, InGraph);

	return true;
}


bool UOptimusDeformer::RemoveGraph(
	UOptimusNodeGraph* InGraph,
	bool bInDeleteGraph
	)
{
	// Not ours?
	const int32 GraphIndex = Graphs.IndexOfByKey(InGraph);
	if (GraphIndex == INDEX_NONE)
	{
		return false;
	}

	if (InGraph->GetGraphType() == EOptimusNodeGraphType::Update)
	{
		return false;
	}

	Graphs.RemoveAt(GraphIndex);

	Notify(EOptimusGlobalNotifyType::GraphRemoved, InGraph);

	if (bInDeleteGraph)
	{
		// Un-parent this graph to a temporary storage and mark it for kill.
		InGraph->Rename(nullptr, GetTransientPackage());
	}

	return true;
}



bool UOptimusDeformer::MoveGraph(
	UOptimusNodeGraph* InGraph, 
	int32 InInsertBefore
	)
{
	const int32 GraphOldIndex = Graphs.IndexOfByKey(InGraph);
	if (GraphOldIndex == INDEX_NONE)
	{
		return false;
	}

	if (InGraph->GetGraphType() != EOptimusNodeGraphType::ExternalTrigger)
	{
		return false;
	}

	// Less than num graphs, because the index is based on the node being moved not being
	// in the list.
	if (InInsertBefore == INDEX_NONE)
	{
		InInsertBefore = GetUpdateGraphIndex();
	}
	else
	{
		const bool bHaveSetupGraph = (Graphs.Num() > 1 && Graphs[0]->GetGraphType() == EOptimusNodeGraphType::Setup);
		InInsertBefore = FMath::Clamp(InInsertBefore, bHaveSetupGraph ? 1 : 0, GetUpdateGraphIndex());
	}

	if (GraphOldIndex == InInsertBefore)
	{
		return true;
	}

	Graphs.RemoveAt(GraphOldIndex);
	Graphs.Insert(InGraph, InInsertBefore);

	Notify(EOptimusGlobalNotifyType::GraphIndexChanged, InGraph);

	return true;
}


bool UOptimusDeformer::RenameGraph(UOptimusNodeGraph* InGraph, const FString& InNewName)
{
	// Not ours?
	int32 GraphIndex = Graphs.IndexOfByKey(InGraph);
	if (GraphIndex == INDEX_NONE)
	{
		return false;
	}

	// Setup and Update graphs cannot be renamed.
	if (InGraph->GetGraphType() == EOptimusNodeGraphType::Setup ||
		InGraph->GetGraphType() == EOptimusNodeGraphType::Update)
	{
		return false;
	}

	if (!UOptimusNodeGraph::IsValidUserGraphName(InNewName))
	{
		return false;
	}

	const bool bSuccess = GetActionStack()->RunAction<FOptimusNodeGraphAction_RenameGraph>(InGraph, FName(*InNewName));
	if (bSuccess)
	{
		Notify(EOptimusGlobalNotifyType::GraphRenamed, InGraph);
	}
	return bSuccess;
}


int32 UOptimusDeformer::GetUpdateGraphIndex() const
{
	if (const UOptimusNodeGraph* UpdateGraph = GetUpdateGraph(); ensure(UpdateGraph != nullptr))
	{
		return UpdateGraph->GetGraphIndex();
	}

	return INDEX_NONE;
}


#undef LOCTEXT_NAMESPACE
