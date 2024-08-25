// Copyright Epic Games, Inc. All Rights Reserved.

#include "SourceControlViewportModule.h"
#include "SourceControlViewportOutlineMenu.h"
#include "SourceControlViewportToolTips.h"

void FSourceControlViewportModule::StartupModule()
{
	ViewportOutlineMenu = MakeShared<FSourceControlViewportOutlineMenu>();
	ViewportOutlineMenu->Init();
	ViewportToolTips = MakeShared<FSourceControlViewportToolTips>();
	ViewportToolTips->Init();
}

void FSourceControlViewportModule::ShutdownModule()
{
	ViewportToolTips.Reset();
	ViewportOutlineMenu.Reset();
}

IMPLEMENT_MODULE( FSourceControlViewportModule, SourceControlViewport );