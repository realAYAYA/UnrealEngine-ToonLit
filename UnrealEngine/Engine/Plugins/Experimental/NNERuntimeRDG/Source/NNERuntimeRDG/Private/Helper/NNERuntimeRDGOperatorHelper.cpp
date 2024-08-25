// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNERuntimeRDGOperatorHelper.h"

namespace UE::NNERuntimeRDG::Private::OperatorHelper
{

	bool GetInt32ArrayFromConstTensor(TArray<int32, TInlineAllocator<NNE::FTensorShape::MaxRank>>& Attr, const NNE::Internal::FTensorRef Tensor)
	{
		check(Tensor != nullptr);
		Attr.Reset();

		if (!Tensor->HasPreparedData())
		{
			return false;
		}

		if (Tensor->GetDataType() == ENNETensorDataType::Int64)
		{
			for (int64 Value64 : Tensor->GetPreparedData<int64>())
			{
				int64 ValueClamped = FMath::Clamp<int64>(Value64, MIN_int32, MAX_int32);
				Attr.Add((int32)ValueClamped);
			}

			return true;
		}
		else if (Tensor->GetDataType() == ENNETensorDataType::Int32)
		{
			Attr.Append(Tensor->GetPreparedData<int32>());
			return true;
		}

		return false;
	}

} // UE::NNERuntimeRDG::Private::OperatorHelper

