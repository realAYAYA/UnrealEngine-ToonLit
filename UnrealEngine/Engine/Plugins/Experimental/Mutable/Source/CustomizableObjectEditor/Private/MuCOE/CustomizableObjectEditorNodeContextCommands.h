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
* Class containing commands for viewport menu actions
*/
class FCustomizableObjectEditorNodeContextCommands : public TCommands<FCustomizableObjectEditorNodeContextCommands>
{
public:
	FCustomizableObjectEditorNodeContextCommands()
		: TCommands<FCustomizableObjectEditorNodeContextCommands>
		(
			TEXT("CustomizableObjectEditorNodeContext"), // Context name for fast lookup
			NSLOCTEXT("Contexts", "CustomizableObjectEditorNodeContext", "CustomizableObject Editor Node Context"), // Localized context name for displaying
			NAME_None, // Parent context name.  
			FAppStyle::GetAppStyleSetName() // Icon Style Set
			)
	{
	}

	/** Refresh material nodes in all children */
	TSharedPtr<FUICommandInfo> RefreshMaterialNodesInAllChildren;

	/** Create comment*/
	TSharedPtr<FUICommandInfo> CreateComment;

public:
	/** Registers our commands with the binding system */
	virtual void RegisterCommands() override;
};


