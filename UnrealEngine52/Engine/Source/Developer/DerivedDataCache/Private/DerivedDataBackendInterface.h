// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreTypes.h"
#include "Containers/BitArray.h"
#include "Containers/StringView.h"
#include "DerivedDataCache.h"
#include "DerivedDataCacheKeyFilter.h"
#include "DerivedDataLegacyCacheStore.h"
#include "HAL/LowLevelMemTracker.h"
#include "Stats/Stats.h"
#include "Templates/DontCopy.h"
#include "Templates/PimplPtr.h"

class FDerivedDataCacheUsageStats;
class FDerivedDataCacheStatsNode;

namespace UE::DerivedData { struct FCacheKey; }
namespace UE::DerivedData::Private { struct FBackendDebugMissState; }

DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Num Gets"),STAT_DDC_NumGets,STATGROUP_DDC, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Num Puts"),STAT_DDC_NumPuts,STATGROUP_DDC, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Num Build"),STAT_DDC_NumBuilds,STATGROUP_DDC, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Num Exists"),STAT_DDC_NumExist,STATGROUP_DDC, );
DECLARE_FLOAT_ACCUMULATOR_STAT_EXTERN(TEXT("Sync Get Time"),STAT_DDC_SyncGetTime,STATGROUP_DDC, );
DECLARE_FLOAT_ACCUMULATOR_STAT_EXTERN(TEXT("ASync Wait Time"),STAT_DDC_ASyncWaitTime,STATGROUP_DDC, );
DECLARE_FLOAT_ACCUMULATOR_STAT_EXTERN(TEXT("Sync Put Time"),STAT_DDC_PutTime,STATGROUP_DDC, );
DECLARE_FLOAT_ACCUMULATOR_STAT_EXTERN(TEXT("Sync Build Time"),STAT_DDC_SyncBuildTime,STATGROUP_DDC, );
DECLARE_FLOAT_ACCUMULATOR_STAT_EXTERN(TEXT("Exists Time"),STAT_DDC_ExistTime,STATGROUP_DDC, );

/** Memory allocation tag for internal allocations of DDC backends */
LLM_DECLARE_TAG_API(DDCBackend, DERIVEDDATACACHE_API);

/** Memory allocation tag for allocations made by DDC but that are passed to other systems */
LLM_DECLARE_TAG_API(UntaggedDDCResult, DERIVEDDATACACHE_API);

namespace UE::DerivedData
{

enum class EBackendLegacyMode
{
	/** Use only GetValue/PutValue. */
	ValueOnly,
	/** Use GetValue/PutValue with a fallback to GetCachedData+PutValue for misses. */
	ValueWithLegacyFallback,
	/** Use only GetCachedData/PutCachedData. */
	LegacyOnly,
};

/**
 * Speed classes. Higher values are faster so > / < comparisons make sense.
 */
enum class EBackendSpeedClass
{
	Unknown,		/* Don't know yet*/
	Slow,			/* Slow, likely a remote drive. Some benefit but handle with care */
	Ok,				/* Ok but not great.  */
	Fast,			/* Fast but seek times still have an impact */
	Local			/* Little to no impact from seek times and extremely fast reads */
};

/** Debug options that can be applied to backends to simulate different behavior */
struct FBackendDebugOptions
{
	/** Apply behavior of this speed class */
	EBackendSpeedClass	SpeedClass;

	/** Filter to control the keys for which to simulate a miss. */
	FCacheKeyFilter SimulateMissFilter;

	/** State for simulated misses. */
	TDontCopy<TPimplPtr<Private::FBackendDebugMissState>> SimulateMissState;

	FBackendDebugOptions();

	/** Fill in the provided structure based on the name of the node (e.g. 'Shared') and the provided token stream. */
	static bool ParseFromTokens(FBackendDebugOptions& OutOptions, const TCHAR* InNodeName, const TCHAR* InTokens);

	bool ShouldSimulatePutMiss(const FCacheKey& Key);
	bool ShouldSimulateGetMiss(const FCacheKey& Key);
};

class FDerivedDataBackend
{
public:
	static FDerivedDataBackend* Create();

	/**
	 * Singleton to retrieve the GLOBAL backend
	 *
	 * @return Reference to the global cache backend
	 */
	static FDerivedDataBackend& Get();

