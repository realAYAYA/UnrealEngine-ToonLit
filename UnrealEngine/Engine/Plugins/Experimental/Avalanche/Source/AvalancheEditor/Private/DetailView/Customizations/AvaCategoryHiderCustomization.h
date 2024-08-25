// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"

class FAvaCategoryHiderCustomization : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance()
	{
		return MakeShared<FAvaCategoryHiderCustomization>();
	}

	virtual ~FAvaCategoryHiderCustomization() override = default;

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

	static void HideCategories(IDetailLayoutBuilder& DetailBuilder);
};
