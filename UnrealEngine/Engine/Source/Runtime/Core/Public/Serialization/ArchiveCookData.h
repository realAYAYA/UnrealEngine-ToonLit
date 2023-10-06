// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"

class ITargetPlatform;
struct FArchiveCookContext;

// Holds archive data only relevant for cooking archives. Fill this out
// as part of FSavePackageArgs.
struct FArchiveCookData
{
	const ITargetPlatform& TargetPlatform;
	FArchiveCookContext& CookContext;
	FArchiveCookData(const ITargetPlatform& InTargetPlatform, FArchiveCookContext& InCookContext) :
		TargetPlatform(InTargetPlatform),
		CookContext(InCookContext)
	{}
};
