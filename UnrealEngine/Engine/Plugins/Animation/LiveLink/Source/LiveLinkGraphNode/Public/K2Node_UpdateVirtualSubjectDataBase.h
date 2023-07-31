// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "K2Node.h"
#include "Textures/SlateIcon.h"

#include "K2Node_UpdateVirtualSubjectDataBase.generated.h"

class FBlueprintActionDatabaseRegistrar;
class FKismetCompilerContext;
class UDataTable;
class UEdGraph;
class UK2Node_CallFunction;
class ULiveLinkRole;

UCLASS(Abstract)
class LIVELINKGRAPHNODE_API UK2Node_UpdateVirtualSubjectDataBase : public UK2Node
{
	GENERATED_BODY()

public:
	//~ Begin UEdGraphNode Interface.
	virtual void AllocateDefaultPins() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual void ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
	virtual bool IsCompatibleWithGraph(const UEdGraph* TargetGraph) const override;
	//~ End UEdGraphNode Interface.

	//~ Begin UK2Node Interface
	virtual bool IsNodeSafeToIgnore() const override { return true; }
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual FText GetMenuCategory() const override;
	virtual bool IsConnectionDisallowed(const UEdGraphPin* MyPin, const UEdGraphPin* OtherPin, FString& OutReason) const override;
	virtual void EarlyValidation(class FCompilerResultsLog& MessageLog) const override;
	//~ End UK2Node Interface


	/** Get the then output pin */
	UEdGraphPin* GetThenPin() const;

	/** Get the Live Link Struct input pin */
	UEdGraphPin* GetLiveLinkStructPin() const;

protected:
	/**
	 * Takes the specified "MutatablePin" and sets its 'PinToolTip' field (according
	 * to the specified description)
	 *
	 * @param   MutatablePin	The pin you want to set tool-tip text on
	 * @param   PinDescription	A string describing the pin's purpose
	 */
	void SetPinToolTip(UEdGraphPin& InOutMutatablePin, const FText& InPinDescription) const;

	/** Returns Struct type associated to the subject's role (static or frame) */
	virtual UScriptStruct* GetStructTypeFromRole(ULiveLinkRole* Role) const PURE_VIRTUAL(UK2Node_UpdateVirtualSubjectDataBase::GetStructTypeFromRole, return nullptr; );

	/** Returns the custom thunk function name */
	virtual FName GetUpdateFunctionName() const PURE_VIRTUAL(UK2Node_UpdateVirtualSubjectDataBase::GetUpdateFunctionName, return NAME_None; );

	/** Returns the struct display name */
	virtual FText GetStructPinName() const PURE_VIRTUAL(UK2Node_UpdateVirtualSubjectDataBase::GetStructPinName, return FText::GetEmpty(); );

	/** Add additionnal pins that the update subject function could need */
	virtual void AddPins(FKismetCompilerContext& CompilerContext, UK2Node_CallFunction* UpdateVirtualSubjectDataFunction) const PURE_VIRTUAL(UK2Node_UpdateVirtualSubjectDataBase::AddPins, );

private:

	/** Returns the Struct type associated to the role */
	UScriptStruct* GetStructTypeFromBlueprint() const;

private:
	
	/** Name of the struct pin */
	static const FName LiveLinkStructPinName;

	/** Struct pin description */
	static const FText LiveLinkStructPinDescription;
};
