// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeProjectorParameter.h"

#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "MuCOE/UnrealEditorPortabilityHelpers.h"

class UCustomizableObjectNodeRemapPins;

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"

void UCustomizableObjectNodeProjectorParameter::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	const FName PropertyName = PropertyChangedEvent.GetPropertyName();

	if (PropertyName == "ProjectionAngle")
	{
		DefaultValue.Angle = FMath::DegreesToRadians(ProjectionAngle);
	}
	else if (PropertyName == "ProjectorBone")
	{
		DefaultValue.Position = (FVector3f)BoneComboBoxLocation;
		DefaultValue.Direction = (FVector3f)BoneComboBoxForwardDirection;
		DefaultValue.Up = (FVector3f)BoneComboBoxUpDirection;
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


void UCustomizableObjectNodeProjectorParameter::BackwardsCompatibleFixup()
{
	Super::BackwardsCompatibleFixup();

	const int32 CustomizableObjectCustomVersion = GetLinkerCustomVersion(FCustomizableObjectCustomVersion::GUID);

	if (CustomizableObjectCustomVersion < FCustomizableObjectCustomVersion::ProjectorNodesDefaultValueFix)
	{
		DefaultValue.ProjectionType = ProjectionType_DEPRECATED;
	}
}


ECustomizableObjectProjectorType UCustomizableObjectNodeProjectorParameter::GetProjectorType() const
{
	return DefaultValue.ProjectionType;
}


FVector UCustomizableObjectNodeProjectorParameter::GetProjectorDefaultPosition() const
{
	return static_cast<FVector>(DefaultValue.Position);
}


void UCustomizableObjectNodeProjectorParameter::SetProjectorDefaultPosition(const FVector& Position)
{
	DefaultValue.Position = static_cast<FVector3f>(Position);
}


FVector UCustomizableObjectNodeProjectorParameter::GetProjectorDefaultDirection() const
{
	return static_cast<FVector>(DefaultValue.Direction);
}


void UCustomizableObjectNodeProjectorParameter::SetProjectorDefaultDirection(const FVector& Direction)
{
	DefaultValue.Direction = static_cast<FVector3f>(Direction);
}


FVector UCustomizableObjectNodeProjectorParameter::GetProjectorDefaultUp() const
{
	return static_cast<FVector>(DefaultValue.Up);
}


void UCustomizableObjectNodeProjectorParameter::SetProjectorDefaultUp(const FVector& Up)
{
	DefaultValue.Up = static_cast<FVector3f>(Up);
}


FVector UCustomizableObjectNodeProjectorParameter::GetProjectorDefaultScale() const
{
	return static_cast<FVector>(DefaultValue.Scale);
}


void UCustomizableObjectNodeProjectorParameter::SetProjectorDefaultScale(const FVector& Scale)
{
	DefaultValue.Scale = static_cast<FVector3f>(Scale);
}


float UCustomizableObjectNodeProjectorParameter::GetProjectorDefaultAngle() const
{
	return ProjectionAngle;
}


void UCustomizableObjectNodeProjectorParameter::SetProjectorDefaultAngle(float Angle)
{
	ProjectionAngle = Angle;
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
