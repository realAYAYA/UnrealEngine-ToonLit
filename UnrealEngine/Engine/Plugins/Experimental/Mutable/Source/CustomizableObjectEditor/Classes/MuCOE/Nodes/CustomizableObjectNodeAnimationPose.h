// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "EdGraph/EdGraphNode.h"
#include "Internationalization/Text.h"
#include "Math/Color.h"
#include "Math/Transform.h"
#include "MuCOE/Nodes/CustomizableObjectNode.h"
#include "UObject/ObjectPtr.h"
#include "UObject/UObjectGlobals.h"

#include "CustomizableObjectNodeAnimationPose.generated.h"

class FString;
class UCustomizableObjectNodeRemapPins;
class UEdGraphPin;
class UObject;
class UPoseAsset;
struct FPropertyChangedEvent;


UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeAnimationPose : public UCustomizableObjectNode
{
public:
	GENERATED_BODY()

	UCustomizableObjectNodeAnimationPose();

	UPROPERTY(EditAnywhere, Category=CustomizableObject)
	TObjectPtr<UPoseAsset> PoseAsset;

	// UObject interface.
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	// Begin EdGraphNode interface
	UEdGraphPin* GetInputMeshPin() const;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	// End EdGraphNode interface

	// UCustomizableObjectNode interface
	void AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins) override;

	// Retrieve the pose information from the PoseAsset
	static void StaticRetrievePoseInformation(UPoseAsset* PoseAsset, class USkeletalMesh* RefSkeletalMesh, TArray<FString>& OutArrayBoneName, TArray<FTransform>& OutArrayTransform);
};

