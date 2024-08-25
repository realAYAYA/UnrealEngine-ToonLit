// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "TextureGraphInsightEditorStyle.h"

class FTextureGraphInsightEditorCommands : public TCommands<FTextureGraphInsightEditorCommands>
{
public:

	FTextureGraphInsightEditorCommands()
		: TCommands<FTextureGraphInsightEditorCommands>(TEXT("TextureGraphInsightEditor"), NSLOCTEXT("Contexts", "TextureGraphInsightEditor", "TextureGraphInsightEditor Plugin"), NAME_None, FTextureGraphInsightEditorStyle::GetStyleSetName())
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;

// public:
	// TSharedPtr< FUICommandInfo > OpenPluginWindow;
};
