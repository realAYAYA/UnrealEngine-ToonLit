// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeProjectorConstant.h"

#include "Containers/UnrealString.h"
#include "EdGraph/EdGraphPin.h"
#include "Internationalization/Internationalization.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "MuCOE/UnrealEditorPortabilityHelpers.h"
#include "UObject/UnrealType.h"

class UCustomizableObjectNodeRemapPins;

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


UCustomizableObjectNodeProjectorConstant::UCustomizableObjectNodeProjectorConstant()
	: Super()
	, ProjectionType(ECustomizableObjectProjectorType::Planar)
	, ProjectionAngle(360.0f)
	, BoneComboBoxLocation(FVector::ZeroVector)
	, BoneComboBoxForwardDirection(FVector::ZeroVector)
	, BoneComboBoxUpDirection(FVector::ZeroVector)
	, ParameterSetModified(-1)
{

}


void UCustomizableObjectNodeProjectorConstant::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	FProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	//if ( PropertyThatChanged && PropertyThatChanged->GetName() == TEXT("NumLODs") )
	{
	}

	const FName PropertyName = Helper_GetPropertyName(PropertyChangedEvent);

	if (PropertyName == "ProjectionType")
	{
		ParameterSetModified = 0;
	}
	else if (PropertyName == "ProjectionAngle")
	{
		Value.Angle = FMath::DegreesToRadians(ProjectionAngle);
		ParameterSetModified = 1;
	}
	else if (PropertyName == "ProjectorBone")
	{
		Value.Position = (FVector3f)BoneComboBoxLocation;
		Value.Direction = (FVector3f)BoneComboBoxForwardDirection;
		Value.Up = (FVector3f)BoneComboBoxUpDirection;
		ParameterSetModified = 2;
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}


void UCustomizableObjectNodeProjectorConstant::AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins)
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();

	FString PinName = TEXT("Value");
	UEdGraphPin* ValuePin = CustomCreatePin(EGPD_Output, Schema->PC_Projector, FName(*PinName));
	ValuePin->bDefaultValueIsIgnored = true;
}


FText UCustomizableObjectNodeProjectorConstant::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("Projector_Constant", "Projector Constant");
}


FLinearColor UCustomizableObjectNodeProjectorConstant::GetNodeTitleColor() const
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();
	return Schema->GetPinTypeColor(Schema->PC_Projector);
}


FText UCustomizableObjectNodeProjectorConstant::GetTooltipText() const
{
		return LOCTEXT("Projector_Constant_Tooltip",
			"Defines a constant projector.It can't move, scale or rotate at runtime. The texture that is projected can still be changed, depending on the configuration of the other inputs of the texture project node that is connected to the projector constant.");
}


#undef LOCTEXT_NAMESPACE
