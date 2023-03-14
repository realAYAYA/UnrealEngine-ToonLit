// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BlueprintNodeSignature.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNodeUtils.h"
#include "InputAction.h"
#include "K2Node.h"
#include "K2Node_EventNodeInterface.h"
#include "Textures/SlateIcon.h"
#include "UObject/ObjectMacros.h"

#include "K2Node_EnhancedInputAction.generated.h"

class FBlueprintActionDatabaseRegistrar;
class FKismetCompilerContext;

UCLASS()
class INPUTBLUEPRINTNODES_API UK2Node_EnhancedInputAction : public UK2Node, public IK2Node_EventNodeInterface
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	TObjectPtr<const UInputAction> InputAction;

	//~ Begin UEdGraphNode Interface.
	virtual void AllocateDefaultPins() override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
	virtual bool IsCompatibleWithGraph(UEdGraph const* Graph) const override;
	virtual UObject* GetJumpTargetForDoubleClick() const override;
	virtual void JumpToDefinition() const override;
	//~ End UEdGraphNode Interface.

	//~ Begin UK2Node Interface
	virtual void ValidateNodeDuringCompilation(class FCompilerResultsLog& MessageLog) const override;
	virtual bool ShouldShowNodeProperties() const override { return true; }
	virtual void ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual FText GetMenuCategory() const override;
	virtual bool CanUserEditPinAdvancedViewFlag() const override { return true; }
	virtual FBlueprintNodeSignature GetSignature() const override;
	virtual void PostReconstructNode();
	virtual void PinConnectionListChanged(UEdGraphPin* Pin);
	virtual bool IsConnectionDisallowed(const UEdGraphPin* MyPin, const UEdGraphPin* OtherPin, FString& OutReason) const override;
	//~ End UK2Node Interface

	//~ Begin IK2Node_EventNodeInterface Interface.
	virtual TSharedPtr<FEdGraphSchemaAction> GetEventNodeAction(const FText& ActionCategory) override;
	//~ End IK2Node_EventNodeInterface Interface.

private:
	FName GetActionName() const;
	void HideEventPins(UEdGraphPin* RetainPin);

	/** Gets the ETriggerEvent from an exec pin based on the Pins name. */
	const ETriggerEvent GetTriggerTypeFromExecPin(const UEdGraphPin* ExecPin) const;
	
	/** Constructing FText strings can be costly, so we cache the node's title/tooltip */
	FNodeTextCache CachedTooltip;
	FNodeTextCache CachedNodeTitle;
};
