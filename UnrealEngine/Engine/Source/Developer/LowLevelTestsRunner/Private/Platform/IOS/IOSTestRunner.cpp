// Copyright Epic Games, Inc. All Rights Reserved.

#if defined(PLATFORM_IOS)
#include "TestRunner.h"

int main(int argc, const char* argv[])
{
	return RunTests(argc, argv);
}

const char* GetCacheDirectory()
{
	return "/private/var/tmp/unit_tests_pds_cache";
}

const char* GetProcessExecutablePath()
{
	return nullptr;
}
#endif