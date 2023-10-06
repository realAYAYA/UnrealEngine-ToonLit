// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMCore/RigVMRegistry.h"
#include "RigVMCore/RigVMFunction.h"
#include "RigVMCore/RigVMTemplate.h"
#include "RigVMCore/RigVMDispatchFactory.h"
#include "RigVMCore/RigVMGraphFunctionDefinition.h"
#include "RigVMGraph.h"
#include "RigVMSchema.generated.h"

#define RIGVMSCHEMA_DEFAULT_FUNCTION_BODY \
const URigVMGraph* Graph = InController->GetGraph(); \
check(Graph); \
if (!InController->IsTransacting() && !IsGraphEditable(Graph)) \
{ \
	return false; \
}

class URigVMController;

/**
 * The Schema is used to determine which actions are allowed
 * on a graph. This includes any topological change.
 */
UCLASS()
class RIGVMDEVELOPER_API URigVMSchema : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	// Default constructor
	URigVMSchema();

	// Returns the execute context struct this schema is using
	UScriptStruct* GetExecuteContextStruct() const { return ExecuteContextStruct; }

	// Returns true if a graph supports a given type
	virtual bool SupportsType(URigVMController* InController, TRigVMTypeIndex InTypeIndex) const;

	// Returns true if a graph supports a given unit function
	virtual bool SupportsUnitFunction(URigVMController* InController, const FRigVMFunction* InUnitFunction) const;

	// Returns true if a graph supports a given dispatch factory
	virtual bool SupportsDispatchFactory(URigVMController* InController, const FRigVMDispatchFactory* InDispatchFactory) const;

	// Returns true if a graph supports a given template
	virtual bool SupportsTemplate(URigVMController* InController, const FRigVMTemplate* InTemplate) const;

	// Returns true if a graph supports a given graph based function
	virtual bool SupportsGraphFunction(URigVMController* InController, const FRigVMGraphFunctionHeader* InGraphFunction) const;

	// Returns true if a graph supports a given external variable
	virtual bool SupportsExternalVariable(URigVMController* InController, const FRigVMExternalVariable* InExternalVariable) const;

	// Returns true if the pin for a given struct should be unfolded into subpins for a graph
	virtual bool ShouldUnfoldStruct(URigVMController* InController, const UStruct* InStruct) const;

	// Returns true if a node name is valid for a given graph
	virtual bool IsValidNodeName(const URigVMGraph* InGraph, const FName& InNodeName) const;

	// Returns true if a given node can be added the graph
	virtual bool CanAddNode(URigVMController* InController, const URigVMNode* InNode) const;

	// Returns true if a given node can be removed from the graph
	virtual bool CanRemoveNode(URigVMController* InController, const URigVMNode* InNode) const;

	// Returns true if a given node can be added the graph
	virtual bool CanRenameNode(URigVMController* InController, const URigVMNode* InNode, const FName& InNewNodeName) const;

	// Returns true if a node can moved to a new position
	virtual bool CanMoveNode(URigVMController* InController, const URigVMNode* InNode, const FVector2D& InNewPosition = FVector2D::ZeroVector) const;

	// Returns true if a node can resized to a new size
	virtual bool CanResizeNode(URigVMController* InController, const URigVMNode* InNode, const FVector2D& InNewSize = {100, 100}) const;

	// Returns true if a node can recolored to a new color
	virtual bool CanRecolorNode(URigVMController* InController, const URigVMNode* InNode, const FLinearColor& InNewColor = FLinearColor::White) const;

	// Returns true if a link can be added between two pins
	virtual bool CanAddLink(URigVMController* InController, const URigVMPin* InSourcePin, const URigVMPin* InTargetPin, const FRigVMByteCode* InByteCode, ERigVMPinDirection InUserLinkDirection = ERigVMPinDirection::IO, bool bInAllowWildcard = false, bool bEnableTypeCasting = true, FString* OutFailureReason = nullptr) const;

	// Returns true if a link exists between two pins and can be broken / removed
	virtual bool CanBreakLink(URigVMController* InController, const URigVMPin* InSourcePin, const URigVMPin* InTargetPin) const;

	// Returns true if a set of nodes can be collapsed
	virtual bool CanCollapseNodes(URigVMController* InController, const TArrayView<URigVMNode* const>& InNodesToCollapse) const;

	// Returns true if a node can be expanded within a graph
	virtual bool CanExpandNode(URigVMController* InController, const URigVMNode* InNodeToExpand) const;

	// Returns true if a pin should be unfolded (represented by its subpins)
	virtual bool CanUnfoldPin(URigVMController* InController, const URigVMPin* InPinToUnfold) const;

	// Returns true if a variable can be bound to a pin
	virtual bool CanBindVariable(URigVMController* InController, const URigVMPin* InPinToBind, const FRigVMExternalVariable* InVariableToBind, const FString& InNewBoundVariablePath) const;

	// Returns true if a variable can be unbound from a pin
	virtual bool CanUnbindVariable(URigVMController* InController, const URigVMPin* InBoundPin) const;

	// Returns true if functions definitions (not refs) can be added to the graph
	virtual bool CanAddFunction(URigVMController* InController, const URigVMNode* InFunctionNode) const;

	// Returns true if a function can be removed
	virtual bool CanRemoveFunction(URigVMController* InController, const URigVMNode* InFunctionNode) const;
	
	static int32 GetMaxNameLength() { return 100; }
	virtual FString GetSanitizedName(const FString& InName, bool bAllowPeriod, bool bAllowSpace) const;
	virtual FString GetSanitizedGraphName(const FString& InName) const;
	virtual FString GetSanitizedNodeName(const FString& InName) const;
	virtual FString GetSanitizedVariableName(const FString& InName) const;
	virtual FString GetSanitizedPinName(const FString& InName) const;
	virtual FString GetSanitizedPinPath(const FString& InName) const;
	virtual FString GetGraphOuterName(const URigVMGraph* InGraph) const;
	virtual	FString GetValidNodeName(const URigVMGraph* InGraph, const FString& InPrefix) const;
	static void SanitizeName(FString& InOutName, bool bAllowPeriod, bool bAllowSpace);

	// Returns a unique name based on a IsNameAvailable predicate
	static FName GetUniqueName(const FName& InName, TFunction<bool(const FName&)> IsNameAvailablePredicate, bool bAllowPeriod, bool bAllowSpace);

protected:

	// Sets the execute context struct this schema is using
	void SetExecuteContextStruct(UScriptStruct* InExecuteContextStruct);

	bool IsGraphEditable(const URigVMGraph* InGraph) const;
	TObjectPtr<URigVMNode> FindEventNode(URigVMController* InController, const UScriptStruct* InScriptStruct) const;

	UPROPERTY(transient)
	TObjectPtr<UScriptStruct> ExecuteContextStruct;
	TArray<UStruct*> ValidExecuteContextStructs;
	FRigVMRegistry* Registry;

	/*
	bool bIsTransacting; // Performing undo/redo transaction
	bool bAllowPrivateFunctions;
	bool bIgnoreFunctionEntryReturnNodes;
	mutable FString LastError;
	mutable FString LastWarning;
	mutable FString LastErrorOrWarning;
	*/

	friend struct FRigVMClient;
	friend class URigVMController;
	friend struct FRigVMBaseAction;
};