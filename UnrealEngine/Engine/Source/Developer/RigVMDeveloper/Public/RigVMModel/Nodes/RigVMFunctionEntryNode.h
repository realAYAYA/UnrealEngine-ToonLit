// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMModel/Nodes/RigVMTemplateNode.h"
#include "RigVMFunctionEntryNode.generated.h"

/**
 * The Function Entry node is used to provide access to the 
 * input pins of the library node for links within.
 */
UCLASS(BlueprintType)
class RIGVMDEVELOPER_API URigVMFunctionEntryNode : public URigVMTemplateNode
{
	GENERATED_BODY()

public:

	// Override template node functions
	virtual UScriptStruct* GetScriptStruct() const override { return nullptr; }
	virtual const FRigVMTemplate* GetTemplate() const override;
	virtual FName GetNotation() const override;

	// Override node functions
	virtual FLinearColor GetNodeColor() const override;
	virtual bool IsDefinedAsVarying() const override;

	// URigVMNode interface
	virtual FString GetNodeTitle() const override;
	virtual  FText GetToolTipText() const override;
	virtual FText GetToolTipTextForPin(const URigVMPin* InPin) const override;

private:

	friend class URigVMController;
};

