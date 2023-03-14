// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Interfaces/IProjectBuildMutatorFeature.h"

class FCryptoKeysProjectBuildMutatorFeature : public IProjectBuildMutatorFeature
{
public:
	virtual bool RequiresProjectBuild(const FName& InPlatformInfoName, FText& OutReason) const override;
};