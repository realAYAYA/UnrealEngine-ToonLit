// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalCompiledShaderCache.h: Metal RHI Compiled Shader Cache.
=============================================================================*/

#pragma once

#include "Misc/ScopeRWLock.h"

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

	mtlpp::Function FindRef(FMetalCompiledShaderKey const& Key)
	{
		FRWScopeLock ScopedLock(Lock, SLT_ReadOnly);
		mtlpp::Function Func = Cache.FindRef(Key);
		return Func;
	}

	mtlpp::Library FindLibrary(mtlpp::Function const& Function)
	{
		FRWScopeLock ScopedLock(Lock, SLT_ReadOnly);
		mtlpp::Library Lib = LibCache.FindRef(Function.GetPtr());
		return Lib;
	}

	void Add(FMetalCompiledShaderKey Key, mtlpp::Library const& Lib, mtlpp::Function const& Function)
	{
		FRWScopeLock ScopedLock(Lock, SLT_Write);
		if (Cache.FindRef(Key) == nil)
		{
			Cache.Add(Key, Function);
			LibCache.Add(Function.GetPtr(), Lib);
		}
	}

private:
	FRWLock Lock;
	TMap<FMetalCompiledShaderKey, mtlpp::Function> Cache;
	TMap<mtlpp::Function::Type, mtlpp::Library> LibCache;
};

extern FMetalCompiledShaderCache& GetMetalCompiledShaderCache();
