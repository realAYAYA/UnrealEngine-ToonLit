// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalShaderDebugCache.cpp: Metal RHI Shader Debug Cache.
=============================================================================*/

#include "CoreMinimal.h"

THIRD_PARTY_INCLUDES_START
#include "mtlpp.hpp"
THIRD_PARTY_INCLUDES_END

#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"

#include "MetalShaderDebugCache.h"
#include "MetalShaderDebugZipFile.h"

#if !UE_BUILD_SHIPPING

FMetalShaderDebugZipFile* FMetalShaderDebugCache::GetDebugFile(FString Path)
{
	FScopeLock Lock(&Mutex);
	FMetalShaderDebugZipFile* Ref = DebugFiles.FindRef(Path);
	if (!Ref)
	{
		Ref = new FMetalShaderDebugZipFile(Path);
		DebugFiles.Add(Path, Ref);
	}
	return Ref;
}

ns::String FMetalShaderDebugCache::GetShaderCode(uint32 ShaderSrcLen, uint32 ShaderSrcCRC)
{
	ns::String Code;
	FScopeLock Lock(&Mutex);
	for (auto const& Ref : DebugFiles)
	{
		Code = Ref.Value->GetShaderCode(ShaderSrcLen, ShaderSrcCRC);
		if (Code)
		{
			break;
		}
	}
	return Code;
}

#endif // !UE_BUILD_SHIPPING
