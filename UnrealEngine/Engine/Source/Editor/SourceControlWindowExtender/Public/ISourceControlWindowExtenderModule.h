// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

/**
 * SourceControlWindowExtender module interface
 */
class ISourceControlWindowExtenderModule : public IModuleInterface
{
public:
	/**
	 * Get reference to the SourceControlWindowExtender module instance
	 */
	static inline ISourceControlWindowExtenderModule& Get()
	{
		return FModuleManager::LoadModuleChecked<ISourceControlWindowExtenderModule>("SourceControlWindowExtender");
	}
};
