// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EdGraph/EdGraphNode.h"
#include "Internationalization/Text.h"
#include "Math/Color.h"
#include "Math/UnrealMathSSE.h"
#include "Math/Vector.h"
#include "MuCO/CustomizableObjectParameterTypeDefinitions.h"
#include "MuCO/CustomizableObjectUIData.h"
#include "MuCOE/Nodes/CustomizableObjectNode.h"
#include "UObject/NameTypes.h"
#include "UObject/UObjectGlobals.h"

#include "CustomizableObjectNodeProjectorConstant.generated.h"

class UCustomizableObjectNodeRemapPins;
class UEdGraphPin;
class UObject;
struct FPropertyChangedEvent;


UCLASS(hideCategories = (CustomizableObjectHide))
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeProjectorConstant : public UCustomizableObjectNode
{
public:
	GENERATED_BODY()

	UCustomizableObjectNodeProjectorConstant();

	/**  */
	UPROPERTY(EditAnywhere, Category= CustomizableObjectHide)
	FCustomizableObjectProjector Value;

	UPROPERTY(EditAnywhere, Category = UI, meta = (DisplayName = "Parameter UI Metadata"))
	FMutableParamUIMetadata ParamUIMetadata;

	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	ECustomizableObjectProjectorType ProjectionType;

	UPROPERTY(EditAnywhere, Category = CustomizableObject, Meta = (DisplayName = "Projection Angle (degrees)"))
	float ProjectionAngle;

	UPROPERTY(EditAnywhere, Category = ProjectorSnapToBone)
	uint32 ReferenceSkeletonIndex = 0;

	UPROPERTY(EditAnywhere, Category = ProjectorSnapToBone)
	FName ProjectorBone;

	/** Temporary variable where to put the location information for bone combo box selection changes (in FCustomizableObjectNodeProjectorParameterDetails)*/
	FVector BoneComboBoxLocation;

	/** Temporary variable where to put the direction information for bone combo box selection changes (in FCustomizableObjectNodeProjectorParameterDetails)*/
	FVector BoneComboBoxForwardDirection;

	/** Temporary variable where to put the direction information for bone combo box selection changes (in FCustomizableObjectNodeProjectorParameterDetails)*/
	FVector BoneComboBoxUpDirection;

	/** Flag to know which parameters have been modified of the node in the details tab.
	* 0 Means projection type
	* 1 projection angle
	* 2 projector transform */
	int32 ParameterSetModified;

	// UObject interface.
	void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	// Begin EdGraphNode interface
	FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	FLinearColor GetNodeTitleColor() const override;
	FText GetTooltipText() const override;

	// UCustomizableObjectNode interface
	void AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins) override;

	UEdGraphPin* ValuePin();
};

