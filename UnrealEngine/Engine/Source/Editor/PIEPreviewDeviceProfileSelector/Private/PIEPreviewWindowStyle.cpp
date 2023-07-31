// Copyright Epic Games, Inc. All Rights Reserved.

#include "PIEPreviewWindowStyle.h"

#if WITH_EDITOR

FPIEPreviewWindowStyle::FPIEPreviewWindowStyle()
{
}

void FPIEPreviewWindowStyle::GetResources(TArray< const FSlateBrush* >& OutBrushes) const
{
	ScreenRotationButtonStyle.GetResources(OutBrushes);
	QuarterMobileContentScaleFactorButtonStyle.GetResources(OutBrushes);
	HalfMobileContentScaleFactorButtonStyle.GetResources(OutBrushes);
	FullMobileContentScaleFactorButtonStyle.GetResources(OutBrushes);
}

const FName FPIEPreviewWindowStyle::TypeName(TEXT("FPIEPreviewWindowStyle"));

const FPIEPreviewWindowStyle& FPIEPreviewWindowStyle::GetDefault()
{
	static FPIEPreviewWindowStyle Default;
	return Default;
}
#endif