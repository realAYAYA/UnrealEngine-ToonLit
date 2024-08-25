// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Filters/IAvaFilterExpressionFactory.h"

class FAvaFilterTypeExpressionFactory final : public IAvaFilterExpressionFactory
{
public:
	static const FName KeyName;

	//~ Begin IAvaFilterExpressionFactory interface
	virtual FName GetFilterIdentifier() const override;
	virtual bool FilterExpression(const IAvaOutlinerItem& InItem, const FAvaTextFilterArgs& InArgs) const override;
	virtual bool SupportsComparisonOperation(const ETextFilterComparisonOperation& InComparisonOperation) const override;
	//~ End IAvaFilterExpressionFactory interface
};
