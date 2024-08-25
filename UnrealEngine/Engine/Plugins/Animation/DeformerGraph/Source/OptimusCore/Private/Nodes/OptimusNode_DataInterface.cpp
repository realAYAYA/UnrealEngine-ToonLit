// Copyright Epic Games, Inc. All Rights Reserved.

#include "Nodes/OptimusNode_DataInterface.h"

#include "OptimusCoreModule.h"
#include "OptimusNodePin.h"
#include "OptimusDataTypeRegistry.h"
#include "OptimusDeformer.h"
#include "OptimusNodeGraph.h"
#include "OptimusNodeSubGraph.h"
#include "OptimusNode_ComponentSource.h"
#include "OptimusObjectVersion.h"

#include "ComputeFramework/ShaderParamTypeDefinition.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(OptimusNode_DataInterface)


#define LOCTEXT_NAMESPACE "OptimusNode_DataInterface"


UOptimusNode_DataInterface::UOptimusNode_DataInterface()
{
}


bool UOptimusNode_DataInterface::ValidateConnection(
	const UOptimusNodePin& InThisNodesPin,
	const UOptimusNodePin& InOtherNodesPin,
	FString* OutReason
	) const
{
	// FIXME: Once we have connection evaluation, use that.
	if (!GetPins().IsEmpty() && &InThisNodesPin == GetPins()[0])
	{
		const UOptimusNode_ComponentSource* SourceNode = Cast<UOptimusNode_ComponentSource>(InOtherNodesPin.GetOwningNode());
		if (SourceNode)
		{
			const UOptimusComponentSource* ComponentSource = SourceNode->GetComponentBinding()->GetComponentSource();
			if (!IsComponentSourceCompatible(ComponentSource))
			{
				if (OutReason)
				{
					*OutReason = FString::Printf(TEXT("This data interface requires a %s which is not a child class of %s from the Component Source."),
						*DataInterfaceData->GetRequiredComponentClass()->GetName(),
						*ComponentSource->GetComponentClass()->GetName());
				}
				return false;
			}	
		}

		// In other cases, the component source may come from the upstream of the connected node (eg. Sub Graph Terminal),
		// and thus no way to provide error check until compile time
	}

	return true;
}

TOptional<FText> UOptimusNode_DataInterface::ValidateForCompile(const FOptimusPinTraversalContext& InContext) const
{
	if (!DataInterfaceClass)
	{
		return LOCTEXT("NoAssociatedClass", "Node has none or invalid data interface class associated with it. Delete and re-create the node.");
	}
	// Ensure that we have something connected to the component binding input pin.
	UOptimusComponentSourceBinding* PrimaryBinding = GetComponentBinding(InContext);
	if (PrimaryBinding == nullptr)
	{
		return FText::Format(LOCTEXT("NoBindingConnected", "No component binding connected to the {0} pin"), FText::FromName(GetComponentPin()->GetUniqueName()));
	}

	// Are all the other connected _input_ pins using the same binding?
	const UOptimusNodeGraph* Graph = GetOwningGraph();
	for (const UOptimusNodePin* Pin: GetPins())
	{
		if (Pin->GetDirection() == EOptimusNodePinDirection::Input && Pin != GetComponentPin())
		{
			TSet<UOptimusComponentSourceBinding*> Bindings = Graph->GetComponentSourceBindingsForPin(Pin, InContext);
			if (Bindings.Num() > 1)
			{
				return FText::Format(LOCTEXT("MultipleBindingsOnPin", "Multiple bindings found for pin {0}"), FText::FromName(Pin->GetUniqueName()));
			}

			if (Bindings.Num() == 1 && !Bindings.Contains(PrimaryBinding))
			{
				return FText::Format(LOCTEXT("IncompatibleBinding", "Bindings for pin {0} are not the same as for the {1} pin"), 
							FText::FromName(Pin->GetUniqueName()), FText::FromName(GetComponentPin()->GetUniqueName()));
			}
		}
	}
	
	return {};
}

