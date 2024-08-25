// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCOE/Nodes/CustomizableObjectNodeModifierBase.h"
#include "MuT/NodeModifier.h"

#include "CustomizableObjectNodeMeshClipMorph.generated.h"

namespace ENodeTitleType { enum Type : int; }

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

	UPROPERTY(EditAnywhere, Category = MeshToClipAndMorph)
	FName BoneName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = MeshToClipAndMorph)
	TArray<FString> Tags;

	/** Policy to use tags in case more than one is added. */
	UPROPERTY(EditAnywhere, Category = MeshToClipAndMorph)
	EMutableMultipleTagPolicy MultipleTagPolicy = EMutableMultipleTagPolicy::OnlyOneRequired;

	UPROPERTY(EditAnywhere, Category = MeshToClipAndMorph)
	uint32 ReferenceSkeletonIndex = 0;

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

private:
	UPROPERTY()
	bool bOldOffset_DEPRECATED;

public:
	UPROPERTY(EditAnywhere, Category = MeshClipParameters, meta = (DisplayName = "Max Effect Radius", ToolTip = "The maximum distance from the origin of the widget where vertices will be affected. If negative, there will be no limit."))
	float MaxEffectRadius;

	bool bUpdateViewportWidget;

	UCustomizableObjectNodeMeshClipMorph();

	// UObject interface.
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void Serialize(FArchive& Ar) override;

	// EdGraphNode interface
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FText GetTooltipText() const override;

	// UCustomizableObjectNode interface
	virtual void AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins) override;
	virtual void PinConnectionListChanged(UEdGraphPin* Pin) override;
	
	// UCustomizableObjectNodeMaterialBase interface
	virtual UEdGraphPin* OutputPin() const override;

	// Own interface
	FVector GetOriginWithOffset() const;
	void FindLocalAxes(FVector& Right, FVector& Up, FVector& Forward) const;

	// Change StartOffset from World to Local of the other way around
	void ChangeStartOffsetTransform();
};

