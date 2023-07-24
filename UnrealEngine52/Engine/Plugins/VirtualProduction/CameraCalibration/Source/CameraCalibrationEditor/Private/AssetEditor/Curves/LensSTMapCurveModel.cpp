// Copyright Epic Games, Inc. All Rights Reserved.

#include "LensSTMapCurveModel.h"

#include "CameraCalibrationEditorLog.h"
#include "LensFile.h"



FLensSTMapCurveModel::FLensSTMapCurveModel(ULensFile* InOwner, float InFocus)
	: FLensDataCurveModel(InOwner)
	, Focus(InFocus)
{
	bIsCurveValid = LensFile->STMapTable.BuildMapBlendingCurve(Focus, CurrentCurve);
}

void FLensSTMapCurveModel::SetKeyPositions(TArrayView<const FKeyHandle> InKeys, TArrayView<const FKeyPosition> InKeyPositions, EPropertyChangeType::Type ChangeType)
{
	//Overrides moving keys when dealing with STMap blending curve
}

void FLensSTMapCurveModel::SetKeyAttributes(TArrayView<const FKeyHandle> InKeys, TArrayView<const FKeyAttributes> InAttributes, EPropertyChangeType::Type ChangeType)
{
	//Applies attributes to copied curve
	FRichCurveEditorModel::SetKeyAttributes(InKeys, InAttributes, ChangeType);

	//Now go through all modified keys and update associated curve table
	if (FSTMapFocusPoint* Point = LensFile->STMapTable.GetFocusPoint(Focus))
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


