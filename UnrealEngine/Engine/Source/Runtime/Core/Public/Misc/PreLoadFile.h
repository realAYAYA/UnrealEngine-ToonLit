// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "HAL/Platform.h"
#include "Misc/DelayedAutoRegister.h"

class FEvent;

struct CORE_API FPreLoadFile : public FDelayedAutoRegisterHelper
{
	FPreLoadFile(const TCHAR* InPath);

	void* TakeOwnershipOfLoadedData(int64* OutFileSize=nullptr);
	static void* TakeOwnershipOfLoadedDataByPath(const TCHAR* Filename, int64* OutFileSize);

protected:
	void KickOffRead();

	bool bIsComplete;
	bool bFailedToOpenInKickOff;
	static bool bSystemNoLongerTakingRequests;
	void* Data;
	int64 FileSize;
	FString Path;
	FEvent* CompletionEvent;
	class IAsyncReadFileHandle* AsyncReadHandle = nullptr;
	class IAsyncReadRequest* SizeRequestHandle = nullptr;
};
