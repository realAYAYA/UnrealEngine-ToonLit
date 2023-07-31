// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Curves/RichCurve.h"
#include "InterchangeCommonAnimationPayload.h"

namespace UE::Interchange
{
	/**
	 * This payload class is use to get a user attribute curve for any translated node.
	 * If the attribute is a multi channel like a position vector we should get 3 curve representing X, Y and Z.
	 */
	struct FAnimationCurvePayloadData
	{
		TArray<FRichCurve> Curves;
	};

	struct FAnimationStepCurvePayloadData
	{
		TArray<FInterchangeStepCurve> StepCurves;

		static FAnimationStepCurvePayloadData FromAnimationCurvePayloadData(const FAnimationCurvePayloadData& CurvePayload)
		{
			FAnimationStepCurvePayloadData AnimationStepCurvePayloadData;
			AnimationStepCurvePayloadData.StepCurves.Reserve(CurvePayload.Curves.Num());
			for (const FRichCurve& RichCurve : CurvePayload.Curves)
			{
				FInterchangeStepCurve& StepCurve = AnimationStepCurvePayloadData.StepCurves.AddDefaulted_GetRef();
				const int32 KeyCount = RichCurve.GetNumKeys();
				StepCurve.KeyTimes.AddZeroed(KeyCount);
				TArray<float> KeyValues;
				KeyValues.AddZeroed(KeyCount);
				int32 KeyIndex = 0;
				for (FKeyHandle KeyHandle = RichCurve.GetFirstKeyHandle(); KeyHandle != FKeyHandle::Invalid(); KeyHandle = RichCurve.GetNextKey(KeyHandle), ++KeyIndex )
				{
					StepCurve.KeyTimes[KeyIndex] = RichCurve.GetKeyTime(KeyHandle);
					KeyValues[KeyIndex] = RichCurve.GetKeyValue(KeyHandle);
				}
				StepCurve.FloatKeyValues = MoveTemp(KeyValues);
			}
			return AnimationStepCurvePayloadData;
		}
	};
}//ns UE::Interchange
