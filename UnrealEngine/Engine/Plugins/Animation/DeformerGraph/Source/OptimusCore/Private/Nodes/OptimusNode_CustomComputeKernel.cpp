// Copyright Epic Games, Inc. All Rights Reserved.

#include "OptimusNode_CustomComputeKernel.h"

#include "OptimusComponentSource.h"
#include "DataInterfaces/OptimusDataInterfaceRawBuffer.h"
#include "OptimusDataTypeRegistry.h"
#include "OptimusHelpers.h"
#include "OptimusNodeGraph.h"
#include "OptimusNodePin.h"
#include "OptimusNode_DataInterface.h"
#include "OptimusObjectVersion.h"
#include "ComponentSources/OptimusSkeletalMeshComponentSource.h"
#include "ComponentSources/OptimusSkinnedMeshComponentSource.h"
#include "DataInterfaces/OptimusDataInterfaceSkinnedMeshExec.h"
#include "Engine/UserDefinedStruct.h"

static const FName DefaultKernelName("MyKernel");
static const FName DefaultGroupName("Group"); 
static const FName InputBindingsName= GET_MEMBER_NAME_CHECKED(UOptimusNode_CustomComputeKernel, InputBindingArray);
static const FName OutputBindingsName = GET_MEMBER_NAME_CHECKED(UOptimusNode_CustomComputeKernel, OutputBindingArray);
static const FName ExtraInputBindingGroupsName = GET_MEMBER_NAME_CHECKED(UOptimusNode_CustomComputeKernel, SecondaryInputBindingGroups);

static FString GetShaderValueTypeFriendlyName(const FOptimusDataTypeRef& InDataType)
{
	FName FriendlyName = NAME_None;
	if (UUserDefinedStruct* UserDefinedStruct = Cast<UUserDefinedStruct>(InDataType->TypeObject))
	{
		FriendlyName = Optimus::GetTypeName(UserDefinedStruct, false);
	}
	return InDataType->ShaderValueType->ToString(FriendlyName);
}


static bool IsConnectableInputPin(const UOptimusNodePin *InPin)
{
	return InPin->GetDirection() == EOptimusNodePinDirection::Input && !InPin->IsGroupingPin();
}

static bool IsConnectableOutputPin(const UOptimusNodePin *InPin)
{
	return InPin->GetDirection() == EOptimusNodePinDirection::Output && !InPin->IsGroupingPin();
}

static bool IsGroupingInputPin(const UOptimusNodePin *InPin)
{
	return InPin->GetDirection() == EOptimusNodePinDirection::Input && InPin->IsGroupingPin();
}



UOptimusNode_CustomComputeKernel::UOptimusNode_CustomComputeKernel()
{
	EnableDynamicPins();
	UpdatePreamble();
	
	KernelName = DefaultKernelName;
}


FString UOptimusNode_CustomComputeKernel::GetKernelSourceText() const
{
	return GetCookedKernelSource(GetPathName(), ShaderSource.ShaderText, KernelName.ToString(), GroupSize);
}

TArray<const UOptimusNodePin*> UOptimusNode_CustomComputeKernel::GetPrimaryGroupInputPins() const
{
	TArray<const UOptimusNodePin*> PrimaryGroupInputPins;
	for (const UOptimusNodePin* Pin: GetPins())
	{
		if (IsConnectableInputPin(Pin))
		{
			PrimaryGroupInputPins.Add(Pin);
		}
	}
	return PrimaryGroupInputPins;
}

#if WITH_EDITOR

FString UOptimusNode_CustomComputeKernel::GetNameForShaderTextEditor() const
{
	return KernelName.ToString();
}

FString UOptimusNode_CustomComputeKernel::GetDeclarations() const
{
	return ShaderSource.Declarations;
}

FString UOptimusNode_CustomComputeKernel::GetShaderText() const
{
	return ShaderSource.ShaderText;
}

void UOptimusNode_CustomComputeKernel::SetShaderText(const FString& NewText) 
{
	Modify();
	ShaderSource.ShaderText = NewText;
}

#endif // WITH_EDITOR

FString UOptimusNode_CustomComputeKernel::GetBindingDeclaration(
	FName BindingName
	) const
{
	auto ParameterBindingPredicate = [BindingName](const FOptimusParameterBinding& InBinding)
	{
		if (InBinding.Name == BindingName)
		{
			return true;	
		}
			
		return false;
	};

	if (const FOptimusParameterBinding* Binding = InputBindingArray.FindByPredicate(ParameterBindingPredicate))
	{
		return GetDeclarationForBinding(*Binding, true);
	}
	if (const FOptimusParameterBinding* Binding = OutputBindingArray.FindByPredicate(ParameterBindingPredicate))
	{
		return GetDeclarationForBinding(*Binding, false);
	}

	return FString();
}


bool UOptimusNode_CustomComputeKernel::CanAddPinFromPin(
	const UOptimusNodePin* InSourcePin,
	EOptimusNodePinDirection InNewPinDirection,
	FString* OutReason
	) const
{
	if (!CanConnectPinToNode(InSourcePin, InNewPinDirection, OutReason))
	{
		return false;
	}
	
	if (InSourcePin->GetDirection() == EOptimusNodePinDirection::Input)
	{
		if (InSourcePin->GetDataDomain().IsSingleton())
		{
			if (OutReason)
			{
				*OutReason = TEXT("Can't add parameter pin as output");
			}

			return false;
		}
	}

	return true;
}


