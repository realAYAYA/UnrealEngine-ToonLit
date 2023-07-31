// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Views/Details/DisplayClusterConfiguratorBaseDetailCustomization.h"

class FDisplayClusterConfiguratorClusterDetailsCustomization final : public FDisplayClusterConfiguratorBaseDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance()
	{
		return MakeShared<FDisplayClusterConfiguratorClusterDetailsCustomization>();
	}

	virtual void CustomizeDetails(IDetailLayoutBuilder& InLayoutBuilder) override;
};
