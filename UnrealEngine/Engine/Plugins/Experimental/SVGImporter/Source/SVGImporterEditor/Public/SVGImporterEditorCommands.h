// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/Commands/Commands.h"

class SVGIMPORTEREDITOR_API FSVGImporterEditorCommands : public TCommands<FSVGImporterEditorCommands>
{
public:
	FSVGImporterEditorCommands();

	//~ Begin TCommands
	virtual void RegisterCommands() override;
	//~ End TCommands

	TSharedPtr<FUICommandInfo> SpawnSVGActor;
};
