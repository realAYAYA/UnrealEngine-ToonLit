// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "Containers/BitArray.h"
#include "Containers/StringView.h"
#include "Misc/AsciiSet.h"
#include "DerivedDataCacheModule.h"

class FDerivedDataCacheUsageStats;
class FDerivedDataCacheStatsNode;
class IDDCCleanup;
struct FDerivedDataCacheResourceStat;
struct FDerivedDataCacheSummaryStats;

/** 
 * Interface for the derived data cache
 * This API is fully threadsafe (with the possible exception of the system interface: NotfiyBootComplete, etc).
 */
class FDerivedDataCacheInterface
{
public:
	virtual ~FDerivedDataCacheInterface() = default;

	//--------------------
	// High Level Interface
	//--------------------

	/** 
	 * Synchronously checks the cache and if the item is present, it returns the cached results, otherwise tells the deriver to build the data and then updates the cache
	 * @param	DataDeriver	plugin to produce cache key and in the event of a miss, return the data.
	 * @param	bDataWasBuilt if non-null, set to true if the data returned had to be built instead of retrieved from the DDC. Used for stat tracking.
	 * @return	true if the data was retrieved from the cache or the deriver built the data sucessfully. false can only occur if the plugin returns false.
	**/
	virtual bool GetSynchronous(class FDerivedDataPluginInterface* DataDeriver, TArray<uint8>& OutData, bool* bDataWasBuilt = nullptr) = 0;

	/** 
	 * Starts the async process of checking the cache and if the item is present, retrieving the cached results, otherwise telling the deriver to build the data and then updating the cache
	 * If the plugin does not support threading, all of the above will be completed before the call returns.
	 * @param	DataDeriver	plugin to produce cache key and in the event of a miss, return the data.
	 * @return	a handle that can be used for PollAsynchronousCompletion, WaitAsynchronousCompletion and GetAsynchronousResults
	**/
	virtual uint32 GetAsynchronous(class FDerivedDataPluginInterface* DataDeriver) = 0;
	/** 
	 * Polls a previous GetAsynchronous get for completion.
	 * @param	Handle	Handle returned from GetAsynchronous.
	 * @return			true if the build / retrieve is complete and the results can be obtained.
	**/
	virtual bool PollAsynchronousCompletion(uint32 Handle) = 0;
	/** 
	 * Blocks the current thread until an previous GetAsynchronous request is ready
	 * @param	Handle	Handle returned from GetAsynchronous.
	**/
	virtual void WaitAsynchronousCompletion(uint32 Handle) = 0;
	/** 
	 * Retrieves the results from an async lookup / build. MUST only be called after the results are known to be ready by one of the aforementioned calls.
	 * @param	Handle	Handle returned from GetAsynchronous.
	 * @param	OutData	Array to receive the output results.
	 * @param	bDataWasBuilt if non-null, set to true if the data returned had to be built instead of retrieved from the DDC. Used for stat tracking.
	 * @return			true if the data was retrieved from the cache or the deriver built the data successfully. false can only occur if the plugin returns false.
	**/
	virtual bool GetAsynchronousResults(uint32 Handle, TArray<uint8>& OutData, bool* bDataWasBuilt = nullptr) = 0;
	virtual bool GetAsynchronousResults(uint32 Handle, TArray64<uint8>& OutData, bool* bDataWasBuilt = nullptr) = 0;

	//--------------------------
	// Low Level Static Helpers
	//--------------------------
	
private:
	static inline constexpr FAsciiSet ValidCacheKeyChars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_$";

	static void AppendSanitized(FString& Out, FStringView In)
	{
		while (true)
		{
			FStringView SanePrefix = FAsciiSet::FindPrefixWith(In, ValidCacheKeyChars);
			Out.Append(SanePrefix);
			In.RemovePrefix(SanePrefix.Len());

			if (In.IsEmpty())
			{
				break;
			}

			Out.Appendf(TEXT("$%x"), uint32(In[0]));
			In.RemovePrefix(1);
		}
	}

public:
	/**
	 * Returns true if character is valid in a DDC cache key without escaping 
	**/
	static bool IsValidCacheChar(const TCHAR C)
	{
		return ValidCacheKeyChars.Contains(C);
	}

	/** 
	 * Static function to make sure a cache key contains only legal characters by using an escape
	 * @param CacheKey							Cache key to sanitize
	 * @return									Sanitized cache key
	**/
	static FString SanitizeCacheKey(const TCHAR* CacheKey)
	{
		FString Out;
		AppendSanitized(Out, FStringView(CacheKey));
		return Out;
	}

