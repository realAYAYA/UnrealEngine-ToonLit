// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMCore/RigVMDispatchFactory.h"
#include "RigVMDispatch_Core.h"
#include "RigVMCore/RigVMByteCode.h"
#include "RigVMDispatch_MakeStruct.generated.h"

USTRUCT(meta=(DisplayName = "Make", Category = "Core", Keywords = "Compose,Composition,Create,Constant", NodeColor = "1,1,1,1"))
struct RIGVM_API FRigVMDispatch_MakeStruct : public FRigVMDispatch_CoreBase
{
	GENERATED_BODY()

public:
	FRigVMDispatch_MakeStruct()
	{
		FactoryScriptStruct = StaticStruct();
	}
	
	virtual const TArray<FRigVMTemplateArgumentInfo>& GetArgumentInfos() const override;
	virtual FRigVMTemplateTypeMap OnNewArgumentType(const FName& InArgumentName, TRigVMTypeIndex InTypeIndex) const override;
#if WITH_EDITOR
	virtual FString GetNodeTitle(const FRigVMTemplateTypeMap& InTypes) const override;
	virtual FText GetNodeTooltip(const FRigVMTemplateTypeMap& InTypes) const override;
	virtual FString GetArgumentMetaData(const FName& InArgumentName, const FName& InMetaDataKey) const override;
	virtual FString GetKeywords() const override;
#endif

protected:
	virtual FRigVMFunctionPtr GetDispatchFunctionImpl(const FRigVMTemplateTypeMap& InTypes) const override { return &FRigVMDispatch_MakeStruct::Execute; }
	static void Execute(FRigVMExtendedExecuteContext& InContext, FRigVMMemoryHandleArray Handles, FRigVMPredicateBranchArray RigVMBranches);

	static const FName ElementsName;
	static const FName StructName;

	friend struct FRigVMDispatch_BreakStruct;
	friend class URigVMController;
};

USTRUCT(meta=(DisplayName = "Break", Keywords = "Decompose,Decomposition"))
struct RIGVM_API FRigVMDispatch_BreakStruct : public FRigVMDispatch_MakeStruct
{
	GENERATED_BODY()

public:
	FRigVMDispatch_BreakStruct()
	{
		FactoryScriptStruct = StaticStruct();
	}

	virtual const TArray<FRigVMTemplateArgumentInfo>& GetArgumentInfos() const override;
#if WITH_EDITOR
	virtual FText GetNodeTooltip(const FRigVMTemplateTypeMap& InTypes) const override;
	virtual FString GetKeywords() const override;
#endif
};

