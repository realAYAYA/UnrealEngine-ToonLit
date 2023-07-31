// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Stats/Stats.h"
#include "Modules/ModuleManager.h"

class APPLEARKITFACESUPPORT_API FAppleARKitFaceSupportModule :
	public IModuleInterface
{
public:
	virtual void StartupModule() override;

	virtual void ShutdownModule() override;
};

DECLARE_LOG_CATEGORY_EXTERN(LogAppleARKitFace, Log, All);

DECLARE_STATS_GROUP(TEXT("Face AR"), STATGROUP_FaceAR, STATCAT_Advanced);
