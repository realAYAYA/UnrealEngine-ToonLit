// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DerivedDataLegacyCacheStore.h"

namespace UE::DerivedData
{

class IMemoryCacheStore : public ILegacyCacheStore
{
public:
	virtual void Delete(const FCacheKey& Key, const FSharedString& Name) = 0;
	virtual void DeleteValue(const FCacheKey& Key, const FSharedString& Name) = 0;

	virtual void Disable() = 0;
};

} // UE::DerivedData
