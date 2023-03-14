// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/BufferedOutputDevice.h"

#include "CoreTypes.h"
#include "Misc/ScopeLock.h"
#include "Templates/UnrealTemplate.h"

void FBufferedOutputDevice::Serialize(const TCHAR* InData, ELogVerbosity::Type Verbosity, const class FName& Category)
{
	if (Verbosity > FilterLevel)
		return;

	FScopeLock ScopeLock(&SynchronizationObject);
	new(BufferedLines)FBufferedLine(InData, Category, Verbosity);
}

void FBufferedOutputDevice::GetContents(TArray<FBufferedLine>& DestBuffer)
{
	FScopeLock ScopeLock(&SynchronizationObject);
	DestBuffer = MoveTemp(BufferedLines);
}
