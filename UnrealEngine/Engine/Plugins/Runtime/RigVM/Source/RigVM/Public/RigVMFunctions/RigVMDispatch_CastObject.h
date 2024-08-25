// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMCore/RigVMDispatchFactory.h"
#include "RigVMDispatch_CastObject.generated.h"

USTRUCT(meta=(DisplayName = "Cast", Category = "Object", Keywords = "As", NodeColor = "1,1,1,1"))
struct RIGVM_API FRigVMDispatch_CastObject : public FRigVMDispatchFactory
{
	GENERATED_BODY()

public:
	FRigVMDispatch_CastObject()
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
	virtual FRigVMFunctionPtr GetDispatchFunctionImpl(const FRigVMTemplateTypeMap& InTypes) const override { return &FRigVMDispatch_CastObject::Execute; }
	static void Execute(FRigVMExtendedExecuteContext& InContext, FRigVMMemoryHandleArray Handles, FRigVMPredicateBranchArray RigVMBranches);

	static const FName ValueName;
	static const FName ResultName;
};
