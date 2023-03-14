// Copyright Epic Games, Inc. All Rights Reserved.

#include "Unix/UnixApplicationErrorOutputDevice.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Misc/App.h"

void FUnixApplicationErrorOutputDevice::HandleErrorRestoreUI()
{
	// do not copy if graphics have not been initialized or if we're on the wrong thread
	if (FApp::CanEverRender() && IsInGameThread())
	{
		FPlatformApplicationMisc::ClipboardCopy(GErrorHist);
	}
}
