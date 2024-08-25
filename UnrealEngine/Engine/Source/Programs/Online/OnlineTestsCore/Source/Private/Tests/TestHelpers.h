// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Logging/LogCategory.h"
#include "Logging/LogVerbosity.h"

class FScopeDisableWarningsInLog
{
public:
	FScopeDisableWarningsInLog(FLogCategoryBase* InLogPtr);
	~FScopeDisableWarningsInLog();

private:
	FLogCategoryBase* LogPtr;
	ELogVerbosity::Type OldLogLevel;
};