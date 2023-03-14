// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPropertyGenerationUtilities.h"

/** Property generation utilities for widgets derived from SDetailsViewBase. */
class FDetailsViewPropertyGenerationUtilities : public IPropertyGenerationUtilities
{
public:
	FDetailsViewPropertyGenerationUtilities(SDetailsViewBase& InDetailsView)
		: DetailsView(&InDetailsView)
	{
	}

	virtual const FCustomPropertyTypeLayoutMap& GetInstancedPropertyTypeLayoutMap() const override
	{
		return DetailsView->GetCustomPropertyTypeLayoutMap();
	}

	virtual void RebuildTreeNodes() override
	{
		DetailsView->RerunCurrentFilter();
	}

private:
	SDetailsViewBase * DetailsView;
};