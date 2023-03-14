// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMTemplateNode.h"
#include "RigVMCore/RigVMExternalVariable.h"
#include "RigVMArrayNode.generated.h"

/**
 * The Array Node represents one of a series available
 * array operations such as SetNum, GetAtIndex etc.
 */
UCLASS(BlueprintType)
class RIGVMDEVELOPER_API URigVMArrayNode : public URigVMTemplateNode
{
	GENERATED_BODY()

public:

	// Default constructor
	URigVMArrayNode();

	// Override of node title
	virtual FString GetNodeTitle() const override;

	// static version
	static FString GetNodeTitle(ERigVMOpCode InOpCode);

	// Override of the node tooltip
	virtual FText GetToolTipText() const override;

	// Override of the node color
	virtual FLinearColor GetNodeColor() const override;

    // Override of the template notation
	virtual FName GetNotation() const override;
	
	// Override of the template
	virtual const FRigVMTemplate* GetTemplate() const override;

	// Override of singleton
	virtual bool IsSingleton() const override { return false; }

	// Returns the op code of this node
	UFUNCTION(BlueprintCallable, Category = RigVMArrayNode)
	ERigVMOpCode GetOpCode() const;

	// Returns the C++ data type of the element
	UFUNCTION(BlueprintCallable, Category = RigVMArrayNode)
	FString GetCPPType() const;

	// Returns the C++ data type struct of the array (or nullptr)
	UFUNCTION(BlueprintCallable, Category = RigVMArrayNode)
	UObject* GetCPPTypeObject() const;

	// Override of node color
	virtual bool IsDefinedAsVarying() const override { return true; }

	// Override loop node
	virtual bool IsLoopNode() const override;

protected:

	virtual FText GetToolTipTextForPin(const URigVMPin* InPin) const override;
	
private:

	UPROPERTY()
	ERigVMOpCode OpCode;

	static const FString ArrayName;
	static const FString NumName;
	static const FString IndexName;
	static const FString ElementName;
	static const FString SuccessName;
	static const FString OtherName;
	static const FString CloneName;
	static const FString CountName;
	static const FString RatioName;
	static const FString ResultName;
	static const FString ContinueName;
	static const FString CompletedName;

	friend class URigVMController;
	friend class URigVMCompiler;
	friend class FRigVMVarExprAST;
	friend class FRigVMParserAST;
};

