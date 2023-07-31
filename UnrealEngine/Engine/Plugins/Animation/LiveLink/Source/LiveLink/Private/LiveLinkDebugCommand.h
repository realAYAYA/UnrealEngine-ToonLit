// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/IConsoleManager.h"
#include "Templates/UniquePtr.h"

class FAutoConsoleCommand;
class FLiveLinkClient;
class SLiveLinkDebugView;

class FLiveLinkDebugCommand
{
public:
	FLiveLinkDebugCommand(FLiveLinkClient& InClient);

private:
	void ShowDebugInfo();
	void HideDebugInfo();

private:
	FLiveLinkClient& Client;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	TUniquePtr<FAutoConsoleCommand> CommandShow;
	TUniquePtr<FAutoConsoleCommand> CommandHide;
#endif

	bool bRenderDebugInfo;
	TWeakPtr<SLiveLinkDebugView> DebugViewEditor;
	TWeakPtr<SLiveLinkDebugView> DebugViewGame;
};
