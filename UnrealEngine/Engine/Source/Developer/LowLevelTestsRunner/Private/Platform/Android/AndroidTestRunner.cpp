// Copyright Epic Games, Inc. All Rights Reserved.

#if defined(PLATFORM_ANDROID)
#include "TestRunner.h"

const char* AndroidPath;

extern "C" int LowLevelTestsMain(int argc, const char* argv[])
{
	return RunTests(argc, argv);
}

const char* GetCacheDirectory()
{
	return AndroidPath;
}

const char* GetProcessExecutablePath()
{
	return AndroidPath;
}
#endif