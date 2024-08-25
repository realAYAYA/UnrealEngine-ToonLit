// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalCompiledShaderCache.cpp: Metal RHI Compiled Shader Cache.
=============================================================================*/

#include "CoreMinimal.h"

THIRD_PARTY_INCLUDES_START
#include "MetalInclude.h"
THIRD_PARTY_INCLUDES_END

#include "MetalCompiledShaderKey.h"
#include "MetalCompiledShaderCache.h"

FMetalCompiledShaderCache& GetMetalCompiledShaderCache()
{
	static FMetalCompiledShaderCache CompiledShaderCache;
	return CompiledShaderCache;
}
