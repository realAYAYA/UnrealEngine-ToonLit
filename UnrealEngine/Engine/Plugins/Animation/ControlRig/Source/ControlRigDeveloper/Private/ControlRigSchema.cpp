// Copyright Epic Games, Inc. All Rights Reserved.

#include "ControlRigSchema.h"
#include "RigVMModel/RigVMController.h"
#include "Rigs/RigHierarchyPose.h"
#include "Curves/CurveFloat.h"
#include "Units/RigUnitContext.h"

UControlRigSchema::UControlRigSchema(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetExecuteContextStruct(FControlRigExecuteContext::StaticStruct());
}

bool UControlRigSchema::ShouldUnfoldStruct(URigVMController* InController, const UStruct* InStruct) const
{
	RIGVMSCHEMA_DEFAULT_FUNCTION_BODY
	
	if(!Super::ShouldUnfoldStruct(InController, InStruct))
	{
		return false;
	}
	if (InStruct == TBaseStructure<FQuat>::Get())
	{
		return false;
	}
	if (InStruct == FRuntimeFloatCurve::StaticStruct())
	{
		return false;
	}
	if (InStruct == FRigPose::StaticStruct())
	{
		return false;
	}
	
	return true;
}
