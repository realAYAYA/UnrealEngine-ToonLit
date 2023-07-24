// Copyright Epic Games, Inc. All Rights Reserved.


#include "Tables/STMapTable.h"

#include "LensFile.h"
#include "LensTableUtils.h"

int32 FSTMapFocusPoint::GetNumPoints() const
{
	return MapBlendingCurve.GetNumKeys();
}

float FSTMapFocusPoint::GetZoom(int32 Index) const
{
	return MapBlendingCurve.Keys[Index].Time;
}

const FSTMapZoomPoint* FSTMapFocusPoint::GetZoomPoint(float InZoom) const
{
	return ZoomPoints.FindByPredicate([InZoom](const FSTMapZoomPoint& Point) { return FMath::IsNearlyEqual(Point.Zoom, InZoom); });
}

FSTMapZoomPoint* FSTMapFocusPoint::GetZoomPoint(float InZoom)
{
	return ZoomPoints.FindByPredicate([InZoom](const FSTMapZoomPoint& Point) { return FMath::IsNearlyEqual(Point.Zoom, InZoom); });
}

bool FSTMapFocusPoint::GetPoint(float InZoom, FSTMapInfo& OutData, float /*InputTolerance*/) const
{
	if (const FSTMapZoomPoint* STMapZoomPoint = GetZoomPoint(InZoom))
	{
		OutData = STMapZoomPoint->STMapInfo;
		return true;
	}

	return false;
}

bool FSTMapFocusPoint::AddPoint(float InZoom, const FSTMapInfo& InData, float InputTolerance, bool bIsCalibrationPoint)
{
	const FKeyHandle Handle = MapBlendingCurve.FindKey(InZoom, InputTolerance);
	if(Handle != FKeyHandle::Invalid())
	{
		const int32 PointIndex = MapBlendingCurve.GetIndexSafe(Handle);
		if(ensure(ZoomPoints.IsValidIndex(PointIndex)))
		{
			//No need to update map curve since x == y
			ZoomPoints[PointIndex].STMapInfo = InData;
			ZoomPoints[PointIndex].DerivedDistortionData.bIsDirty = true;
		}
		else
		{
			return false;
		}
	}
	else
	{
		//Add new zoom point
		const FKeyHandle NewKeyHandle = MapBlendingCurve.AddKey(InZoom, InZoom);
		MapBlendingCurve.SetKeyTangentMode(NewKeyHandle, ERichCurveTangentMode::RCTM_Auto);
		MapBlendingCurve.SetKeyInterpMode(NewKeyHandle, RCIM_Cubic);

		const int32 KeyIndex = MapBlendingCurve.GetIndexSafe(NewKeyHandle);
		FSTMapZoomPoint NewZoomPoint;
		NewZoomPoint.Zoom = InZoom;
		NewZoomPoint.STMapInfo = InData;
		NewZoomPoint.bIsCalibrationPoint = bIsCalibrationPoint;
		ZoomPoints.Insert(MoveTemp(NewZoomPoint), KeyIndex);
	}

	return true;
}

bool FSTMapFocusPoint::SetPoint(float InZoom, const FSTMapInfo& InData, float InputTolerance)
{
	const FKeyHandle Handle = MapBlendingCurve.FindKey(InZoom, InputTolerance);
	if(Handle != FKeyHandle::Invalid())
	{
		const int32 PointIndex = MapBlendingCurve.GetIndexSafe(Handle);
		if(ensure(ZoomPoints.IsValidIndex(PointIndex)))
		{
			//No need to update map curve since x == y
			ZoomPoints[PointIndex].STMapInfo = InData;
			ZoomPoints[PointIndex].DerivedDistortionData.bIsDirty = true;
			return true;
		}
	}

	return false;
}

