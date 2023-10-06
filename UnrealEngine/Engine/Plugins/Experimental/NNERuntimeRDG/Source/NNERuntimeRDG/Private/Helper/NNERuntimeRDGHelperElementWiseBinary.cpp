// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNERuntimeRDGHelperElementWiseBinary.h"
#include "NNERuntimeRDGTensorIdxIterator.h"
#include "NNETypes.h"
#include "NNETensor.h"
#include "Math/UnrealMathUtility.h"
#include "MathUtil.h"

namespace UE::NNERuntimeRDG::Internal::CPUHelper::ElementWiseBinary
{
	template<NNE::Internal::EElementWiseBinaryOperatorType OpType, typename TData> TData Apply(TData X, TData Y);

	template<> float Apply<NNE::Internal::EElementWiseBinaryOperatorType::Add, float>(float X, float Y) { return X + Y; }
	template<> float Apply<NNE::Internal::EElementWiseBinaryOperatorType::Div, float>(float X, float Y) { return X / Y; }
	template<> float Apply<NNE::Internal::EElementWiseBinaryOperatorType::Mod, float>(float X, float Y) { return FMath::Fmod(X, Y); }
	template<> float Apply<NNE::Internal::EElementWiseBinaryOperatorType::Mul, float>(float X, float Y) { return X * Y; }
	template<> float Apply<NNE::Internal::EElementWiseBinaryOperatorType::Prelu, float>(float X, float Y) { return (X < 0.0f) ? (Y * X) : X; }
	template<> float Apply<NNE::Internal::EElementWiseBinaryOperatorType::Pow, float>(float X, float Y) { return FMath::Pow(X, Y); }
	template<> float Apply<NNE::Internal::EElementWiseBinaryOperatorType::Sub, float>(float X, float Y) { return X - Y; }

	template<> int32 Apply<NNE::Internal::EElementWiseBinaryOperatorType::Add, int32>(int32 X, int32 Y) { return X + Y; }
	template<> int32 Apply<NNE::Internal::EElementWiseBinaryOperatorType::Div, int32>(int32 X, int32 Y) { return X / Y; }
	template<> int32 Apply<NNE::Internal::EElementWiseBinaryOperatorType::Mod, int32>(int32 X, int32 Y) { return X % Y; }
	template<> int32 Apply<NNE::Internal::EElementWiseBinaryOperatorType::Mul, int32>(int32 X, int32 Y) { return X * Y; }
	template<> int32 Apply<NNE::Internal::EElementWiseBinaryOperatorType::Prelu, int32>(int32 X, int32 Y) { return (X < 0.0f) ? (Y * X) : X; }
	template<> int32 Apply<NNE::Internal::EElementWiseBinaryOperatorType::Pow, int32>(int32 X, int32 Y) { return (int32)FMath::Pow((float)X, (float)Y); }
	template<> int32 Apply<NNE::Internal::EElementWiseBinaryOperatorType::Sub, int32>(int32 X, int32 Y) { return X - Y; }

	template<> int64 Apply<NNE::Internal::EElementWiseBinaryOperatorType::Add, int64>(int64 X, int64 Y) { return X + Y; }
	template<> int64 Apply<NNE::Internal::EElementWiseBinaryOperatorType::Div, int64>(int64 X, int64 Y) { return X / Y; }
	template<> int64 Apply<NNE::Internal::EElementWiseBinaryOperatorType::Mod, int64>(int64 X, int64 Y) { return X % Y; }
	template<> int64 Apply<NNE::Internal::EElementWiseBinaryOperatorType::Mul, int64>(int64 X, int64 Y) { return X * Y; }
	template<> int64 Apply<NNE::Internal::EElementWiseBinaryOperatorType::Prelu, int64>(int64 X, int64 Y) { return (X < 0.0f) ? (Y * X) : X; }
	template<> int64 Apply<NNE::Internal::EElementWiseBinaryOperatorType::Pow, int64>(int64 X, int64 Y) { return (int64)FMath::Pow((float)X, (float)Y); }
	template<> int64 Apply<NNE::Internal::EElementWiseBinaryOperatorType::Sub, int64>(int64 X, int64 Y) { return X - Y; }

