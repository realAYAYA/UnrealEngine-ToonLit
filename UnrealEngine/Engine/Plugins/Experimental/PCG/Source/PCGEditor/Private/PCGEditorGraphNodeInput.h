// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "PCGEditorGraphNodeBase.h"

#include "PCGEditorGraphNodeInput.generated.h"

UCLASS()
class UPCGEditorGraphNodeInput : public UPCGEditorGraphNodeBase
{
	GENERATED_BODY()

public:
	// ~Begin UEdGraphNode interface
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual void AllocateDefaultPins() override;
	virtual bool CanUserDeleteNode() const override { return false; }
	virtual bool CanDuplicateNode() const override { return false; }
	virtual void ReconstructNode() override;
	// ~End UEdGraphNode interface
};
