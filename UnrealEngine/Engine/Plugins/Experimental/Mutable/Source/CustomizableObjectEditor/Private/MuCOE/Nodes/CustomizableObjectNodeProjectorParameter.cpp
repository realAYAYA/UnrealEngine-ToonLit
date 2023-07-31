// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeProjectorParameter.h"

#include "EdGraph/EdGraphPin.h"
#include "Internationalization/Internationalization.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "MuCOE/UnrealEditorPortabilityHelpers.h"
#include "UObject/UnrealType.h"

class UCustomizableObjectNodeRemapPins;

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"

void UCustomizableObjectNodeProjectorParameter::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	FProperty* PropertyThatChanged = PropertyChangedEvent.Property;

	const FName PropertyName = Helper_GetPropertyName(PropertyChangedEvent);

	if (PropertyName == "ProjectionType")
	{
		ParameterSetModified = 0;
	}
	else if (PropertyName == "ProjectionAngle")
	{
		DefaultValue.Angle = FMath::DegreesToRadians(ProjectionAngle);
		ParameterSetModified = 1;
	}
	else if (PropertyName == "ProjectorBone")
	{
		DefaultValue.Position = (FVector3f)BoneComboBoxLocation;
		DefaultValue.Direction = (FVector3f)BoneComboBoxForwardDirection;
		DefaultValue.Up = (FVector3f)BoneComboBoxUpDirection;
		ParameterSetModified = 2;
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}


void UCustomizableObjectNodeProjectorParameter::AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins)
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();

	FString PinName = TEXT("Value");
	UEdGraphPin* ValuePin = CustomCreatePin(EGPD_Output, Schema->PC_Projector, FName(*PinName));
	ValuePin->bDefaultValueIsIgnored = true;
}


FText UCustomizableObjectNodeProjectorParameter::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("Projector_Parameter", "Projector Parameter");
}


FLinearColor UCustomizableObjectNodeProjectorParameter::GetNodeTitleColor() const
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();
	return Schema->GetPinTypeColor(Schema->PC_Projector);
}


FText UCustomizableObjectNodeProjectorParameter::GetTooltipText() const
{
	return LOCTEXT("Projector_Parameter_Tooltip", "Exposes a runtime modifiable projector parameter from the Customizable Object.");
}

#undef LOCTEXT_NAMESPACE
