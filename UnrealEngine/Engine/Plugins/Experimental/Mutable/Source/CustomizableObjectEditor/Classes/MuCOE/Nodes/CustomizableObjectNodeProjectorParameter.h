// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCOE/Nodes/CustomizableObjectNode.h"

#include "CustomizableObjectNodeProjectorParameter.generated.h"

namespace ENodeTitleType { enum Type : int; }

class UCustomizableObjectNodeRemapPins;
class UEdGraphPin;
class UObject;
struct FPropertyChangedEvent;


UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeProjectorParameter : public UCustomizableObjectNode
{
public:
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category=CustomizableObject, meta = (ShowOnlyInnerProperties))
	FCustomizableObjectProjector DefaultValue;

	UPROPERTY(EditAnywhere, Category=CustomizableObject)
	FString ParameterName = "Default Name";

	UPROPERTY(EditAnywhere, Category = UI, meta = (DisplayName = "Parameter UI Metadata"))
	FMutableParamUIMetadata ParamUIMetadata;

	UPROPERTY(EditAnywhere, Category = CustomizableObject, Meta = (DisplayName = "Projection Angle (degrees)"))
	float ProjectionAngle = 360.0f;

	UPROPERTY(EditAnywhere, Category = ProjectorSnapToBone)
	uint32 ReferenceSkeletonIndex = 0;

	UPROPERTY(EditAnywhere, Category = ProjectorSnapToBone)
	FName ProjectorBone;

	/** Temporary variable where to put the location information for bone combo box selection changes (in FCustomizableObjectNodeProjectorParameterDetails)*/
	FVector BoneComboBoxLocation = FVector::ZeroVector;

	/** Temporary variable where to put the direction information for bone combo box selection changes (in FCustomizableObjectNodeProjectorParameterDetails)*/
	FVector BoneComboBoxForwardDirection = FVector::ZeroVector;

	/** Temporary variable where to put the direction information for bone combo box selection changes (in FCustomizableObjectNodeProjectorParameterDetails)*/
	FVector BoneComboBoxUpDirection = FVector::ZeroVector;

	// UObject interface.
	void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	// Begin EdGraphNode interface
	FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	FLinearColor GetNodeTitleColor() const override;
	FText GetTooltipText() const override;

	// UCustomizableObjectNode interface
	void AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins) override;
	virtual bool IsAffectedByLOD() const override { return false; }
	virtual void BackwardsCompatibleFixup() override;

	// Own interface
	UEdGraphPin* ProjectorPin() const
	{
		return FindPin(TEXT("Value"));
	}

	ECustomizableObjectProjectorType GetProjectorType() const;

	FVector GetProjectorDefaultPosition() const;
	
	void SetProjectorDefaultPosition(const FVector& Position);

	FVector GetProjectorDefaultDirection() const;

	void SetProjectorDefaultDirection(const FVector& Direction);

	FVector GetProjectorDefaultUp() const;

	void SetProjectorDefaultUp(const FVector& Up);

	FVector GetProjectorDefaultScale() const;

	void SetProjectorDefaultScale(const FVector& Scale);

	float GetProjectorDefaultAngle() const;

	void SetProjectorDefaultAngle(float Angle);

private:

	UPROPERTY()
	ECustomizableObjectProjectorType ProjectionType_DEPRECATED;
};

