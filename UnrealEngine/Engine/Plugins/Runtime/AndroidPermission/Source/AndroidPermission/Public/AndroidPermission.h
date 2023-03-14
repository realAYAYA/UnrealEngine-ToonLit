// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

ANDROIDPERMISSION_API DECLARE_LOG_CATEGORY_EXTERN(LogAndroidPermission, Log, All);

class FAndroidPermissionModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