void UOptimusNode_DataInterface::SaveState(FArchive& Ar) const
{
	Super::SaveState(Ar);
	// This fella does the heavy lifting of serializing object references. 
	// FMemoryWriter and fam do not handle UObject* serialization on their own.
	FObjectAndNameAsStringProxyArchive NodeProxyArchive(
			Ar, /* bInLoadIfFindFails=*/ false);
	DataInterfaceData->SerializeScriptProperties(NodeProxyArchive);	
}

void UOptimusNode_DataInterface::RestoreState(FArchive& Ar)
{
	Super::RestoreState(Ar);

	DataInterfaceData = NewObject<UOptimusComputeDataInterface>(this, DataInterfaceClass);
	
	FObjectAndNameAsStringProxyArchive NodeProxyArchive(
			Ar, /* bInLoadIfFindFails=*/true);
	DataInterfaceData->SerializeScriptProperties(NodeProxyArchive);
}

bool UOptimusNode_DataInterface::IsComponentSourceCompatible(const UOptimusComponentSource* InComponentSource) const
{
	return InComponentSource && InComponentSource->GetComponentClass()->IsChildOf(DataInterfaceData->GetRequiredComponentClass());
}

void UOptimusNode_DataInterface::Serialize(FArchive& Ar)
{
 	Super::Serialize(Ar);
	Ar.UsingCustomVersion(FOptimusObjectVersion::GUID);
}


UOptimusComputeDataInterface* UOptimusNode_DataInterface::GetDataInterface(UObject* InOuter) const
{
	// Asset is probably broken, or refers to a class that no longer exists. 
	if (!DataInterfaceClass)
	{
		return nullptr;
	}
	
	// Legacy data may not have a DataInterfaceData object.
	if (DataInterfaceData == nullptr || !DataInterfaceData->IsA(DataInterfaceClass))
	{
		return NewObject<UOptimusComputeDataInterface>(InOuter, DataInterfaceClass);
	}

	FObjectDuplicationParameters DupParams = InitStaticDuplicateObjectParams(DataInterfaceData, InOuter);
	return Cast<UOptimusComputeDataInterface>(StaticDuplicateObjectEx(DupParams));
}


int32 UOptimusNode_DataInterface::GetDataFunctionIndexFromPin(const UOptimusNodePin* InPin) const
{
	if (!InPin || InPin->GetParentPin() != nullptr)
	{
		return INDEX_NONE;
	}

	// FIXME: This information should be baked into the pin definition so we don't have to
	// look it up repeatedly.
	const TArray<FOptimusCDIPinDefinition> PinDefinitions = DataInterfaceData->GetPinDefinitions();

	int32 PinIndex = INDEX_NONE;
	for (int32 Index = 0 ; Index < PinDefinitions.Num(); ++Index)
	{
		if (InPin->GetUniqueName() == PinDefinitions[Index].PinName)
		{
			PinIndex = Index;
			break;
		}
	}
	if (!ensure(PinIndex != INDEX_NONE))
	{
		return INDEX_NONE;
	}

	const FString FunctionName = PinDefinitions[PinIndex].DataFunctionName;
	
	TArray<FShaderFunctionDefinition> FunctionDefinitions;
	if (InPin->GetDirection() == EOptimusNodePinDirection::Input)
	{
		DataInterfaceData->GetSupportedOutputs(FunctionDefinitions);
	}
	else
	{
		DataInterfaceData->GetSupportedInputs(FunctionDefinitions);
	}
	
	return FunctionDefinitions.IndexOfByPredicate(
		[FunctionName](const FShaderFunctionDefinition& InDef)
		{
			return InDef.Name == FunctionName;
		});
}


void UOptimusNode_DataInterface::SetDataInterfaceClass(
	TSubclassOf<UOptimusComputeDataInterface> InDataInterfaceClass
	)
{
	DataInterfaceClass = InDataInterfaceClass;
	DataInterfaceData = NewObject<UOptimusComputeDataInterface>(this, DataInterfaceClass);
}

