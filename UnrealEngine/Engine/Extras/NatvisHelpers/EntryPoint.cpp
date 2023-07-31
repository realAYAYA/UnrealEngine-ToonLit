// Copyright Epic Games, Inc. All Rights Reserved.

#include "Globals.h"

extern "C" __declspec(dllexport) void InitNatvisHelpers(uint8** NameTable, FChunkedFixedUObjectArray* ObjectArray)
{
	GNameBlocksDebug = NameTable;
	GObjectArrayForDebugVisualizers = ObjectArray;
}
