// Copyright Epic Games, Inc. All Rights Reserved.

#if defined(PLATFORM_LINUX)
#include "TestRunner.h"
#include <unistd.h>

const char* GetProcessExecutablePath()
{
	static char Buffer[512] = { 0 };
	ssize_t Result = readlink("/proc/self/exe", Buffer, sizeof(Buffer));
	if (Result > 0 && Result < (sizeof(Buffer) - 1))
	{
		Buffer[Result] = '\0';

		//Cut off the module file
		for (int Index = (int)Result; Index >= 0; --Index)
		{
			if (Buffer[Index] == '/')
			{
				Buffer[Index + 1] = '\0';
				break;
			}
		}

		return Buffer;
	}

	return nullptr;
}

const char* GetCacheDirectory()
{
	return "/var/tmp/playground_test_pds_cache";
}

int main(int argc, const char* argv[])
{
	return RunTests(argc, argv);
}
#endif