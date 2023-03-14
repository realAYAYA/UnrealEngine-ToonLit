// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "Styling/AppStyle.h"

class FNewGizmoEnableCommands : public TCommands<FNewGizmoEnableCommands>
{
public:

	FNewGizmoEnableCommands()
		: TCommands<FNewGizmoEnableCommands>(TEXT("NewGizmoEnable"), NSLOCTEXT("Contexts", "NewGizmoEnable", "NewGizmoEnable Plugin"), NAME_None, FAppStyle::GetAppStyleSetName())
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;

public:
	TSharedPtr< FUICommandInfo > ToggleNewGizmos;
};
