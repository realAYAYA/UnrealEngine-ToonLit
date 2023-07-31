// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraphNodeUtils.h"
#include "LiveLinkRole.h"
#include "K2Node.h"
#include "Textures/SlateIcon.h"
#include "UObject/ObjectMacros.h"

#include "K2Node_EvaluateLiveLinkFrame.generated.h"

class FBlueprintActionDatabaseRegistrar;
class FKismetCompilerContext;
class UDataTable;
class UEdGraph;
class UK2Node_CallFunction;


UCLASS(Abstract)
class LIVELINKGRAPHNODE_API UK2Node_EvaluateLiveLinkFrame : public UK2Node
{
	GENERATED_BODY()

public:
	//~ Begin UEdGraphNode Interface.
	virtual void AllocateDefaultPins() override;
	virtual void PinDefaultValueChanged(UEdGraphPin* Pin) override;
	virtual FText GetTooltipText() const override;
	virtual void ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
	virtual void PostReconstructNode() override;
	//~ End UEdGraphNode Interface.

	//~ Begin UK2Node Interface
	virtual bool IsNodeSafeToIgnore() const override { return true; }
	virtual void ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins) override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual FText GetMenuCategory() const override;
	virtual bool IsConnectionDisallowed(const UEdGraphPin* MyPin, const UEdGraphPin* OtherPin, FString& OutReason) const override;
	virtual void EarlyValidation(class FCompilerResultsLog& MessageLog) const override;
	virtual void PreloadRequiredAssets() override;
	virtual void NotifyPinConnectionListChanged(UEdGraphPin* Pin) override;
	//~ End UK2Node Interface

	/** Get the return types of our struct */
	UScriptStruct* GetReturnTypeForOutputDataStruct() const;

	/** Get the then output pin */
	UEdGraphPin* GetThenPin() const;

	/** Get the Live Link Role input pin */
	UEdGraphPin* GetLiveLinkRolePin() const;

	/** Get the Live Link Subject input pin */
	UEdGraphPin* GetLiveLinkSubjectPin() const;

	/** Get the exec output pin for when no frame is available for the desired role */
	UEdGraphPin* GetFrameNotAvailablePin() const;

	/** Get the result output pin */
	UEdGraphPin* GetResultingDataPin() const;

	/** Get the type that the Live Link Role evaluates to to return */
	UScriptStruct* GetLiveLinkRoleOutputStructType() const;
	UScriptStruct* GetLiveLinkRoleOutputFrameStructType() const;

protected:

	/** Get the name of the UFunction we are trying to build from */
	virtual FName GetEvaluateFunctionName() const PURE_VIRTUAL(UK2Node_EvaluateLiveLinkFrame::GetEvaluateFunctionName, return NAME_None; );

	/** Add additionnal pin that the evaluate function would need */
	virtual void AddPins(FKismetCompilerContext& CompilerContext, UK2Node_CallFunction* EvaluateLiveLinkFrameFunction) PURE_VIRTUAL(UK2Node_EvaluateLiveLinkFrame::AddPins, );

	/**
	 * Takes the specified "MutatablePin" and sets its 'PinToolTip' field (according
	 * to the specified description)
	 *
	 * @param   MutatablePin	The pin you want to set tool-tip text on
	 * @param   PinDescription	A string describing the pin's purpose
	 */
	void SetPinToolTip(UEdGraphPin& InOutMutatablePin, const FText& InPinDescription) const;

	/** Set the return type of our structs */
	void SetReturnTypeForOutputStruct(UScriptStruct* InClass);

	/** Queries for the authoritative return type, then modifies the return pin to match */
	void RefreshDataOutputPinType();

	TSubclassOf<ULiveLinkRole> GetDefaultRolePinValue() const;

	/** Verify if selected role class is valid for evaluation */
	bool IsRoleValidForEvaluation(TSubclassOf<ULiveLinkRole> InRoleClass) const;
};
