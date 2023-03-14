// Copyright Epic Games, Inc. All Rights Reserved.


/*=============================================================================================
	AndroidPlatformStackWalk.h: Android platform stack walk functions
==============================================================================================*/

#pragma once
#include "GenericPlatform/GenericPlatformStackWalk.h"

struct FAsyncThreadBackTrace
{
	std::atomic<int32> Flag;		//0 or 1 to indicate if stack capture has been finished
	int32 Depth;
	static constexpr int StackTraceMaxDepth = 100;
	uint64 BackTrace[StackTraceMaxDepth];
	uint32 ThreadID;
	static constexpr int MaxThreadName = 20;
	char ThreadName[MaxThreadName];
};

/**
* Android platform stack walking
*/
struct CORE_API FAndroidPlatformStackWalk : public FGenericPlatformStackWalk
{
	typedef FGenericPlatformStackWalk Parent;

	static void ProgramCounterToSymbolInfo(uint64 ProgramCounter, FProgramCounterSymbolInfo& out_SymbolInfo);
	static uint32 CaptureStackBackTrace(uint64* BackTrace, uint32 MaxDepth, void* Context = nullptr);
	static bool SymbolInfoToHumanReadableString(const FProgramCounterSymbolInfo& SymbolInfo, ANSICHAR* HumanReadableString, SIZE_T HumanReadableStringSize);

	static uint32 CaptureThreadStackBackTrace(uint64 ThreadId, uint64* BackTrace, uint32 MaxDepth, void* Context = nullptr);
	static int CaptureThreadStackBackTraceAsync(FAsyncThreadBackTrace* BackTrace);

	static void HandleBackTraceSignal(siginfo* Info, void* Context);

	static bool InitStackWalking();
};

typedef FAndroidPlatformStackWalk FPlatformStackWalk;