	virtual ~FDerivedDataBackend() = default;

	/**
	 * Singleton to retrieve the root cache
	 * @return Reference to the global cache root
	 */
	virtual ILegacyCacheStore& GetRoot() = 0;

	virtual int32 GetMaxKeyLength() const = 0;

	//--------------------
	// System Interface, copied from FDerivedDataCacheInterface
	//--------------------

	virtual void NotifyBootComplete() = 0;
	virtual void AddToAsyncCompletionCounter(int32 Addend) = 0;
	virtual bool AnyAsyncRequestsRemaining() = 0;
	virtual bool IsShuttingDown() = 0;
	virtual void WaitForQuiescence(bool bShutdown = false) = 0;
	virtual void GetDirectories(TArray<FString>& OutResults) = 0;
	virtual bool GetUsingSharedDDC() const = 0;
	virtual const TCHAR* GetGraphName() const = 0;
	virtual const TCHAR* GetDefaultGraphName() const = 0;

	/**
	 * Mounts a read-only pak file.
	 *
	 * @param PakFilename Pak filename
	 */
	virtual ILegacyCacheStore* MountPakFile(const TCHAR* PakFilename) = 0;

	/**
	 * Unmounts a read-only pak file.
	 *
	 * @param PakFilename Pak filename
	 */
	virtual bool UnmountPakFile(const TCHAR* PakFilename) = 0;

	/**
	 *  Gather the usage of the DDC hierarchically.
	 */
	virtual TSharedRef<FDerivedDataCacheStatsNode> GatherUsageStats() const = 0;
};

/** Lexical conversions from and to enums */

[[nodiscard]] inline const TCHAR* LexToString(const EBackendLegacyMode Value)
{
	switch (Value)
	{
	case EBackendLegacyMode::ValueOnly:
		return TEXT("ValueOnly");
	case EBackendLegacyMode::ValueWithLegacyFallback:
		return TEXT("ValueWithLegacyFallback");
	case EBackendLegacyMode::LegacyOnly:
		return TEXT("LegacyOnly");
	default:
		checkNoEntry();
		return TEXT("Unknown");
	}
}

[[nodiscard]] inline bool TryLexFromString(EBackendLegacyMode& OutValue, const FStringView String)
{
	if (String == TEXTVIEW("ValueOnly"))
	{
		OutValue = EBackendLegacyMode::ValueOnly;
		return true;
	}
	if (String == TEXTVIEW("ValueWithLegacyFallback"))
	{
		OutValue = EBackendLegacyMode::ValueWithLegacyFallback;
		return true;
	}
	if (String == TEXTVIEW("LegacyOnly"))
	{
		OutValue = EBackendLegacyMode::LegacyOnly;
		return true;
	}
	return false;
}

inline const TCHAR* LexToString(EBackendSpeedClass SpeedClass)
{
	switch (SpeedClass)
	{
	case EBackendSpeedClass::Unknown:
		return TEXT("Unknown");
	case EBackendSpeedClass::Slow:
		return TEXT("Slow");
	case EBackendSpeedClass::Ok:
		return TEXT("Ok");
	case EBackendSpeedClass::Fast:
		return TEXT("Fast");
	case EBackendSpeedClass::Local:
		return TEXT("Local");
	}

	checkNoEntry();
	return TEXT("Unknown value! (Update LexToString!)");
}

inline void LexFromString(EBackendSpeedClass& OutValue, const TCHAR* Buffer)
{
	OutValue = EBackendSpeedClass::Unknown;

	if (FCString::Stricmp(Buffer, TEXT("Slow")) == 0)
	{
		OutValue = EBackendSpeedClass::Slow;
	}
	else if (FCString::Stricmp(Buffer, TEXT("Ok")) == 0)
	{
		OutValue = EBackendSpeedClass::Ok;
	}
	else if (FCString::Stricmp(Buffer, TEXT("Fast")) == 0)
	{
		OutValue = EBackendSpeedClass::Fast;
	}
	else if (FCString::Stricmp(Buffer, TEXT("Local")) == 0)
	{
		OutValue = EBackendSpeedClass::Local;
	}
}

} // UE::DerivedData
