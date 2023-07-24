// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextInterface_Bool.h"
#include "AnimNextInterface.h"
#include "AnimNextInterfaceTypes.h"
#include "AnimNextInterfaceKernel.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNextInterface_Bool)

bool UAnimNextInterface_Bool_And::GetDataImpl(const UE::AnimNext::Interface::FContext& Context) const
{
	using namespace UE::AnimNext::Interface;
	
	check(Inputs.Num() > 0);

	bool bResult = true;
	TAllocParam<bool> IntermediateParam(Context);
	TParam<bool> Result = Context.GetResult<bool>();
	
	for(const TScriptInterface<IAnimNextInterface>& Input : Inputs)
	{
		bResult &= UE::AnimNext::Interface::GetDataSafe(Input, Context, IntermediateParam);

		FKernel::Run(Context,
			[](bool& OutResult, bool InIntermediate)
			{
				OutResult = OutResult && InIntermediate;
			},
			Result, IntermediateParam);
	}

	return bResult;
}

bool UAnimNextInterface_Bool_Not::GetDataImpl(const UE::AnimNext::Interface::FContext& Context) const
{
	using namespace UE::AnimNext::Interface;

	TParam<bool> Result = Context.GetResult<bool>();
	
	TAllocParam<bool> CurrentValue(Context);
	bool bResult = UE::AnimNext::Interface::GetDataSafe(Input, Context, CurrentValue);

	FKernel::Run(Context,
		[](bool& OutResult, bool InCurrent)
		{
			OutResult = !InCurrent;
		},
		Result, CurrentValue);

	return bResult;
}