	template<NNE::Internal::EElementWiseBinaryOperatorType OpType, typename TData> void Apply(const NNE::Internal::FTensor& LHSTensor, const NNE::Internal::FTensor& RHSTensor, NNE::Internal::FTensor& OutputTensor)
	{
		//Heuristic to avoid unexpected performance hit. This helper being intended for shape related arithmetic only.
		static constexpr int32 MaxItemInInputTensors = NNE::FTensorShape::MaxRank * 2;

		if (LHSTensor.HasPreparedData() && 
			RHSTensor.HasPreparedData() && 
			(LHSTensor.GetVolume() <= MaxItemInInputTensors) &&
			(RHSTensor.GetVolume() <= MaxItemInInputTensors))
		{
			TConstArrayView<TData> LHSData = LHSTensor.GetPreparedData<TData>();
			TConstArrayView<TData> RHSData = RHSTensor.GetPreparedData<TData>();
			TArray<TData> OutputData;
			OutputData.Reserve(OutputTensor.GetVolume());

			Private::TensorIdxIterator it(OutputTensor.GetShape());
			do
			{
				int32 LHSIdx = it.GetIndexToBroadcastedShape(LHSTensor.GetShape());
				int32 RHSIdx = it.GetIndexToBroadcastedShape(RHSTensor.GetShape());
				TData LHSValue = LHSData.GetData()[LHSIdx];
				TData RHSValue = RHSData.GetData()[RHSIdx];
				OutputData.Add(Apply<OpType, TData>(LHSValue, RHSValue));
			} while (it.Advance());

			check(OutputData.Num() == OutputTensor.GetVolume());
			OutputTensor.SetPreparedData<TData>(OutputData);
		}
	}

	template<typename TData> void ApplyResolvedDataType(NNE::Internal::EElementWiseBinaryOperatorType OpType, const NNE::Internal::FTensor& LHSTensor, const NNE::Internal::FTensor& RHSTensor, NNE::Internal::FTensor& OutputTensor)
	{
		switch (OpType)
		{
		case NNE::Internal::EElementWiseBinaryOperatorType::Add:
			Apply<NNE::Internal::EElementWiseBinaryOperatorType::Add, TData>(LHSTensor, RHSTensor, OutputTensor);
			break;
		case NNE::Internal::EElementWiseBinaryOperatorType::Div:
			Apply<NNE::Internal::EElementWiseBinaryOperatorType::Div, TData>(LHSTensor, RHSTensor, OutputTensor);
			break;
		case NNE::Internal::EElementWiseBinaryOperatorType::Mod:
			Apply<NNE::Internal::EElementWiseBinaryOperatorType::Mod, TData>(LHSTensor, RHSTensor, OutputTensor);
			break;
		case NNE::Internal::EElementWiseBinaryOperatorType::Mul:
			Apply<NNE::Internal::EElementWiseBinaryOperatorType::Mul, TData>(LHSTensor, RHSTensor, OutputTensor);
			break;
		case NNE::Internal::EElementWiseBinaryOperatorType::Prelu:
			Apply<NNE::Internal::EElementWiseBinaryOperatorType::Prelu, TData>(LHSTensor, RHSTensor, OutputTensor);
			break;
		case NNE::Internal::EElementWiseBinaryOperatorType::Pow:
			Apply<NNE::Internal::EElementWiseBinaryOperatorType::Pow, TData>(LHSTensor, RHSTensor, OutputTensor);
			break;
		case NNE::Internal::EElementWiseBinaryOperatorType::Sub:
			Apply<NNE::Internal::EElementWiseBinaryOperatorType::Sub, TData>(LHSTensor, RHSTensor, OutputTensor);
			break;
		default:
			break;
		}
	}

	void Apply(NNE::Internal::EElementWiseBinaryOperatorType OpType, const NNE::Internal::FTensor& LHSTensor, const NNE::Internal::FTensor& RHSTensor, NNE::Internal::FTensor& OutputTensor)
	{
		switch (OutputTensor.GetDataType())
		{
		case ENNETensorDataType::Float:
			ApplyResolvedDataType<float>(OpType, LHSTensor, RHSTensor, OutputTensor);
			break;
		case ENNETensorDataType::Int32:
			ApplyResolvedDataType<int32>(OpType, LHSTensor, RHSTensor, OutputTensor);
			break;
		case ENNETensorDataType::Int64:
			ApplyResolvedDataType<int64>(OpType, LHSTensor, RHSTensor, OutputTensor);
			break;
		default:
			break;
		}
	}
	
} // UE::NNERuntimeRDG::Internal::CPUHelper::ElementWiseBinary
