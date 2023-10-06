// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/MovieGraphNode.h"

#include "Graph/MovieGraphConfig.h"
#include "UObject/TextProperty.h"

FName UMovieGraphNode::GlobalsPinName = FName("Globals");
FString UMovieGraphNode::GlobalsPinNameString = FString("Globals");

namespace UE::MovieGraph::Private
{
	EMovieGraphValueType GetValueTypeFromProperty(const FProperty* InSourceProperty)
	{
		if (CastField<FBoolProperty>(InSourceProperty))
		{
			return EMovieGraphValueType::Bool;
		}
		if (const FByteProperty* ByteProperty = CastField<FByteProperty>(InSourceProperty))
		{
			return ByteProperty->IsEnum() ? EMovieGraphValueType::Enum : EMovieGraphValueType::Byte;
		}
		if (CastField<FIntProperty>(InSourceProperty))
		{
			return EMovieGraphValueType::Int32;
		}
		if (CastField<FInt64Property>(InSourceProperty))
		{
			return EMovieGraphValueType::Int64;
		}
		if (CastField<FFloatProperty>(InSourceProperty))
		{
			return EMovieGraphValueType::Float;
		}
		if (CastField<FDoubleProperty>(InSourceProperty))
		{
			return EMovieGraphValueType::Double;
		}
		if (CastField<FNameProperty>(InSourceProperty))
		{
			return EMovieGraphValueType::Name;
		}
		if (CastField<FStrProperty>(InSourceProperty))
		{
			return EMovieGraphValueType::String;
		}
		if (CastField<FTextProperty>(InSourceProperty))
		{
			return EMovieGraphValueType::Text;
		}
		if (CastField<FEnumProperty>(InSourceProperty))
		{
			return EMovieGraphValueType::Enum;
		}
		if (CastField<FStructProperty>(InSourceProperty))
		{
			return EMovieGraphValueType::Struct;
		}
		if (CastField<FObjectProperty>(InSourceProperty))
		{
			return EMovieGraphValueType::Object;
		}
		if (CastField<FSoftObjectProperty>(InSourceProperty))
		{
			return EMovieGraphValueType::SoftObject;
		}
		if (CastField<FClassProperty>(InSourceProperty))
		{
			return EMovieGraphValueType::Class;
		}
		if (CastField<FSoftClassProperty>(InSourceProperty))
		{
			return EMovieGraphValueType::SoftClass;
		}

		// Handle array property
		if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(InSourceProperty))
		{
			return GetValueTypeFromProperty(ArrayProperty->Inner);	
		}

		return EMovieGraphValueType::None;
	}
}

UMovieGraphNode::UMovieGraphNode()
{
}

TArray<FMovieGraphPinProperties> UMovieGraphNode::GetExposedPinProperties() const
{
	TArray<FMovieGraphPinProperties> Properties;

	// Exposed properties are tracked by name; convert to pin properties
	for (const FMovieGraphPropertyInfo& PropertyInfo : GetExposedProperties())
	{
		if (PropertyInfo.ValueType == EMovieGraphValueType::None)
		{
			continue;
		}
		
		constexpr bool bAllowMultipleConnections = false;
		Properties.Add(FMovieGraphPinProperties(PropertyInfo.Name, PropertyInfo.ValueType, bAllowMultipleConnections));
	}

	return Properties;
}