UOptimusComponentSourceBinding* UOptimusNode_DataInterface::GetComponentBinding(const FOptimusPinTraversalContext& InContext) const
{
	const UOptimusNodeGraph* Graph = GetOwningGraph();
	TSet<UOptimusComponentSourceBinding*> Bindings = Graph->GetComponentSourceBindingsForPin(GetComponentPin(), InContext);
	
	if (!Bindings.IsEmpty() && ensure(Bindings.Num() == 1))
	{
		return Bindings.Array()[0];
	}

	// Default to the primary binding, but only if we're at the top-most level of the graph.
	if (Optimus::IsExecutionGraphType(Graph->GetGraphType()))
	{
		const UOptimusDeformer* Deformer = Cast<UOptimusDeformer>(Graph->GetCollectionOwner());
		if (ensure(Deformer))
		{
			return Deformer->GetPrimaryComponentBinding();
		}
	}
	else
	{
		const UOptimusNodeSubGraph* SubGraph = Cast<UOptimusNodeSubGraph>(Graph);
		if (ensure(SubGraph))
		{
			return SubGraph->GetDefaultComponentBinding(InContext);
		}
	}

	
	return nullptr;
}

EOptimusPinMutability UOptimusNode_DataInterface::GetOutputPinMutability(const UOptimusNodePin* InPin) const
{
	const TArray<FOptimusCDIPinDefinition> PinDefinitions = DataInterfaceData->GetPinDefinitions();

	int32 PinDefinitionIndex = INDEX_NONE;
	for (int32 Index = 0 ; Index < PinDefinitions.Num(); ++Index)
	{
		if (PinDefinitions[Index].PinName == InPin->GetUniqueName())
		{
			PinDefinitionIndex = Index;
			break;
		}
	}
	if (!ensure(PinDefinitionIndex != INDEX_NONE))
	{
		return EOptimusPinMutability::Mutable;
	}

	return PinDefinitions[PinDefinitionIndex].bMutable ? EOptimusPinMutability::Mutable : EOptimusPinMutability::Immutable;
}


void UOptimusNode_DataInterface::PostLoad() 
{
	Super::PostLoad();

	// Previously DataInterfaceData wasn't always created.
	if (DataInterfaceClass && !DataInterfaceData)
	{
		DataInterfaceData = NewObject<UOptimusComputeDataInterface>(this, DataInterfaceClass);
	}

	// Add in the component pin.
	if (GetLinkerCustomVersion(FOptimusObjectVersion::GUID) < FOptimusObjectVersion::ComponentProviderSupport)
	{
		CreateComponentPin();
	}
}


void UOptimusNode_DataInterface::ConstructNode()
{
	// Create the component pin.
	if (ensure(DataInterfaceClass))
	{
		if (!DataInterfaceData)
		{
			DataInterfaceData = NewObject<UOptimusComputeDataInterface>(this, DataInterfaceClass);
		}
		SetDisplayName(FText::FromString(DataInterfaceData->GetDisplayName()));
		CreateComponentPin();
		CreatePinsFromDataInterface(DataInterfaceData);
	}
}


void UOptimusNode_DataInterface::PostDuplicate(EDuplicateMode::Type DuplicateMode)
{
	// Currently duplication doesn't set the correct outer so fix here.
	// We can remove this when duplication handles the outer correctly.
	if (ensure(DataInterfaceData) && DataInterfaceData->GetOuter() != this)
	{
		FObjectDuplicationParameters DupParams = InitStaticDuplicateObjectParams(DataInterfaceData, this);
		
		DataInterfaceData = Cast<UOptimusComputeDataInterface>(StaticDuplicateObjectEx(DupParams));	
	}
}


