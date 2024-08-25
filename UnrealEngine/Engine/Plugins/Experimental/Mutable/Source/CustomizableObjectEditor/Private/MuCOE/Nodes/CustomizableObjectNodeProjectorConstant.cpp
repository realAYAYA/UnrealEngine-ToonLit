// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeProjectorConstant.h"

#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "MuCOE/UnrealEditorPortabilityHelpers.h"

class UCustomizableObjectNodeRemapPins;

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


UCustomizableObjectNodeProjectorConstant::UCustomizableObjectNodeProjectorConstant()
	: Super()
	, ProjectionAngle(360.0f)
	, BoneComboBoxLocation(FVector::ZeroVector)
	, BoneComboBoxForwardDirection(FVector::ZeroVector)
	, BoneComboBoxUpDirection(FVector::ZeroVector)
{

}


void UCustomizableObjectNodeProjectorConstant::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	const FName PropertyName = PropertyChangedEvent.GetPropertyName();

	if (PropertyName == "ProjectionAngle")
	{
		Value.Angle = FMath::DegreesToRadians(ProjectionAngle);
	}
	else if (PropertyName == "ProjectorBone")
	{
		Value.Position = (FVector3f)BoneComboBoxLocation;
		Value.Direction = (FVector3f)BoneComboBoxForwardDirection;
		Value.Up = (FVector3f)BoneComboBoxUpDirection;
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


void UCustomizableObjectNodeProjectorConstant::BackwardsCompatibleFixup()
{
	Super::BackwardsCompatibleFixup();

	const int32 CustomizableObjectCustomVersion = GetLinkerCustomVersion(FCustomizableObjectCustomVersion::GUID);

	if (CustomizableObjectCustomVersion < FCustomizableObjectCustomVersion::ProjectorNodesDefaultValueFix)
	{
		Value.ProjectionType = ProjectionType_DEPRECATED;
	}
}


ECustomizableObjectProjectorType UCustomizableObjectNodeProjectorConstant::GetProjectorType() const
{
	return Value.ProjectionType;
}


FVector UCustomizableObjectNodeProjectorConstant::GetProjectorPosition() const
{
	return static_cast<FVector>(Value.Position);
}


void UCustomizableObjectNodeProjectorConstant::SetProjectorPosition(const FVector& Position)
{
	Value.Position = static_cast<FVector3f>(Position);
}


FVector UCustomizableObjectNodeProjectorConstant::GetProjectorDirection() const
{
	return static_cast<FVector>(Value.Direction);
}


void UCustomizableObjectNodeProjectorConstant::SetProjectorDirection(const FVector& Direction)
{
	Value.Direction = static_cast<FVector3f>(Direction);
}


FVector UCustomizableObjectNodeProjectorConstant::GetProjectorUp() const
{
	return static_cast<FVector>(Value.Up);
}


void UCustomizableObjectNodeProjectorConstant::SetProjectorUp(const FVector& Up)
{
	Value.Up = static_cast<FVector3f>(Up);
}


FVector UCustomizableObjectNodeProjectorConstant::GetProjectorScale() const
{
	return static_cast<FVector>(Value.Scale);
}


void UCustomizableObjectNodeProjectorConstant::SetProjectorScale(const FVector& Scale)
{
	Value.Scale = static_cast<FVector3f>(Scale);
}


float UCustomizableObjectNodeProjectorConstant::GetProjectorAngle()
{
	return ProjectionAngle;
}


void UCustomizableObjectNodeProjectorConstant::SetProjectorAngle(float Angle)
{
	ProjectionAngle = Angle;
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