void UMovieGraphNode::TogglePromotePropertyToPin(const FName& PropertyName)
{
	// Ensure this is a property that can be promoted
	bool bIsPromotableProperty = false;
	FMovieGraphPropertyInfo OverrideablePropertyInfo;
	for (const FMovieGraphPropertyInfo& ExposablePropertyInfo : GetOverrideablePropertyInfo())
	{
		if (ExposablePropertyInfo.Name == PropertyName)
		{
			bIsPromotableProperty = true;
			OverrideablePropertyInfo = ExposablePropertyInfo;
			break;
		}
	}

	// Add this property to promoted properties if it hasn't already been promoted, otherwise remove it
	if (bIsPromotableProperty)
	{
		const bool bFoundExposedPropertyInfo = ExposedPropertyInfo.ContainsByPredicate([&OverrideablePropertyInfo](const FMovieGraphPropertyInfo& ExposedInfo)
		{
			return OverrideablePropertyInfo.IsSamePropertyAs(ExposedInfo);
		});

		if (!bFoundExposedPropertyInfo)
		{
			ExposedPropertyInfo.Add(MoveTemp(OverrideablePropertyInfo));
		}
		else
		{
			ExposedPropertyInfo.RemoveAll([&OverrideablePropertyInfo](const FMovieGraphPropertyInfo& ExposedInfo)
			{
				return OverrideablePropertyInfo.IsSamePropertyAs(ExposedInfo);
			});
		}
	}

	UpdatePins();
}

TArray<const FProperty*> UMovieGraphNode::GetAllOverrideableProperties() const
{
	TArray<const FProperty*> AllProperties;
	
	for (TFieldIterator<FProperty> PropertyIterator(GetClass()); PropertyIterator; ++PropertyIterator)
	{
		if (UMovieGraphConfig::FindOverridePropertyForRealProperty(GetClass(), *PropertyIterator))
		{
			AllProperties.Add(*PropertyIterator);
		}
	}

	if (const UPropertyBag* PropertyBag = DynamicProperties.GetPropertyBagStruct())
	{
		for (const FPropertyBagPropertyDesc& Desc : PropertyBag->GetPropertyDescs())
		{
			if (FindOverridePropertyForDynamicProperty(Desc.Name))
			{
				AllProperties.Add(Desc.CachedProperty);
			}
		}
	}

	return AllProperties;
}

void UMovieGraphNode::UpdatePins()
{
	TArray<FMovieGraphPinProperties> InputPinProperties = GetInputPinProperties();
	TArray<FMovieGraphPinProperties> OutputPinProperties = GetOutputPinProperties();

	// Include the exposed dynamic properties in the input pins
	InputPinProperties.Append(GetExposedPinProperties());

	auto UpdatePins = [this](TArray<UMovieGraphPin*>& Pins, const TArray<FMovieGraphPinProperties>& PinProperties)
	{
		bool bAppliedEdgeChanges = false;
		bool bChangedPins = false;

		// Find unmatched pins vs. properties (via name matching)
		TArray<UMovieGraphPin*> UnmatchedPins;
		for (UMovieGraphPin* Pin : Pins)
		{
			if (const FMovieGraphPinProperties* MatchingProperties = PinProperties.FindByPredicate([Pin](const FMovieGraphPinProperties& Prop) { return Prop.Label == Pin->Properties.Label; }))
			{
				if (Pin->Properties != *MatchingProperties)
				{
					Pin->Modify();
					Pin->Properties = *MatchingProperties;
					//bAppliedEdgeCHanges |= Pin->BreakAllIncompatibleEdges();
					bChangedPins = true;
				}
			}
			else
			{
				UnmatchedPins.Add(Pin);
			}
		}

		// Now do the opposite, find any properties that don't have pins
		TArray<FMovieGraphPinProperties> UnmatchedProperties;
		for (const FMovieGraphPinProperties& Properties : PinProperties)
		{
			if (!Pins.FindByPredicate([&Properties](const UMovieGraphPin* Pin) { return Pin->Properties.Label == Properties.Label; }))
			{
				UnmatchedProperties.Add(Properties);
			}
		}

		if (!UnmatchedPins.IsEmpty() || !UnmatchedProperties.IsEmpty())
		{
			Modify();
			bChangedPins = true;
		}

		// Remove old pins
		for (int32 UnmatchedPinIndex = UnmatchedPins.Num() - 1; UnmatchedPinIndex >= 0; UnmatchedPinIndex--)
		{
			const int32 PinIndex = Pins.IndexOfByKey(UnmatchedPins[UnmatchedPinIndex]);
			check(PinIndex >= 0);

			//bAppliedEdgeCHanges |= Pins[PinIndex]->BreakAllEdges();
			Pins.RemoveAt(PinIndex);
		}

		// Add new pins
		for (const FMovieGraphPinProperties& UnmatchedProperty : UnmatchedProperties)
		{
			const int32 InsertIndex = PinProperties.IndexOfByKey(UnmatchedProperty);
			UMovieGraphPin* NewPin = NewObject<UMovieGraphPin>(this);
			NewPin->Node = this;
			NewPin->Properties = UnmatchedProperty;
			Pins.Insert(NewPin, InsertIndex);
		}

		// return bAppliedEdgeChanges ? EPCGChangeType::Edge : None) | (bChangedPins ? EPCGChangeType::Node : None
	};

	UpdatePins(MutableView(InputPins), InputPinProperties);
	UpdatePins(MutableView(OutputPins), OutputPinProperties);

	OnNodeChangedDelegate.Broadcast(this);
}

