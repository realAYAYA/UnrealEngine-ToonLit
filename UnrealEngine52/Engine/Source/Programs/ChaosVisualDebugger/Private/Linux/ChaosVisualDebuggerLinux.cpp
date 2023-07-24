// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosVisualDebuggerMain.h"
#include "LaunchEngineLoop.h"
#include "UnixCommonStartup.h"

int main(int argc, char *argv[])
{
	return CommonUnixMain(argc, argv, &ChaosVisualDebuggerMain, [] { FEngineLoop::AppExit(); });
}
