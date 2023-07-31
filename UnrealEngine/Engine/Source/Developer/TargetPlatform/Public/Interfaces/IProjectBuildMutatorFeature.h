// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Features/IModularFeature.h"

#define PROJECT_BUILD_MUTATOR_FEATURE "ProjectBuildMutatorFeature"

class IProjectBuildMutatorFeature : public IModularFeature
{
public:
	virtual bool RequiresProjectBuild(const FName& InPlatformInfoName, FText& OutReason) const = 0;
};
