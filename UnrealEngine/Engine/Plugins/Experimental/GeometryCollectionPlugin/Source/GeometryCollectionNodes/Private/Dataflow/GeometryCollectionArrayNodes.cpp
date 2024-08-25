// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/GeometryCollectionArrayNodes.h"
#include "Dataflow/DataflowCore.h"


//#include UE_INLINE_GENERATED_CPP_BY_NAME(GeometryCollectionArrayNodes)

namespace Dataflow
{

	void GeometryCollectionArrayNodes()
	{
		static const FLinearColor CDefaultNodeBodyTintColor = FLinearColor(0.f, 0.f, 0.f, 0.5f);

		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FGetFloatArrayElementDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FFloatArrayToIntArrayDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FGetArrayElementDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FGetNumArrayElementsDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FBoolArrayToFaceSelectionDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FFloatArrayToVertexSelectionDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FFloatArrayNormalizeDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FVectorArrayNormalizeDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FUnionIntArraysDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FRemoveFloatArrayElementDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FFloatArrayComputeStatisticsDataflowNode);

	}
}

void FGetFloatArrayElementDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA(&FloatValue))
	{
		const TArray<float>& InArray = GetValue(Context, &FloatArray);
		const int32& InIndex = GetValue(Context, &Index);
		const float OutputValue = InArray.IsValidIndex(InIndex) ? InArray[InIndex] : 0.0f;

		SetValue(Context, OutputValue, &FloatValue);
	}
}

void FFloatArrayToIntArrayDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<TArray<int32>>(&IntArray))
	{
		TArray<float> FloatVal = GetValue<TArray<float>>(Context, &FloatArray);
		TArray<int32> RetVal; RetVal.SetNumUninitialized(FloatVal.Num());
		if (Function == EFloatArrayToIntArrayFunctionEnum::Dataflow_FloatToInt_Function_Floor)
		{
			for (int32 i = 0; i < FloatVal.Num(); i++)
			{
				RetVal[i] = FMath::FloorToInt32(FloatVal[i]);
			}
		}
		else if (Function == EFloatArrayToIntArrayFunctionEnum::Dataflow_FloatToInt_Function_Ceil)
		{
			for (int32 i = 0; i < FloatVal.Num(); i++)
			{
				RetVal[i] = FMath::CeilToInt32(FloatVal[i]);
			}
		}
		else if (Function == EFloatArrayToIntArrayFunctionEnum::Dataflow_FloatToInt_Function_Round)
		{
			for (int32 i = 0; i < FloatVal.Num(); i++)
			{
				RetVal[i] = FMath::RoundToInt32(FloatVal[i]);
			}
		}
		else if (Function == EFloatArrayToIntArrayFunctionEnum::Dataflow_FloatToInt_Function_Truncate)
		{
			for (int32 i = 0; i < FloatVal.Num(); i++)
			{
				RetVal[i] = int32(FMath::TruncToFloat(FloatVal[i]));
			}
		}

		else if (Function == EFloatArrayToIntArrayFunctionEnum::Dataflow_FloatToInt_NonZeroToIndex)
		{
			int32 RetValIndex = 0;
			for (int32 i = 0; i < FloatVal.Num(); i++)
			{
				if (FloatVal[i] != 0.0)
				{
					RetVal[RetValIndex] = i;
					RetValIndex++;
				}
			}
			RetVal.SetNum(RetValIndex);
		}
		else if (Function == EFloatArrayToIntArrayFunctionEnum::Dataflow_FloatToInt_ZeroToIndex)
		{
			int32 RetValIndex = 0;
			for (int32 i = 0; i < FloatVal.Num(); i++)
			{
				if (FloatVal[i] == 0.0)
				{
					RetVal[RetValIndex] = i;
					RetValIndex++;
				}
			}
			RetVal.SetNum(RetValIndex);
		}

		SetValue(Context, MoveTemp(RetVal), &IntArray);
	}
}

void FGetArrayElementDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FVector>(&Point))
	{
		const TArray<FVector>& Array = GetValue<TArray<FVector>>(Context, &Points);
		if (Array.Num() > 0 && Index >= 0 && Index < Array.Num())
		{
			SetValue(Context, Array[Index], &Point);
			return;
		}

		SetValue(Context, FVector(0.f), &Point);
	}
}

void FGetNumArrayElementsDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<int32>(&NumElements))
	{
		if (IsConnected<TArray<float>>(&FloatArray))
		{
			SetValue(Context, GetValue<TArray<float>>(Context, &FloatArray).Num(), &NumElements);
			return;
		}
		else if (IsConnected<TArray<int32>>(&IntArray))
		{
			SetValue(Context, GetValue<TArray<int32>>(Context, &IntArray).Num(), &NumElements);
			return;
		}
		else if (IsConnected<TArray<FVector>>(&Points))
		{
			SetValue(Context, GetValue<TArray<FVector>>(Context, &Points).Num(), &NumElements);
			return;
		}
		else if (IsConnected<TArray<FVector3f>>(&Vector3fArray))
		{
			SetValue(Context, GetValue<TArray<FVector3f>>(Context, &Vector3fArray).Num(), &NumElements);
			return;
		}

		SetValue(Context, 0, &NumElements);
	}
}

void FBoolArrayToFaceSelectionDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FDataflowFaceSelection>(&FaceSelection))
	{
		const TArray<bool>& InBoolAttributeData = GetValue<TArray<bool>>(Context, &BoolAttributeData);

		FDataflowFaceSelection NewFaceSelection;
		NewFaceSelection.Initialize(InBoolAttributeData.Num(), false);
		NewFaceSelection.SetFromArray(InBoolAttributeData);

		SetValue(Context, MoveTemp(NewFaceSelection), &FaceSelection);
	}
}


void FFloatArrayToVertexSelectionDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FDataflowVertexSelection>(&VertexSelection))
	{
		const TArray<float>& InFloatArray = GetValue<TArray<float>>(Context, &FloatArray);

		FDataflowVertexSelection NewVertexSelection;
		NewVertexSelection.Initialize(InFloatArray.Num(), false);

		for (int32 Idx = 0; Idx < InFloatArray.Num(); ++Idx)
		{
			if (Operation == ECompareOperation1Enum::Dataflow_Compare_Equal)
			{
				if (InFloatArray[Idx] == Threshold)
				{
					NewVertexSelection.SetSelected(Idx);
				}
			}
			else if (Operation == ECompareOperation1Enum::Dataflow_Compare_Smaller)
			{
				if (InFloatArray[Idx] < Threshold)
				{
					NewVertexSelection.SetSelected(Idx);
				}
			}
			else if (Operation == ECompareOperation1Enum::Dataflow_Compare_SmallerOrEqual)
			{
				if (InFloatArray[Idx] <= Threshold)
				{
					NewVertexSelection.SetSelected(Idx);
				}
			}
			else if (Operation == ECompareOperation1Enum::Dataflow_Compare_Greater)
			{
				if (InFloatArray[Idx] > Threshold)
				{
					NewVertexSelection.SetSelected(Idx);
				}
			}
			else if (Operation == ECompareOperation1Enum::Dataflow_Compare_GreaterOrEqual)
			{
				if (InFloatArray[Idx] >= Threshold)
				{
					NewVertexSelection.SetSelected(Idx);
				}
			}
		}

		SetValue(Context, MoveTemp(NewVertexSelection), &VertexSelection);
	}
}

void FFloatArrayNormalizeDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<TArray<float>>(&OutFloatArray))
	{
		const TArray<float>& InInFloatArray = GetValue<TArray<float>>(Context, &InFloatArray);
		const FDataflowVertexSelection& InSelection = GetValue<FDataflowVertexSelection>(Context, &Selection);
		const float InMinRange = GetValue<float>(Context, &MinRange);
		const float InMaxRange = GetValue<float>(Context, &MaxRange);

		const int32 NumElems = InFloatArray.Num();

		TArray<float> NewFloatArray;
		NewFloatArray.Init(false, NumElems);

		if (IsConnected<FDataflowVertexSelection>(&Selection))
		{
			if (InInFloatArray.Num() == InSelection.Num())
			{
				// Compute Min/Max
				float MinValue = 1e9, MaxValue = 1e-9;

				for (int32 Idx = 0; Idx < NumElems; ++Idx)
				{
					if (InSelection.IsSelected(Idx))
					{
						if (InInFloatArray[Idx] < MinValue)
						{
							MinValue = InInFloatArray[Idx];
						}

						if (InInFloatArray[Idx] > MaxValue)
						{
							MaxValue = InInFloatArray[Idx];
						}
					}
				}

				for (int32 Idx = 0; Idx < NumElems; ++Idx)
				{
					if (InSelection.IsSelected(Idx))
					{
						// Normalize it
						NewFloatArray[Idx] = (NewFloatArray[Idx] - MinValue) / (MaxValue - MinValue);

						// Transform it into (MinRange, MaxRange)
						NewFloatArray[Idx] = InMinRange + NewFloatArray[Idx] * (InMaxRange - InMinRange);
					}
				}

				SetValue(Context, MoveTemp(NewFloatArray), &OutFloatArray);
				return;
			}
		}
		else
		{
			// Compute Min/Max
			float MinValue = 1e9, MaxValue = 1e-9;

			for (int32 Idx = 0; Idx < NumElems; ++Idx)
			{
				if (InInFloatArray[Idx] < MinValue)
				{
					MinValue = InInFloatArray[Idx];
				}

				if (InInFloatArray[Idx] > MaxValue)
				{
					MaxValue = InInFloatArray[Idx];
				}
			}

			for (int32 Idx = 0; Idx < NumElems; ++Idx)
			{
				// Normalize it
				NewFloatArray[Idx] = (NewFloatArray[Idx] - MinValue) / (MaxValue - MinValue);

				// Transform it into (MinRange, MaxRange)
				NewFloatArray[Idx] = InMinRange + NewFloatArray[Idx] * (InMaxRange - InMinRange);
			}

			SetValue(Context, MoveTemp(NewFloatArray), &OutFloatArray);
			return;
		}

		SetValue(Context, TArray<float>(), &OutFloatArray);
	}
}


void FVectorArrayNormalizeDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<TArray<FVector>>(&OutVectorArray))
	{
		const TArray<FVector>& InInVectorArray = GetValue<TArray<FVector>>(Context, &InVectorArray);
		const FDataflowVertexSelection& InSelection = GetValue<FDataflowVertexSelection>(Context, &Selection);
		const float InMagnitude = GetValue<float>(Context, &Magnitude);

		const int32 NumElems = InInVectorArray.Num();

		TArray<FVector> NewVectorArray;
		NewVectorArray.Init(FVector(0.f), NumElems);

		if (IsConnected<FDataflowVertexSelection>(&Selection))
		{
			if (InInVectorArray.Num() == InSelection.Num())
			{
				for (int32 Idx = 0; Idx < NumElems; ++Idx)
				{
					FVector Vector = InInVectorArray[Idx];

					if (InSelection.IsSelected(Idx))
					{
						Vector.Normalize();
						Vector *= InMagnitude;
						NewVectorArray[Idx] = Vector;
					}
				}

				SetValue(Context, MoveTemp(NewVectorArray), &OutVectorArray);
				return;
			}
		}
		else
		{
			for (int32 Idx = 0; Idx < NumElems; ++Idx)
			{
				FVector Vector = InInVectorArray[Idx];

				Vector.Normalize();
				Vector *= InMagnitude;
				NewVectorArray[Idx] = Vector;
			}

			SetValue(Context, MoveTemp(NewVectorArray), &OutVectorArray);
			return;
		}

		SetValue<TArray<FVector>>(Context, TArray<FVector>(), &OutVectorArray);
	}
}

void FUnionIntArraysDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<TArray<int32>>(&OutArray))
	{
		TArray<int32> Array1 = GetValue<TArray<int32>>(Context, &InArray1);
		TArray<int32> Array2 = GetValue<TArray<int32>>(Context, &InArray2);
		TArray<int32> OutputArray;
		for (int32 i = 0; i < Array1.Num(); i++)
		{
			OutputArray.AddUnique(Array1[i]);
		}
		for (int32 i = 0; i < Array2.Num(); i++)
		{
			OutputArray.AddUnique(Array2[i]);
		}
		SetValue(Context, MoveTemp(OutputArray), &OutArray);
	}
}

void FRemoveFloatArrayElementDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA(&FloatArray))
	{
		TArray<float> Array = GetValue<TArray<float>>(Context, &FloatArray);
		if (Array.Num() > 0 && Array.IsValidIndex(Index))
		{
			if (bPreserveOrder)
			{
				Array.RemoveAt(Index);
			}
			else
			{
				Array.RemoveAtSwap(Index, 1, EAllowShrinking::No);
			}

			SetValue(Context, Array, &FloatArray);
			return;
		}

		SetValue(Context, Array, &FloatArray);
	}
}

// -----------------------------------------------------------------------------------------------

static float ComputeMin(const TArray<float>& FloatArray, TArray<int32>& IndexArr)
{
	float MinVal = TNumericLimits<float>::Max();
	for (int32 Idx = 0; Idx < FloatArray.Num(); ++Idx)
	{
		if (FloatArray[Idx] < MinVal)
		{
			MinVal = FloatArray[Idx];
		}
	}

	for (int32 Idx = 0; Idx < FloatArray.Num(); ++Idx)
	{
		if (FloatArray[Idx] == MinVal)
		{
			IndexArr.Add(Idx);
		}
	}

	return MinVal;
}

