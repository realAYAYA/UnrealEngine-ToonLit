// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Singleton wrapper to manage the Tcls proxy connection
 */
class FTencentDll
{
public:
	FTencentDll();
	virtual ~FTencentDll();
	void* Load(const FString& DllPath, const FString& DllFile);
	void Unload();
	inline bool IsLoaded() const { return DllHandle != nullptr; }
	inline void* GetDllHandle() const { return DllHandle; }

private:
	void* DllHandle;
};

