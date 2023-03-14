// Copyright Epic Games, Inc. All Rights Reserved.

#include "InterchangeCommonAnimationPayload.h"

#include "CoreMinimal.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InterchangeCommonAnimationPayload)

#if WITH_ENGINE
#include "Curves/RichCurve.h"
#endif //WITH_ENGINE

namespace UE::Interchange
{
	void FAnimationBakeTransformPayloadData::Serialize(FArchive& Ar)
	{
		Ar << BakeFrequency;
		Ar << RangeStartTime;
		Ar << RangeEndTime;
		Ar << Transforms;
	}
}

#if WITH_ENGINE
void FInterchangeCurveKey::ToRichCurveKey(FRichCurveKey& RichCurveKey) const
{
	RichCurveKey.Time = Time;
	RichCurveKey.Value = Value;
	switch (InterpMode)
	{
		case EInterchangeCurveInterpMode::Constant:
			RichCurveKey.InterpMode = ERichCurveInterpMode::RCIM_Constant;
			break;
		case EInterchangeCurveInterpMode::Cubic:
			RichCurveKey.InterpMode = ERichCurveInterpMode::RCIM_Cubic;
			break;
		case EInterchangeCurveInterpMode::Linear:
			RichCurveKey.InterpMode = ERichCurveInterpMode::RCIM_Linear;
			break;
		default:
			RichCurveKey.InterpMode = ERichCurveInterpMode::RCIM_None;
			break;
	}
	switch (TangentMode)
	{
		case EInterchangeCurveTangentMode::Auto:
			RichCurveKey.TangentMode = ERichCurveTangentMode::RCTM_Auto;
			break;
		case EInterchangeCurveTangentMode::Break:
			RichCurveKey.TangentMode = ERichCurveTangentMode::RCTM_Break;
			break;
		case EInterchangeCurveTangentMode::User:
			RichCurveKey.TangentMode = ERichCurveTangentMode::RCTM_User;
			break;
		default:
			RichCurveKey.TangentMode = ERichCurveTangentMode::RCTM_None;
			break;
	}
	switch (TangentWeightMode)
	{
		case EInterchangeCurveTangentWeightMode::WeightedArrive:
			RichCurveKey.TangentWeightMode = ERichCurveTangentWeightMode::RCTWM_WeightedArrive;
			break;
		case EInterchangeCurveTangentWeightMode::WeightedBoth:
			RichCurveKey.TangentWeightMode = ERichCurveTangentWeightMode::RCTWM_WeightedBoth;
			break;
		case EInterchangeCurveTangentWeightMode::WeightedLeave:
			RichCurveKey.TangentWeightMode = ERichCurveTangentWeightMode::RCTWM_WeightedLeave;
			break;
		default:
			RichCurveKey.TangentWeightMode = ERichCurveTangentWeightMode::RCTWM_WeightedNone;
			break;
	}
	RichCurveKey.ArriveTangent = ArriveTangent;
	RichCurveKey.ArriveTangentWeight = ArriveTangentWeight;
	RichCurveKey.LeaveTangent = LeaveTangent;
	RichCurveKey.LeaveTangentWeight = LeaveTangentWeight;

}
#endif //WITH_ENGINE

void FInterchangeCurveKey::Serialize(FArchive& Ar)
{
	Ar << InterpMode;
	Ar << TangentMode;
	Ar << TangentWeightMode;
	Ar << Time;
	Ar << Value;
	Ar << ArriveTangent;
	Ar << ArriveTangentWeight;
	Ar << LeaveTangent;
	Ar << LeaveTangentWeight;
}

#if WITH_ENGINE
void FInterchangeCurve::ToRichCurve(FRichCurve& OutRichCurve) const
{
	OutRichCurve.Keys.Reserve(Keys.Num());
	for (const FInterchangeCurveKey& CurveKey : Keys)
	{
		FKeyHandle RichCurveKeyHandle = OutRichCurve.AddKey(CurveKey.Time, CurveKey.Value);
		CurveKey.ToRichCurveKey(OutRichCurve.GetKey(RichCurveKeyHandle));
	}
	OutRichCurve.AutoSetTangents();
}
#endif //WITH_ENGINE

void FInterchangeCurve::Serialize(FArchive& Ar)
{
	Ar << Keys;
}

void FInterchangeStepCurve::RemoveRedundantKeys(float Threshold)
{
	const int32 KeyCount = KeyTimes.Num();
	if (KeyCount < 2)
	{
		//Nothing to optimize
		return;
	}

	if (FloatKeyValues.IsSet())
	{
		InternalRemoveRedundantKey<float>(FloatKeyValues.GetValue(), [Threshold](const float& ValueA, const float& ValueB)
			{
				return FMath::IsNearlyEqual(ValueA, ValueB, Threshold);
			});
	}
	else if (IntegerKeyValues.IsSet())
	{
		InternalRemoveRedundantKey<int32>(IntegerKeyValues.GetValue(), [](const int32& ValueA, const int32& ValueB)
			{
				return ValueA == ValueB;
			});
	}
	else if (StringKeyValues.IsSet())
	{
		InternalRemoveRedundantKey<FString>(StringKeyValues.GetValue(), [](const FString& ValueA, const FString& ValueB)
			{
				return ValueA == ValueB;
			});
	}
}

void FInterchangeStepCurve::Serialize(FArchive& Ar)
{
	Ar << KeyTimes;
	Ar << FloatKeyValues;
	Ar << IntegerKeyValues;
	Ar << StringKeyValues;
}

template<typename ValueType>
void FInterchangeStepCurve::InternalRemoveRedundantKey(TArray<ValueType>& Values, TFunctionRef<bool(const ValueType& ValueA, const ValueType& ValueB)> CompareFunction)
{
	const int32 KeyCount = Values.Num();
	TArray<float> NewKeyTimes;
	NewKeyTimes.Reserve(KeyCount);
	TArray<ValueType> NewValues;
	NewValues.Reserve(KeyCount);
	ValueType LastValue = Values[0];
	for (int32 KeyIndex = 1; KeyIndex < KeyCount; ++KeyIndex)
	{
		//Only add the not equal keys
		if (!CompareFunction(LastValue, Values[KeyIndex]))
		{
			LastValue = Values[KeyIndex];
			NewKeyTimes.Add(KeyTimes[KeyIndex]);
			NewValues.Add(Values[KeyIndex]);
		}
	}
	NewKeyTimes.Shrink();
	NewValues.Shrink();
	KeyTimes = MoveTemp(NewKeyTimes);
	Values = MoveTemp(NewValues);
}

template void FInterchangeStepCurve::InternalRemoveRedundantKey<float>(TArray<float>& Values, TFunctionRef<bool(const float& ValueA, const float& ValueB)> CompareFunction);
template void FInterchangeStepCurve::InternalRemoveRedundantKey<int32>(TArray<int32>& Values, TFunctionRef<bool(const int32& ValueA, const int32& ValueB)> CompareFunction);
template void FInterchangeStepCurve::InternalRemoveRedundantKey<FString>(TArray<FString>& Values, TFunctionRef<bool(const FString& ValueA, const FString& ValueB)> CompareFunction);
