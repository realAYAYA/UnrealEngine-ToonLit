// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Rundown/Factories/Filters/IAvaRundownFilterExpressionFactory.h"

class FAvaRundownFilterComboPageExpressionFactory : public IAvaRundownFilterExpressionFactory
{
public:
	//~ Begin IAvaFilterExpressionFactory interface
	virtual FName GetFilterIdentifier() const override;
	virtual bool FilterExpression(const FAvaRundownPage& InItem, const FAvaRundownTextFilterArgs& InArgs) const override;
	virtual bool SupportsComparisonOperation(ETextFilterComparisonOperation InComparisonOperation, EAvaRundownSearchListType InPlaylistSearchListType) const override;
	//~ End IAvaFilterExpressionFactory interface
};