	/** 
	 * Static function to build a cache key out of the plugin name, versions and plugin specific info
	 * @param PluginName						Name of the derived data type
	 * @param VersionString						Version string of the derived data
	 * @param PluginSpecificCacheKeySuffix		GUIDS and / or Hashes, etc to uniquely identify the specific cache entry
	 * @return									Assembled cache key
	**/
	static FString BuildCacheKey(const TCHAR* PluginName, const TCHAR* VersionString, const TCHAR* PluginSpecificCacheKeySuffix)
	{
		return BuildCacheKey(FStringView(PluginName), FStringView(VersionString), FStringView(PluginSpecificCacheKeySuffix));
	}
	
	static FString BuildCacheKey(FStringView PluginName, FStringView VersionString, FStringView PluginSpecificCacheKeySuffix)
	{
		FString Out;
		Out.Reserve(PluginName.Len() + 1 + VersionString.Len() + 1 + PluginSpecificCacheKeySuffix.Len());
		AppendSanitized(Out, PluginName);
		Out.AppendChar(TEXT('_'));
		AppendSanitized(Out, VersionString);
		Out.AppendChar(TEXT('_'));
		AppendSanitized(Out, PluginSpecificCacheKeySuffix);
		return Out;
	}

	//--------------------
	// Low Level Interface
	//--------------------

	/** 
	 * Synchronously checks the cache and if the item is present, it returns the cached results, otherwise it returns false
	 * @param	CacheKey	Key to identify the data
	 * @return	true if the data was retrieved from the cache
	**/
	UE_DEPRECATED(4.25, "'GetSynchronous' without a DebugContext is deprecated. Please provide a DebugContext!")
	bool GetSynchronous(const TCHAR* CacheKey, TArray<uint8>& OutData) { return GetSynchronous(CacheKey, OutData, TEXT("Unknown Context")); }

	/** 
	 * Starts the async process of checking the cache and if the item is present, retrieving the cached results
	 * @param	CacheKey		Key to identify the data
	 * @return	a handle that can be used for PollAsynchronousCompletion, WaitAsynchronousCompletion and GetAsynchronousResults
	**/
	UE_DEPRECATED(4.25, "'GetAsynchronous' without a DebugContext is deprecated. Please provide a DebugContext!")
	uint32 GetAsynchronous(const TCHAR* CacheKey) { return GetAsynchronous(CacheKey, TEXT("Unknown Context")); }

	/** 
	 * Puts data into the cache. This is fire-and-forget and typically asynchronous.
	 * @param	CacheKey	Key to identify the data
	 * @param	Data		Data to put in the cache under this key
	**/
	UE_DEPRECATED(4.25, "'Put' without a DebugContext is deprecated. Please provide a DebugContext!")
	void Put(const TCHAR* CacheKey, TArray<uint8>& Data, bool bPutEvenIfExists = false) { Put(CacheKey, Data, TEXT("Unknown Context"), bPutEvenIfExists); }

	/** 
	 * Synchronously checks the cache and if the item is present, it returns the cached results, otherwise it returns false.
	 *
	 * Prefer to use a FDerivedDataPluginInterface instead of generating the key directly.
	 *
	 * @param	CacheKey		Key to identify the data.
	 * @param	DebugContext	A string used to describe the data being generated. Typically the path to the object that it is generated from is sufficient.
	 * @return	true if the data was retrieved from the cache.
	**/
	virtual bool GetSynchronous(const TCHAR* CacheKey, TArray<uint8>& OutData, FStringView DebugContext) = 0; 
	virtual bool GetSynchronous(const TCHAR* CacheKey, TArray64<uint8>& OutData, FStringView DebugContext) = 0; 

	/** 
	 * Starts the async process of checking the cache and if the item is present, retrieving the cached results.
	 *
	 * Prefer to use a FDerivedDataPluginInterface instead of generating the key directly.
	 *
	 * @param	CacheKey		Key to identify the data.
	 * @param	DebugContext	A string used to describe the data being generated. Typically the path to the object that it is generated from is sufficient.
	 * @return	A handle that can be used for PollAsynchronousCompletion, WaitAsynchronousCompletion, and GetAsynchronousResults.
	**/
	virtual uint32 GetAsynchronous(const TCHAR* CacheKey, FStringView DebugContext) = 0;

	/** 
	 * Puts data into the cache. This is fire-and-forget and typically asynchronous.
	 *
	 * Prefer to use a FDerivedDataPluginInterface instead of generating the key directly.
	 *
	 * @param	CacheKey	Key to identify the data
	 * @param	Data		Data to put in the cache under this key
	 * @param	DataContext	A string used to describe the data being generated. Typically the path to the object that it is generated from is sufficient.
	**/
	virtual void Put(const TCHAR* CacheKey, TArrayView64<const uint8> Data, FStringView DataContext, bool bPutEvenIfExists = false) = 0;

