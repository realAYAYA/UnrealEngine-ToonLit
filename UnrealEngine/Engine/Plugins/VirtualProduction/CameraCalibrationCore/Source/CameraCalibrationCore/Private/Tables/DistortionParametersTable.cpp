// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tables/DistortionParametersTable.h"

#include "LensFile.h"
#include "LensTableUtils.h"

int32 FDistortionFocusPoint::GetNumPoints() const
{
	return MapBlendingCurve.GetNumKeys();
}

float FDistortionFocusPoint::GetZoom(int32 Index) const
{
	return MapBlendingCurve.Keys[Index].Time;
}

bool FDistortionFocusPoint::GetPoint(float InZoom, FDistortionInfo& OutData, float InputTolerance) const
{
	const FKeyHandle Handle = MapBlendingCurve.FindKey(InZoom, InputTolerance);
	if(Handle != FKeyHandle::Invalid())
	{
		const int32 PointIndex = MapBlendingCurve.GetIndexSafe(Handle);
		check(ZoomPoints.IsValidIndex(PointIndex));

		OutData = ZoomPoints[PointIndex].DistortionInfo;

		return true;
	}

	return false;
}

bool FDistortionFocusPoint::AddPoint(float InZoom, const FDistortionInfo& InData, float InputTolerance, bool /*bIsCalibrationPoint*/)
{
	if (SetPoint(InZoom, InData, InputTolerance))
	{
		return true;
	}

	//Add new zoom point
	const FKeyHandle NewKeyHandle = MapBlendingCurve.AddKey(InZoom, InZoom);
	MapBlendingCurve.SetKeyTangentMode(NewKeyHandle, ERichCurveTangentMode::RCTM_Auto);
	MapBlendingCurve.SetKeyInterpMode(NewKeyHandle, RCIM_Cubic);

	//Insert point at the same index as the curve key
	const int32 KeyIndex = MapBlendingCurve.GetIndexSafe(NewKeyHandle);
	FDistortionZoomPoint NewZoomPoint;
	NewZoomPoint.Zoom = InZoom;
	NewZoomPoint.DistortionInfo = InData;
	ZoomPoints.Insert(MoveTemp(NewZoomPoint), KeyIndex);

	// The function return true all the time
	return true;
}

bool FDistortionFocusPoint::SetPoint(float InZoom, const FDistortionInfo& InData, float InputTolerance)
{
	const FKeyHandle Handle = MapBlendingCurve.FindKey(InZoom, InputTolerance);
	if(Handle != FKeyHandle::Invalid())
	{
		const int32 PointIndex = MapBlendingCurve.GetIndexSafe(Handle);
		check(ZoomPoints.IsValidIndex(PointIndex));

		//No need to update map curve since x == y
		ZoomPoints[PointIndex].DistortionInfo = InData;

		return true;
	}

	return false;
}

void FDistortionFocusPoint::RemovePoint(float InZoomValue)
{
	const int32 FoundIndex = ZoomPoints.IndexOfByPredicate([InZoomValue](const FDistortionZoomPoint& Point) { return FMath::IsNearlyEqual(Point.Zoom, InZoomValue); });
	if(FoundIndex != INDEX_NONE)
	{
		ZoomPoints.RemoveAt(FoundIndex);
	}

	const FKeyHandle KeyHandle = MapBlendingCurve.FindKey(InZoomValue);
	if(KeyHandle != FKeyHandle::Invalid())
	{
		MapBlendingCurve.DeleteKey(KeyHandle);
	}
}

bool FDistortionFocusPoint::IsEmpty() const
{
	return MapBlendingCurve.IsEmpty();
}

void FDistortionFocusPoint::SetParameterValue(int32 InZoomIndex, float InZoomValue, int32 InParameterIndex, float InParameterValue)
{
	//We can't move keys on the time axis so our indices should match
	if (ZoomPoints.IsValidIndex(InZoomIndex))
	{
		if (ensure(FMath::IsNearlyEqual(ZoomPoints[InZoomIndex].Zoom, InZoomValue)))
		{
			ZoomPoints[InZoomIndex].DistortionInfo.Parameters[InParameterIndex] = InParameterValue;
		}
	}
}

int32 FDistortionTable::GetTotalPointNum() const
{
	return LensDataTableUtils::GetTotalPointNum(FocusPoints);
}

UScriptStruct* FDistortionTable::GetScriptStruct() const
{
	return StaticStruct();
}

