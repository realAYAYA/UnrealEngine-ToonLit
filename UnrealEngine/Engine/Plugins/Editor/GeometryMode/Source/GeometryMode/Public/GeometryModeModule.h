// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"

class FName;

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

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