void UMovieGraphNode::UpdateDynamicProperties()
{
	TArray<FPropertyBagPropertyDesc> DesiredDynamicProperties = GetDynamicPropertyDescriptions();

	// Check to see if we need to remake our property bag
	bool bHasAllProperties = DynamicProperties.GetNumPropertiesInBag() == DesiredDynamicProperties.Num();
	if (bHasAllProperties)
	{
		// If there's still the same number of properties before/after, then we need to do the more expensive
		// check, which is to see if every property in the desired list is already inside the property bag.
		for (const FPropertyBagPropertyDesc& Desc : DesiredDynamicProperties)
		{
			bool bBagContainsProperty = DynamicProperties.FindPropertyDescByName(Desc.Name) != nullptr;
			if (!bBagContainsProperty)
			{
				bHasAllProperties = false;
				break;
			}
		}
	}

	// If we don't have all the properties in our bag already, we need to generate a new bag with the correct
	// layout, and then we have to migrate the existing bag over (so existing, matching, values stay).
	if (!bHasAllProperties)
	{
		FInstancedPropertyBag NewPropertyBag;
		NewPropertyBag.AddProperties(DesiredDynamicProperties);

		DynamicProperties.MigrateToNewBagInstance(NewPropertyBag);
	}
}

UMovieGraphConfig* UMovieGraphNode::GetGraph() const
{
	return Cast<UMovieGraphConfig>(GetOuter());
}

UMovieGraphPin* UMovieGraphNode::GetInputPin(const FName& Label) const
{
	for (UMovieGraphPin* InputPin : InputPins)
	{
		if (InputPin->Properties.Label == Label)
		{
			return InputPin;
		}
	}

	return nullptr;
}

UMovieGraphPin* UMovieGraphNode::GetOutputPin(const FName& Label) const
{
	for (UMovieGraphPin* OutputPin : OutputPins)
	{
		if (OutputPin->Properties.Label == Label)
		{
			return OutputPin;
		}
	}

	return nullptr;
}

#if WITH_EDITOR
FLinearColor UMovieGraphNode::GetNodeTitleColor() const
{
	return FLinearColor::Black;
}

FSlateIcon UMovieGraphNode::GetIconAndTint(FLinearColor& OutColor) const
{
	OutColor = FLinearColor::White;
	return FSlateIcon();
}
#endif // WITH_EDITOR

void UMovieGraphNode::PostLoad()
{
	Super::PostLoad();

	RegisterDelegates();
}

bool UMovieGraphNode::SetDynamicPropertyValue(const FName PropertyName, const FString& InNewValue)
{
	return DynamicProperties.SetValueSerializedString(PropertyName, InNewValue) == EPropertyBagResult::Success;
}

bool UMovieGraphNode::GetDynamicPropertyValue(const FName PropertyName, FString& OutValue)
{
	const TValueOrError<FString, EPropertyBagResult> Result = DynamicProperties.GetValueSerializedString(PropertyName);
	if (Result.IsValid())
	{
		OutValue = Result.GetValue();
	}

	return Result.IsValid();
}

