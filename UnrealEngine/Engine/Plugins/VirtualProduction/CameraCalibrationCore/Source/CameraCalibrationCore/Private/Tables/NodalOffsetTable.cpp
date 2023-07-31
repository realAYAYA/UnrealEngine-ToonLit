// Copyright Epic Games, Inc. All Rights Reserved.


#include "Tables/NodalOffsetTable.h"

#include "LensFile.h"
#include "LensTableUtils.h"

int32 FNodalOffsetFocusPoint::GetNumPoints() const
{
	return LocationOffset[0].GetNumKeys();
}

float FNodalOffsetFocusPoint::GetZoom(int32 Index) const
{
	return LocationOffset[0].Keys[Index].Time;
}

bool FNodalOffsetFocusPoint::GetPoint(float InZoom, FNodalPointOffset& OutData, float InputTolerance) const
{	
	for(int32 Index = 0; Index < LocationDimension; ++Index)
	{
		FKeyHandle Handle = LocationOffset[Index].FindKey(InZoom, InputTolerance);
		if(Handle != FKeyHandle::Invalid())
		{
			OutData.LocationOffset[Index] = LocationOffset[Index].GetKeyValue(Handle);
		}
		else
		{
			return false;
		}
	}

	FRotator Rotator;
	for(int32 Index = 0; Index < RotationDimension; ++Index)
	{
		FKeyHandle Handle = RotationOffset[Index].FindKey(InZoom, InputTolerance);
		if(Handle != FKeyHandle::Invalid())
		{
			Rotator.SetComponentForAxis(static_cast<EAxis::Type>(Index+1), RotationOffset[Index].GetKeyValue(Handle));
		}
		else
		{
			return false;
		}
	}

	OutData.RotationOffset = Rotator.Quaternion();

	return true;
}

bool FNodalOffsetFocusPoint::AddPoint(float InZoom, const FNodalPointOffset& InData, float InputTolerance, bool /** bIsCalibrationPoint */)
{
	for(int32 Index = 0; Index < LocationDimension; ++Index)
	{
		FKeyHandle Handle = LocationOffset[Index].FindKey(InZoom, InputTolerance);
		if(Handle != FKeyHandle::Invalid())
		{
			LocationOffset[Index].SetKeyValue(Handle, InData.LocationOffset[Index]);	
		}
		else
		{
			Handle = LocationOffset[Index].AddKey(InZoom, InData.LocationOffset[Index]);
			LocationOffset[Index].SetKeyTangentMode(Handle, ERichCurveTangentMode::RCTM_Auto);
			LocationOffset[Index].SetKeyInterpMode(Handle, RCIM_Cubic);
		}
	}

	const FRotator NewRotator = InData.RotationOffset.Rotator();
	for(int32 Index = 0; Index < RotationDimension; ++Index)
	{
		FKeyHandle Handle = RotationOffset[Index].FindKey(InZoom, InputTolerance);
		if(Handle != FKeyHandle::Invalid())
		{
			RotationOffset[Index].SetKeyValue(Handle, NewRotator.GetComponentForAxis(static_cast<EAxis::Type>(Index+1)));	
		}
		else
		{
			Handle = RotationOffset[Index].AddKey(InZoom, NewRotator.GetComponentForAxis(static_cast<EAxis::Type>(Index+1)));
			RotationOffset[Index].SetKeyTangentMode(Handle, ERichCurveTangentMode::RCTM_Auto);
			RotationOffset[Index].SetKeyInterpMode(Handle, RCIM_Cubic);
		}
	}
	return true;
}

bool FNodalOffsetFocusPoint::SetPoint(float InZoom, const FNodalPointOffset& InData, float InputTolerance)
{
	for(int32 Index = 0; Index < LocationDimension; ++Index)
	{
		FKeyHandle Handle = LocationOffset[Index].FindKey(InZoom, InputTolerance);
		if(Handle != FKeyHandle::Invalid())
		{
			LocationOffset[Index].SetKeyValue(Handle, InData.LocationOffset[Index]);	
		}
		else
		{
			return false;
		}
	}

	const FRotator NewRotator = InData.RotationOffset.Rotator();
	for(int32 Index = 0; Index < RotationDimension; ++Index)
	{
		FKeyHandle Handle = RotationOffset[Index].FindKey(InZoom, InputTolerance);
		if(Handle != FKeyHandle::Invalid())
		{
			RotationOffset[Index].SetKeyValue(Handle, NewRotator.GetComponentForAxis(static_cast<EAxis::Type>(Index+1)));	
		}
		else
		{
			return false;
		}
	}
	return true;
}

void FNodalOffsetFocusPoint::RemovePoint(float InZoomValue)
{
	for(int32 Index = 0; Index < LocationDimension; ++Index)
	{
		const FKeyHandle KeyHandle = LocationOffset[Index].FindKey(InZoomValue);
		if(KeyHandle != FKeyHandle::Invalid())
		{
			LocationOffset[Index].DeleteKey(KeyHandle);
		}
	}

	for(int32 Index = 0; Index < RotationDimension; ++Index)
	{
		const FKeyHandle KeyHandle = RotationOffset[Index].FindKey(InZoomValue);
		if(KeyHandle != FKeyHandle::Invalid())
		{
			RotationOffset[Index].DeleteKey(KeyHandle);
		}
	}
}

