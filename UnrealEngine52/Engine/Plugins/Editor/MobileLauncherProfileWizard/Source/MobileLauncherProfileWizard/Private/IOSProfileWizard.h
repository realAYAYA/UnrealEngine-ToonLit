// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "ILauncherProfileManager.h"

class FIOSProfileWizard : public ILauncherProfileWizard
{
public:
	virtual FText GetName() const override;
	virtual FText GetDescription() const override;
	virtual void HandleCreateLauncherProfile(const ILauncherProfileManagerRef& ProfileManager) override;
};

