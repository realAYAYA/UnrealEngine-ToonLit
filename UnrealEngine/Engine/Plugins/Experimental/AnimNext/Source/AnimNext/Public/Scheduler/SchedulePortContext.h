// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Param/ParamCompatibility.h"
#include "Param/ParamStackLayerHandle.h"

namespace UE::AnimNext
{

struct FSchedulePortContext
{
	FSchedulePortContext() = delete;

	FSchedulePortContext(const TUniquePtr<FParamStackLayer>& InResultLayer)
		: ResultLayer(InResultLayer)
	{}

	// Get a pointer to a parameter's value given a FParamId.
	// @param	InParamId			Parameter ID to find the currently-pushed value for
	// @param	OutResult			Optional ptr to aresult that will be filled if an error occurs
	// @return nullptr if the parameter is not mapped
	template<typename ValueType>
	const ValueType* GetParamPtr(FParamId InParamId, FParamResult* OutResult = nullptr) const
	{
		TConstArrayView<uint8> Data;
		FParamTypeHandle TypeHandle;
		FParamResult Result = GetParamData(InParamId, FParamTypeHandle::GetHandle<ValueType>(), Data, TypeHandle);
		if (OutResult)
		{
			*OutResult = Result;
		}
		return reinterpret_cast<const ValueType*>(Data.GetData());
	}

	// Get a const reference to the value of a parameter given a FParamId.
	// @param	InParamId			Parameter ID to find the currently-pushed value for
	// @param	OutResult			Optional ptr to a result that will be filled if an error occurs
	// @return a const reference to the parameter's value. Function asserts if the parameter is not present
	template<typename ValueType>
	const ValueType& GetParam(FParamId InParamId, FParamResult* OutResult = nullptr) const
	{
		const ValueType* ParamPtr = GetParamPtr<ValueType>(InParamId, OutResult);
		check(ParamPtr);
		return *ParamPtr;
	}

	ANIMNEXT_API FParamResult GetParamData(FParamId InId, FParamTypeHandle InTypeHandle, TConstArrayView<uint8>& OutParamData, FParamTypeHandle& OutParamTypeHandle, FParamCompatibility InRequiredCompatibility = FParamCompatibility::Equal()) const;

private:
	// The result layer that the port operates on
	const TUniquePtr<FParamStackLayer>& ResultLayer;
};

}
