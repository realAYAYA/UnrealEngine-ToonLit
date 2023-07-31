// Copyright Epic Games, Inc. All Rights Reserved.

#include "Nodes/OptimusNode_DataInterface.h"

#include "OptimusCoreModule.h"
#include "OptimusNodePin.h"
#include "OptimusDataTypeRegistry.h"
#include "OptimusNodeGraph.h"
#include "OptimusNode_ComponentSource.h"
#include "OptimusObjectVersion.h"

#include "ComputeFramework/ShaderParamTypeDefinition.h"


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
		if (!SourceNode)
		{
			if (OutReason)
			{
				*OutReason = TEXT("Other node should be a Component Source node");
			}
			return false;
		}

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

	return true;
}

TOptional<FText> UOptimusNode_DataInterface::ValidateForCompile() const
{
	// Ensure that we have something connected to the component binding input pin.
	UOptimusComponentSourceBinding* PrimaryBinding = GetComponentBinding();
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
			TSet<UOptimusComponentSourceBinding*> Bindings = Graph->GetComponentSourceBindingsForPin(Pin);
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

UOptimusComponentSourceBinding* UOptimusNode_DataInterface::GetComponentBinding() const
{
	const UOptimusNodeGraph* Graph = GetOwningGraph();
	TSet<UOptimusComponentSourceBinding*> Bindings = Graph->GetComponentSourceBindingsForPin(GetComponentPin());
	
	if (!Bindings.IsEmpty() && ensure(Bindings.Num() == 1))
	{
		return Bindings.Array()[0];
	}
	
	return nullptr;
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
	if (ensure(DataInterfaceData))
	{
		DataInterfaceData->Rename(nullptr, GetOuter());
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
