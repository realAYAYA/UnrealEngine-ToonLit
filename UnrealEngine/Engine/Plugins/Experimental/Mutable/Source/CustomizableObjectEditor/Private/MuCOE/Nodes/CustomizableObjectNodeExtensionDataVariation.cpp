// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeExtensionDataVariation.h"

#include "MuCOE/ExtensionDataCompilerInterface.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "MuCOE/GraphTraversal.h"
#include "MuT/NodeExtensionDataVariation.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CustomizableObjectNodeExtensionDataVariation)

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"

void UCustomizableObjectNodeExtensionDataVariation::PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent)
{
	if (FProperty* PropertyThatChanged = InPropertyChangedEvent.Property)
	{
		ReconstructNode();
	}

	Super::PostEditChangeProperty(InPropertyChangedEvent);
}

FLinearColor UCustomizableObjectNodeExtensionDataVariation::GetNodeTitleColor() const
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();
	return Schema->GetPinTypeColor(GetCategory());
}

bool UCustomizableObjectNodeExtensionDataVariation::IsAffectedByLOD() const
{
	return false;
}

bool UCustomizableObjectNodeExtensionDataVariation::ShouldAddToContextMenu(FText& OutCategory) const
{
	OutCategory = FText::FromName(GetCategory());
	return true;
}

void UCustomizableObjectNodeExtensionDataVariation::AllocateDefaultPins(UCustomizableObjectNodeRemapPins* InRemapPins)
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();

	UEdGraphPin* OutputPin = CustomCreatePin(EGPD_Output, GetCategory(), GetOutputPinName());
	OutputPin->bDefaultValueIsIgnored = true;

	for (int32 VariationIndex = Variations.Num() - 1; VariationIndex >= 0; --VariationIndex)
	{
		constexpr bool bIsArray = false;
		UEdGraphPin* VariationPin = CustomCreatePin(EGPD_Input, GetCategory(), GetVariationPinName(VariationIndex), bIsArray);
		VariationPin->bDefaultValueIsIgnored = true;
		VariationPin->PinFriendlyName = FText::Format(LOCTEXT("VariationPinFriendlyName", "Variation {0} [{1}]"), { VariationIndex, FText::FromString(Variations[VariationIndex].Tag) });
	}

	UEdGraphPin* DefaultVariation = CustomCreatePin(EGPD_Input, GetCategory(), GetDefaultPinName(), false);
	DefaultVariation->bDefaultValueIsIgnored = true;
}

mu::NodeExtensionDataPtr UCustomizableObjectNodeExtensionDataVariation::GenerateMutableNode(FExtensionDataCompilerInterface& InCompilerInterface) const
{
	mu::NodeExtensionDataVariationPtr VariationNode = new mu::NodeExtensionDataVariation;

	if (const UEdGraphPin* ConnectedPin = FollowInputPin(*GetDefaultPin()))
	{
		if (const ICustomizableObjectExtensionNode* ExtensionNode = Cast<ICustomizableObjectExtensionNode>(ConnectedPin->GetOwningNode()))
		{
			if (mu::NodeExtensionDataPtr DefaultValueExtensionData = ExtensionNode->GenerateMutableNode(InCompilerInterface))
			{
				VariationNode->SetDefaultValue(DefaultValueExtensionData);
			}
			else
			{
				InCompilerInterface.CompilerLog(LOCTEXT("ExtensionDataVariationFailed", "Extension Data Variation failed"), this);
			}
		}
	}

	VariationNode->SetVariationCount(Variations.Num());
	for (int32 VariationIndex = 0; VariationIndex < Variations.Num(); ++VariationIndex)
	{
		if (const UEdGraphPin* VariationPin = GetVariationPin(VariationIndex))
		{
			VariationNode->SetVariationTag(VariationIndex, Variations[VariationIndex].Tag);
			if (const UEdGraphPin* ConnectedPin = FollowInputPin(*VariationPin))
			{
				if (const ICustomizableObjectExtensionNode* ExtensionNode = Cast<ICustomizableObjectExtensionNode>(ConnectedPin->GetOwningNode()))
				{
					if (mu::NodeExtensionDataPtr VariationExtensionData = ExtensionNode->GenerateMutableNode(InCompilerInterface))
					{
						VariationNode->SetVariationValue(VariationIndex, VariationExtensionData);
					}
				}
			}
		}
	}

	InCompilerInterface.AddGeneratedNode(this);

	return VariationNode;
}

FName UCustomizableObjectNodeExtensionDataVariation::GetDefaultPinName() const
{
	return TEXT("Default");
}

FName UCustomizableObjectNodeExtensionDataVariation::GetVariationPinName(int32 InIndex) const
{
	return FName{ *FString::Format(TEXT("Variation {0}"), { InIndex }) };
}

UEdGraphPin* UCustomizableObjectNodeExtensionDataVariation::GetDefaultPin() const
{
	return FindPin(GetDefaultPinName());
}

UEdGraphPin* UCustomizableObjectNodeExtensionDataVariation::GetVariationPin(int32 InIndex) const
{
	return FindPin(GetVariationPinName(InIndex));
}

int32 UCustomizableObjectNodeExtensionDataVariation::GetNumVariations() const
{
	return Variations.Num();
}

#undef LOCTEXT_NAMESPACE