UOptimusNodePin* UOptimusNode_CustomComputeKernel::TryAddPinFromPin(
	UOptimusNodePin* InSourcePin,
	FName InNewPinName
	) 
{
	const EOptimusNodePinDirection NewPinDirection =
		InSourcePin->GetDirection() == EOptimusNodePinDirection::Input ?
				EOptimusNodePinDirection::Output : EOptimusNodePinDirection::Input;
	
	TArray<FOptimusParameterBinding>& BindingArray = 
		InSourcePin->GetDirection() == EOptimusNodePinDirection::Input ?
			OutputBindingArray.InnerArray : InputBindingArray.InnerArray;

	FOptimusParameterBinding Binding;
	Binding.Name = InNewPinName;
	Binding.DataType = {InSourcePin->GetDataType()};
	Binding.DataDomain = InSourcePin->GetDataDomain();

	Modify();
	BindingArray.Add(Binding);
	UpdatePreamble();
	
	UOptimusNodePin* NewPin	= AddPinDirect(InNewPinName, NewPinDirection, Binding.DataDomain, Binding.DataType);

	return NewPin;
}


bool UOptimusNode_CustomComputeKernel::RemoveAddedPin(
	UOptimusNodePin* InAddedPinToRemove
	)
{
	TArray<FOptimusParameterBinding>& BindingArray = 
		InAddedPinToRemove->GetDirection() == EOptimusNodePinDirection::Input ?
			InputBindingArray.InnerArray : OutputBindingArray.InnerArray;

	Modify();
	BindingArray.RemoveAll(
		[InAddedPinToRemove](const FOptimusParameterBinding& Binding)
		{
			return Binding.Name == InAddedPinToRemove->GetFName(); 
		});
	UpdatePreamble();
	return RemovePinDirect(InAddedPinToRemove);
}


FName UOptimusNode_CustomComputeKernel::GetSanitizedNewPinName(
	FName InPinName
	)
{
	FName NewName = Optimus::GetSanitizedNameForHlsl(InPinName);

	NewName = Optimus::GetUniqueNameForScope(this, NewName);

	return NewName;
}

TArray<FName> UOptimusNode_CustomComputeKernel::GetExecutionDomains() const
{
	// Find all component sources for the primary pins. If we end up with any other number
	// than one, then something's gone wrong and we can't determine the execution domains.
	TSet<UOptimusComponentSourceBinding*> PrimaryBindings;

	const UOptimusNodeGraph* Graph = GetOwningGraph();
	for (const UOptimusNodePin* Pin: GetPins())
	{
		if (IsConnectableInputPin(Pin))
		{
			PrimaryBindings.Append(Graph->GetComponentSourceBindingsForPin(Pin));
		}
	}

	if (PrimaryBindings.Num() == 1)
	{
		return PrimaryBindings.Array()[0]->GetComponentSource()->GetExecutionDomains();
	}
	else
	{
		return {};
	}
}

void UOptimusNode_CustomComputeKernel::OnDataTypeChanged(FName InTypeName)
{
	Super::OnDataTypeChanged(InTypeName);
	
	UpdatePreamble();
}


void UOptimusNode_CustomComputeKernel::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	Ar.UsingCustomVersion(FOptimusObjectVersion::GUID);
}


#if WITH_EDITOR
void UOptimusNode_CustomComputeKernel::PostEditChangeProperty(
	FPropertyChangedEvent& PropertyChangedEvent
	)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.ChangeType & EPropertyChangeType::ValueSet)
	{
		PropertyValueChanged(PropertyChangedEvent);
	}
	else if (PropertyChangedEvent.ChangeType & EPropertyChangeType::ArrayAdd)
	{
		PropertyArrayItemAdded(PropertyChangedEvent);
	}
	else if (PropertyChangedEvent.ChangeType & EPropertyChangeType::ArrayRemove)
	{
		PropertyArrayItemRemoved(PropertyChangedEvent);
	}
	else if (PropertyChangedEvent.ChangeType & EPropertyChangeType::ArrayClear)
	{
		PropertyArrayCleared(PropertyChangedEvent);
	}
	else if (PropertyChangedEvent.ChangeType & EPropertyChangeType::ArrayMove)
	{
		PropertyArrayItemMoved(PropertyChangedEvent);
	}
}

