// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IRewindDebuggerView.h"
#include "IRewindDebuggerViewCreator.h"
#include "SPropertiesDebugViewBase.h"

namespace TraceServices { class IAnalysisSession; }

class SMontageView : public SPropertiesDebugViewBase
{
public:
	virtual void GetVariantsAtFrame(const TraceServices::FFrame& InFrame, TArray<TSharedRef<FVariantTreeNode>>& OutVariants) const override;
	virtual FName GetName() const override;
	
	void SetAssetFilter(uint64 InAssetIdFilter)
	{
		AssetIdFilter = InAssetIdFilter;
		bAssetFilterSet = true;
	}

	void ClearAssetFilter()
	{
		bAssetFilterSet = false;
	}

private:
	uint64 AssetIdFilter = 0;
	bool bAssetFilterSet = false;
};