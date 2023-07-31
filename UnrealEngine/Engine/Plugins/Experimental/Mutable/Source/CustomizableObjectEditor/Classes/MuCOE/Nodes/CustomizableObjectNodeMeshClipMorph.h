// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "EdGraph/EdGraphNode.h"
#include "Internationalization/Text.h"
#include "Math/Color.h"
#include "Math/UnrealMathSSE.h"
#include "Math/Vector.h"
#include "MuCOE/Nodes/CustomizableObjectNodeModifierBase.h"
#include "UObject/NameTypes.h"
#include "UObject/UObjectGlobals.h"

#include "CustomizableObjectNodeMeshClipMorph.generated.h"

class FArchive;
class UCustomizableObjectNodeRemapPins;
class UEdGraphPin;
class UObject;
struct FPropertyChangedEvent;

UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeMeshClipMorph : public UCustomizableObjectNodeModifierBase
{
public:
	GENERATED_BODY()

	UCustomizableObjectNodeMeshClipMorph();

	UPROPERTY(EditAnywhere, Category = MeshToClipAndMorph)
	FName BoneName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = MeshToClipAndMorph)
	TArray<FString> Tags;

	UPROPERTY(EditAnywhere, Category = MeshToClipAndMorph)
	uint32 ReferenceSkeletonIndex = 0;

	// UObject interface.
	void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void Serialize(FArchive& Ar) override;

	// Begin EdGraphNode interface
	FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	FLinearColor GetNodeTitleColor() const override;
	FText GetTooltipText() const override;

	// UCustomizableObjectNode interface
	void AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins) override;
	void PinConnectionListChanged(UEdGraphPin* Pin) override;

	UPROPERTY(EditAnywhere, Category = MeshClipParameters, meta = (DisplayName="Morph Start Offset", ToolTip="Offset from the origin of the selected bone to the actual start of the morph."))
	FVector StartOffset;

	UPROPERTY(EditAnywhere, Category = MeshClipParameters, meta = (DisplayName = "Local Start Offset", ToolTip = "Toggles between a local or global start offset."))
	bool bLocalStartOffset;

	UPROPERTY(EditAnywhere, Category = MeshClipParameters, meta = (DisplayName="Morph length", ToolTip="The length from the morph start to the clip plane."))
	float B;

	UPROPERTY(EditAnywhere, Category = MeshClipParameters, meta = (DisplayName="Ellipse Radius 1", ToolTip="First radius of the ellipse that the mesh is morphed into."))
	float Radius;

	UPROPERTY(EditAnywhere, Category = MeshClipParameters, meta = (DisplayName = "Ellipse Radius 2", ToolTip = "Second radius of the ellipse that the mesh is morphed into."))
	float Radius2;

	UPROPERTY(EditAnywhere, Category = MeshClipParameters, meta = (DisplayName = "Ellipse Rotation", ToolTip = "Ellipse Rotation in degrees around the bone axis."))
	float RotationAngle;

	UPROPERTY(EditAnywhere, Category = MeshClipParameters, meta = (DisplayName = "Morph Curve Control", ToolTip = "Controls the morph curve shape. A value of 1 is linear, less than 1 is concave and greater than 1 convex."))
	float Exponent;

	UPROPERTY(EditAnywhere, Category = MeshClipParameters, meta = (DisplayName = "Invert normal direction", ToolTip = "Flag to invert the normal direction"))
	bool bInvertNormal;

	UPROPERTY()
	FVector Origin;

	UPROPERTY()
	FVector Normal;
	
	UPROPERTY(EditAnywhere, Category = MeshClipParameters, meta = (DisplayName = "Max Effect Radius", ToolTip = "The maximum distance from the origin of the widget where vertices will be affected. If negative, there will be no limit."))
	float MaxEffectRadius;

	bool bUpdateViewportWidget;

	FVector GetOriginWithOffset() const;

	void FindLocalAxes(FVector& Right, FVector& Up, FVector& Forward) const;

	// Change StartOffset from World to Local of the other way arround
	void ChangeStartOffsetTransform();

	virtual UEdGraphPin* OutputPin() const override
	{
		return FindPin(TEXT("Material"));
	}
};