void UOptimusNode_CustomComputeKernel::PropertyValueChanged(
	const FPropertyChangedEvent& InPropertyChangedEvent
	)
{
	auto UpdatePinNamesFromBindings = [this](
		UObject* InNameScope,
		TArrayView<UOptimusNodePin* const> InPins,
		TFunction<bool(const UOptimusNodePin*)> InPinFilter,
		FOptimusParameterBindingArray &InBindings,
		TFunction<void(UObject*, FOptimusParameterBinding&, UOptimusNodePin*)> InApplyFunc
		)
	{
		int32 Index = 0;
		for (UOptimusNodePin* Pin: InPins)
		{
			if (InPinFilter(Pin))
			{
				FOptimusParameterBinding& Binding = InBindings[Index];

				InApplyFunc(InNameScope, Binding, Pin);

				Index++;
			}
		}
	};

	auto UpdateAllBindings = [&](
		TFunction<void(UObject*, FOptimusParameterBinding&, UOptimusNodePin*)> InApplyFunc
		) -> bool
	{
		if (InPropertyChangedEvent.GetMemberPropertyName() == InputBindingsName)
		{
			UpdatePinNamesFromBindings(this, GetPins(), IsConnectableInputPin, InputBindingArray, InApplyFunc);
			return true;
		}
		if (InPropertyChangedEvent.GetMemberPropertyName() == OutputBindingsName)
		{
			UpdatePinNamesFromBindings(this, GetPins(), IsConnectableOutputPin, OutputBindingArray, InApplyFunc);
			return true;
		}
		if (InPropertyChangedEvent.GetMemberPropertyName() == ExtraInputBindingGroupsName)
		{
			int32 GroupIndex = 0;
			for (UOptimusNodePin* GroupPin: GetPins())
			{
				if (IsGroupingInputPin(GroupPin))
				{
					FOptimusSecondaryInputBindingsGroup& BindingsGroup = SecondaryInputBindingGroups[GroupIndex];
					UpdatePinNamesFromBindings(GroupPin, GroupPin->GetSubPins(), IsConnectableInputPin, BindingsGroup.BindingArray, InApplyFunc);

					GroupIndex++;
				}
			}
			return true;
		}

		return false;
	};
	
	
	if (InPropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_STRING_CHECKED(UOptimusNode_CustomComputeKernel, KernelName))
	{
		SetDisplayName(FText::FromName(KernelName));
		UpdatePreamble();
	}
	else if (InPropertyChangedEvent.GetMemberPropertyName() == GET_MEMBER_NAME_STRING_CHECKED(UOptimusNode_CustomComputeKernel, SecondaryInputBindingGroups) &&
			 InPropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_STRING_CHECKED(FOptimusValidatedName, Name))
	{
		// The group name or secondary pin names changed.
		int32 Index = 0;
		bool bUpdatePreamble = false;
		for (UOptimusNodePin* GroupPin: GetPins())
		{
			if (IsGroupingInputPin(GroupPin))
			{
				FOptimusSecondaryInputBindingsGroup& SecondaryGroup = SecondaryInputBindingGroups[Index++]; 
				if (GroupPin->GetFName() != SecondaryGroup.GroupName)
				{
					SecondaryGroup.GroupName = Optimus::GetUniqueNameForScope(this, SecondaryGroup.GroupName);
					SetPinName(GroupPin, SecondaryGroup.GroupName);
					bUpdatePreamble = true;
				}
			}
		}

		auto UpdateName = [this](UObject* InNameScope, FOptimusParameterBinding& InBinding, UOptimusNodePin* InPin)
		{
			if (InPin->GetFName() != InBinding.Name)
			{
				InBinding.Name = Optimus::GetUniqueNameForScope(InNameScope, InBinding.Name);
				SetPinName(InPin, InBinding.Name);
			}
		};

		if (bUpdatePreamble || UpdateAllBindings(UpdateName))
		{
			UpdatePreamble();
		}
	}
	else if (InPropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_STRING_CHECKED(FOptimusValidatedName, Name))
	{
		auto UpdateName = [this](UObject* InNameScope, FOptimusParameterBinding& InBinding, UOptimusNodePin* InPin)
		{
			if (InPin->GetFName() != InBinding.Name)
			{
				InBinding.Name = Optimus::GetUniqueNameForScope(InNameScope, InBinding.Name);
				SetPinName(InPin, InBinding.Name);
			}
		};

		if (UpdateAllBindings(UpdateName))
		{
			UpdatePreamble();
			return;
		}
	}
	else if (InPropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_STRING_CHECKED(FOptimusDataTypeRef, TypeName))
	{
		auto UpdatePinType = [this](UObject* , FOptimusParameterBinding& InBinding, UOptimusNodePin* InPin)
		{
			const FOptimusDataTypeHandle DataType = InBinding.DataType.Resolve();
			if (InPin->GetDataType() != DataType)
			{
				SetPinDataType(InPin, DataType);
			}
		};

		if (UpdateAllBindings(UpdatePinType))
		{
			UpdatePreamble();
			return;
		}
	}
	else if (InPropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_STRING_CHECKED(FOptimusParameterBinding, DataDomain))
	{
		auto UpdatePinDataDomain = [this](UObject* , FOptimusParameterBinding& InBinding, UOptimusNodePin* InPin)
		{
			if (InPin->GetDataDomain() != InBinding.DataDomain)
			{
				SetPinDataDomain(InPin, InBinding.DataDomain);
			}
		};

		if (UpdateAllBindings(UpdatePinDataDomain))
		{
			UpdatePreamble();
			return;
		}
	}
}


