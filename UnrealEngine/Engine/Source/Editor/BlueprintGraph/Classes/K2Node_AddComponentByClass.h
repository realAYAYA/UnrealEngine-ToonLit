// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "CoreMinimal.h"
#include "Internationalization/Text.h"
#include "K2Node_ConstructObjectFromClass.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "K2Node_AddComponentByClass.generated.h"

class FKismetCompilerContext;
class UClass;
class UEdGraph;
class UEdGraphPin;
class UObject;

/**
 * Implementation of K2Node for creating a component based on a selected or passed in class
 */
UCLASS()
class BLUEPRINTGRAPH_API UK2Node_AddComponentByClass : public UK2Node_ConstructObjectFromClass
{
	GENERATED_BODY()

public:
	UK2Node_AddComponentByClass(const FObjectInitializer& ObjectInitializer);
	
	//~ Begin UEdGraphNode Interface.
	virtual void AllocateDefaultPins() override;
	virtual void ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	virtual void PinDefaultValueChanged(UEdGraphPin* Pin) override;
	//~ End UEdGraphNode Interface.

	//~ Begin UK2Node_ConstructObjectFromClass Interface
	virtual void CreatePinsForClass(UClass* InClass, TArray<UEdGraphPin*>* OutClassPins) override;

protected:
	virtual FText GetBaseNodeTitle() const override;
	virtual FText GetDefaultNodeTitle() const override;
	virtual FText GetNodeTitleFormat() const override;
	virtual UClass* GetClassPinBaseClass() const override;
	virtual bool IsSpawnVarPin(UEdGraphPin* Pin) const override;
	//~ End UK2Node_ConstructObjectFromClass Interface

	UEdGraphPin* GetRelativeTransformPin() const;
	UEdGraphPin* GetManualAttachmentPin() const;

	/** Returns true if the currently selected or linked class is known to be a scene component */
	bool IsSceneComponent() const;

	/** Utility function to set whether the scene component specific pins are hidden or not */
	void SetSceneComponentPinsHidden(bool bHidden);
};
