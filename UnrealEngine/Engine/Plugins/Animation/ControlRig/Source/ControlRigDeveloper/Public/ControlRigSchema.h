// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMModel/RigVMSchema.h"
#include "ControlRigSchema.generated.h"

UCLASS(BlueprintType)
class CONTROLRIGDEVELOPER_API UControlRigSchema : public URigVMSchema
{
	GENERATED_UCLASS_BODY()

public:

	virtual bool ShouldUnfoldStruct(URigVMController* InController, const UStruct* InStruct) const override;

	virtual bool SupportsUnitFunction(URigVMController* InController, const FRigVMFunction* InUnitFunction) const override;
};