void UOptimusNode_CustomComputeKernel::PropertyArrayItemAdded(
	const FPropertyChangedEvent& InPropertyChangedEvent
	)
{
	auto GetArrayIndex = [Event=InPropertyChangedEvent](
		const FString& InArrayName,
		const auto& InArray
		) -> int32
	{
		int32 ArrayIndex = Event.GetArrayIndex(InArrayName);
		if (ArrayIndex == INDEX_NONE)
		{
			ArrayIndex = InArray.Num() - 1;
		}
		return ArrayIndex;
	};
	auto AddPinForBinding = [this](
		FOptimusParameterBinding& InBinding,
		FName InName,
		EOptimusNodePinDirection InDirection,
		UOptimusNodePin* InBeforePin = nullptr,
		UOptimusNodePin* InGroupPin = nullptr
	) -> void
	{
		InBinding.Name = Optimus::GetUniqueNameForScope(InGroupPin ? Cast<UObject>(InGroupPin) : this, InName);
		InBinding.DataType = FOptimusDataTypeRegistry::Get().FindType(*FFloatProperty::StaticClass());
		if (InDirection == EOptimusNodePinDirection::Input)
		{
			// Default to parameters for input bindings.
			InBinding.DataDomain = FOptimusDataDomain();
		}
		else
		{
			// Pick a suitable fallback for output pins.
			FName DomainName = GetExecutionDomain();
			if (DomainName.IsNone())
			{
				if (TArray<FName> ExecutionDomains = GetExecutionDomains(); !ExecutionDomains.IsEmpty())
				{
					DomainName = ExecutionDomains[0];
				}
				else
				{
					// FIXME: There should be a generic mechanism to get the most suitable default. 
					DomainName = UOptimusSkeletalMeshComponentSource::Domains::Vertex;
				}
			}
			InBinding.DataDomain = FOptimusDataDomain({DomainName});
		}

		AddPin(InBinding.Name, InDirection, InBinding.DataDomain, InBinding.DataType, InBeforePin, InGroupPin);

		UpdatePreamble();
	};

	// Find the top-level pin before the index of the given direction. If we reach a group pin, return that so that
	// we can insert before the group pin.
	auto FindTopLevelBeforePin = [this](
		EOptimusNodePinDirection InDirection,
		int32 InsertIndex
		) -> UOptimusNodePin * 
	{
		for (UOptimusNodePin *Pin: GetPins())
		{
			if (Pin->IsGroupingPin())
			{
				return Pin;
			}
			if (Pin->GetDirection() == InDirection)
			{
				if (InsertIndex-- == 0)
				{
					return Pin;
				}
			}
		}
		return nullptr;
	};

	if (InPropertyChangedEvent.GetMemberPropertyName() == ExtraInputBindingGroupsName)
	{
		if (InPropertyChangedEvent.GetPropertyName() == ExtraInputBindingGroupsName)
		{
			const int32 GroupIndex = InPropertyChangedEvent.GetArrayIndex(ExtraInputBindingGroupsName.ToString());
			if (!ensure(GroupIndex != INDEX_NONE))
			{
				return;
			}

			// If adding to the list of groups before the last, then we need to specify the before-pin otherwise
			// we just add the pin to the end.
			UOptimusNodePin* BeforePin = nullptr;
			if (GroupIndex < (SecondaryInputBindingGroups.Num() - 1))
			{
				BeforePin = GetPins()[InputBindingArray.Num() + OutputBindingArray.Num() + GroupIndex];
			}
			
			const FName GroupName = Optimus::GetUniqueNameForScope(this, DefaultGroupName);

			AddGroupingPin(GroupName, EOptimusNodePinDirection::Input, BeforePin);

			SecondaryInputBindingGroups[GroupIndex].GroupName = GroupName;

			UpdatePreamble();
		}
		else if (InPropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(FOptimusParameterBindingArray, InnerArray))
		{
			// Unfortunately, we don't get told which group index we're under. So find the group where the binding
			// count is one greater than the pin count.
			if (!ensure(!SecondaryInputBindingGroups.IsEmpty()))
			{
				return;
			}
			
			int32 GroupIndex = 0;
			UOptimusNodePin* GroupPin = nullptr;
			for (UOptimusNodePin *Pin: GetPins())
			{
				if (Pin->IsGroupingPin())
				{
					if (SecondaryInputBindingGroups[GroupIndex].BindingArray.Num() == (Pin->GetSubPins().Num() + 1))
					{
						GroupPin = Pin;
						break;
					}
					GroupIndex++;
				}
			}

			if (!ensure(GroupPin))
			{
				return;
			}
			
			const int32 BindingIndex = InPropertyChangedEvent.GetArrayIndex(GET_MEMBER_NAME_STRING_CHECKED(FOptimusParameterBindingArray, InnerArray));

			FOptimusSecondaryInputBindingsGroup& BindingGroup = SecondaryInputBindingGroups[GroupIndex];
			
			UOptimusNodePin* BeforePin = nullptr;
			if (BindingIndex < (BindingGroup.BindingArray.Num() - 1))
			{
				BeforePin = GroupPin->GetSubPins()[BindingIndex];
			}

			AddPinForBinding(BindingGroup.BindingArray[BindingIndex], "Input", EOptimusNodePinDirection::Input, BeforePin, GroupPin);
			
			// Make sure the group pin is expanded so that the change is visible in the graph.
			GroupPin->SetIsExpanded(true);
		}
	}
	if (InPropertyChangedEvent.GetMemberPropertyName() == InputBindingsName)
	{
		const int32 BindingIndex = GetArrayIndex(GET_MEMBER_NAME_STRING_CHECKED(FOptimusParameterBindingArray, InnerArray), InputBindingArray);
		
		UOptimusNodePin* BeforePin = FindTopLevelBeforePin(EOptimusNodePinDirection::Input, BindingIndex);

		AddPinForBinding(InputBindingArray[BindingIndex], "Input", EOptimusNodePinDirection::Input, BeforePin);
	}
	else if (InPropertyChangedEvent.GetMemberPropertyName() == OutputBindingsName)
	{
		const int32 BindingIndex = GetArrayIndex(GET_MEMBER_NAME_STRING_CHECKED(FOptimusParameterBindingArray, InnerArray), OutputBindingArray);

		UOptimusNodePin* BeforePin = FindTopLevelBeforePin(EOptimusNodePinDirection::Output, BindingIndex);
		
		AddPinForBinding(OutputBindingArray[BindingIndex], "Output", EOptimusNodePinDirection::Output, BeforePin);
	}
}


