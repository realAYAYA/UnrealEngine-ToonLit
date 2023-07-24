// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigUnit_AnimNextInterfaceParameter.h"
#include "AnimNextInterfaceUnitContext.h"
#include "AnimNextInterfaceExecuteContext.h"
#include "AnimNextInterface.h"
#include "AnimNextInterfaceTypes.h"
#include "AnimNextInterfaceKernel.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigUnit_AnimNextInterfaceParameter)

bool FRigUnit_AnimNextInterfaceParameter::GetParameterInternal(FName InName, const FAnimNextInterfaceExecuteContext& InContext, void* OutResult)
{
	return true;
}

FRigUnit_AnimNextInterfaceParameter_Float_Execute()
{
	GetParameterInternal(Parameter, ExecuteContext, &Result);
}

FRigUnit_AnimNextInterfaceParameter_AnimNextInterface_Execute()
{
	GetParameterInternal(Parameter, ExecuteContext, &Result);
}

FRigUnit_AnimNextInterface_Float_Execute()
{
	using namespace UE::AnimNext::Interface;

	// @TODO: ensure that context arg is always present to avoid these checks here
	const FContext& AnimNextInterfaceContext = ExecuteContext.GetContext();

	// Wrap the internal result we are going to be writing to 
	TWrapParam<float> CallResult(&Result);

	// Call the interface
	ExecuteContext.SetResult(UE::AnimNext::Interface::GetDataSafe(AnimNextInterface, AnimNextInterfaceContext, CallResult));
}
/*
FRigUnit_AnimNextInterface_Pose_Execute()
{
	using namespace UE::AnimNext::Interface;

	// @TODO: ensure that context arg is always present to avoid these checks here
	if(Context.State == EControlRigState::Update && ExecuteContext.OpaqueArguments.Num() > 1 && ExecuteContext.OpaqueArguments[1] != nullptr)
	{
		const FAnimNextInterfaceUnitContext& AnimNextInterfaceUnitContext = *static_cast<const FAnimNextInterfaceUnitContext*>(ExecuteContext.OpaqueArguments[1]);
		const FContext& AnimNextInterfaceContext = AnimNextInterfaceUnitContext.AnimNextInterfaceContext;

		// Wrap the internal result we are going to be writing to 
		TWrapParam<FAnimNextInterfaceExecuteContext> CallResult(&Result);

		// Call the interface
		AnimNextInterfaceUnitContext.bResult &= UE::AnimNext::Interface::GetDataSafe(PoseInterface, AnimNextInterfaceContext, CallResult);
	}
}
*/
FRigUnit_FloatOperator_Execute()
{}

FRigUnit_PoseOperator_Execute()
{}

FRigUnit_AnimNextInterface_SequencePlayer_Execute()
{}

struct FSpringDamperState
{
	float Value = 0.0f;
	float ValueRate = 0.0f;
};

IMPLEMENT_ANIM_NEXT_INTERFACE_STATE_TYPE(FSpringDamperState, SpringDamperState);

FRigUnit_TestFloatState_Execute()
{
	using namespace UE::AnimNext::Interface;

	// @TODO: ensure that context arg is always present to avoid these checks here
	const FContext& AnimNextInterfaceContext = ExecuteContext.GetContext();

	const TParam<FSpringDamperState> State = AnimNextInterfaceContext.GetState<FSpringDamperState>(ExecuteContext.GetInterface(), 0);
	const float DeltaTime = AnimNextInterfaceContext.GetDeltaTime();

	FKernel::Run(AnimNextInterfaceContext,
		[DeltaTime, &Result](FSpringDamperState& InOutState,
					float InTargetValue,
					float InTargetValueRate,
					float InSmoothingTime,
					float InDampingRatio)
			{
				FMath::SpringDamperSmoothing(
					InOutState.Value,
					InOutState.ValueRate,
					InTargetValue,
					InTargetValueRate,
					DeltaTime,
					InSmoothingTime,
					InDampingRatio);

				Result = InOutState.Value;
			},
			State, TargetValue, TargetValueRate, SmoothingTime, DampingRatio);
}
