// Copyright Epic Games, Inc. All Rights Reserved.

#include "CrossCompiler.h"

#include "hlslcc.h"

namespace CrossCompiler
{
	FCriticalSection* GetCrossCompilerLock()
	{
		static FCriticalSection HlslCcCs;
		return &HlslCcCs;
	}
}