void UOptimusNode_CustomComputeKernel::PropertyArrayItemRemoved(const FPropertyChangedEvent& InPropertyChangedEvent)
{
	auto RemoveTopLevelPinByIndex = [this, Event=InPropertyChangedEvent](EOptimusNodePinDirection InPinDirection)
	{
		int32 PinIndex = Event.GetArrayIndex(GET_MEMBER_NAME_STRING_CHECKED(FOptimusParameterBindingArray, InnerArray));
		if (!ensure(PinIndex != INDEX_NONE))
		{
			return;
		}
		
		for (UOptimusNodePin *Pin: GetPins())
		{
			if (Pin->GetDirection() == InPinDirection && !Pin->IsGroupingPin())
			{
				if (PinIndex-- == 0)
				{
					RemovePin(Pin);
					UpdatePreamble();
					return;
				}
			}
		}
	};
	
	if (InPropertyChangedEvent.GetMemberPropertyName() == ExtraInputBindingGroupsName)
	{
		if (InPropertyChangedEvent.GetPropertyName() == ExtraInputBindingGroupsName)
		{
			int32 GroupIndex = InPropertyChangedEvent.GetArrayIndex(ExtraInputBindingGroupsName.ToString());
			if (!ensure(GroupIndex != INDEX_NONE))
			{
				return;
			}

			for (UOptimusNodePin *Pin: GetPins())
			{
				if (Pin->IsGroupingPin())
				{
					if (GroupIndex-- == 0)
					{
						RemovePin(Pin);
						UpdatePreamble();
						return;
					}
				}
			}
		}
		else if (InPropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(FOptimusParameterBindingArray, InnerArray))
		{
			// Unfortunately, we don't get told which group index we're under. So find the group where the binding
			// count is one less than the pin count.
			if (!ensure(!SecondaryInputBindingGroups.IsEmpty()))
			{
				return;
			}
			
			int32 GroupIndex = 0;
			UOptimusNodePin* GroupPin = nullptr;
			for (UOptimusNodePin *Pin: GetPins())
			{
				if (Pin->IsGroupingPin())
				{
					if (SecondaryInputBindingGroups[GroupIndex].BindingArray.Num() == (Pin->GetSubPins().Num() - 1))
					{
						GroupPin = Pin;
						break;
					}
					GroupIndex++;
				}
			}

			if (!ensure(GroupPin))
			{
				return;
			}

			if (SecondaryInputBindingGroups[GroupIndex].BindingArray.IsEmpty())
			{
				// If the group goes empty, collapse the pin for consistent look.
				GroupPin->SetIsExpanded(false);
			}
			
			const int32 BindingIndex = InPropertyChangedEvent.GetArrayIndex(GET_MEMBER_NAME_STRING_CHECKED(FOptimusParameterBindingArray, InnerArray));
			if (ensure(GroupPin->GetSubPins().IsValidIndex(BindingIndex)))
			{
				RemovePin(GroupPin->GetSubPins()[BindingIndex]);
				UpdatePreamble();
			}
		}
	}
	else if (InPropertyChangedEvent.GetMemberPropertyName() == InputBindingsName)
	{
		RemoveTopLevelPinByIndex(EOptimusNodePinDirection::Input);
	}
	else if (InPropertyChangedEvent.GetMemberPropertyName() == OutputBindingsName)
	{
		RemoveTopLevelPinByIndex(EOptimusNodePinDirection::Output);
	}
}

void UOptimusNode_CustomComputeKernel::PropertyArrayCleared(
	const FPropertyChangedEvent& InPropertyChangedEvent
	)
{
	auto RemoveAllPinsByPredicate = [this](
		TArrayView<UOptimusNodePin* const> InPins,
		TFunction<bool(const UOptimusNodePin*)> InPredicate)
	{
		// Make a copy of the pins, since we're removing from the array represented by the view.
		for (UOptimusNodePin* Pin: TArray<UOptimusNodePin*>(InPins))
		{
			if (InPredicate(Pin))
			{
				RemovePin(Pin);
			}
		}
		UpdatePreamble();
	};
	
	if (InPropertyChangedEvent.GetMemberPropertyName() == ExtraInputBindingGroupsName)
	{
		if (InPropertyChangedEvent.GetPropertyName() == ExtraInputBindingGroupsName)
		{
			RemoveAllPinsByPredicate(GetPins(), IsGroupingInputPin);
			UpdatePreamble();
		}
		else if (InPropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(FOptimusParameterBindingArray, InnerArray))
		{
			// Unfortunately, we don't get told which group index we're under. So find the group where the binding
			// count is one less than the pin count.
			if (!ensure(!SecondaryInputBindingGroups.IsEmpty()))
			{
				return;
			}
			
			int32 GroupIndex = 0;
			UOptimusNodePin* GroupPin = nullptr;
			for (UOptimusNodePin *Pin: GetPins())
			{
				if (Pin->IsGroupingPin())
				{
					// If the binding array is empty but the pins are still there, then that's our group.
					if (SecondaryInputBindingGroups[GroupIndex].BindingArray.IsEmpty() && !Pin->GetSubPins().IsEmpty())
					{
						GroupPin = Pin;
						break;
					}
					GroupIndex++;
				}
			}

			if (!ensure(GroupPin))
			{
				return;
			}

			// Since  the group is empty, collapse the pin for consistent look.
			GroupPin->SetIsExpanded(false);
			
			RemoveAllPinsByPredicate(GroupPin->GetSubPins(), IsConnectableInputPin);
			UpdatePreamble();
		}
	}
	else if (InPropertyChangedEvent.GetMemberPropertyName() == InputBindingsName)
	{
		RemoveAllPinsByPredicate(GetPins(), IsConnectableInputPin);
		UpdatePreamble();
	}
	else if (InPropertyChangedEvent.GetMemberPropertyName() == OutputBindingsName)
	{
		RemoveAllPinsByPredicate(GetPins(), IsConnectableOutputPin);
		UpdatePreamble();
	}
}

