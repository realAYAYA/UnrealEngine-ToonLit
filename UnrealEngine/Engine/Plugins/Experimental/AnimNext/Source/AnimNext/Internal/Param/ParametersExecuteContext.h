// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RigVMCore/RigVMExecuteContext.h"
#include "Units/RigUnit.h"
#include "Graph/GraphExecuteContext.h"
#include "ParametersExecuteContext.generated.h"

namespace UE::AnimNext
{
	struct FContext;
}

USTRUCT(BlueprintType)
struct FAnimNextParametersExecuteContext : public FAnimNextGraphExecuteContext
{
	GENERATED_BODY()

	FAnimNextParametersExecuteContext()
		: FAnimNextGraphExecuteContext()
	{
	}
};

USTRUCT(meta=(ExecuteContext="FAnimNextParametersExecuteContext"))
struct FRigUnit_AnimNextParametersBase : public FRigUnit
{
	GENERATED_BODY()
};
