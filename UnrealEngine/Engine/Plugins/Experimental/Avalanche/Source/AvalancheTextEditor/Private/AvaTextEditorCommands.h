// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/Commands/Commands.h"

class FAvaTextEditorCommands : public TCommands<FAvaTextEditorCommands>
{
public:
	FAvaTextEditorCommands();

	//~ Begin TCommands
	virtual void RegisterCommands() override;
	//~ End TCommands

	TSharedPtr<FUICommandInfo> Tool_Actor_Text3D;
};