void UOptimusNode_DataInterface::CreatePinsFromDataInterface(
	const UOptimusComputeDataInterface* InDataInterface
	)
{
	// A data interface provides read and write functions. A data interface node exposes
	// the read functions as output pins to be fed into kernel nodes (or into other interface
	// nodes' write functions). Conversely all write functions are exposed as input pins,
	// since the data is being written to.
	const TArray<FOptimusCDIPinDefinition> PinDefinitions = InDataInterface->GetPinDefinitions();

	TArray<FShaderFunctionDefinition> ReadFunctions;
	InDataInterface->GetSupportedInputs(ReadFunctions);
	
	TMap<FString, const FShaderFunctionDefinition *> ReadFunctionMap;
	for (const FShaderFunctionDefinition& Def: ReadFunctions)
	{
		ReadFunctionMap.Add(Def.Name, &Def);
	}

	TArray<FShaderFunctionDefinition> WriteFunctions;
	InDataInterface->GetSupportedOutputs(WriteFunctions);
	
	TMap<FString, const FShaderFunctionDefinition *> WriteFunctionMap;
	for (const FShaderFunctionDefinition& Def: WriteFunctions)
	{
		WriteFunctionMap.Add(Def.Name, &Def);
	}

	for (const FOptimusCDIPinDefinition& Def: PinDefinitions)
	{
		if (ensure(!Def.PinName.IsNone()))
		{
			CreatePinFromDefinition(Def, ReadFunctionMap, WriteFunctionMap);
		}
	}
}

