// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

// Insights
#include "Insights/ViewModels/Filters.h"

namespace Insights
{

////////////////////////////////////////////////////////////////////////////////////////////////////

class FMemoryFilterValueConverter :public IFilterValueConverter
{
public:
	virtual bool Convert(const FString& Input, int64& Output, FText& OutError) const override;
	virtual FText GetTooltipText() const override;
	virtual FText GetHintText() const override;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace Insights
