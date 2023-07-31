// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "RenderDocPluginStyle.h"
#include "Framework/Commands/Commands.h"

class FRenderDocPluginCommands : public TCommands<FRenderDocPluginCommands>
{
public:
	FRenderDocPluginCommands()
		: TCommands<FRenderDocPluginCommands>(TEXT("RenderDocPlugin"), NSLOCTEXT("Contexts", "RenderDocPlugin", "RenderDoc Plugin"), NAME_None, FRenderDocPluginStyle::Get()->GetStyleSetName())
	{			
	}

	virtual void RegisterCommands() override;

	TSharedPtr<class FUICommandInfo> CaptureFrameCommand;
};

#endif