void UOptimusNode_CustomComputeKernel::PropertyArrayItemMoved(
	const FPropertyChangedEvent& InPropertyChangedEvent
	)
{
	auto FindMovedBindingAndMoveMatchingPin = [this](
		TArrayView<UOptimusNodePin* const> InPins,
		const TArray<FName>& BindingNameArray,
		TFunction<bool(const UOptimusNodePin *)> InPinPredicate
		)
	{
		int32 BindingIndex = 0;
		
		// Find the first entry that's different. That's an element we can consider moved. Since array move only
		// deals with a single item moving either forward or backward.
		for (const UOptimusNodePin* Pin: InPins)
		{
			if (InPinPredicate(Pin))
			{
				if (Pin->GetFName() != BindingNameArray[BindingIndex])
				{
					break;
				}
				
				BindingIndex++;
				
				if (BindingIndex == BindingNameArray.Num())
				{
					// Nothing got moved.
					return;
				}
			}
		}

		UOptimusNodePin* MovedPin = *InPins.FindByPredicate([Name=BindingNameArray[BindingIndex]](const UOptimusNodePin* InPin)
		{
			return InPin->GetFName() == Name;
		});

		const UOptimusNodePin* NextPin = nullptr;
		const FName NextPinName = (BindingIndex < (BindingNameArray.Num() - 1)) ? BindingNameArray[BindingIndex + 1] : NAME_None;
		if (!NextPinName.IsNone())
		{
			NextPin = *InPins.FindByPredicate([NextPinName](const UOptimusNodePin* InPin)
			{
				return InPin->GetFName() == NextPinName;
			});
		}

		MovePin(MovedPin, NextPin);
	};

	auto MakeBindingNameArray = [](const FOptimusParameterBindingArray& InBindings) -> TArray<FName>
	{
		TArray<FName> Result;
		for (const FOptimusParameterBinding& Binding: InBindings)
		{
			Result.Add(Binding.Name);
		}
		return Result;
	};
	
	if (InPropertyChangedEvent.GetMemberPropertyName() == ExtraInputBindingGroupsName)
	{
		if (InPropertyChangedEvent.GetPropertyName() == ExtraInputBindingGroupsName)
		{
			TArray<FName> GroupNames;
			for (const FOptimusSecondaryInputBindingsGroup& BindingsGroup: SecondaryInputBindingGroups)
			{
				GroupNames.Add(BindingsGroup.GroupName);
			}
			
			FindMovedBindingAndMoveMatchingPin(GetPins(), GroupNames, IsGroupingInputPin);
			UpdatePreamble();
			
		}
		else if (InPropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(FOptimusParameterBindingArray, InnerArray))
		{
			// Unfortunately, we don't get told which group index we're under. So find the group where the binding
			// count is one less than the pin count.
			if (!ensure(!SecondaryInputBindingGroups.IsEmpty()))
			{
				return;
			}

			auto PinsAndBindingsDiffer = [](const FOptimusParameterBindingArray& InBindings, TArrayView<UOptimusNodePin* const> InPins)
			{
				if (ensure(InBindings.Num() == InPins.Num()))
				{
					for (int32 Index = 0; Index < InBindings.Num(); Index++)
					{
						if (InBindings[Index].Name != InPins[Index]->GetFName())
						{
							return true;
						}
					}
				}
				return false;
			};
			
			int32 GroupIndex = 0;
			UOptimusNodePin* GroupPin = nullptr;
			for (UOptimusNodePin *Pin: GetPins())
			{
				if (Pin->IsGroupingPin())
				{
					// If the binding array is empty but the pins are still there, then that's our group.
					if (PinsAndBindingsDiffer(SecondaryInputBindingGroups[GroupIndex].BindingArray, Pin->GetSubPins()))
					{
						GroupPin = Pin;
						break;
					}
					GroupIndex++;
				}
			}

			if (!ensure(GroupPin))
			{
				return;
			}

			FindMovedBindingAndMoveMatchingPin(
				GroupPin->GetSubPins(), MakeBindingNameArray(SecondaryInputBindingGroups[GroupIndex].BindingArray),
				IsConnectableInputPin);
			UpdatePreamble();
		}
	}
	else if (InPropertyChangedEvent.GetMemberPropertyName() == InputBindingsName)
	{
		FindMovedBindingAndMoveMatchingPin(GetPins(), MakeBindingNameArray(InputBindingArray), IsConnectableInputPin);
		UpdatePreamble();
	}
	else if (InPropertyChangedEvent.GetMemberPropertyName() == OutputBindingsName)
	{
		FindMovedBindingAndMoveMatchingPin(GetPins(), MakeBindingNameArray(OutputBindingArray), IsConnectableOutputPin);
		UpdatePreamble();
	}
}

#endif

