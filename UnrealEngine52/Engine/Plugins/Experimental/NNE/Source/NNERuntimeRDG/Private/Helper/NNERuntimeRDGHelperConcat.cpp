// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNERuntimeRDGHelperConcat.h"

#include "NNECoreTypes.h"
#include "NNERuntimeRDGTensorIdxIterator.h"

namespace UE::NNERuntimeRDG::Internal::CPUHelper::Concat
{
	void Apply(TConstArrayView<NNECore::Internal::FTensorRef> InputTensors, NNECore::Internal::FTensor& OutputTensor, int32 Axis)
	{
		static constexpr int32 MaxItemInOutputTensor = NNECore::FTensorShape::MaxRank * 2;
		
		if (OutputTensor.GetVolume() >= MaxItemInOutputTensor)
		{
			return;
		}

		if (OutputTensor.GetDataType() != ENNETensorDataType::Float)
		{
			return;
		}
		
		for (NNECore::Internal::FTensorRef InputTensor : InputTensors)
		{
			check(InputTensor != nullptr);
			if (!InputTensor->HasPreparedData())
			{
				return;
			}
		}

		check(Axis >= 0 && Axis < OutputTensor.GetShape().Rank());

		TArray<float> OutputData;
		const Private::TensorIdxIterator itOutput(OutputTensor.GetShape());
		int32 ConcatAxisOffset = 0; 

		OutputData.SetNumUninitialized(OutputTensor.GetVolume());
		
		for (NNECore::Internal::FTensorRef InputTensor : InputTensors)
		{
			TConstArrayView<float> InputData = InputTensor->GetPreparedData<float>();
			Private::TensorIdxIterator itInput(InputTensor->GetShape());

			do
			{
				TArray<uint32> CurOutputPosition(itInput.GetPositions());
				CurOutputPosition[Axis] += ConcatAxisOffset;
				
				float Value = InputData[itInput.GetIndex()];
				OutputData[itOutput.GetIndexFromPosition(CurOutputPosition)] = Value;
				
			} while (itInput.Advance());
			
			ConcatAxisOffset += InputTensor->GetShape().GetData()[Axis];
		}

		OutputTensor.SetPreparedData<float>(OutputData);
	}
	
} // UE::NNERuntimeRDG::Internal::CPUHelper::Concat
