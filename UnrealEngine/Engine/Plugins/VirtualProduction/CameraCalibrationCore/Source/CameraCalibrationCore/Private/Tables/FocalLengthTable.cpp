// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tables/FocalLengthTable.h"

#include "LensFile.h"
#include "LensTableUtils.h"

int32 FFocalLengthFocusPoint::GetNumPoints() const
{
	return Fx.GetNumKeys();
}

float FFocalLengthFocusPoint::GetZoom(int32 Index) const
{
	return Fx.Keys[Index].Time;
}

bool FFocalLengthFocusPoint::GetPoint(float InZoom, FFocalLengthZoomPoint& OutZoomPont, float InputTolerance) const
{
	const FKeyHandle FxHandle = Fx.FindKey(InZoom, InputTolerance);
	if(FxHandle != FKeyHandle::Invalid())
	{
		const FKeyHandle FyHandle = Fy.FindKey(InZoom, InputTolerance);
		const int32 PointIndex = Fx.GetIndexSafe(FxHandle);
		check(FyHandle != FKeyHandle::Invalid() && ZoomPoints.IsValidIndex(PointIndex))

		OutZoomPont.FocalLengthInfo.FxFy.X = Fx.GetKeyValue(FxHandle);
		OutZoomPont.FocalLengthInfo.FxFy.Y = Fy.GetKeyValue(FyHandle);
		OutZoomPont.bIsCalibrationPoint = ZoomPoints[PointIndex].bIsCalibrationPoint;

		return true;
	}

	return false;
}

bool FFocalLengthFocusPoint::AddPoint(float InZoom, const FFocalLengthInfo& InData, float InputTolerance, bool bIsCalibrationPoint)
{
	if (SetPoint(InZoom, InData, InputTolerance))
	{
		return true;
	}
	
	//Add new zoom point
	const FKeyHandle NewKeyHandle = Fx.AddKey(InZoom, InData.FxFy.X);
	Fx.SetKeyTangentMode(NewKeyHandle, ERichCurveTangentMode::RCTM_Auto);
	Fx.SetKeyInterpMode(NewKeyHandle, RCIM_Cubic);

	Fy.AddKey(InZoom, InData.FxFy.Y, false, NewKeyHandle);
	Fy.SetKeyTangentMode(NewKeyHandle, ERichCurveTangentMode::RCTM_Auto);
	Fy.SetKeyInterpMode(NewKeyHandle, RCIM_Cubic);
	
	const int32 KeyIndex = Fx.GetIndexSafe(NewKeyHandle);
	FFocalLengthZoomPoint NewFocalLengthPoint;
	NewFocalLengthPoint.Zoom = InZoom;
	NewFocalLengthPoint.bIsCalibrationPoint = bIsCalibrationPoint;
	NewFocalLengthPoint.FocalLengthInfo.FxFy = InData.FxFy;
	ZoomPoints.Insert(MoveTemp(NewFocalLengthPoint), KeyIndex);

	// The function return true all the time
	return true;
}

bool FFocalLengthFocusPoint::SetPoint(float InZoom, const FFocalLengthInfo& InData, float InputTolerance)
{
	const FKeyHandle FxHandle = Fx.FindKey(InZoom, InputTolerance);
	if(FxHandle != FKeyHandle::Invalid())
	{
		const FKeyHandle FyHandle = Fy.FindKey(InZoom, InputTolerance);
		const int32 PointIndex = Fx.GetIndexSafe(FxHandle);
		check(FyHandle != FKeyHandle::Invalid() && ZoomPoints.IsValidIndex(PointIndex))
	
		Fx.SetKeyValue(FxHandle, InData.FxFy.X);
		Fy.SetKeyValue(FyHandle, InData.FxFy.Y);
		ZoomPoints[PointIndex].FocalLengthInfo = InData;

		return true;
	}

	return false;
}

bool FFocalLengthFocusPoint::GetValue(int32 Index, FFocalLengthInfo& OutData) const
{
	if(ZoomPoints.IsValidIndex(Index))
	{
		OutData = ZoomPoints[Index].FocalLengthInfo;

		return true;
	}

	return false;
}

void FFocalLengthFocusPoint::RemovePoint(float InZoomValue)
{
	const int32 FoundIndex = ZoomPoints.IndexOfByPredicate([InZoomValue](const FFocalLengthZoomPoint& Point) { return FMath::IsNearlyEqual(Point.Zoom, InZoomValue); });
	if(FoundIndex != INDEX_NONE)
	{
		ZoomPoints.RemoveAt(FoundIndex);
	}

	const FKeyHandle FxHandle = Fx.FindKey(InZoomValue);
	if(FxHandle != FKeyHandle::Invalid())
	{
		Fx.DeleteKey(FxHandle);
	}

	const FKeyHandle FyHandle = Fy.FindKey(InZoomValue);
	if (FyHandle != FKeyHandle::Invalid())
	{
		Fy.DeleteKey(FyHandle);
	}
}

