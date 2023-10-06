// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "DerivedDataLegacyCacheStore.h"

namespace UE::DerivedData
{

class IPakFileCacheStore : public ILegacyCacheStore
{
public:
	virtual void Close() = 0;
	virtual bool IsWritable() const = 0;
	virtual bool SaveCache() = 0;
	virtual bool LoadCache(const TCHAR* InFilename) = 0;
	virtual void MergeCache(IPakFileCacheStore* OtherPak) = 0;
	virtual const FString& GetFilename() const = 0;

	static bool SortAndCopy(const FString& InputFilename, const FString& OutputFilename);
};

} // UE::DerivedData
