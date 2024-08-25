// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMModel/Nodes/RigVMTemplateNode.h"
#include "RigVMCore/RigVMStruct.h"
#include "UObject/StructOnScope.h"
#include "RigVMUnitNode.generated.h"

class URigVMHost;

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
	virtual const TArray<FName>& GetControlFlowBlocks() const override;
	virtual const bool IsControlFlowBlockSliced(const FName& InBlockName) const override;
	virtual TArray<FRigVMUserWorkflow> GetSupportedWorkflows(ERigVMUserWorkflowType InType, const UObject* InSubject) const override;
	virtual TArray<URigVMPin*> GetAggregateInputs() const override;
	virtual TArray<URigVMPin*> GetAggregateOutputs() const override;
	virtual FName GetNextAggregateName(const FName& InLastAggregatePinName) const override;

	virtual bool IsOutDated() const override;
	virtual FString GetDeprecatedMetadata() const override;

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

	// Returns an instance of the struct with values backed by the memory of a currently running host
	TSharedPtr<FStructOnScope> ConstructLiveStructInstance(URigVMHost* InHost, int32 InSliceIndex = 0) const;
	
	// Updates the memory of a host to match a specific struct instance
	bool UpdateHostFromStructInstance(URigVMHost* InHost, TSharedPtr<FStructOnScope> InInstance, int32 InSliceIndex = 0) const;

	// Compares two struct instances and returns differences in pin values
	void ComputePinValueDifferences(TSharedPtr<FStructOnScope> InCurrentInstance, TSharedPtr<FStructOnScope> InDesiredInstance, TMap<FString, FString>& OutNewPinDefaultValues) const;

	// Compares a desired struct instance with the node's default and returns differences in pin values
	void ComputePinValueDifferences(TSharedPtr<FStructOnScope> InDesiredInstance, TMap<FString, FString>& OutNewPinDefaultValues) const;

	// Returns true if the node is part of the debugged runtime rig
	bool IsPartOfRuntime() const;

	// Returns true if the node is part of the debugged runtime rig
	bool IsPartOfRuntime(URigVMHost* InHost) const;

	// Returns a copy of the struct with the current values
	template <
		typename T,
		typename TEnableIf<TModels_V<CRigVMUStruct, T>>::Type* = nullptr
	>
	T ConstructStructInstance() const
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

	virtual uint32 GetStructureHash() const override;

	// allows the node to support non-native pins
	virtual bool HasNonNativePins() const { return false; }

protected:

	virtual FText GetToolTipTextForPin(const URigVMPin* InPin) const override;
	virtual bool ShouldInputPinComputeLazily(const URigVMPin* InPin) const override;
	void EnumeratePropertiesOnHostAndStructInstance(
		URigVMHost* InHost,
		TSharedPtr<FStructOnScope> InInstance, 
		bool bPreferLiterals,
		TFunction<void(const URigVMPin*,const FProperty*,uint8*,const FProperty*,uint8*)> InEnumerationFunction,
		int32 InSliceIndex = 0) const;

private:

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UScriptStruct> ScriptStruct_DEPRECATED;

	UPROPERTY()
	FName MethodName_DEPRECATED;
#endif

	friend class URigVMController;
};

