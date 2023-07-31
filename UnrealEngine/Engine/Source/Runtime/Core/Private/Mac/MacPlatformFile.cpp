// Copyright Epic Games, Inc. All Rights Reserved.

#include "Mac/MacPlatformFile.h"

IPlatformFile& IPlatformFile::GetPlatformPhysical()
{
	static FApplePlatformFile MacPlatformSingleton;
	return MacPlatformSingleton;
}
