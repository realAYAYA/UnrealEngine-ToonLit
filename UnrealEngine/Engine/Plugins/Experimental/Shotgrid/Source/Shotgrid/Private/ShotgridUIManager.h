// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/UniquePtr.h"

class FShotgridUIManagerImpl;

class FShotgridUIManager
{
public:
	static void Initialize();
	static void Shutdown();

private:
	static TUniquePtr<FShotgridUIManagerImpl> Instance;
};
