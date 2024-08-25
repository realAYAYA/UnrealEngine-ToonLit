// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalCompiledShaderCache.h: Metal RHI Compiled Shader Cache.
=============================================================================*/

#pragma once

#include "Misc/ScopeRWLock.h"
#include "MetalRHIPrivate.h"

struct FMetalCompiledShaderCache
{
public:
	FMetalCompiledShaderCache()
	{
		// VOID
	}

	~FMetalCompiledShaderCache()
	{
		// VOID
	}

	MTLFunctionPtr FindRef(FMetalCompiledShaderKey const& Key)
	{
		FRWScopeLock ScopedLock(Lock, SLT_ReadOnly);
        MTLFunctionPtr Func = Cache.FindRef(Key);
		return Func;
	}

	MTLLibraryPtr FindLibrary(MTLFunctionPtr Function)
	{
		FRWScopeLock ScopedLock(Lock, SLT_ReadOnly);
        MTLLibraryPtr Lib = LibCache.FindRef(Function->functionType());
		return Lib;
	}

	void Add(FMetalCompiledShaderKey Key, MTLLibraryPtr Lib, MTLFunctionPtr Function)
	{
		FRWScopeLock ScopedLock(Lock, SLT_Write);
		if (Cache.FindRef(Key).get() == nullptr)
		{
			Cache.Add(Key, Function);
			LibCache.Add(Function->functionType(), Lib);
		}
	}

private:
	FRWLock Lock;
	TMap<FMetalCompiledShaderKey, MTLFunctionPtr> Cache;
	TMap<MTL::FunctionType, MTLLibraryPtr> LibCache;
};

extern FMetalCompiledShaderCache& GetMetalCompiledShaderCache();
