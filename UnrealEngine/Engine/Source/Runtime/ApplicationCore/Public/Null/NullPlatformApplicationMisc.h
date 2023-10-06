// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GenericPlatform/GenericPlatformApplicationMisc.h"

struct FNullPlatformApplicationMisc : public FGenericPlatformApplicationMisc
{
	static APPLICATIONCORE_API class GenericApplication* CreateApplication();

	static APPLICATIONCORE_API bool IsUsingNullApplication();
};
