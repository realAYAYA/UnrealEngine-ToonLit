// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

typedef FName FEditorModeID;

struct GEOMETRYMODE_API FGeometryEditingModes
{
	static FEditorModeID EM_Geometry;
	static FEditorModeID EM_Bsp;
	static FEditorModeID EM_TextureAlign;
};
/**
 * Geometry mode module
 */
class FGeometryModeModule : public IModuleInterface
{
public:
	// IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	// End of IModuleInterface
};
