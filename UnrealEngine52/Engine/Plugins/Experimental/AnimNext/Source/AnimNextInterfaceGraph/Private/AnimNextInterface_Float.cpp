// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextInterface_Float.h"
#include "AnimNextInterface.h"
#include "AnimNextInterfaceTypes.h"
#include "AnimNextInterfaceKernel.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNextInterface_Float)

#define LOCTEXT_NAMESPACE "AnimNextInterface"

bool UAnimNextInterface_Float_Multiply::GetDataImpl(const UE::AnimNext::Interface::FContext& Context) const
{
	using namespace UE::AnimNext::Interface;
	
	check(Inputs.Num() > 0);

	bool bResult = true;
	TAllocParam<float> IntermediateParam(Context);
	TParam<float> Result = Context.GetResult<float>();
	
	for(const TScriptInterface<IAnimNextInterface>& Input : Inputs)
	{
		bResult &= UE::AnimNext::Interface::GetDataSafe(Input, Context, IntermediateParam);

		FKernel::Run(Context,
			[](float& OutResult, float InIntermediate)
			{
				OutResult *= InIntermediate;
			},
			Result, IntermediateParam);
	}

	return bResult;
}

bool UAnimNextInterface_Float_InterpTo::GetDataImpl(const UE::AnimNext::Interface::FContext& Context) const
{
	using namespace UE::AnimNext::Interface;

	TParam<float> Result = Context.GetResult<float>();
	const float DeltaTime = Context.GetDeltaTime();
	
	TAllocParam<float> CurrentValue(Context);
	bool bResult = UE::AnimNext::Interface::GetDataSafe(Current, Context, CurrentValue);

	TAllocParam<float> TargetValue(Context);
	bResult &= UE::AnimNext::Interface::GetDataSafe(Target, Context, TargetValue);

	TAllocParam<float> SpeedValue(Context);
	bResult &= UE::AnimNext::Interface::GetDataSafe(Speed, Context, SpeedValue);

	FKernel::Run(Context,
		[DeltaTime](float& OutResult, float InCurrent, float InTarget, float InSpeed)
		{
			OutResult = FMath::FInterpConstantTo(InCurrent, InTarget, DeltaTime, InSpeed);
		},
		Result, CurrentValue, TargetValue, SpeedValue);

	return bResult;
}

struct FSpringInterpState
{
	float Value;
	float ValueRate;
};

IMPLEMENT_ANIM_NEXT_INTERFACE_STATE_TYPE(FSpringInterpState, SpringInterpState);

bool UAnimNextInterface_Float_SpringInterp::GetDataImpl(const UE::AnimNext::Interface::FContext& Context) const
{
	using namespace UE::AnimNext::Interface;
	
	const TParam<FSpringInterpState> State = Context.GetState<FSpringInterpState>(this, 0);
	const float DeltaTime = Context.GetDeltaTime();

	TAllocParam<float> TargetValueParam(Context);
	bool bResult = UE::AnimNext::Interface::GetDataSafe(TargetValue, Context, TargetValueParam);

	TAllocParam<float> TargetValueRateParam(Context);
	bResult &= UE::AnimNext::Interface::GetDataSafe(TargetValueRate, Context, TargetValueRateParam);

	TAllocParam<float> SmoothingTimeParam(Context);
	bResult &= UE::AnimNext::Interface::GetDataSafe(SmoothingTime, Context, SmoothingTimeParam);

	TAllocParam<float> DampingRatioParam(Context);
	bResult &= UE::AnimNext::Interface::GetDataSafe(DampingRatio, Context, DampingRatioParam);

	FKernel::Run(Context,
	[DeltaTime](FSpringInterpState& InOutState,
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
		},
		State, TargetValueParam, TargetValueRateParam, SmoothingTimeParam, DampingRatioParam);

	return bResult;
}


#undef LOCTEXT_NAMESPACE
