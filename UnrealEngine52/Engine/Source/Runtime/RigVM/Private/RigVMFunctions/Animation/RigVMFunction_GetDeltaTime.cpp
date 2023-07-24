// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMFunctions/Animation/RigVMFunction_GetDeltaTime.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigVMFunction_GetDeltaTime)

FRigVMFunction_GetDeltaTime_Execute()
{
	Result = ExecuteContext.GetDeltaTime();
}

#if WITH_DEV_AUTOMATION_TESTS
#include "RigVMCore/RigVMStructTest.h"

IMPLEMENT_RIGVMSTRUCT_AUTOMATION_TEST(FRigVMFunction_GetDeltaTime)
{
	ExecuteContext.SetDeltaTime(0.2f);
	Execute();
	AddErrorIfFalse(FMath::IsNearlyEqual(Unit.Result, 0.2f), TEXT("unexpected delta time"));
	return true;
}
#endif