void UOptimusNode_DataInterface::CreatePinFromDefinition(
	const FOptimusCDIPinDefinition& InDefinition,
	const TMap<FString, const FShaderFunctionDefinition*>& InReadFunctionMap,
	const TMap<FString, const FShaderFunctionDefinition*>& InWriteFunctionMap
	)	
{
	const FOptimusDataTypeRegistry& TypeRegistry = FOptimusDataTypeRegistry::Get();

	// If there's no count function, then we have a value pin. The data function should
	// have a return parameter but no input parameters. The value function only exists in 
	// the read function map and so can only be an output pin.
	if (InDefinition.DataDimensions.IsEmpty())
	{
		if (!InReadFunctionMap.Contains(InDefinition.DataFunctionName))
		{
			UE_LOG(LogOptimusCore, Error, TEXT("Data function %s given for pin %s in %s does not exist"),
				*InDefinition.DataFunctionName, *InDefinition.PinName.ToString(), *DataInterfaceClass->GetName());
			return;
		}

		const FShaderFunctionDefinition* FuncDef = InReadFunctionMap[InDefinition.DataFunctionName];
		if (!FuncDef->bHasReturnType || FuncDef->ParamTypes.Num() != 1)
		{
			UE_LOG(LogOptimusCore, Error, TEXT("Data function %s given for pin %s in %s does not return a single value"),
				*InDefinition.DataFunctionName, *InDefinition.PinName.ToString(), *DataInterfaceClass->GetName());
			return;
		}

		const FShaderValueTypeHandle ValueTypeHandle = FuncDef->ParamTypes[0].ValueType;
		const FOptimusDataTypeRef PinDataType = TypeRegistry.FindType(ValueTypeHandle);
		if (!PinDataType.IsValid())
		{
			UE_LOG(LogOptimusCore, Error, TEXT("Data function %s given for pin %s in %s uses unsupported type '%s'"),
				*InDefinition.DataFunctionName, *InDefinition.PinName.ToString(), *DataInterfaceClass->GetName(),
				*ValueTypeHandle->ToString());
			return;
		}

		AddPinDirect(InDefinition.PinName, EOptimusNodePinDirection::Output, {}, PinDataType);
	}
	else if (!InDefinition.DataFunctionName.IsEmpty())
	{
		// The count function is always in the read function list.
		for (const FOptimusCDIPinDefinition::FDimensionInfo& ContextInfo: InDefinition.DataDimensions)
		{
			if (!InReadFunctionMap.Contains(ContextInfo.CountFunctionName))
			{
				UE_LOG(LogOptimusCore, Error, TEXT("Count function %s given for pin %s in %s does not exist"),
					*ContextInfo.CountFunctionName, *InDefinition.PinName.ToString(), *DataInterfaceClass->GetName());
				return;
			}
		}

		FShaderValueTypeHandle ValueTypeHandle;
		EOptimusNodePinDirection PinDirection;
		
		if (InReadFunctionMap.Contains(InDefinition.DataFunctionName))
		{
			PinDirection = EOptimusNodePinDirection::Output;
			const FShaderFunctionDefinition* FuncDef = InReadFunctionMap[InDefinition.DataFunctionName];

			// FIXME: Ensure it takes a scalar uint/int as input index.
			if (!FuncDef->bHasReturnType || FuncDef->ParamTypes.Num() != (1 + InDefinition.DataDimensions.Num()))
			{
				UE_LOG(LogOptimusCore, Error, TEXT("Data read function %s given for pin %s in %s is not properly declared."),
					*InDefinition.DataFunctionName, *InDefinition.PinName.ToString(), *DataInterfaceClass->GetName());
				return;
			}

			// The return type dictates the pin type.
			ValueTypeHandle = FuncDef->ParamTypes[0].ValueType;
		}
		else if (InWriteFunctionMap.Contains(InDefinition.DataFunctionName))
		{
			PinDirection = EOptimusNodePinDirection::Input;
			
			const FShaderFunctionDefinition* FuncDef = InWriteFunctionMap[InDefinition.DataFunctionName];

			// FIXME: Ensure it takes a scalar uint/int as input index.
			if (FuncDef->bHasReturnType || FuncDef->ParamTypes.Num() != (1 + InDefinition.DataDimensions.Num()))
			{
				UE_LOG(LogOptimusCore, Error, TEXT("Data write function %s given for pin %s in %s is not properly declared."),
					*InDefinition.DataFunctionName, *InDefinition.PinName.ToString(), *DataInterfaceClass->GetName());
				return;
			}

			// The second argument dictates the pin type.
			ValueTypeHandle = FuncDef->ParamTypes[1].ValueType;
		}
		else
		{
			UE_LOG(LogOptimusCore, Error, TEXT("Data function %s given for pin %s in %s does not exist"),
				*InDefinition.DataFunctionName, *InDefinition.PinName.ToString(), *DataInterfaceClass->GetName());
			return;
		}

		const FOptimusDataTypeRef PinDataType = TypeRegistry.FindType(ValueTypeHandle);
		if (!PinDataType.IsValid())
		{
			UE_LOG(LogOptimusCore, Error, TEXT("Data function %s given for pin %s in %s uses unsupported type '%s'"),
				*InDefinition.DataFunctionName, *InDefinition.PinName.ToString(), *DataInterfaceClass->GetName(),
				*ValueTypeHandle->ToString());
			return;
		}

		TArray<FName> ContextNames;
		for (const FOptimusCDIPinDefinition::FDimensionInfo& ContextInfo: InDefinition.DataDimensions)
		{
			ContextNames.Add(ContextInfo.ContextName);
		}

		const FOptimusDataDomain DataDomain{ContextNames, InDefinition.DomainMultiplier};
		AddPinDirect(InDefinition.PinName, PinDirection, DataDomain, PinDataType);
	}
	else
	{
		UE_LOG(LogOptimusCore, Error, TEXT("No data function given for pin %s in %s"),
			*InDefinition.PinName.ToString(), *DataInterfaceClass->GetName());
	}
}

void UOptimusNode_DataInterface::CreateComponentPin()
{
	const FOptimusDataTypeRegistry& TypeRegistry = FOptimusDataTypeRegistry::Get();
	FOptimusDataTypeRef ComponentSourceType = TypeRegistry.FindType(*UOptimusComponentSourceBinding::StaticClass());

	const UOptimusComponentSource* ComponentSource = UOptimusComponentSource::GetSourceFromDataInterface(DataInterfaceData);
	if (ensure(ComponentSourceType.IsValid()) &&
		ensure(ComponentSource))
	{
		// For back-comp: If we're coming in here from PostLoad and pins already exist, make sure to inject this new pin
		// as the first pin in the list.
		UOptimusNodePin* BeforePin = GetPins().IsEmpty() ? nullptr : GetPins()[0];
		
		AddPinDirect(ComponentSource->GetBindingName(), EOptimusNodePinDirection::Input, {}, ComponentSourceType, BeforePin);
	}
}

UOptimusNodePin* UOptimusNode_DataInterface::GetComponentPin() const
{
	// Is always the first pin, per CreateComponentPin.
	return GetPins()[0];
}

#undef LOCTEXT_NAMESPACE
