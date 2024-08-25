// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMCore/RigVMDispatchFactory.h"
#include "Graph/AnimNextExecuteContext.h"
#include "RigVMDispatch_GetParameter.generated.h"

namespace UE::AnimNext::UncookedOnly
{
	struct FUtils;
}

/*
 * Gets a parameter's value
 */
USTRUCT(meta = (DisplayName = "Get Parameter", Category="Parameters", NodeColor = "0.8, 0, 0.2, 1"))
struct ANIMNEXT_API FRigVMDispatch_GetParameter : public FRigVMDispatchFactory
{
	GENERATED_BODY()

	FRigVMDispatch_GetParameter();

	static const FName ParameterName;
	static const FName ValueName;

private:
	friend struct UE::AnimNext::UncookedOnly::FUtils;

	virtual UScriptStruct* GetExecuteContextStruct() const { return FAnimNextExecuteContext::StaticStruct(); }
	virtual FName GetArgumentNameForOperandIndex(int32 InOperandIndex, int32 InTotalOperands) const override;
	virtual const TArray<FRigVMTemplateArgumentInfo>& GetArgumentInfos() const override;
#if WITH_EDITOR
	virtual FString GetArgumentMetaData(const FName& InArgumentName, const FName& InMetaDataKey) const override;
#endif
	virtual FRigVMTemplateTypeMap OnNewArgumentType(const FName& InArgumentName, TRigVMTypeIndex InTypeIndex) const override;
	virtual bool IsSingleton() const override { return true; }

	virtual FRigVMFunctionPtr GetDispatchFunctionImpl(const FRigVMTemplateTypeMap& InTypes) const override
	{
		return &FRigVMDispatch_GetParameter::Execute;
	}
	static void Execute(FRigVMExtendedExecuteContext& InContext, FRigVMMemoryHandleArray Handles, FRigVMPredicateBranchArray RigVMBranches);

	static const FName ExecuteContextName;
	static const FName ParameterIdName;
	static const FName TypeHandleName;
};
