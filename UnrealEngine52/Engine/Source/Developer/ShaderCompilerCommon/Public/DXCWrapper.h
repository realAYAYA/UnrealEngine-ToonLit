// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/Platform.h"
#include "Templates/RefCounting.h"

struct FDllHandle : public FRefCountedObject
{
private:
	void* Handle = nullptr;

public:
	FDllHandle(const TCHAR* InFilename);
	virtual ~FDllHandle();
};

class SHADERCOMPILERCOMMON_API FDxcModuleWrapper
{
private:
	uint32 ModuleVersionHash = 0;
protected:
	FORCEINLINE uint32 GetModuleVersionHash() const
	{
		return ModuleVersionHash;
	}
public:
	FDxcModuleWrapper();
	virtual ~FDxcModuleWrapper();
};

class SHADERCOMPILERCOMMON_API FShaderConductorModuleWrapper : private FDxcModuleWrapper
{
private:
	uint32 ModuleVersionHash = 0;
protected:
	FORCEINLINE uint32 GetModuleVersionHash() const
	{
		return ModuleVersionHash;
	}
public:
	FShaderConductorModuleWrapper();
	virtual ~FShaderConductorModuleWrapper();
};