void FSTMapFocusPoint::RemovePoint(float InZoomValue)
{
	const int32 FoundIndex = ZoomPoints.IndexOfByPredicate([InZoomValue](const FSTMapZoomPoint& Point) { return FMath::IsNearlyEqual(Point.Zoom, InZoomValue); });
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

bool FSTMapFocusPoint::IsEmpty() const
{
	return MapBlendingCurve.IsEmpty();
}

void FSTMapTable::ForEachPoint(FFocusPointCallback InCallback) const
{
	for (const FSTMapFocusPoint& Point : FocusPoints)
	{
		InCallback(Point);
	}
}

bool FSTMapTable::DoesFocusPointExists(float InFocus) const
{
	if (GetFocusPoint(InFocus) != nullptr)
	{
		return true;
	}

	return false;
}

bool FSTMapTable::DoesZoomPointExists(float InFocus, float InZoom, float InputTolerance) const
{
	FSTMapInfo STMapInfo;
	if (GetPoint(InFocus, InZoom, STMapInfo, InputTolerance))
	{
		return true;
	}

	return false;
}

const FBaseFocusPoint* FSTMapTable::GetBaseFocusPoint(int32 InIndex) const
{
	if (FocusPoints.IsValidIndex(InIndex))
	{
		return &FocusPoints[InIndex];
	}

	return nullptr;
}

int32 FSTMapTable::GetTotalPointNum() const
{
	return LensDataTableUtils::GetTotalPointNum(FocusPoints);
}

UScriptStruct* FSTMapTable::GetScriptStruct() const
{
	return StaticStruct();
}

bool FSTMapTable::BuildMapBlendingCurve(float InFocus, FRichCurve& OutCurve)
{
	if(FSTMapFocusPoint* FocusPoint = GetFocusPoint(InFocus))
	{
		OutCurve = FocusPoint->MapBlendingCurve;
		return true;
	}

	return false;
}

const FSTMapFocusPoint* FSTMapTable::GetFocusPoint(float InFocus) const
{
	return FocusPoints.FindByPredicate([InFocus](const FSTMapFocusPoint& Point) { return FMath::IsNearlyEqual(Point.Focus, InFocus); });
}

FSTMapFocusPoint* FSTMapTable::GetFocusPoint(float InFocus)
{
	return FocusPoints.FindByPredicate([InFocus](const FSTMapFocusPoint& Point) { return FMath::IsNearlyEqual(Point.Focus, InFocus); });
}

TConstArrayView<FSTMapFocusPoint> FSTMapTable::GetFocusPoints() const
{
	return FocusPoints;
}

TArrayView<FSTMapFocusPoint> FSTMapTable::GetFocusPoints()
{
	return FocusPoints;
}

void FSTMapTable::RemoveFocusPoint(float InFocus)
{
	LensDataTableUtils::RemoveFocusPoint(FocusPoints, InFocus);
}

void FSTMapTable::RemoveZoomPoint(float InFocus, float InZoom)
{
	LensDataTableUtils::RemoveZoomPoint(FocusPoints, InFocus, InZoom);
}

TMap<ELensDataCategory, FLinkPointMetadata> FSTMapTable::GetLinkedCategories() const
{
	static TMap<ELensDataCategory, FLinkPointMetadata> LinkedToCategories =
	{
		{ELensDataCategory::Zoom, {true}},
		{ELensDataCategory::ImageCenter, {true}},
		{ELensDataCategory::NodalOffset, {false}},
	};
	return LinkedToCategories;
}

bool FSTMapTable::AddPoint(float InFocus, float InZoom, const FSTMapInfo& InData, float InputTolerance,
                           bool bIsCalibrationPoint)
{
	return LensDataTableUtils::AddPoint(FocusPoints, InFocus, InZoom, InData, InputTolerance, bIsCalibrationPoint);
}

bool FSTMapTable::GetPoint(const float InFocus, const float InZoom, FSTMapInfo& OutData, float InputTolerance) const
{
	if (const FSTMapFocusPoint* STMapFocusPoint = GetFocusPoint(InFocus))
	{
		FSTMapInfo SMapInfo;
		if (STMapFocusPoint->GetPoint(InZoom, SMapInfo, InputTolerance))
		{
			// Copy struct to outer
			OutData = SMapInfo;
			return true;
		}
	}
	
	return false;
}

bool FSTMapTable::SetPoint(float InFocus, float InZoom, const FSTMapInfo& InData, float InputTolerance)
{
	return LensDataTableUtils::SetPoint(*this, InFocus, InZoom, InData, InputTolerance);
}

