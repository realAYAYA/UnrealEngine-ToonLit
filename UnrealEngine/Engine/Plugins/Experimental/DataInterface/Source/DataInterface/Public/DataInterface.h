// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DataInterfaceContext.h"

namespace UE::DataInterface
{

template<typename InterfaceType, typename ValueType>
static bool GetDataSafe(const TScriptInterface<InterfaceType> DataInterface, const FContext& InContext, ValueType& Result)
{
	if(DataInterface.GetInterface() != nullptr)
	{
		if constexpr (TIsDerivedFrom<ValueType, FParam>::Value)
		{
			return DataInterface.GetInterface()->GetData(InContext, Result);
		}
		else
		{
			TWrapParam<ValueType> WrappedResult(InContext, &Result);
			return DataInterface.GetInterface()->GetData(InContext, WrappedResult);
		}
	}

	return false;
}

template<typename ValueType>
static bool GetParameter(const FContext& InContext, FName InKey, ValueType& Result)
{
	if constexpr (TIsDerivedFrom<ValueType, FParam>::Value)
	{
		return InContext.GetParameter(InKey, Result);
	}
	else
	{
		TWrapParam<ValueType> WrappedResult(InContext, &Result);
		return InContext.GetParameter(InKey, WrappedResult);
	}
}

}