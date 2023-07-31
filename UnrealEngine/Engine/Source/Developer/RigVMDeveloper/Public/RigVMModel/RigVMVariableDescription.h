// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMCore/RigVMExternalVariable.h"
#include "RigVMModel/RigVMNode.h"
#include "EdGraphSchema_K2.h"
#include "RigVMDeveloperTypeUtils.h"
#include "RigVMVariableDescription.generated.h"

/**
 * The variable description is used to convey information
 * about unique variables within a Graph. Multiple Variable
 * Nodes can share the same variable description.
 */
USTRUCT(BlueprintType)
struct FRigVMGraphVariableDescription
{
	GENERATED_BODY()

public:

	// comparison operator
	bool operator ==(const FRigVMGraphVariableDescription& Other) const
	{
		return Name == Other.Name;
	}

	// The name of the variable
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = RigVMGraphVariableDescription)
	FName Name;

	// The C++ data type of the variable
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = RigVMGraphVariableDescription)
	FString CPPType;

	// The Struct of the C++ data type of the variable (or nullptr)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = RigVMGraphVariableDescription)
	TObjectPtr<UObject> CPPTypeObject = nullptr;

	// The default value of the variable
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = RigVMGraphVariableDescription)
	FString DefaultValue;

	// Returns nullptr external variable matching this description
	FORCEINLINE FRigVMExternalVariable ToExternalVariable() const
	{
		return RigVMTypeUtils::ExternalVariableFromRigVMVariableDescription(*this);
	}

	FORCEINLINE FEdGraphPinType ToPinType() const
	{
		return RigVMTypeUtils::PinTypeFromRigVMVariableDescription(*this);
	}

	FORCEINLINE bool ChangeType(const FEdGraphPinType& PinType)
	{
		UObject* Object = nullptr;
		const bool bSuccess = RigVMTypeUtils::CPPTypeFromPinType(PinType, CPPType, &Object);
		CPPTypeObject = Object;
		return bSuccess;
	}
	
};
