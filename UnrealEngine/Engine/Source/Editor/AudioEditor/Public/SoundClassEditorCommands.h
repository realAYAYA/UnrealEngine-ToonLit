// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "Internationalization/Internationalization.h"
#include "Styling/AppStyle.h"
#include "Templates/SharedPointer.h"
#include "UObject/NameTypes.h"
#include "UObject/UnrealNames.h"

class FUICommandInfo;

/*-----------------------------------------------------------------------------
   FSoundCueGraphEditorCommands
-----------------------------------------------------------------------------*/

class FSoundClassEditorCommands : public TCommands<FSoundClassEditorCommands>
{
public:
	/** Constructor */
	FSoundClassEditorCommands()
		: TCommands<FSoundClassEditorCommands>("SoundClassEditor", NSLOCTEXT("Contexts", "SoundClassEditor", "SoundClass Editor"), NAME_None, FAppStyle::GetAppStyleSetName())
	{
	}
	
	/** Plays the SoundCue or stops the currently playing cue/node */
	TSharedPtr<FUICommandInfo> ToggleSolo;

	/** Plays the SoundCue or stops the currently playing cue/node */
	TSharedPtr<FUICommandInfo> ToggleMute;

	/** Initialize commands */
	virtual void RegisterCommands() override;
};
