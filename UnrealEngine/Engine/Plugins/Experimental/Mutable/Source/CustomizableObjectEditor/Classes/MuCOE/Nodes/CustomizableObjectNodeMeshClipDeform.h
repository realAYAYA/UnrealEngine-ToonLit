// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "EdGraph/EdGraphNode.h"
#include "HAL/Platform.h"
#include "Internationalization/Text.h"
#include "Math/Color.h"
#include "MuCOE/Nodes/CustomizableObjectNodeModifierBase.h"
#include "UObject/UObjectGlobals.h"

#include "CustomizableObjectNodeMeshClipDeform.generated.h"

class UCustomizableObjectNodeRemapPins;
class UEdGraphPin;
class UObject;

UENUM()
enum class EShapeBindingMethod : uint32
{
	ClosestProject = 0,
	ClosestToSurface = 1,
	NormalProject = 2
};

UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeMeshClipDeform : public UCustomizableObjectNodeModifierBase
{
public:
	GENERATED_BODY()

	UCustomizableObjectNodeMeshClipDeform();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = MeshClipDeform)
	TArray<FString> Tags;
	
	UPROPERTY(EditAnywhere, Category = MeshClipDeform)
	EShapeBindingMethod BindingMethod;
	
	// Begin EdGraphNode interface
	FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	FLinearColor GetNodeTitleColor() const override;
	FText GetTooltipText() const override;

	// UCustomizableObjectNode interface
	void AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins) override;
	
	inline UEdGraphPin* ClipShapePin() const
	{
		return FindPin(TEXT("Clip Shape"), EGPD_Input);
	}

	UEdGraphPin* OutputPin() const override
	{
		return FindPin(TEXT("Material"));
	}
};

