// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/App.h"

class FBlackmagic
{
public:
	static bool Initialize();
	static bool IsInitialized();
	static void Shutdown();

	static bool CanUseBlackmagicCard() { return (FApp::CanEverRender() || bCanForceBlackmagicUsage); }

private:
	static void LogInfo(const TCHAR* InFormat, ...);
	static void LogWarning(const TCHAR* InFormat, ...);
	static void LogError(const TCHAR* InFormat, ...);
	static void LogVerbose(const TCHAR* InFormat, ...);

private:
	static void* LibHandle;
	static bool bInitialized;
	static bool bCanForceBlackmagicUsage;
};
