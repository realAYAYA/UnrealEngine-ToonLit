// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/StrongObjectPtr.h"

class UPackage;
class UWorld;
struct FSoftObjectPath;

/** Common utility functions for implementing Managed Instance. */ 
class FAvaRundownManagedInstanceUtils
{
public:
	static UPackage* MakeManagedInstancePackage(const FSoftObjectPath& InAssetPath);
	static void PreventWorldFromBeingSeenAsLeakingByLevelEditor(UWorld* InWorld);
};