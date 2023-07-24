// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMTemplateNode.h"
#include "RigVMArrayNode.generated.h"

/**
 * The Array Node represents one of a series available
 * array operations such as SetNum, GetAtIndex etc.
 */
UCLASS(BlueprintType, Deprecated)
class RIGVMDEVELOPER_API UDEPRECATED_RigVMArrayNode : public URigVMTemplateNode
{
	GENERATED_BODY()

public:

	// Default constructor
	UDEPRECATED_RigVMArrayNode();

	// Returns the op code of this node
	UFUNCTION(BlueprintCallable, Category = RigVMArrayNode)
	ERigVMOpCode GetOpCode() const;

	// Returns the C++ data type of the element
	UFUNCTION(BlueprintCallable, Category = RigVMArrayNode)
	FString GetCPPType() const;

	// Returns the C++ data type struct of the array (or nullptr)
	UFUNCTION(BlueprintCallable, Category = RigVMArrayNode)
	UObject* GetCPPTypeObject() const;

private:

	UPROPERTY()
	ERigVMOpCode OpCode;

	friend class URigVMController;
	friend class URigVMCompiler;
	friend class FRigVMVarExprAST;
	friend class FRigVMParserAST;
};

