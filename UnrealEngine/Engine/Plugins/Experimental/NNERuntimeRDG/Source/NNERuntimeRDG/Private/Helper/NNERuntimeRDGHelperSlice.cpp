// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNERuntimeRDGHelperSlice.h"
#include "NNERuntimeRDGTensorIdxIterator.h"

namespace UE::NNERuntimeRDG::Internal::CPUHelper::Slice
{
	template<typename TData> void ApplyResolvedInputType(const NNE::Internal::FTensor& InputTensor, NNE::Internal::FTensor& OutputTensor, TConstArrayView<int32> Starts)
	{
		check(InputTensor.HasPreparedData());
		check(InputTensor.GetShape().Rank() == Starts.Num());
		check(OutputTensor.GetShape().Rank() == Starts.Num());

		TArray<TData> OutputData;
		TConstArrayView<TData> InputData = InputTensor.GetPreparedData<TData>();
		Private::TensorIdxIterator itOutput(OutputTensor.GetShape());
		const Private::TensorIdxIterator itInput(InputTensor.GetShape());

		OutputData.SetNumUninitialized(OutputTensor.GetVolume());
		do
		{
			TArray<uint32> CurInputPosition(itOutput.GetPositions());
			for (int r = 0; r < CurInputPosition.Num(); ++r)
			{
				CurInputPosition[r] += Starts[r];
			}

			TData Value = InputData[itInput.GetIndexFromPosition(CurInputPosition)];
			OutputData[itOutput.GetIndex()] = Value;

		} while (itOutput.Advance());

		OutputTensor.SetPreparedData<TData>(OutputData);
	}

	void Apply(const NNE::Internal::FTensor& InputTensor, NNE::Internal::FTensor& OutputTensor, TConstArrayView<int32> Starts)
	{
		static constexpr int32 MaxItemInOutputTensor = NNE::FTensorShape::MaxRank * 2;

		if (OutputTensor.GetVolume() >= MaxItemInOutputTensor)
		{
			return;
		}

		if (!InputTensor.HasPreparedData())
		{
			return;
		}

		switch (InputTensor.GetDataType())
		{
		case ENNETensorDataType::Int32:
			ApplyResolvedInputType<int32>(InputTensor, OutputTensor, Starts);
			break;

		case ENNETensorDataType::Int64:
			ApplyResolvedInputType<int64>(InputTensor, OutputTensor, Starts);
			break;

		case ENNETensorDataType::Float:
			ApplyResolvedInputType<float>(InputTensor, OutputTensor, Starts);
			break;
		}
	}
	
} // UE::NNERuntimeRDG::Private::CPUHelper::Slice