const FBoolProperty* UMovieGraphNode::FindOverridePropertyForDynamicProperty(const FName& InPropertyName) const
{
	const FString OverridePropertyName = FString::Printf(TEXT("bOverride_%s"), *InPropertyName.ToString());
	const FPropertyBagPropertyDesc* Desc = DynamicProperties.FindPropertyDescByName(FName(OverridePropertyName));

	if (!Desc)
	{
		return nullptr;
	}

	if (const FBoolProperty* DescBoolProperty = CastField<FBoolProperty>(Desc->CachedProperty))
	{
		return DescBoolProperty;
	}
	
	return nullptr;
}

bool UMovieGraphNode::IsDynamicPropertyOverridden(const FName& InPropertyName) const
{
	if (const FBoolProperty* OverrideProperty = FindOverridePropertyForDynamicProperty(InPropertyName))
	{
		TValueOrError<bool, EPropertyBagResult> Result = DynamicProperties.GetValueBool(OverrideProperty->GetFName());
		return Result.IsValid() && Result.GetValue();
	}

	return false;
}

void UMovieGraphNode::SetDynamicPropertyOverridden(const FName& InPropertyName, const bool bIsOverridden)
{
	if (const FBoolProperty* OverrideProperty = FindOverridePropertyForDynamicProperty(InPropertyName))
	{
		DynamicProperties.SetValueBool(OverrideProperty->GetFName(), bIsOverridden);
	}
}

TArray<FMovieGraphPropertyInfo> UMovieGraphNode::GetOverrideablePropertyInfo() const
{
	TArray<FMovieGraphPropertyInfo> OverrideableProperties;
	
	for (TFieldIterator<FProperty> PropertyIterator(GetClass()); PropertyIterator; ++PropertyIterator)
	{
		if (UMovieGraphConfig::FindOverridePropertyForRealProperty(GetClass(), *PropertyIterator))
		{
			FMovieGraphPropertyInfo Info;
			Info.Name = PropertyIterator->GetFName();
			Info.bIsDynamicProperty = false;
			Info.ValueType = UE::MovieGraph::Private::GetValueTypeFromProperty(*PropertyIterator);
			
			OverrideableProperties.Add(MoveTemp(Info));
		}
	}

	for (const FPropertyBagPropertyDesc& Desc : GetDynamicPropertyDescriptions())
	{
		if (FindOverridePropertyForDynamicProperty(Desc.Name))
		{
			FMovieGraphPropertyInfo Info;
			Info.Name = Desc.Name;
			Info.bIsDynamicProperty = true;
			Info.ValueType = static_cast<EMovieGraphValueType>(Desc.ValueType);

			OverrideableProperties.Add(MoveTemp(Info));
		}
	}

	return OverrideableProperties;
}

TArray<UMovieGraphPin*> UMovieGraphNode::EvaluatePinsToFollow(FMovieGraphEvaluationContext& InContext) const
{
	TArray<UMovieGraphPin*> PinsToFollow;

	// By default we provide all Input Pins to this node that are the Branch Type.
	// You should override this in downstream nodes that need custom logic, such
	// as branch or switch nodes.
	for (const TObjectPtr<UMovieGraphPin>& InputPin : GetInputPins())
	{
		if (InputPin->Properties.bIsBranch)
		{
			PinsToFollow.Add(InputPin);
		}
	}
	return PinsToFollow;
}

TArray<FMovieGraphPinProperties> UMovieGraphSettingNode::GetInputPinProperties() const
{
	TArray<FMovieGraphPinProperties> Properties;
	Properties.Add(FMovieGraphPinProperties::MakeBranchProperties());
	return Properties;
}

TArray<FMovieGraphPinProperties> UMovieGraphSettingNode::GetOutputPinProperties() const
{
	TArray<FMovieGraphPinProperties> Properties;
	Properties.Add(FMovieGraphPinProperties::MakeBranchProperties());
	return Properties;
}
