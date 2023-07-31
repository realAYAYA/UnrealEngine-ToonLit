// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/Commands/Commands.h"
#include "HAL/Platform.h"
#include "Internationalization/Internationalization.h"
#include "Styling/AppStyle.h"
#include "Templates/SharedPointer.h"
#include "UObject/NameTypes.h"
#include "UObject/UnrealNames.h"

class FUICommandInfo;

/**
 * Class containing commands for static mesh editor viewport LOD actions
 */
class FStaticMeshViewportLODCommands : public TCommands<FStaticMeshViewportLODCommands>
{
public:
	FStaticMeshViewportLODCommands()
		: TCommands<FStaticMeshViewportLODCommands>
		(
			TEXT("StaticMeshViewportLODCmd"), // Context name for fast lookup
			NSLOCTEXT("Contexts", "StaticMeshViewportLODCmd", "Static Mesh Viewport LOD Command"), // Localized context name for displaying
			NAME_None, // Parent context name. 
			FAppStyle::GetAppStyleSetName() // Icon Style Set
		)
	{
	}

	/** LOD Auto */
	TSharedPtr< FUICommandInfo > LODAuto;

	/** LOD 0 */
	TSharedPtr< FUICommandInfo > LOD0;

public:
	/** Registers our commands with the binding system */
	virtual void RegisterCommands() override;
};
