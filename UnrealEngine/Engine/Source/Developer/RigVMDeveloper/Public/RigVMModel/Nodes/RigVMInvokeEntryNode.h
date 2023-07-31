// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMModel/RigVMNode.h"
#include "RigVMInvokeEntryNode.generated.h"

/**
 * The Invoke Entry Node is used to invoke another entry from the 
 * the currently running entry.
 */
UCLASS(BlueprintType)
class RIGVMDEVELOPER_API URigVMInvokeEntryNode : public URigVMNode
{
	GENERATED_BODY()

public:

	// Default constructor
	URigVMInvokeEntryNode();

	// Override of node title
	virtual FString GetNodeTitle() const;

	// Returns the name of the entry to run
	UFUNCTION(BlueprintCallable, Category = RigVMInvokeEntryNode)
	FName GetEntryName() const;

	// Override of node title
	virtual FLinearColor GetNodeColor() const override { return FLinearColor::Red; }
	virtual bool IsDefinedAsVarying() const override { return true; }

private:

	static const FString EntryName;

	URigVMPin* GetEntryNamePin() const;
	

	friend class URigVMController;
	friend class UControlRigBlueprint;
	friend struct FRigVMRemoveNodeAction;
	friend class URigVMCompiler;
	friend class FRigVMParserAST;
	friend class URigVMPin;
};

