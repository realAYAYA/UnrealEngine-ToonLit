// Copyright Epic Games, Inc. All Rights Reserved.

#include "TestHelpers.h"

FScopeDisableWarningsInLog::FScopeDisableWarningsInLog(FLogCategoryBase* InLogPtr)
{
	LogPtr = InLogPtr;
	OldLogLevel = LogPtr->GetVerbosity();
	LogPtr->SetVerbosity(ELogVerbosity::Error);
}

FScopeDisableWarningsInLog::~FScopeDisableWarningsInLog()
{
	LogPtr->SetVerbosity(OldLogLevel);
}