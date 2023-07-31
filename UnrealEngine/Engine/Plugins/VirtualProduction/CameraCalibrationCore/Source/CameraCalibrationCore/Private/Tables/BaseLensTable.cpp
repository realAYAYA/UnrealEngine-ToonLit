// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tables/BaseLensTable.h"
#include "LensFile.h"

FName FBaseLensTable::GetFriendlyPointName(ELensDataCategory InCategory)
{
	switch (InCategory)
	{
		case ELensDataCategory::Zoom: return TEXT("Focal Length");
		case ELensDataCategory::Distortion: return TEXT("Distortion Parameters");
		case ELensDataCategory::ImageCenter: return TEXT("Image Center");
		case ELensDataCategory::STMap: return TEXT("ST Map");
		case ELensDataCategory::NodalOffset: return TEXT("Nodal Offset");
	}

	return TEXT("");
}

void FBaseLensTable::ForEachFocusPoint(FFocusPointCallback InCallback, const float InFocus,float InputTolerance) const
{
	ForEachPoint([this, InCallback, InFocus, InputTolerance](const FBaseFocusPoint& InFocusPoint)
	{
		if (!FMath::IsNearlyEqual(InFocusPoint.GetFocus(), InFocus, InputTolerance))
		{
			return;
		}

		InCallback(InFocusPoint);
	});
}

void FBaseLensTable::ForEachLinkedFocusPoint(FLinkedFocusPointCallback InCallback, const float InFocus, float InputTolerance) const
{
	if (!ensure(LensFile.IsValid()))
	{
		return;
	}
	
	const TMap<ELensDataCategory, FLinkPointMetadata> LinkedCategories = GetLinkedCategories();
	for (const TPair<ELensDataCategory, FLinkPointMetadata>& LinkedCategoryPair : LinkedCategories)
	{
		const FBaseLensTable* const LinkDataTable = LensFile->GetDataTable(LinkedCategoryPair.Key);
		if (!ensure(LinkDataTable))
		{
			return;
		}
		
		LinkDataTable->ForEachPoint([this, InCallback, InFocus, InputTolerance, LinkedCategoryPair](const FBaseFocusPoint& InFocusPoint)
		{
			if (!FMath::IsNearlyEqual(InFocusPoint.GetFocus(), InFocus, InputTolerance))
			{
				return;
			}

			InCallback(InFocusPoint, LinkedCategoryPair.Key, LinkedCategoryPair.Value);
		});
	}
}

bool FBaseLensTable::HasLinkedFocusValues(const float InFocus, float InputTolerance) const
{
	if (!ensure(LensFile.IsValid()))
	{
		return false;
	}
	
	const TMap<ELensDataCategory, FLinkPointMetadata> LinkedCategories = GetLinkedCategories();
	for (const TPair<ELensDataCategory, FLinkPointMetadata>& LinkedCategoryPair : LinkedCategories)
	{
		const FBaseLensTable* const LinkDataTable = LensFile->GetDataTable(LinkedCategoryPair.Key);
		if (!ensure(LinkDataTable))
		{
			return false;
		}

		if (LinkDataTable->DoesFocusPointExists(InFocus))
		{
			return true;
		}
	}

	return false;
}

bool FBaseLensTable::HasLinkedZoomValues(const float InFocus, const float InZoomPoint, float InputTolerance) const
{
	if (!ensure(LensFile.IsValid()))
	{
		return false;
	}
	
	const TMap<ELensDataCategory, FLinkPointMetadata> LinkedCategories = GetLinkedCategories();
	for (const TPair<ELensDataCategory, FLinkPointMetadata>& LinkedCategoryPair : LinkedCategories)
	{
		const FBaseLensTable* const LinkDataTable = LensFile->GetDataTable(LinkedCategoryPair.Key);
		if (!ensure(LinkDataTable))
		{
			return false;
		}
	
		if (LinkDataTable->DoesZoomPointExists(InFocus, InZoomPoint, InputTolerance))
		{
			return true;
		}
	}

	return false;
}

bool FBaseLensTable::IsFocusBetweenNeighbor(const float InFocusPoint, const float InFocusValueToEvaluate) const
{
	const int32 PointNum = GetFocusPointNum();

	// Return true if there is no neighbor and only one focus point
	if (PointNum == 1)
	{
		return true;
	}

	TOptional<float> MinValue;
	TOptional<float> MaxValue;

	// Loop through all table points
	for (int32 PointIndex = 0; PointIndex < PointNum; ++PointIndex)
	{
		if (const FBaseFocusPoint* const FocusPoint = GetBaseFocusPoint(PointIndex))
		{
			// check if the given point is same as point from loop
			if (FMath::IsNearlyEqual(FocusPoint->GetFocus(), InFocusPoint))
			{
				// Get neighbor point indexes
				const int32 MaxIndex = (PointIndex + 1);
				const int32 MinIndex = (PointIndex - 1);

				// Set min from index - 1 value
				if (MinIndex >= 0 && MinIndex < PointNum)
				{
					MinValue = GetBaseFocusPoint(MinIndex)->GetFocus();
				}
				// If min index not valid set the min value from current loop focus point
				else
				{
					MinValue = FocusPoint->GetFocus();
				}

				// Set max from index + 1 value
				if (MaxIndex < PointNum && PointNum >= 0)
				{
					MaxValue = GetBaseFocusPoint(MaxIndex)->GetFocus();
				}
				// If max index not valid set the max value from current loop focus point
				else
				{
					MaxValue = FocusPoint->GetFocus();
				}

				// Stop executing, if given point is same as point from loop
				break;
			}
		}
	}

	// If min or max not set or then are equal return false
	if (!MinValue.IsSet() || !MaxValue.IsSet() || FMath::IsNearlyEqual(MinValue.GetValue(), MaxValue.GetValue()))
	{
		return false;
	}

	// return true if evaluate value fit into the neighbor range
	if ((MinValue.GetValue() < InFocusValueToEvaluate || FMath::IsNearlyEqual(MinValue.GetValue(), InFocusValueToEvaluate)) &&
		(MaxValue.GetValue() > InFocusValueToEvaluate || FMath::IsNearlyEqual(MaxValue.GetValue(), InFocusValueToEvaluate)))
	{
		return true;
	}
	
	return false;
}