bool FNodalOffsetFocusPoint::IsEmpty() const
{
	return LocationOffset[0].IsEmpty();
}

bool FNodalOffsetTable::DoesZoomPointExists(float InFocus, float InZoom, float InputTolerance) const
{
	FNodalPointOffset NodalPointOffset;
	if (GetPoint(InFocus, InZoom, NodalPointOffset, InputTolerance))
	{
		return true;
	}

	return false;
}

const FBaseFocusPoint* FNodalOffsetTable::GetBaseFocusPoint(int32 InIndex) const
{
	if (FocusPoints.IsValidIndex(InIndex))
	{
		return &FocusPoints[InIndex];
	}

	return nullptr;
}

TMap<ELensDataCategory, FLinkPointMetadata> FNodalOffsetTable::GetLinkedCategories() const
{
	static TMap<ELensDataCategory, FLinkPointMetadata> LinkedToCategories =
	{
		{ELensDataCategory::Distortion, {false}},
		{ELensDataCategory::Zoom, {false}},
		{ELensDataCategory::STMap, {false}},
		{ELensDataCategory::ImageCenter, {false}},
	};
	return LinkedToCategories;
}

int32 FNodalOffsetTable::GetTotalPointNum() const
{
	return LensDataTableUtils::GetTotalPointNum(FocusPoints);
}

UScriptStruct* FNodalOffsetTable::GetScriptStruct() const
{
	return StaticStruct();
}

bool FNodalOffsetTable::BuildParameterCurve(float InFocus, int32 ParameterIndex, EAxis::Type InAxis, FRichCurve& OutCurve) const
{
	if((ParameterIndex >= 0) && (ParameterIndex < 2) && (InAxis != EAxis::None))
	{
		if(const FNodalOffsetFocusPoint* FocusPoint = GetFocusPoint(InFocus))
		{
			if(ParameterIndex == 0)
			{
				OutCurve = FocusPoint->LocationOffset[static_cast<uint8>(InAxis) - 1];;
			}
			else
			{
				OutCurve = FocusPoint->RotationOffset[static_cast<uint8>(InAxis) - 1];
			}
			return true;
		}	
	}

	return false;
}

const FNodalOffsetFocusPoint* FNodalOffsetTable::GetFocusPoint(float InFocus) const
{
	return FocusPoints.FindByPredicate([InFocus](const FNodalOffsetFocusPoint& Point) { return FMath::IsNearlyEqual(Point.Focus, InFocus); });
}

FNodalOffsetFocusPoint* FNodalOffsetTable::GetFocusPoint(float InFocus)
{
	return FocusPoints.FindByPredicate([InFocus](const FNodalOffsetFocusPoint& Point) { return FMath::IsNearlyEqual(Point.Focus, InFocus); });
}

TConstArrayView<FNodalOffsetFocusPoint> FNodalOffsetTable::GetFocusPoints() const
{
	return FocusPoints;
}

TArray<FNodalOffsetFocusPoint>& FNodalOffsetTable::GetFocusPoints()
{
	return FocusPoints;
}

void FNodalOffsetTable::ForEachPoint(FFocusPointCallback InCallback) const
{
	for (const FNodalOffsetFocusPoint& Point : FocusPoints)
	{
		InCallback(Point);
	}
}

void FNodalOffsetTable::RemoveFocusPoint(float InFocus)
{
	LensDataTableUtils::RemoveFocusPoint(FocusPoints, InFocus);
}

void FNodalOffsetTable::RemoveZoomPoint(float InFocus, float InZoom)
{
	LensDataTableUtils::RemoveZoomPoint(FocusPoints, InFocus, InZoom);
}

bool FNodalOffsetTable::DoesFocusPointExists(float InFocus) const
{
	if (GetFocusPoint(InFocus) != nullptr)
	{
		return true;
	}

	return false;
}

bool FNodalOffsetTable::AddPoint(float InFocus, float InZoom, const FNodalPointOffset& InData, float InputTolerance, bool bIsCalibrationPoint)
{
	return LensDataTableUtils::AddPoint(FocusPoints, InFocus, InZoom, InData, InputTolerance, bIsCalibrationPoint);
}

bool FNodalOffsetTable::GetPoint(const float InFocus, const float InZoom, FNodalPointOffset& OutData, float InputTolerance) const
{
	if (const FNodalOffsetFocusPoint* NodalOffsetFocusPoint = GetFocusPoint(InFocus))
	{
		FNodalPointOffset NodalPointOffset;
		if (NodalOffsetFocusPoint->GetPoint(InZoom, NodalPointOffset, InputTolerance))
		{
			// Copy struct to outer
			OutData = NodalPointOffset;
			return true;
		}
	}
	
	return false;
}

bool FNodalOffsetTable::SetPoint(float InFocus, float InZoom, const FNodalPointOffset& InData, float InputTolerance)
{
	return LensDataTableUtils::SetPoint(*this, InFocus, InZoom, InData, InputTolerance);
}

