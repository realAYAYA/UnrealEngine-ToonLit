// Copyright Epic Games, Inc. All Rights Reserved.

#include "LensImageCenterCurveModel.h"

#include "CameraCalibrationEditorLog.h"
#include "LensFile.h"

#define LOCTEXT_NAMESPACE "LensImageCenterCurveModel"


FLensImageCenterCurveModel::FLensImageCenterCurveModel(ULensFile* InOwner, float InFocus, int32 InParameterIndex)
	: FLensDataCurveModel(InOwner)
	, Focus(InFocus)
	, ParameterIndex(InParameterIndex)
{
	//No curve to show when selecting root image center. Maybe show both?
	bIsCurveValid = LensFile->ImageCenterTable.BuildParameterCurve(Focus, ParameterIndex, CurrentCurve);
}

void FLensImageCenterCurveModel::SetKeyPositions(TArrayView<const FKeyHandle> InKeys, TArrayView<const FKeyPosition> InKeyPositions, EPropertyChangeType::Type ChangeType)
{
	FRichCurveEditorModel::SetKeyPositions(InKeys, InKeyPositions, ChangeType);

	if (FImageCenterFocusPoint* Point = LensFile->ImageCenterTable.GetFocusPoint(Focus))
	{
		FRichCurve* ActiveCurve = nullptr;
		if(ParameterIndex == 0)
		{
			ActiveCurve = &Point->Cx;
		}
		else
		{
			ActiveCurve = &Point->Cy;
		}
		
		for (int32 Index = 0; Index < InKeys.Num(); ++Index)
		{
			const FKeyHandle Handle = InKeys[Index];
			const int32 KeyIndex = CurrentCurve.GetIndexSafe(Handle);
			if (KeyIndex != INDEX_NONE)
			{
				ActiveCurve->Keys[KeyIndex] = CurrentCurve.GetKey(Handle);
			}
		}

		ActiveCurve->AutoSetTangents();
	}
}

void FLensImageCenterCurveModel::SetKeyAttributes(TArrayView<const FKeyHandle> InKeys, TArrayView<const FKeyAttributes> InAttributes, EPropertyChangeType::Type ChangeType)
{
	//Applies attributes to copied curve
	FRichCurveEditorModel::SetKeyAttributes(InKeys, InAttributes, ChangeType);

	if (FImageCenterFocusPoint* Point = LensFile->ImageCenterTable.GetFocusPoint(Focus))
	{
		FRichCurve* ActiveCurve = nullptr;
		if(ParameterIndex == 0)
		{
			ActiveCurve = &Point->Cx;
		}
		else
		{
			ActiveCurve = &Point->Cy;
		}
		
		for (int32 Index = 0; Index < InKeys.Num(); ++Index)
		{
			const FKeyHandle Handle = InKeys[Index];
			const int32 KeyIndex = CurrentCurve.GetIndexSafe(Handle);
			
			//We can't move keys on the time axis so our indices should match
			//Copy over the tangent attributes
			if(ensure(ActiveCurve->Keys.IsValidIndex(KeyIndex)))
			{
				const FRichCurveKey& Key = CurrentCurve.GetKey(Handle);
				ActiveCurve->Keys[KeyIndex].InterpMode = Key.InterpMode;
				ActiveCurve->Keys[KeyIndex].ArriveTangent = Key.ArriveTangent;
				ActiveCurve->Keys[KeyIndex].LeaveTangent = Key.LeaveTangent;
				ActiveCurve->Keys[KeyIndex].TangentMode = Key.TangentMode;
			}
		}
	}
}

FText FLensImageCenterCurveModel::GetValueLabel() const
{
	return LOCTEXT("ImageCenterValueLabel", "(normalized)");
}

#undef LOCTEXT_NAMESPACE