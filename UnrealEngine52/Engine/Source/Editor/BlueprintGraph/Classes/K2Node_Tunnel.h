// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "HAL/Platform.h"
#include "Internationalization/Text.h"
#include "K2Node.h"
#include "K2Node_EditablePinBase.h"
#include "Templates/SharedPointer.h"
#include "UObject/NameTypes.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ObjectPtr.h"
#include "UObject/UObjectGlobals.h"

#include "K2Node_Tunnel.generated.h"

class FArchive;
class FCompilerResultsLog;
class UBlueprint;
class UEdGraph;
class UEdGraphPin;
class UObject;
struct FEdGraphPinType;

UCLASS()
class BLUEPRINTGRAPH_API UK2Node_Tunnel : public UK2Node_EditablePinBase
{
	GENERATED_UCLASS_BODY()

	// A tunnel node either has output pins that came from another tunnel's input pins, or vice versa
	// Note: OutputSourceNode might be equal to InputSinkNode
	
	// The output pins of this tunnel node came from the input pins of OutputSourceNode
	UPROPERTY()
	TObjectPtr<UK2Node_Tunnel> OutputSourceNode;

	// The input pins of this tunnel go to the output pins of InputSinkNode
	UPROPERTY()
	TObjectPtr<UK2Node_Tunnel> InputSinkNode;

	// Whether this node is allowed to have inputs
	UPROPERTY()
	uint32 bCanHaveInputs:1;

	// Whether this node is allowed to have outputs
	UPROPERTY()
	uint32 bCanHaveOutputs:1;

	// The metadata for the function/subgraph associated with this tunnel node; it's only editable and used
	// on the tunnel entry node inside the subgraph or macro.  This structure is ignored on any other tunnel nodes.
	UPROPERTY()
	struct FKismetUserDeclaredFunctionMetadata MetaData;

	//~ Begin UEdGraphNode Interface.
	virtual void DestroyNode() override;
	virtual void PostPasteNode() override;
	virtual FText GetTooltipText() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual bool CanUserDeleteNode() const override;
	virtual bool CanDuplicateNode() const override;
	virtual UObject* GetJumpTargetForDoubleClick() const override;
	virtual FName CreateUniquePinName(FName SourcePinName) const override;
	virtual void ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins) override;
	virtual bool IsCompatibleWithGraph(const UEdGraph* InGraph) const override;
	//~ End UEdGraphNode Interface

	//~ Begin UK2Node Interface.
	virtual bool IsNodeSafeToIgnore() const override;
	virtual bool DrawNodeAsEntry() const override;
	virtual bool DrawNodeAsExit() const override;
	virtual bool NodeCausesStructuralBlueprintChange() const override { return true; }
	virtual void ClearCachedBlueprintData(UBlueprint* Blueprint) override;
	virtual void ValidateNodeDuringCompilation(FCompilerResultsLog& MessageLog) const override;
	virtual void FixupPinStringDataReferences(FArchive* SavingArchive) override;
	//~ End UK2Node Interface

	//~ Begin UK2Node_EditablePinBase Interface.
	virtual UEdGraphPin* CreatePinFromUserDefinition(const TSharedPtr<FUserPinInfo> NewPinInfo) override;
	virtual bool CanModifyExecutionWires() override;
	virtual ERenamePinResult RenameUserDefinedPinImpl(const FName OldName, const FName NewName, bool bTest = false) override;
	virtual bool CanUseRefParams() const override { return true; }
	virtual bool CanCreateUserDefinedPin(const FEdGraphPinType& InPinType, EEdGraphPinDirection InDesiredDirection, FText& OutErrorMessage) override;
	virtual bool ModifyUserDefinedPinDefaultValue(TSharedPtr<FUserPinInfo> PinInfo, const FString& NewDefaultValue) override;
	//~ End UK2Node_EditablePinBase Interface

protected:
	/**
	* Handles any work needed to be done after fixing up all wildcard pins during reconstruction
	*
	* @param bInAllWildcardPinsUnlinked	TRUE if all wildcard pins were unlinked
	*/
	virtual void PostFixupAllWildcardPins(bool bInAllWildcardPinsUnlinked) {}

	// Utility function that subclasses must call after allocating their default pins
	void CacheWildcardPins();

	// Cache of the pins that were created as wildcards
	TArray<UEdGraphPin*> WildcardPins;

public:
	// The input pins of this tunnel go to the output pins of InputSinkNode (can be NULL).
	virtual UK2Node_Tunnel* GetInputSink() const;

	// The output pins of this tunnel node came from the input pins of OutputSourceNode (can be NULL).
	virtual UK2Node_Tunnel* GetOutputSource() const;
};



