// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SPropertiesDebugViewBase.h"

namespace TraceServices { class IAnalysisSession; }

class SAnimationCurvesView : public SPropertiesDebugViewBase
{
public:
	virtual void GetVariantsAtFrame(const TraceServices::FFrame& InFrame, TArray<TSharedRef<FVariantTreeNode>>& OutVariants) const override;
	virtual FName GetName() const override;
	
	void SetCurveFilter(uint32 InCurveFilter)
	{
		CurveFilter = InCurveFilter;
		bCurveFilterSet = true;
	}

	void ClearCurveFilter()
	{
		bCurveFilterSet = false;
	}

private:
	uint32 CurveFilter = 0;
	bool bCurveFilterSet = false;
};