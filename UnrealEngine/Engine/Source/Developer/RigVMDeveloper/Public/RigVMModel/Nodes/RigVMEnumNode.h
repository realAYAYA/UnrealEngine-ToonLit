// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "Math/Color.h"
#include "RigVMModel/RigVMNode.h"
#include "RigVMModel/RigVMPin.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "RigVMEnumNode.generated.h"

class UEnum;
class UObject;
struct FFrame;

/**
 * The Enum Node represents a constant enum value for use within the graph.
 */
UCLASS(BlueprintType)
class RIGVMDEVELOPER_API URigVMEnumNode : public URigVMNode
{
	GENERATED_BODY()

public:

	// Default constructor
	URigVMEnumNode();

	// Override of node title
	virtual FString GetNodeTitle() const;

	// Returns the enum itself
	UFUNCTION(BlueprintCallable, Category = RigVMEnumNode)
	UEnum* GetEnum() const;

	// Returns the C++ data type of the parameter
	UFUNCTION(BlueprintCallable, Category = RigVMEnumNode)
	FString GetCPPType() const;

	// Returns the C++ data type struct of the parameter (or nullptr)
	UFUNCTION(BlueprintCallable, Category = RigVMEnumNode)
	UObject* GetCPPTypeObject() const;

	// Returns the default value of the parameter as a string
	FString GetDefaultValue(const URigVMPin::FPinOverride& InOverride = URigVMPin::EmptyPinOverride) const;

	// Override of node title
	virtual FLinearColor GetNodeColor() const override { return FLinearColor::Blue; }

	virtual bool IsDefinedAsVarying() const override { return true; }

private:

	static const FString EnumName;
	static const FString EnumValueName;
	static const FString EnumIndexName;
	
	friend class URigVMController;
	friend class URigVMCompiler;
	friend struct FRigVMAddEnumNodeAction;
	friend class UControlRigEnumNodeSpawner;
	friend class FRigVMParserAST;
};

