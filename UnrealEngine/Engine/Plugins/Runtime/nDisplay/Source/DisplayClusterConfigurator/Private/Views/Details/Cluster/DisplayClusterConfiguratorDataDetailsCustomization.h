// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Views/Details/DisplayClusterConfiguratorBaseDetailCustomization.h"

class FDisplayClusterConfiguratorDataDetailsCustomization final : public FDisplayClusterConfiguratorBaseDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance()
	{
		return MakeShared<FDisplayClusterConfiguratorDataDetailsCustomization>();
	}

	virtual void CustomizeDetails(IDetailLayoutBuilder& InLayoutBuilder) override;
};
