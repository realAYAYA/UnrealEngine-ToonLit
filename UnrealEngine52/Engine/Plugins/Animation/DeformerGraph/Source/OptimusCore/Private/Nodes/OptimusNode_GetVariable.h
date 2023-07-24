// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IOptimusValueProvider.h"
#include "OptimusNode.h"

#include "OptimusNode_GetVariable.generated.h"


class UOptimusVariableDescription;


// Information to hold data on the variable definition that persists over node duplication.
// FIXME: This could be generalized in a better fashion.
USTRUCT()
struct FOptimusNode_GetVariable_DuplicationInfo
{
	GENERATED_BODY()
	
	UPROPERTY()
	FName VariableName;

	UPROPERTY()
	FOptimusDataTypeRef DataType;

	UPROPERTY()
	FString DefaultValue;
};


UCLASS(Hidden)
class UOptimusNode_GetVariable : 
	public UOptimusNode,
	public IOptimusValueProvider
{
	GENERATED_BODY()

public:
	void SetVariableDescription(UOptimusVariableDescription* InVariableDesc);

	UOptimusVariableDescription* GetVariableDescription() const;
	
	// UOptimusNode overrides
	FName GetNodeCategory() const override 
	{
		return CategoryName::Variables;
	}

	TOptional<FText> ValidateForCompile() const override;
	
	// IOptimusValueProvider overrides 
	FString GetValueName() const override;
	FOptimusDataTypeRef GetValueType() const override;
	FShaderValueType::FValue GetShaderValue() const override;
	
protected:
	void ConstructNode() override;

	void PreDuplicateRequirementActions(const UOptimusNodeGraph* InTargetGraph, FOptimusCompoundAction* InCompoundAction) override;

	// UObject overrides
	void PostDuplicate(EDuplicateMode::Type DuplicateMode) override;
	void ExportCustomProperties(FOutputDevice& Out, uint32 Indent) override;
	void ImportCustomProperties(const TCHAR* SourceText, FFeedbackContext* Warn) override;
	
private:
	UPROPERTY()
	TWeakObjectPtr<UOptimusVariableDescription> VariableDesc;
	
	// Duplication data across graphs
	UPROPERTY(DuplicateTransient)
	FOptimusNode_GetVariable_DuplicationInfo DuplicationInfo;
};
