// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/Commands/Commands.h"

class FAvaInteractiveToolsCommands : public TCommands<FAvaInteractiveToolsCommands>
{
public:
	FAvaInteractiveToolsCommands();

	//~ Begin TCommands
	virtual void RegisterCommands() override;
	//~ End TCommands

	void RegisterCategoryCommands();
	void Register2DCommands();
	void Register3DCommands();
	void RegisterActorCommands();
	void RegisterLayoutCommands();

	// Categories
	TSharedPtr<FUICommandInfo> Category_2D;
	TSharedPtr<FUICommandInfo> Category_3D;
	TSharedPtr<FUICommandInfo> Category_Actor;
	TSharedPtr<FUICommandInfo> Category_Layout;

	TSharedPtr<FUICommandInfo> Tool_Actor_Null;
	TSharedPtr<FUICommandInfo> Tool_Actor_Spline;
};