void UOptimusNode_CustomComputeKernel::PostLoad()
{
	if (GetLinkerCustomVersion(FOptimusObjectVersion::GUID) < FOptimusObjectVersion::SwitchToParameterBindingArrayStruct)
	{
		Modify();
		InputBindingArray.InnerArray = InputBindings_DEPRECATED;
		OutputBindingArray.InnerArray = OutputBindings_DEPRECATED;
	}
	
	if (!Parameters_DEPRECATED.IsEmpty())
	{
PRAGMA_DISABLE_DEPRECATION_WARNINGS		
		TArray<FOptimusParameterBinding> ParameterInputBindings;

		for (const FOptimus_ShaderBinding& OldBinding: Parameters_DEPRECATED)
		{
			FOptimusParameterBinding NewBinding;
			NewBinding.Name = OldBinding.Name;
			NewBinding.DataType = OldBinding.DataType;
			NewBinding.DataDomain = FOptimusDataDomain();
			
			ParameterInputBindings.Add(NewBinding);
		}
		
		InputBindingArray.InnerArray.Insert(ParameterInputBindings, 0);
		
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}
	
	Super::PostLoad();
	
	if (GetLinkerCustomVersion(FOptimusObjectVersion::GUID) < FOptimusObjectVersion::ComponentProviderSupport)
	{
		ExecutionDomain.Name = UOptimusSkeletalMeshComponentSource::Domains::Vertex;
		
		// Check if there's an execution node connected and grab the domain from it.
		FOptimusDataTypeHandle IntVector3Type = FOptimusDataTypeRegistry::Get().FindType(Optimus::GetTypeName(TBaseStructure<FIntVector3>::Get()));

		for (const UOptimusNodePin* Pin: GetPins())
		{
			if (Pin->GetDirection() == EOptimusNodePinDirection::Input && Pin->GetDataType() == IntVector3Type)
			{
				TArray<FOptimusRoutedNodePin> ConnectedPins = Pin->GetConnectedPinsWithRouting();
				if (ConnectedPins.Num() == 1)
				{
					if (const UOptimusNode_DataInterface* DataInterfaceNode = Cast<UOptimusNode_DataInterface>(ConnectedPins[0].NodePin->GetOwningNode()))
					{
						if (const UOptimusSkinnedMeshExecDataInterface* ExecDataInterface = Cast<UOptimusSkinnedMeshExecDataInterface>(DataInterfaceNode->GetDataInterface(GetTransientPackage())))
						{
							switch(ExecDataInterface->Domain)
							{
							default:
							case EOptimusSkinnedMeshExecDomain::Vertex:
								ExecutionDomain.Name = UOptimusSkinnedMeshComponentSource::Domains::Vertex;
								break;
							case EOptimusSkinnedMeshExecDomain::Triangle:
								ExecutionDomain.Name = UOptimusSkinnedMeshComponentSource::Domains::Triangle;
								break;
							}
						}
					}
				}
			}
		}
	}
	
}


void UOptimusNode_CustomComputeKernel::ConstructNode()
{
	// After a duplicate, the kernel node has no pins, so we need to reconstruct them from
	// the bindings. We can assume that all naming clashes have already been dealt with.
	for (const FOptimusParameterBinding& Binding: InputBindingArray)
	{
		AddPinDirect(Binding.Name, EOptimusNodePinDirection::Input, Binding.DataDomain, Binding.DataType);
	}
	for (const FOptimusParameterBinding& Binding: OutputBindingArray)
	{
		AddPinDirect(Binding.Name, EOptimusNodePinDirection::Output, Binding.DataDomain, Binding.DataType);
	}

	// FIXME: Group pins.
	for (const FOptimusSecondaryInputBindingsGroup& InputGroup: SecondaryInputBindingGroups)
	{
		UOptimusNodePin* GroupPin = AddGroupingPinDirect(InputGroup.GroupName, EOptimusNodePinDirection::Input);

		for (const FOptimusParameterBinding& Binding: InputGroup.BindingArray)
		{
			AddPinDirect(Binding.Name, EOptimusNodePinDirection::Input, Binding.DataDomain, Binding.DataType, nullptr, GroupPin);
		}
	}
}


bool UOptimusNode_CustomComputeKernel::ValidateConnection(
	const UOptimusNodePin& InThisNodesPin,
	const UOptimusNodePin& InOtherNodesPin,
	FString* OutReason
	) const
{
	
	return true;
}

TOptional<FText> UOptimusNode_CustomComputeKernel::ValidateForCompile() const
{
	if (TOptional<FText> Result = Super::ValidateForCompile(); Result.IsSet())
	{
		return Result;
	}
	return {};
}


