// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/Platform.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"


class EDITORINTERACTIVETOOLSFRAMEWORK_API FEditorInteractiveToolsFrameworkGlobals
{
public:
	// This is the key returned by AddComponentTargetFactory() for the FStaticMeshComponentTargetFactory created/registered
	// in StartupModule() below. Use this key to find/remove that module registration if you need to.
	static int32 RegisteredStaticMeshTargetFactoryKey;
};

class FEditorInteractiveToolsFrameworkModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
