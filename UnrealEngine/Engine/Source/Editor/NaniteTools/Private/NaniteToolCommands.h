// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "Styling/AppStyle.h"
#include "Engine/StaticMesh.h"

class FNaniteToolCommands : public TCommands<FNaniteToolCommands>
{
public:
	FNaniteToolCommands()
	: TCommands<FNaniteToolCommands>(
		TEXT("NaniteTools"), // Context name for fast lookup
		NSLOCTEXT("Contexts", "NaniteTools", "Nanite Tools"), // Localized context name for displaying
		NAME_None, // Parent
		FAppStyle::GetAppStyleSetName() // Icon Style Set
	)
	{
	}

	virtual void RegisterCommands() override;

public:
	// Navigation commands
	TSharedPtr<FUICommandInfo> ShowInContentBrowser;

	// Nanite commands
	TSharedPtr<FUICommandInfo> EnableNanite;
	TSharedPtr<FUICommandInfo> DisableNanite;
};

void ModifyNaniteEnable(TArray<TWeakObjectPtr<UStaticMesh>>& MeshesToProcess, bool bNaniteEnable);