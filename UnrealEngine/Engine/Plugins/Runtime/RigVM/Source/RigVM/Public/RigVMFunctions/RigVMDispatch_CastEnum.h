// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMCore/RigVMDispatchFactory.h"
#include "RigVMDispatch_CastEnum.generated.h"

struct FRigVMDispatch_CastEnumBase
{
	static const FName ValueName;
	static const FName ResultName;
};

USTRUCT(meta=(DisplayName = "Cast", Category = "Enum", Keywords = "As", NodeColor = "1,1,1,1"))
struct RIGVM_API FRigVMDispatch_CastEnumToInt : public FRigVMDispatchFactory, public FRigVMDispatch_CastEnumBase
{
	GENERATED_BODY()

public:
	FRigVMDispatch_CastEnumToInt()
	{
		FactoryScriptStruct = StaticStruct();
	}
	
	virtual const TArray<FRigVMTemplateArgumentInfo>& GetArgumentInfos() const override;
	virtual bool GetPermutationsFromArgumentType(const FName& InArgumentName, const TRigVMTypeIndex& InTypeIndex, TArray<FRigVMTemplateTypeMap, TInlineAllocator<1>>& OutPermutations) const override;
#if WITH_EDITOR
	virtual FString GetNodeTitle(const FRigVMTemplateTypeMap& InTypes) const override;
	virtual FText GetNodeTooltip(const FRigVMTemplateTypeMap& InTypes) const override;
#endif
	virtual bool IsSingleton() const override { return true; }
	
protected:
	virtual FRigVMFunctionPtr GetDispatchFunctionImpl(const FRigVMTemplateTypeMap& InTypes) const override { return &FRigVMDispatch_CastEnumToInt::Execute; }
	static void Execute(FRigVMExtendedExecuteContext& InContext, FRigVMMemoryHandleArray Handles, FRigVMPredicateBranchArray RigVMBranches);
};

USTRUCT(meta=(DisplayName = "Cast", Category = "Enum", Keywords = "As", NodeColor = "1,1,1,1"))
struct RIGVM_API FRigVMDispatch_CastIntToEnum : public FRigVMDispatchFactory, public FRigVMDispatch_CastEnumBase
{
	GENERATED_BODY()

public:
	FRigVMDispatch_CastIntToEnum()
	{
		FactoryScriptStruct = StaticStruct();
	}
	
	virtual const TArray<FRigVMTemplateArgumentInfo>& GetArgumentInfos() const override;
	virtual bool GetPermutationsFromArgumentType(const FName& InArgumentName, const TRigVMTypeIndex& InTypeIndex, TArray<FRigVMTemplateTypeMap, TInlineAllocator<1>>& OutPermutations) const override;
#if WITH_EDITOR
	virtual FString GetNodeTitle(const FRigVMTemplateTypeMap& InTypes) const override;
	virtual FText GetNodeTooltip(const FRigVMTemplateTypeMap& InTypes) const override;
#endif
	virtual bool IsSingleton() const override { return true; }
	
protected:
	virtual FRigVMFunctionPtr GetDispatchFunctionImpl(const FRigVMTemplateTypeMap& InTypes) const override { return &FRigVMDispatch_CastIntToEnum::Execute; }
	static void Execute(FRigVMExtendedExecuteContext& InContext, FRigVMMemoryHandleArray Handles, FRigVMPredicateBranchArray RigVMBranches);
};
