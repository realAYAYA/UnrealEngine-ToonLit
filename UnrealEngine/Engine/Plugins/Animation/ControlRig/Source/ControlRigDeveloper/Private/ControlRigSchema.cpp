// Copyright Epic Games, Inc. All Rights Reserved.

#include "ControlRigSchema.h"
#include "ControlRigBlueprint.h"
#include "RigVMModel/RigVMController.h"
#include "Rigs/RigHierarchyPose.h"
#include "Curves/CurveFloat.h"
#include "Units/RigUnitContext.h"
#include "Units/Modules/RigUnit_ConnectorExecution.h"
#include "Units/Modules/RigUnit_ConnectionCandidates.h"

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

bool UControlRigSchema::SupportsUnitFunction(URigVMController* InController, const FRigVMFunction* InUnitFunction) const
{
	if (InUnitFunction->Struct == FRigUnit_ConnectorExecution::StaticStruct() ||
		InUnitFunction->Struct == FRigUnit_GetCandidates::StaticStruct() ||
		InUnitFunction->Struct == FRigUnit_DiscardMatches::StaticStruct())
	{
		if (UControlRigBlueprint* Blueprint = Cast<UControlRigBlueprint>(InController->GetOuter()))
		{
			if (Blueprint->IsControlRigModule())
			{
				return true;
			}
			return false;
		}
	}
	return Super::SupportsUnitFunction(InController, InUnitFunction);
}
