// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Customizations/DynamicEntryWidgetDetailsBase.h"

class FListViewBaseDetails : public FDynamicEntryWidgetDetailsBase
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override;
};
