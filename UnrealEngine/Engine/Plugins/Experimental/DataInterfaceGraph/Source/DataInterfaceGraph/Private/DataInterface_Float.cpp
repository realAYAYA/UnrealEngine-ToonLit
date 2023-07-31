// Copyright Epic Games, Inc. All Rights Reserved.

#include "DataInterface_Float.h"
#include "DataInterface.h"
#include "DataInterfaceTypes.h"
#include "DataInterfaceKernel.h"

#define LOCTEXT_NAMESPACE "DataInterface"

bool UDataInterface_Float_Multiply::GetDataImpl(const UE::DataInterface::FContext& Context) const
{
	using namespace UE::DataInterface;
	
	check(Inputs.Num() > 0);

	bool bResult = true;
	TAllocParam<float> IntermediateParam(Context);
	TParam<float> Result = Context.GetResult<float>();
	
	for(const TScriptInterface<IDataInterface>& Input : Inputs)
	{
		bResult &= UE::DataInterface::GetDataSafe(Input, Context, IntermediateParam);

		//FKernel::Run(Context,
		//	[](float& OutResult, float InIntermediate)
		//	{
		//		OutResult *= InIntermediate;
		//	},
		//	Result, IntermediateParam);
	}

	return bResult;
}

bool UDataInterface_Float_InterpTo::GetDataImpl(const UE::DataInterface::FContext& Context) const
{
	using namespace UE::DataInterface;

	TParam<float> Result = Context.GetResult<float>();
	const float DeltaTime = Context.GetDeltaTime();
	
	TAllocParam<float> CurrentValue(Context);
	bool bResult = UE::DataInterface::GetDataSafe(Current, Context, CurrentValue);

	TAllocParam<float> TargetValue(Context);
	bResult &= UE::DataInterface::GetDataSafe(Target, Context, TargetValue);

	TAllocParam<float> SpeedValue(Context);
	bResult &= UE::DataInterface::GetDataSafe(Speed, Context, SpeedValue);

	//FKernel::Run(Context,
	//	[DeltaTime](float& OutResult, float InCurrent, float InTarget, float InSpeed)
	//	{
	//		OutResult = FMath::FInterpConstantTo(InCurrent, InTarget, DeltaTime, InSpeed);
	//	},
	//	Result, CurrentValue, TargetValue, SpeedValue);

	return bResult;
}

struct FSpringInterpState
{
	float Value;
	float ValueRate;
};

IMPLEMENT_DATA_INTERFACE_STATE_TYPE(FSpringInterpState, SpringInterpState);

bool UDataInterface_Float_SpringInterp::GetDataImpl(const UE::DataInterface::FContext& Context) const
{
	using namespace UE::DataInterface;
	
	const TParam<FSpringInterpState> State = Context.GetState<FSpringInterpState>(this, 0);
	const float DeltaTime = Context.GetDeltaTime();

	TAllocParam<float> TargetValueParam(Context);
	bool bResult = UE::DataInterface::GetDataSafe(TargetValue, Context, TargetValueParam);

	TAllocParam<float> TargetValueRateParam(Context);
	bResult &= UE::DataInterface::GetDataSafe(TargetValueRate, Context, TargetValueRateParam);

	TAllocParam<float> SmoothingTimeParam(Context);
	bResult &= UE::DataInterface::GetDataSafe(SmoothingTime, Context, SmoothingTimeParam);

	TAllocParam<float> DampingRatioParam(Context);
	bResult &= UE::DataInterface::GetDataSafe(DampingRatio, Context, DampingRatioParam);

	//FKernel::Run(Context,
	//[DeltaTime](FSpringInterpState& InOutState,
	//				float InTargetValue,
	//				float InTargetValueRate,
	//				float InSmoothingTime,
	//				float InDampingRatio)
	//	{
	//		FMath::SpringDamperSmoothing(
	//			InOutState.Value,
	//			InOutState.ValueRate,
	//			InTargetValue,
	//			InTargetValueRate,
	//			DeltaTime,
	//			InSmoothingTime,
	//			InDampingRatio);
	//	},
	//	State, TargetValueParam, TargetValueRateParam, SmoothingTimeParam, DampingRatioParam);

	return bResult;
}


#undef LOCTEXT_NAMESPACE