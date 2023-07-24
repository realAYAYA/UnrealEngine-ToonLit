// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "K2Node.h"

#include "Library/DMXEntityReference.h"

#include "K2Node_GetDMXFixturePatch.generated.h"

class FNodeHandlingFunctor;
class FKismetCompilerContext;
class UDMXEntityFixturePatch;

UCLASS()
class DMXBLUEPRINTGRAPH_API UK2Node_GetDMXFixturePatch
	: public UK2Node
{
	GENERATED_BODY()

public:
	//~ Begin UObject interface
	virtual void Serialize(FArchive& Ar) override;
	//~ End UObject interface

	//~ Begin UEdGraphNode Interface
	virtual void AllocateDefaultPins() override;
	virtual FText GetTooltipText() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual void PinDefaultValueChanged(UEdGraphPin* Pin) override;
	//~ End UEdGraphNode Interface

	//~ Begin K2Node Interface
	virtual bool IsNodePure() const override { return true; }
	virtual void ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual FText GetMenuCategory() const override;
	//~ End K2Node Interface

public:
	UEdGraphPin* GetInputDMXFixturePatchPin() const;
	UEdGraphPin* GetOutputDMXFixturePatchPin() const;

	/** Gets the DMX Fixture Patch pin value as String */
	FString GetFixturePatchValueAsString() const;
	/** Gets the DMX Fixture Patch Reference from input pin */
	FDMXEntityFixturePatchRef GetFixturePatchRefFromPin() const;
	/** Updates values in the Fixture Patch input pin struct */
	void SetInFixturePatchPinValue(const FDMXEntityFixturePatchRef& InPatchRef) const;

protected:
	void NotifyInputChanged();

public:
	static const FName InputDMXFixturePatchPinName;
	static const FName OutputDMXFixturePatchPinName;
};
