// Copyright Epic Games, Inc. All Rights Reserved.

#include "LensDistortionParametersCurveModel.h"

#include "CameraCalibrationEditorLog.h"
#include "LensFile.h"

#define LOCTEXT_NAMESPACE "LensDistortionParametersCurveModel"


FLensDistortionParametersCurveModel::FLensDistortionParametersCurveModel(ULensFile* InOwner, float InFocus, int32 InParameterIndex)
	: FLensDataCurveModel(InOwner)
	, Focus(InFocus)
	, ParameterIndex(InParameterIndex)
{
	bIsCurveValid = LensFile->DistortionTable.BuildParameterCurve(Focus, ParameterIndex, CurrentCurve);
	if(bIsCurveValid == false)
	{
		UE_LOG(LogCameraCalibrationEditor, Warning, TEXT("Could not build distortion parameter curve for index '%d' in '%s'"), ParameterIndex, *LensFile->GetName());
	}
}

void FLensDistortionParametersCurveModel::SetKeyPositions(TArrayView<const FKeyHandle> InKeys, TArrayView<const FKeyPosition> InKeyPositions, EPropertyChangeType::Type ChangeType)
{
	//Discard any key modifications when dealing with the map blending curve
	if (ParameterIndex == INDEX_NONE)
	{
		return;
	}

	FRichCurveEditorModel::SetKeyPositions(InKeys, InKeyPositions, ChangeType);

	if (FDistortionFocusPoint* Points = LensFile->DistortionTable.GetFocusPoint(Focus))
	{
		for (int32 Index = 0; Index < InKeys.Num(); ++Index)
		{
			const FKeyHandle Handle = InKeys[Index];
			const int32 KeyIndex = CurrentCurve.GetIndexSafe(Handle);
			if (KeyIndex != INDEX_NONE)
			{
				//We can't move keys on the time axis so our indices should match
				const FRichCurveKey& Key = CurrentCurve.GetKey(Handle);
				Points->SetParameterValue(KeyIndex, Key.Time, ParameterIndex, Key.Value);
			}
			else
			{
				UE_LOG(LogCameraCalibrationEditor, Warning, TEXT("Could not find distortion parameter curve key for focus '%0.2f' and zoom '%0.2f' in '%s'"), Focus, InKeyPositions[Index].InputValue, *LensFile->GetName());
			}
		}
	}
}

void FLensDistortionParametersCurveModel::SetKeyAttributes(TArrayView<const FKeyHandle> InKeys, TArrayView<const FKeyAttributes> InAttributes, EPropertyChangeType::Type ChangeType)
{
	//Discard any key attributes change for parameters curve
	if(ParameterIndex != INDEX_NONE)
	{
		return;
	}

	//Applies attributes to copied curve
   	FRichCurveEditorModel::SetKeyAttributes(InKeys, InAttributes, ChangeType);

	if (FDistortionFocusPoint* Point = LensFile->DistortionTable.GetFocusPoint(Focus))
	{
		for (int32 Index = 0; Index < InKeys.Num(); ++Index)
		{
			const FKeyHandle Handle = InKeys[Index];
			const int32 KeyIndex = CurrentCurve.GetIndexSafe(Handle);
			if (KeyIndex != INDEX_NONE)
			{
				//We can't move keys on the time axis so our indices should match
				const FRichCurveKey& Key = CurrentCurve.GetKey(Handle);
				Point->MapBlendingCurve.Keys[KeyIndex] = Key;
			}
		}

		Point->MapBlendingCurve.AutoSetTangents();
	}
}

FText FLensDistortionParametersCurveModel::GetValueLabel() const
{
	if (ParameterIndex != INDEX_NONE)
	{
		return LOCTEXT("ParameterValueLabel", "(unitless)");
	}
	return FText();
}

#undef LOCTEXT_NAMESPACE

