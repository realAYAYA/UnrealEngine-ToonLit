// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/Commands/Commands.h"

class FStaticMeshEditorModelingCommands : public TCommands<FStaticMeshEditorModelingCommands>
{
public:
	FStaticMeshEditorModelingCommands();
	void RegisterCommands() override;
	static const FStaticMeshEditorModelingCommands& Get();

	TSharedPtr<FUICommandInfo> ToggleStaticMeshEditorModelingMode;
};

