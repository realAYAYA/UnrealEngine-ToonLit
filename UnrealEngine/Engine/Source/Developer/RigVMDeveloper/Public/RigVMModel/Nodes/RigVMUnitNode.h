// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMModel/Nodes/RigVMTemplateNode.h"
#include "RigVMCore/RigVMStruct.h"
#include "UObject/StructOnScope.h"
#include "RigVMUnitNode.generated.h"

/**
 * The Struct Node represents a Function Invocation of a RIGVM_METHOD
 * declared on a USTRUCT. Struct Nodes have input / output pins for all
 * struct UPROPERTY members.
 */
UCLASS(BlueprintType)
class RIGVMDEVELOPER_API URigVMUnitNode : public URigVMTemplateNode
{
	GENERATED_BODY()

public:

	// UObject interface
	virtual void PostLoad() override;

	// Override node functions
	virtual FString GetNodeTitle() const override;
	virtual FText GetToolTipText() const override;
	virtual bool IsDefinedAsConstant() const override;
	virtual bool IsDefinedAsVarying() const override;
	virtual FName GetEventName() const override;
	virtual bool CanOnlyExistOnce() const override;
	virtual bool IsLoopNode() const override;
	virtual TArray<FRigVMUserWorkflow> GetSupportedWorkflows(ERigVMUserWorkflowType InType, const UObject* InSubject) const override;
	virtual TArray<URigVMPin*> GetAggregateInputs() const override;
	virtual TArray<URigVMPin*> GetAggregateOutputs() const override;
	virtual FName GetNextAggregateName(const FName& InLastAggregatePinName) const override;

	bool IsDeprecated() const;
	FString GetDeprecatedMetadata() const;


	// URigVMTemplateNode interface
	virtual UScriptStruct* GetScriptStruct() const override;

	// Returns the name of the declared RIGVM_METHOD
	UFUNCTION(BlueprintCallable, Category = RigVMUnitNode)
	virtual FName GetMethodName() const;

	// Returns the default value for the struct as text
	UFUNCTION(BlueprintCallable, Category = RigVMUnitNode)
	FString GetStructDefaultValue() const;

	// Returns an instance of the struct with the current values.
	// @param bUseDefault If set to true the default struct will be created - otherwise the struct will contains the values from the node
	TSharedPtr<FStructOnScope> ConstructStructInstance(bool bUseDefault = false) const;

	// Returns a copy of the struct with the current values
	template <
		typename T,
		typename TEnableIf<TModels<CRigVMUStruct, T>::Value>::Type* = nullptr
	>
	FORCEINLINE T ConstructStructInstance() const
	{
		if(!ensure(T::StaticStruct() == GetScriptStruct()))
		{
			return T();
		}

		TSharedPtr<FStructOnScope> Instance = ConstructStructInstance(false);
		const T& InstanceRef = *(const T*)Instance->GetStructMemory();
		return InstanceRef;
	}

	virtual FRigVMStructUpgradeInfo GetUpgradeInfo() const override;

protected:

	virtual FText GetToolTipTextForPin(const URigVMPin* InPin) const override;

private:

	UPROPERTY()
	TObjectPtr<UScriptStruct> ScriptStruct_DEPRECATED;

	UPROPERTY()
	FName MethodName_DEPRECATED;

	friend class URigVMController;
};

