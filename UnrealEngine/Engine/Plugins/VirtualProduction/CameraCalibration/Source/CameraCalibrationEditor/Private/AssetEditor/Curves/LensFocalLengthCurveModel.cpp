// Copyright Epic Games, Inc. All Rights Reserved.

#include "LensFocalLengthCurveModel.h"

#include "CameraCalibrationEditorLog.h"
#include "LensFile.h"

#define LOCTEXT_NAMESPACE "LensFocalLengthCurveModel"


FLensFocalLengthCurveModel::FLensFocalLengthCurveModel(ULensFile* InOwner, float InFocus, int32 InParameterIndex)
	: FLensDataCurveModel(InOwner)
	, Focus(InFocus)
	, ParameterIndex(InParameterIndex)
{
	if(ParameterIndex == INDEX_NONE)
	{
		if (FFocalLengthFocusPoint* Point = LensFile->FocalLengthTable.GetFocusPoint(Focus))
		{
			FRichCurve* ActiveCurve = &Point->Fx;
			auto Iterator = ActiveCurve->GetKeyHandleIterator();
			for (const FRichCurveKey& Key : ActiveCurve->GetConstRefOfKeys())
			{
				const float Scale = LensFile->LensInfo.SensorDimensions.X;
				const FKeyHandle NewHandle = CurrentCurve.AddKey(Key.Time, Key.Value * Scale, false, *Iterator);

				FRichCurveKey& NewKey = CurrentCurve.GetKey(*Iterator);
				NewKey.TangentMode = Key.TangentMode;
				NewKey.InterpMode = Key.InterpMode;
				NewKey.ArriveTangent = Key.ArriveTangent * Scale;
				NewKey.LeaveTangent = Key.LeaveTangent * Scale;
				++Iterator;
			}
		}

		bIsCurveValid = true;
	}
	else if(ParameterIndex >= 0 && ParameterIndex < 2)
	{
		if (FFocalLengthFocusPoint* Point = LensFile->FocalLengthTable.GetFocusPoint(Focus))
		{
			bIsCurveValid = LensFile->FocalLengthTable.BuildParameterCurve(Focus, ParameterIndex, CurrentCurve);
		}
	}
	
}

void FLensFocalLengthCurveModel::SetKeyPositions(TArrayView<const FKeyHandle> InKeys, TArrayView<const FKeyPosition> InKeyPositions, EPropertyChangeType::Type ChangeType)
{
	//Reject anything below 1mm
	const double MinimumFocalLength = (ParameterIndex == INDEX_NONE) ? 1.0 : (1.0 / LensFile->LensInfo.SensorDimensions[ParameterIndex]);

	TArray<FKeyHandle> FilteredHandles;
	TArray<FKeyPosition> FilteredPosition;
	FilteredHandles.Reserve(InKeys.Num());
	FilteredPosition.Reserve(InKeys.Num());
	for(int32 Index = 0; Index < InKeys.Num(); ++Index)
	{
		if(InKeyPositions[Index].OutputValue >= MinimumFocalLength)
		{
			FilteredHandles.Add(InKeys[Index]);
			FilteredPosition.Add(InKeyPositions[Index]);
		}
	}
	
	FRichCurveEditorModel::SetKeyPositions(FilteredHandles, FilteredPosition, ChangeType);

	if (FFocalLengthFocusPoint* Point = LensFile->FocalLengthTable.GetFocusPoint(Focus))
	{
		FRichCurve* ActiveCurve = nullptr;
		float Scale = 1.0f;
		int32 FxFyIndex = ParameterIndex;
		if(ParameterIndex == INDEX_NONE)
		{
			ActiveCurve = &Point->Fx;
			Scale = 1.0f / LensFile->LensInfo.SensorDimensions.X;
			FxFyIndex = 0; //mm focal length curve changes Fx
		}
		else
		{
			ActiveCurve = GetRichCurveForParameterIndex(*Point);
		}
		
		for (int32 Index = 0; Index < InKeys.Num(); ++Index)
		{
			const FKeyHandle Handle = InKeys[Index];
			const int32 KeyIndex = CurrentCurve.GetIndexSafe(Handle);
			if (KeyIndex != INDEX_NONE)
			{
				if(ensure(ActiveCurve->Keys.IsValidIndex(KeyIndex)
					&& Point->ZoomPoints.IsValidIndex(KeyIndex)))
				{
					const float NewNormalizedFocalLength = CurrentCurve.GetKeyValue(Handle) * Scale;
					ActiveCurve->Keys[KeyIndex].Value = NewNormalizedFocalLength;
					Point->ZoomPoints[KeyIndex].FocalLengthInfo.FxFy[FxFyIndex] = NewNormalizedFocalLength;
				}
			}
		}	

		ActiveCurve->AutoSetTangents();
	}
}

void FLensFocalLengthCurveModel::SetKeyAttributes(TArrayView<const FKeyHandle> InKeys, TArrayView<const FKeyAttributes> InAttributes, EPropertyChangeType::Type ChangeType)
{
	//Applies attributes to copied curve
	FRichCurveEditorModel::SetKeyAttributes(InKeys, InAttributes, ChangeType);

	if (FFocalLengthFocusPoint* Point = LensFile->FocalLengthTable.GetFocusPoint(Focus))
	{
		FRichCurve* ActiveCurve = nullptr;
		float Scale = 1.0f;
		int32 FxFyIndex = ParameterIndex;
		if(ParameterIndex == INDEX_NONE)
		{
			ActiveCurve = &Point->Fx;
			Scale = 1.0f / LensFile->LensInfo.SensorDimensions.X;
			FxFyIndex = 0; //mm focal length curve changes Fx
		}
		else
		{
			ActiveCurve = GetRichCurveForParameterIndex(*Point);
		}
		
		for (int32 Index = 0; Index < InKeys.Num(); ++Index)
		{
			const FKeyHandle Handle = InKeys[Index];
			const int32 KeyIndex = CurrentCurve.GetIndexSafe(Handle);
			if (KeyIndex != INDEX_NONE)
			{
				if(ensure(ActiveCurve->Keys.IsValidIndex(KeyIndex)))
				{
					const FRichCurveKey& Key = CurrentCurve.GetKey(Handle);
					FRichCurveKey& DestinationKey = ActiveCurve->Keys[KeyIndex];
					DestinationKey.InterpMode = Key.InterpMode;
					DestinationKey.ArriveTangent = Key.ArriveTangent * Scale;
					DestinationKey.LeaveTangent = Key.LeaveTangent * Scale;
					DestinationKey.TangentMode = Key.TangentMode;
				}
			}
		}	
	}

}

FRichCurve* FLensFocalLengthCurveModel::GetRichCurveForParameterIndex(FFocalLengthFocusPoint& InFocusPoint) const
{
	if(ParameterIndex == 0)
	{
		return &InFocusPoint.Fx;
	}
	else
	{
		return &InFocusPoint.Fy;
	}
}

FText FLensFocalLengthCurveModel::GetValueLabel() const
{
	if (ParameterIndex == INDEX_NONE)
	{
		return LOCTEXT("ZoomValueLabelMM", "(mm)");
	}
	return LOCTEXT("ZoomValueLabelNormalized", "(normalized)");
}

FText FLensFocalLengthCurveModel::GetValueUnitSuffixLabel() const
{
	if (ParameterIndex == INDEX_NONE)
	{
		return LOCTEXT("UnitSuffixLabelZoom", "mm");
	}
	return FText();
}

#undef LOCTEXT_NAMESPACE