bool FDistortionTable::BuildParameterCurve(float InFocus, int32 ParameterIndex, FRichCurve& OutCurve) const
{
	if (const FDistortionFocusPoint* ThisFocusPoints = GetFocusPoint(InFocus))
	{
		//Go over each zoom points
		for (const FDistortionZoomPoint& ZoomPoint : ThisFocusPoints->ZoomPoints)
		{
			if (ZoomPoint.DistortionInfo.Parameters.IsValidIndex(ParameterIndex))
			{
				const FKeyHandle Handle = OutCurve.AddKey(ZoomPoint.Zoom, ZoomPoint.DistortionInfo.Parameters[ParameterIndex]);
				OutCurve.SetKeyInterpMode(Handle, RCIM_Linear);
			}
			else //Defaults to map blending
			{
				OutCurve = ThisFocusPoints->MapBlendingCurve;
				return true;
			}
		}

		return true;
	}

	return false;
}

const FDistortionFocusPoint* FDistortionTable::GetFocusPoint(float InFocus) const
{
	return FocusPoints.FindByPredicate([InFocus](const FDistortionFocusPoint& Point) { return FMath::IsNearlyEqual(Point.Focus, InFocus); });
}

FDistortionFocusPoint* FDistortionTable::GetFocusPoint(float InFocus)
{
	return FocusPoints.FindByPredicate([InFocus](const FDistortionFocusPoint& Point) { return FMath::IsNearlyEqual(Point.Focus, InFocus); });
}

void FDistortionTable::ForEachPoint(FFocusPointCallback InCallback) const
{
	for (const FDistortionFocusPoint& Point : FocusPoints)
	{
		InCallback(Point);
	}
}

TConstArrayView<FDistortionFocusPoint> FDistortionTable::GetFocusPoints() const
{
	return FocusPoints;
}

TArray<FDistortionFocusPoint>& FDistortionTable::GetFocusPoints()
{
	return FocusPoints;
}

void FDistortionTable::RemoveFocusPoint(float InFocus)
{
	LensDataTableUtils::RemoveFocusPoint(FocusPoints, InFocus);
}

void FDistortionTable::RemoveZoomPoint(float InFocus, float InZoom)
{
	LensDataTableUtils::RemoveZoomPoint(FocusPoints, InFocus, InZoom);
}

bool FDistortionTable::DoesFocusPointExists(float InFocus) const
{
	if (GetFocusPoint(InFocus) != nullptr)
	{
		return true;
	}

	return false;
}

bool FDistortionTable::DoesZoomPointExists(float InFocus, float InZoom, float InputTolerance) const
{
	FDistortionInfo DistortionInfo;
	if (GetPoint(InFocus, InZoom, DistortionInfo, InputTolerance))
	{
		return true;
	}

	return false;
}

const FBaseFocusPoint* FDistortionTable::GetBaseFocusPoint(int32 InIndex) const
{
	if (FocusPoints.IsValidIndex(InIndex))
	{
		return &FocusPoints[InIndex];
	}

	return nullptr;
}

TMap<ELensDataCategory, FLinkPointMetadata> FDistortionTable::GetLinkedCategories() const
{
	static TMap<ELensDataCategory, FLinkPointMetadata> LinkedToCategories =
	{
		{ELensDataCategory::Zoom, {true}},
		{ELensDataCategory::ImageCenter, {true}},
		{ELensDataCategory::NodalOffset, {false}},
	};
	return LinkedToCategories;
}

bool FDistortionTable::AddPoint(float InFocus, float InZoom, const FDistortionInfo& InData, float InputTolerance, bool bIsCalibrationPoint)
{
	return LensDataTableUtils::AddPoint(FocusPoints, InFocus, InZoom, InData, InputTolerance, bIsCalibrationPoint);
}

bool FDistortionTable::GetPoint(const float InFocus, const float InZoom, FDistortionInfo& OutData, float InputTolerance) const
{
	if (const FDistortionFocusPoint* DistortionFocusPoint = GetFocusPoint(InFocus))
	{
		FDistortionInfo DistortionInfo;
		if (DistortionFocusPoint->GetPoint(InZoom, DistortionInfo, InputTolerance))
		{
			// Copy struct to outer
			OutData = DistortionInfo;
			return true;
		}
	}
	
	return false;
}

bool FDistortionTable::SetPoint(float InFocus, float InZoom, const FDistortionInfo& InData, float InputTolerance)
{
	return LensDataTableUtils::SetPoint(*this, InFocus, InZoom, InData, InputTolerance);
}