bool FFocalLengthFocusPoint::IsEmpty() const
{
	return Fx.IsEmpty();
}

bool FFocalLengthTable::DoesZoomPointExists(float InFocus, float InZoom, float InputTolerance) const
{
	FFocalLengthInfo FocalLengthInfo;
	if (GetPoint(InFocus, InZoom, FocalLengthInfo, InputTolerance))
	{
		return true;
	}

	return false;
}

const FBaseFocusPoint* FFocalLengthTable::GetBaseFocusPoint(int32 InIndex) const
{
	if (FocusPoints.IsValidIndex(InIndex))
	{
		return &FocusPoints[InIndex];
	}

	return nullptr;
}


TMap<ELensDataCategory, FLinkPointMetadata> FFocalLengthTable::GetLinkedCategories() const
{
	static TMap<ELensDataCategory, FLinkPointMetadata> LinkedToCategories =
	{
		{ELensDataCategory::Distortion, {true}},
		{ELensDataCategory::ImageCenter, {true}},
		{ELensDataCategory::STMap, {true}},
		{ELensDataCategory::NodalOffset, {false}},
	};
	return LinkedToCategories;
}

int32 FFocalLengthTable::GetTotalPointNum() const
{
	return LensDataTableUtils::GetTotalPointNum(FocusPoints);
}

UScriptStruct* FFocalLengthTable::GetScriptStruct() const
{
	return StaticStruct();
}

bool FFocalLengthTable::BuildParameterCurve(float InFocus, int32 ParameterIndex, FRichCurve& OutCurve) const
{
	if(ParameterIndex >= 0 && ParameterIndex < 2)
	{
		if(const FFocalLengthFocusPoint* FocusPoint = GetFocusPoint(InFocus))
		{
			if(ParameterIndex == 0)
			{
				OutCurve = FocusPoint->Fx;
			}
			else
			{
				OutCurve = FocusPoint->Fy;
			}
			return true;
		}	
	}
	
	return false;
}

const FFocalLengthFocusPoint* FFocalLengthTable::GetFocusPoint(float InFocus) const
{
	return FocusPoints.FindByPredicate([InFocus](const FFocalLengthFocusPoint& Point) { return FMath::IsNearlyEqual(Point.Focus, InFocus); });
}

FFocalLengthFocusPoint* FFocalLengthTable::GetFocusPoint(float InFocus)
{
	return FocusPoints.FindByPredicate([InFocus](const FFocalLengthFocusPoint& Point) { return FMath::IsNearlyEqual(Point.Focus, InFocus); });
}

TConstArrayView<FFocalLengthFocusPoint> FFocalLengthTable::GetFocusPoints() const
{
	return FocusPoints;
}

void FFocalLengthTable::ForEachPoint(FFocusPointCallback InCallback) const
{
	for (const FFocalLengthFocusPoint& Point : FocusPoints)
	{
		InCallback(Point);
	}
}

void FFocalLengthTable::RemoveFocusPoint(float InFocus)
{
	LensDataTableUtils::RemoveFocusPoint(FocusPoints, InFocus);
}

void FFocalLengthTable::RemoveZoomPoint(float InFocus, float InZoom)
{
	LensDataTableUtils::RemoveZoomPoint(FocusPoints, InFocus, InZoom);
}

bool FFocalLengthTable::DoesFocusPointExists(float InFocus) const
{
	if (GetFocusPoint(InFocus) != nullptr)
	{
		return true;
	}

	return false;
}

bool FFocalLengthTable::AddPoint(float InFocus, float InZoom, const FFocalLengthInfo& InData, float InputTolerance, bool bIsCalibrationPoint)
{
	return LensDataTableUtils::AddPoint(FocusPoints, InFocus, InZoom, InData, InputTolerance, bIsCalibrationPoint);
}

bool FFocalLengthTable::GetPoint(const float InFocus, const float InZoom, FFocalLengthInfo& OutData, float InputTolerance) const
{
	if (const FFocalLengthFocusPoint* FocalLengthFocusPoint = GetFocusPoint(InFocus))
	{
		FFocalLengthZoomPoint ZoomPont;
		if (FocalLengthFocusPoint->GetPoint(InZoom, ZoomPont, InputTolerance))
		{
			// Copy struct to outer
			OutData = ZoomPont.FocalLengthInfo;
			return true;
		}
	}
	
	return false;
}

bool FFocalLengthTable::SetPoint(float InFocus, float InZoom, const FFocalLengthInfo& InData, float InputTolerance)
{
	return LensDataTableUtils::SetPoint(*this, InFocus, InZoom, InData, InputTolerance);
}
