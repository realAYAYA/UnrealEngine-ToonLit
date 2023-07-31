// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigUnit_DataInterfaceParameter.h"
#include "DataInterfaceUnitContext.h"
#include "DataInterfaceExecuteContext.h"
#include "DataInterface.h"
#include "DataInterfaceTypes.h"
#include "DataInterfaceKernel.h"

bool FRigUnit_DataInterfaceParameter::GetParameterInternal(FName InName, const FDataInterfaceUnitContext& InContext, void* OutResult)
{
	return true;
}

FRigUnit_DataInterfaceParameter_Float_Execute()
{
	if(Context.State == EControlRigState::Update)
	{
		GetParameterInternal(Parameter, static_cast<const FDataInterfaceUnitContext&>(Context), &Result);
	}
}

FRigUnit_DataInterfaceParameter_DataInterface_Execute()
{
	if(Context.State == EControlRigState::Update)
	{
		GetParameterInternal(Parameter, static_cast<const FDataInterfaceUnitContext&>(Context), &Result);
	}
}

FRigUnit_DataInterface_Float_Execute()
{
	using namespace UE::DataInterface;

	// @TODO: ensure that context arg is always present to avoid these checks here
	// Could bind the RigVM to a specific execution context UStruct?
	if(Context.State == EControlRigState::Update)
	{
		const FDataInterfaceExecuteContext& DataInterfaceExecuteContext = static_cast<const FDataInterfaceExecuteContext&>(RigVMExecuteContext);
		const FContext& DataInterfaceContext = DataInterfaceExecuteContext.GetContext();

		// Wrap the internal result we are going to be writing to 
		TWrapParam<float> CallResult(DataInterfaceContext, &Result);

		// Call the interface
		DataInterfaceExecuteContext.SetResult(UE::DataInterface::GetDataSafe(DataInterface, DataInterfaceContext, CallResult));
	}
}
/*
FRigUnit_DataInterface_Pose_Execute()
{
	using namespace UE::DataInterface;

	// @TODO: ensure that context arg is always present to avoid these checks here
	// Could bind the RigVM to a specific execution context UStruct?
	if(Context.State == EControlRigState::Update && RigVMExecuteContext.OpaqueArguments.Num() > 1 && RigVMExecuteContext.OpaqueArguments[1] != nullptr)
	{
		const FDataInterfaceUnitContext& DataInterfaceUnitContext = *static_cast<const FDataInterfaceUnitContext*>(RigVMExecuteContext.OpaqueArguments[1]);
		const FContext& DataInterfaceContext = DataInterfaceUnitContext.DataInterfaceContext;

		// Wrap the internal result we are going to be writing to 
		TWrapParam<FDataInterfaceExecuteContext> CallResult(DataInterfaceContext, &Result);

		// Call the interface
		DataInterfaceUnitContext.bResult &= UE::DataInterface::GetDataSafe(PoseInterface, DataInterfaceContext, CallResult);
	}
}
*/
FRigUnit_FloatOperator_Execute()
{}

FRigUnit_PoseOperator_Execute()
{}

FRigUnit_DataInterface_SequencePlayer_Execute()
{}

struct FSpringDamperState
{
	float Value = 0.0f;
	float ValueRate = 0.0f;
};

IMPLEMENT_DATA_INTERFACE_STATE_TYPE(FSpringDamperState, SpringDamperState);

FRigUnit_TestFloatState_Execute()
{
	using namespace UE::DataInterface;

	// @TODO: ensure that context arg is always present to avoid these checks here
	// Could bind the RigVM to a specific execution context UStruct?
	if(Context.State == EControlRigState::Update)
	{
		const FDataInterfaceExecuteContext& DataInterfaceExecuteContext = static_cast<const FDataInterfaceExecuteContext&>(RigVMExecuteContext);
		const FContext& DataInterfaceContext = DataInterfaceExecuteContext.GetContext();

		const TParam<FSpringDamperState> State = DataInterfaceContext.GetState<FSpringDamperState>(DataInterfaceExecuteContext.GetInterface(), 0);
		const float DeltaTime = DataInterfaceContext.GetDeltaTime();

		//FKernel::Run(DataInterfaceContext,
		//	[DeltaTime, &Result](FSpringDamperState& InOutState,
		//				float InTargetValue,
		//				float InTargetValueRate,
		//				float InSmoothingTime,
		//				float InDampingRatio)
		//		{
		//			FMath::SpringDamperSmoothing(
		//				InOutState.Value,
		//				InOutState.ValueRate,
		//				InTargetValue,
		//				InTargetValueRate,
		//				DeltaTime,
		//				InSmoothingTime,
		//				InDampingRatio);

		//			Result = InOutState.Value;
		//		},
		//		State, TargetValue, TargetValueRate, SmoothingTime, DampingRatio);
	}
}
