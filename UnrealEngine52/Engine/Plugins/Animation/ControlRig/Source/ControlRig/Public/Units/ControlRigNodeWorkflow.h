// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Rigs/RigHierarchy.h"
#include "RigVMCore/RigVMUserWorkflow.h"
#include "ControlRigNodeWorkflow.generated.h"

UCLASS(BlueprintType)
class CONTROLRIG_API UControlRigWorkflowOptions : public URigVMUserWorkflowOptions
{
	GENERATED_BODY()

public:
	
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = Options)
	TObjectPtr<const URigHierarchy> Hierarchy;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = Options)
	TArray<FRigElementKey> Selection;

	UFUNCTION(BlueprintCallable, Category = "Options")
	bool EnsureAtLeastOneRigElementSelected() const;
};

UCLASS(BlueprintType)
class CONTROLRIG_API UControlRigTransformWorkflowOptions : public UControlRigWorkflowOptions
{
	GENERATED_BODY()

public:

	// The type of transform to retrieve from the hierarchy
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Options)
	TEnumAsByte<ERigTransformType::Type> TransformType = ERigTransformType::CurrentGlobal;

	UFUNCTION()
	TArray<FRigVMUserWorkflow> ProvideWorkflows(const UObject* InSubject);

protected:

#if WITH_EDITOR
	static bool PerformTransformWorkflow(const URigVMUserWorkflowOptions* InOptions, UObject* InController);
#endif
};