	/**
	 * Hint that the data associated with the key is transient and may be optionally purged from the cache.
	 * @param	CacheKey	Key that is associated with transient data.
	 */
	virtual void MarkTransient(const TCHAR* CacheKey) = 0;

	/**
	 * Returns true if the data associated with the key is likely to exist in the cache.
	 * Even if this function returns true, a get for this key may still fail!
	 * @param	CacheKey	Key to see if data probably exists.
	 */
	virtual bool CachedDataProbablyExists(const TCHAR* CacheKey) = 0;

	/**
	 * Returns whether the data associated with each key is likely to exist in the cache.
	 * Even if this function returns true, a get for one of the keys may still fail!
	 * @param	CacheKeys	Keys to see if data probably exists.
	 * @return				A bit array with bits indicating whether the data for the corresponding key will probably be found
	 */
	virtual TBitArray<> CachedDataProbablyExistsBatch(TConstArrayView<FString> CacheKeys) = 0;

	/**
	 * Returns true if the data associated with each key is likely to exist in the cache.
	 * Even if this function returns true, a get for one of the keys may still fail!
	 * @param	CacheKeys	Keys to see if data probably exists.
	 */
	virtual bool AllCachedDataProbablyExists(TConstArrayView<FString> CacheKeys) = 0;

	/**
	 * Synchronous attempt to make sure the cached data will be available as optimally as possible.
	 *
	 * @param	CacheKeys		Keys to identify the data.
	 * @param	DebugContext	A string used to describe the data being generated. Typically the path to the object that it is generated from is sufficient.
	 * @return					true if the data will probably be found in a fast backend on a future request.
	 */
	virtual bool TryToPrefetch(TConstArrayView<FString> CacheKeys, FStringView DebugContext) = 0;

	//--------------------
	// System Interface
	//--------------------

	/**
	 * Notify the system that the boot process is complete and so we can write the boot cache and get rid of it
	 */
	virtual void NotifyBootComplete() = 0;

	/**
	 * Adds or subtracts a number from the thread safe counter which tracks outstanding async requests. This is used to ensure everything is complete prior to shutdown.
	 */
	virtual void AddToAsyncCompletionCounter(int32 Addend) = 0;

	/**
	 * Are there any async DDC requests in progress?
	 */
	virtual bool AnyAsyncRequestsRemaining() const = 0;

	/**
	 * Wait for all outstanding async DDC operations to complete.
	 */
	virtual void WaitForQuiescence(bool bShutdown = false) = 0;

	/**
	 * Retrieve the directories used by the DDC
	 */
	virtual void GetDirectories(TArray<FString>& OutResults) = 0;

	/**
	* Retrieve whether a Shared DDC is being used
	*/
	virtual bool GetUsingSharedDDC() const = 0;

	/**
	 * Retrieve the name of the graph used when configuring DDC
	 */
	virtual const TCHAR* GetGraphName() const = 0;

	/**
	 * Retrieve the name of the 'default' graph, which may or may not be the active graph currently.
	 */
	virtual const TCHAR* GetDefaultGraphName() const = 0;

	/**
	 * Are we currently using the default graph configuration?
	 */
	bool IsDefaultGraph() const { return TStringView<TCHAR>(GetGraphName()).Equals(GetDefaultGraphName()); }

	/**
	 * Retrieve the interface to the background cache cleanup.
	 */
	UE_DEPRECATED(5.0, "This has been replaced by UE::DerivedData::GetCache().GetMaintainer().")
	virtual IDDCCleanup* GetCleanup() const = 0;

	//--------------------
	// UsageStats Interface
	//--------------------

	/**
	 * Retrieve usage stats by the DDC
	 */
	virtual void GatherUsageStats(TMap<FString, FDerivedDataCacheUsageStats>& UsageStats) = 0;

	/**
	 * Retrieve usage stats by the DDC
	 */
	UE_DEPRECATED(4.27, "This overload of GatherUsageStats is temporary. Please use other overload.")
	virtual TSharedRef<FDerivedDataCacheStatsNode> GatherUsageStats() const = 0;

	virtual void GatherResourceStats(TArray<FDerivedDataCacheResourceStat>& DDCResourceStats) const = 0;

	virtual void GatherSummaryStats(FDerivedDataCacheSummaryStats& DDCSummaryStats) const = 0;

	//-----------------------
	// Notification Interface
	//-----------------------

	enum EDDCNotification
	{
		// DDC performance notification generated when not using a shared cache
		SharedDDCPerformanceNotification
	};


	DECLARE_MULTICAST_DELEGATE_OneParam(FOnDDCNotification, EDDCNotification);

	/**
	* Retrieve the event delegate for cache put notifications
	*/
	virtual FOnDDCNotification& GetDDCNotificationEvent() = 0;

};