static float ComputeMax(const TArray<float>& FloatArray, TArray<int32>& IndexArr)
{
	float MaxVal = TNumericLimits<float>::Min();
	for (int32 Idx = 0; Idx < FloatArray.Num(); ++Idx)
	{
		if (FloatArray[Idx] > MaxVal)
		{
			MaxVal = FloatArray[Idx];
		}
	}

	for (int32 Idx = 0; Idx < FloatArray.Num(); ++Idx)
	{
		if (FloatArray[Idx] == MaxVal)
		{
			IndexArr.Add(Idx);
		}
	}

	return MaxVal;
}

static float ComputeMean(const TArray<float>& FloatArray, TArray<int32>& IndexArr)
{
	double SumVal = 0.f;

	for (int32 Idx = 0; Idx < FloatArray.Num(); ++Idx)
	{
		SumVal += FloatArray[Idx];
	}

	return (float)(SumVal / (double)FloatArray.Num());
}

static float ComputeMedian(TArray<float> FloatArray, TArray<int32>& IndexArr)
{
	FloatArray.Sort();
	float MedianVal;
	int32 NumElems = FloatArray.Num();

	if (NumElems % 2 == 0)
	{
		MedianVal = (FloatArray[NumElems / 2 - 1] + FloatArray[NumElems / 2]) / 2.f;

		IndexArr.Add(NumElems / 2 - 1);
		IndexArr.Add(NumElems / 2);
	}
	else
	{
		MedianVal = FloatArray[(NumElems - 1) / 2];

		IndexArr.Add((NumElems - 1) / 2);
	}

	return MedianVal;
}

static float ComputeMode(const TArray<float>& FloatArray, TArray<int32>& IndexArr)
{
	float MaxVal = 0.f;
	int32 MaxCount = 0;

	for (int32 Idx1 = 0; Idx1 < FloatArray.Num(); ++Idx1)
	{
		int32 Count = 0;

		for (int32 Idx2 = 0; Idx2 < FloatArray.Num(); ++Idx2)
		{
			if (FloatArray[Idx2] == FloatArray[Idx1])
			{
				Count++;
			}

			if (Count > MaxCount)
			{
				MaxCount = Count;
				MaxVal = FloatArray[Idx1];
			}
		}
	}

	for (int32 Idx = 0; Idx < FloatArray.Num(); ++Idx)
	{
		if (FloatArray[Idx] == MaxVal)
		{
			IndexArr.Add(Idx);
		}
	}

	return MaxVal;
}

static float ComputeSum(const TArray<float>& FloatArray, TArray<int32>& IndexArr)
{
	double SumVal = 0.f;

	for (int32 Idx = 0; Idx < FloatArray.Num(); ++Idx)
	{
		SumVal += FloatArray[Idx];
	}

	return (float)SumVal;
}

// -----------------------------------------------------------------------------------------------

void FFloatArrayComputeStatisticsDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA(&Value) || Out->IsA(&Indices))
	{
		const TArray<float>& InFloatArray = GetValue(Context, &FloatArray);
		const FDataflowTransformSelection& InTransformSelection = GetValue(Context, &TransformSelection);

		TArray<float> Array;
		if (IsConnected<FDataflowTransformSelection>(&TransformSelection) && InTransformSelection.Num() == InFloatArray.Num())
		{
			for (int32 Idx = 0; Idx < InFloatArray.Num(); ++Idx)
			{
				if (InTransformSelection.IsSelected(Idx))
				{
					Array.Add(InFloatArray[Idx]);
				}
			}
		}
		else
		{
			for (int32 Idx = 0; Idx < InFloatArray.Num(); ++Idx)
			{
				Array.Add(InFloatArray[Idx]);
			}
		}

		float OutValue;
		TArray<int32> OutIndices;

		switch (OperationName)
		{
		case EStatisticsOperationEnum::Dataflow_EStatisticsOperationEnum_Min:
			OutValue = ComputeMin(Array, OutIndices);
			break;
		case EStatisticsOperationEnum::Dataflow_EStatisticsOperationEnum_Max:
			OutValue = ComputeMax(Array, OutIndices);
			break;
		case EStatisticsOperationEnum::Dataflow_EStatisticsOperationEnum_Mean:
			OutValue = ComputeMean(Array, OutIndices);
			break;
		case EStatisticsOperationEnum::Dataflow_EStatisticsOperationEnum_Median:
			OutValue = ComputeMedian(Array, OutIndices);
			break;
		case EStatisticsOperationEnum::Dataflow_EStatisticsOperationEnum_Mode:
			OutValue = ComputeMode(Array, OutIndices);
			break;
		case EStatisticsOperationEnum::Dataflow_EStatisticsOperationEnum_Sum:
			OutValue = ComputeSum(Array, OutIndices);
			break;
		}

		SetValue(Context, OutValue, &Value);
		SetValue(Context, OutIndices, &Indices);
	}
}
