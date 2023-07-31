// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	HoloLensStackWalk.cpp: HoloLens implementations of stack walk functions
=============================================================================*/

#include "HoloLensPlatformStackWalk.h"

TArray<FProgramCounterSymbolInfo> FHololensPlatformStackWalk::GetStack(int32 IgnoreCount, int32 MaxDepth, void* Context)
{
	TArray<FProgramCounterSymbolInfo> Stack;
	FProgramCounterSymbolInfo dummy;
	Stack.Push(dummy);
	return Stack;
}