void UOptimusNode_CustomComputeKernel::UpdatePreamble()
{
	TSet<FString> StructsSeen;
	TArray<FString> Structs;

	auto CollectStructs = [&StructsSeen, &Structs](const auto& BindingArray)
	{
		for (const FOptimusParameterBinding &Binding: BindingArray)
		{
			if (Binding.DataType.IsValid() && Binding.DataType->ShaderValueType.IsValid())
			{
				const FShaderValueType& ValueType = *Binding.DataType->ShaderValueType;
				if (ValueType.Type == EShaderFundamentalType::Struct)
				{
					TArray<FShaderValueTypeHandle> StructTypes = Binding.DataType->ShaderValueType->GetMemberStructTypes();
					StructTypes.Add(Binding.DataType->ShaderValueType);

					TMap<FName, FName> FriendlyNameMap;
					for (const FShaderValueTypeHandle& TypeHandle : StructTypes )
					{
						FOptimusDataTypeHandle DataTypeHandle = FOptimusDataTypeRegistry::Get().FindType(TypeHandle->Name);
						if (UUserDefinedStruct* Struct = Cast<UUserDefinedStruct>(DataTypeHandle->TypeObject))
						{
							FriendlyNameMap.Add(TypeHandle->Name) = Optimus::GetTypeName(Struct, false);
						}
					}

					for (const FShaderValueTypeHandle& TypeHandle : StructTypes )
					{
						const FString StructName = TypeHandle->ToString();
						if (!StructsSeen.Contains(StructName))
						{
							Structs.Add(TypeHandle->GetTypeDeclaration(FriendlyNameMap, true) + TEXT(";\n\n"));
							StructsSeen.Add(StructName);
						}	
					}
				}
			}
		}
	};

	CollectStructs(InputBindingArray);
	CollectStructs(OutputBindingArray);
	for (const FOptimusSecondaryInputBindingsGroup& InputGroup: SecondaryInputBindingGroups)
	{
		CollectStructs(InputGroup.BindingArray);
	}
	
	TArray<FString> Declarations;

	// FIXME: Lump input/output functions together into single context.
	auto ContextsPredicate = [](const FOptimusParameterBinding& A, const FOptimusParameterBinding &B)
	{
		for (int32 Index = 0; Index < FMath::Min(A.DataDomain.DimensionNames.Num(), B.DataDomain.DimensionNames.Num()); Index++)
		{
			if (A.DataDomain.DimensionNames[Index] != B.DataDomain.DimensionNames[Index])
			{
				return FNameLexicalLess()(A.DataDomain.DimensionNames[Index], B.DataDomain.DimensionNames[Index]);
			}
		}
		return false;
	};
	
	TSet<TArray<FName>> SeenDataDomains;

	auto AddCountFunctionIfNeeded = [&Declarations, &SeenDataDomains](const TArray<FName>& InContextNames)
	{
		if (!InContextNames.IsEmpty() && !SeenDataDomains.Contains(InContextNames))
		{
			FString CountNameInfix;

			for (FName ContextName: InContextNames)
			{
				CountNameInfix.Append(ContextName.ToString());
			}
			Declarations.Add(FString::Printf(TEXT("uint Num%s();"), *CountNameInfix));
			SeenDataDomains.Add(InContextNames);
		}
	};

	auto AddInputBindings = [&](const TArray<FOptimusParameterBinding>& InBindings)
	{
		for (const FOptimusParameterBinding& Binding: InBindings)
		{
			AddCountFunctionIfNeeded(Binding.DataDomain.DimensionNames);
		
			TArray<FString> Indexes;
			for (FString IndexName: GetIndexNamesFromDataDomainLevels(Binding.DataDomain.DimensionNames))
			{
				Indexes.Add(FString::Printf(TEXT("uint %s"), *IndexName));
			}

			if (Binding.DataType->ShaderValueType.IsValid())
			{
				Declarations.Add(GetDeclarationForBinding(Binding, true));
			}
			else
			{
				Declarations.Add(FString::Printf(TEXT("// Error: Binding \"%s\" is not supported"), *Binding.Name.ToString()) );
			}
		}
	};

	TArray<FOptimusParameterBinding> Bindings = InputBindingArray.InnerArray;
	Bindings.Sort(ContextsPredicate);
	AddInputBindings(Bindings);

	Bindings = OutputBindingArray.InnerArray;
	Bindings.Sort(ContextsPredicate);
	for (const FOptimusParameterBinding& Binding: Bindings)
	{
		AddCountFunctionIfNeeded(Binding.DataDomain.DimensionNames);
		
		TArray<FString> Indexes;
		for (FString IndexName: GetIndexNamesFromDataDomainLevels(Binding.DataDomain.DimensionNames))
		{
			Indexes.Add(FString::Printf(TEXT("uint %s"), *IndexName));
		}
		
		Declarations.Add(GetDeclarationForBinding(Binding, false));
	}

	for (const FOptimusSecondaryInputBindingsGroup& InputGroup: SecondaryInputBindingGroups)
	{
		Bindings = InputGroup.BindingArray.InnerArray;
		if (!Bindings.IsEmpty())
		{
			Bindings.Sort(ContextsPredicate);
			Declarations.Add(FString::Printf(TEXT("namespace %s {"), *InputGroup.GroupName.ToString()));
			AddInputBindings(Bindings);
			Declarations.Add("}");
		}
	}
	
	ShaderSource.Declarations.Reset();
	if (!Structs.IsEmpty())
	{
		ShaderSource.Declarations += TEXT("// Type declarations\n");
		ShaderSource.Declarations += FString::Join(Structs, TEXT("\n")) + TEXT("\n");
	}
	if (!Declarations.IsEmpty())
	{
		ShaderSource.Declarations += TEXT("// Parameters and resource read/write functions\n");
		ShaderSource.Declarations += FString::Join(Declarations, TEXT("\n"));
	}
	ShaderSource.Declarations += "\n// Resource Indexing\n";
	ShaderSource.Declarations += "uint Index;	// From SV_DispatchThreadID.x\n";
}


FString UOptimusNode_CustomComputeKernel::GetDeclarationForBinding(const FOptimusParameterBinding& Binding, bool bIsInput)
{
	TArray<FString> Indexes;
	for (FString IndexName: GetIndexNamesFromDataDomainLevels(Binding.DataDomain.DimensionNames))
	{
		Indexes.Add(FString::Printf(TEXT("uint %s"), *IndexName));
	}

	if (bIsInput)
	{
		return FString::Printf(TEXT("%s Read%s(%s);"), 
			*GetShaderValueTypeFriendlyName(Binding.DataType), *Binding.Name.ToString(), *FString::Join(Indexes, TEXT(", ")));
	}
	else
	{
		return FString::Printf(TEXT("void Write%s(%s, %s Value);"),
					*Binding.Name.ToString(), *FString::Join(Indexes, TEXT(", ")), *GetShaderValueTypeFriendlyName(Binding.DataType));
	}
}
