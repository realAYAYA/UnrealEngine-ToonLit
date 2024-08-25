// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMCore/RigVMDispatchFactory.h"
#include "RigVMDispatch_Core.h"
#include "RigVMCore/RigVMByteCode.h"
#include "RigVMDispatch_Constant.generated.h"

USTRUCT(meta=(DisplayName = "Constant", Category = "Core", Keywords = "Value,Reroute", NodeColor = "1,1,1,1"))
struct RIGVM_API FRigVMDispatch_Constant : public FRigVMDispatch_CoreBase
{
	GENERATED_BODY()

public:
	FRigVMDispatch_Constant()
	{
		FactoryScriptStruct = StaticStruct();
	}

	virtual const TArray<FRigVMTemplateArgumentInfo>& GetArgumentInfos() const override;
	virtual FRigVMTemplateTypeMap OnNewArgumentType(const FName& InArgumentName, TRigVMTypeIndex InTypeIndex) const override;
#if WITH_EDITOR
	virtual FString GetNodeTitle(const FRigVMTemplateTypeMap& InTypes) const override;
	virtual FText GetNodeTooltip(const FRigVMTemplateTypeMap& InTypes) const override;
#endif
	virtual bool IsSingleton() const override { return true; }
	
protected:
	virtual FRigVMFunctionPtr GetDispatchFunctionImpl(const FRigVMTemplateTypeMap& InTypes) const override { return &FRigVMDispatch_Constant::Execute; }
	static void Execute(FRigVMExtendedExecuteContext& InContext, FRigVMMemoryHandleArray Handles, FRigVMPredicateBranchArray RigVMBranches);

	static const FName ValueName;

	friend class URigVMController;
};
