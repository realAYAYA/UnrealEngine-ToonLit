// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMCore/RigVMDispatchFactory.h"
#include "AnimNextParameterExecuteContext.h"
#include "RigVMDispatch_SetLayerParameter.generated.h"

namespace UE::AnimNext::UncookedOnly
{
struct FUtils;
}

/*
 * Sets a parameter's value
 */
USTRUCT(meta=(DisplayName = "Set Block Parameter", Category="Parameters", NodeColor = "0.8, 0, 0.2, 1"))
struct ANIMNEXT_API FRigVMDispatch_SetLayerParameter : public FRigVMDispatchFactory
{
	GENERATED_BODY()

	FRigVMDispatch_SetLayerParameter();

private:
	friend struct UE::AnimNext::UncookedOnly::FUtils;

	virtual UScriptStruct* GetExecuteContextStruct() const { return FAnimNextParameterExecuteContext::StaticStruct(); }
	virtual FName GetArgumentNameForOperandIndex(int32 InOperandIndex, int32 InTotalOperands) const override;
	virtual const TArray<FRigVMTemplateArgumentInfo>& GetArgumentInfos() const override;
#if WITH_EDITOR
	virtual FString GetArgumentMetaData(const FName& InArgumentName, const FName& InMetaDataKey) const override;
#endif
	virtual TArray<FRigVMExecuteArgument>& GetExecuteArguments_Impl(const FRigVMDispatchContext& InContext) const override;
	virtual FRigVMTemplateTypeMap OnNewArgumentType(const FName& InArgumentName, TRigVMTypeIndex InTypeIndex) const override;
	virtual bool IsSingleton() const override { return true; } 

	virtual FRigVMFunctionPtr GetDispatchFunctionImpl(const FRigVMTemplateTypeMap& InTypes) const override
	{
		return &FRigVMDispatch_SetLayerParameter::Execute;
	}
	static void Execute(FRigVMExtendedExecuteContext& InContext, FRigVMMemoryHandleArray Handles, FRigVMPredicateBranchArray RigVMBranches);

	static const FName ExecuteContextName;
	static const FName ParameterName;
	static const FName ParameterIdName;
	static const FName TypeHandleName;
	static const FName ValueName;
};
