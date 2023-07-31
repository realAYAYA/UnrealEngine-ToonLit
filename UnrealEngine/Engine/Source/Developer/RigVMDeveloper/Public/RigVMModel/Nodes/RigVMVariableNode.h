// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMCore/RigVMExternalVariable.h"
#include "RigVMModel/RigVMNode.h"
#include "RigVMModel/RigVMVariableDescription.h"
#include "RigVMVariableNode.generated.h"

/**
 * The Variable Node represents a mutable value / local state within the
 * the Function / Graph. Variable Node's can be a getter or a setter.
 * Getters are pure nodes with just an output value pin, while setters
 * are mutable nodes with an execute and input value pin.
 */
UCLASS(BlueprintType)
class RIGVMDEVELOPER_API URigVMVariableNode : public URigVMNode
{
	GENERATED_BODY()

public:

	// Default constructor
	URigVMVariableNode();

	// Override of node title
	virtual FString GetNodeTitle() const;

	// Returns the name of the variable
	UFUNCTION(BlueprintCallable, Category = RigVMVariableNode)
	FName GetVariableName() const;

	// Returns true if this node is a variable getter
	UFUNCTION(BlueprintCallable, Category = RigVMVariableNode)
	bool IsGetter() const;

	// Returns true if this variable is an external variable
	UFUNCTION(BlueprintCallable, Category = RigVMVariableNode)
	bool IsExternalVariable() const;

	// Returns true if this variable is a local variable
	UFUNCTION(BlueprintCallable, Category = RigVMVariableNode)
	bool IsLocalVariable() const;

	// Returns true if this variable is an input argument
	UFUNCTION(BlueprintCallable, Category = RigVMVariableNode)
	bool IsInputArgument() const;

	// Returns the C++ data type of the variable
	UFUNCTION(BlueprintCallable, Category = RigVMVariableNode)
	FString GetCPPType() const;

	// Returns the C++ data type struct of the variable (or nullptr)
	UFUNCTION(BlueprintCallable, Category = RigVMVariableNode)
	UObject* GetCPPTypeObject() const;

	// Returns the default value of the variable as a string
	UFUNCTION(BlueprintCallable, Category = RigVMVariableNode)
	FString GetDefaultValue() const;

	// Returns this variable node's variable description
	UFUNCTION(BlueprintCallable, Category = RigVMVariableNode)
	FRigVMGraphVariableDescription GetVariableDescription() const;

	// Override of node title
	virtual FLinearColor GetNodeColor() const override { return FLinearColor::Blue; }
	virtual bool IsDefinedAsVarying() const override { return true; }

private:

	static const FString VariableName;
	static const FString ValueName;

	URigVMPin* GetVariableNamePin() const;
	URigVMPin* GetValuePin() const;
	

	friend class URigVMController;
	friend class UControlRigBlueprint;
	friend struct FRigVMRemoveNodeAction;
	friend class URigVMPin;
	friend class URigVMCompiler;
	friend class FRigVMVarExprAST;
	friend class FRigVMParserAST;
};

