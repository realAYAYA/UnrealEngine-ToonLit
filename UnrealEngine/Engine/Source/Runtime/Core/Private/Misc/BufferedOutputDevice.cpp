// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/BufferedOutputDevice.h"

#include "Misc/ScopeLock.h"
#include "Templates/UnrealTemplate.h"

void FBufferedOutputDevice::Serialize(const TCHAR* InData, ELogVerbosity::Type Verbosity, const FName& Category)
{
	if (Verbosity > FilterLevel)
	{
		return;
	}

	FScopeLock ScopeLock(&SynchronizationObject);
	BufferedLines.Emplace(InData, Category, Verbosity);
}

void FBufferedOutputDevice::GetContents(TArray<FBufferedLine>& DestBuffer)
{
	FScopeLock ScopeLock(&SynchronizationObject);
	DestBuffer = MoveTemp(BufferedLines);
}

void FBufferedOutputDevice::RedirectTo(FOutputDevice& Ar)
{
	FScopeLock ScopeLock(&SynchronizationObject);
	for (const FBufferedLine& BufferedLine : BufferedLines)
	{
		Ar.Serialize(BufferedLine.Data.Get(), BufferedLine.Verbosity, BufferedLine.Category);
	}
}
