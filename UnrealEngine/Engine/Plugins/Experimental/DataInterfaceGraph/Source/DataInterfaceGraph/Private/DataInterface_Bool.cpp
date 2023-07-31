// Copyright Epic Games, Inc. All Rights Reserved.

#include "DataInterface_Bool.h"
#include "DataInterface.h"
#include "DataInterfaceTypes.h"
#include "DataInterfaceKernel.h"

bool UDataInterface_Bool_And::GetDataImpl(const UE::DataInterface::FContext& Context) const
{
	using namespace UE::DataInterface;
	
	check(Inputs.Num() > 0);

	bool bResult = true;
	TAllocParam<bool> IntermediateParam(Context);
	TParam<bool> Result = Context.GetResult<bool>();
	
	for(const TScriptInterface<IDataInterface>& Input : Inputs)
	{
		bResult &= UE::DataInterface::GetDataSafe(Input, Context, IntermediateParam);

		//FKernel::Run(Context,
		//	[](bool& OutResult, bool InIntermediate)
		//	{
		//		OutResult = OutResult && InIntermediate;
		//	},
		//	Result, IntermediateParam);
	}

	return bResult;
}

bool UDataInterface_Bool_Not::GetDataImpl(const UE::DataInterface::FContext& Context) const
{
	using namespace UE::DataInterface;

	TParam<bool> Result = Context.GetResult<bool>();
	
	TAllocParam<bool> CurrentValue(Context);
	bool bResult = UE::DataInterface::GetDataSafe(Input, Context, CurrentValue);

	//FKernel::Run(Context,
	//	[](bool& OutResult, bool InCurrent)
	//	{
	//		OutResult = !InCurrent;
	//	},
	//	Result, CurrentValue);

	return bResult;
}
