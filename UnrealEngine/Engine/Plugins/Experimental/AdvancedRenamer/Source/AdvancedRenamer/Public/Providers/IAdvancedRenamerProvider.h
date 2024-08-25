// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"

class FString;

class IAdvancedRenamerProvider
{
public:
	virtual ~IAdvancedRenamerProvider() = default;
	virtual int32 Num() const = 0;
	virtual bool IsValidIndex(int32 Index) const = 0;
	virtual uint32 GetHash(int32 Index) const = 0;
	virtual FString GetOriginalName(int32 Index) const = 0;
	virtual bool RemoveIndex(int32 Index) = 0;
	virtual bool CanRename(int32 Index) const = 0;
	virtual bool ExecuteRename(int32 Index, const FString& NewName) = 0;
};
