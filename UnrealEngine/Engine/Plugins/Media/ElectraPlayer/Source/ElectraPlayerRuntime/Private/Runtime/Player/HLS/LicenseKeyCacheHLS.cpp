// Copyright Epic Games, Inc. All Rights Reserved.

#include "LicenseKeyCacheHLS.h"
#include "Player/PlayerSessionServices.h"
#include "Containers/LruCache.h"

namespace Electra
{


class FLicenseKeyCacheHLS : public ILicenseKeyCacheHLS
{
public:
	FLicenseKeyCacheHLS(IPlayerSessionServices* SessionServices);
	virtual ~FLicenseKeyCacheHLS();

	virtual TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe> GetLicenseKeyFor(const TSharedPtr<const FManifestHLSInternal::FMediaStream::FDRMKeyInfo, ESPMode::ThreadSafe>& LicenseKeyInfo) override;
	virtual void AddLicenseKey(const TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe>& LicenseKey, const TSharedPtr<const FManifestHLSInternal::FMediaStream::FDRMKeyInfo, ESPMode::ThreadSafe>& LicenseKeyInfo, const FTimeValue& ExpiresAtUTC) override;

private:

	static const uint32 kMaxCacheEntries = 32;

	IPlayerSessionServices* SessionServices;

	struct FEntry
	{
		FManifestHLSInternal::FMediaStream::FDRMKeyInfo		LicenseKeyInfo;
		TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe>		LicenseKeyData;
		FTimeValue											ExpiresAt;
	};

	FCriticalSection					   	Lock;
	TLruCache<FString, TSharedPtr<FEntry>> 	Cache;
};

/***************************************************************************************************************************************************/
/***************************************************************************************************************************************************/
/***************************************************************************************************************************************************/

ILicenseKeyCacheHLS* ILicenseKeyCacheHLS::Create(IPlayerSessionServices* SessionServices)
{
	return new FLicenseKeyCacheHLS(SessionServices);
}

/***************************************************************************************************************************************************/
/***************************************************************************************************************************************************/
/***************************************************************************************************************************************************/

FLicenseKeyCacheHLS::FLicenseKeyCacheHLS(IPlayerSessionServices* InSessionServices)
	: SessionServices(InSessionServices)
	, Cache(kMaxCacheEntries)
{
}

FLicenseKeyCacheHLS::~FLicenseKeyCacheHLS()
{
	FScopeLock ScopeLock(&Lock);
	Cache.Empty();
}

TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe> FLicenseKeyCacheHLS::GetLicenseKeyFor(const TSharedPtr<const FManifestHLSInternal::FMediaStream::FDRMKeyInfo, ESPMode::ThreadSafe>& LicenseKeyInfo)
{
	FScopeLock ScopeLock(&Lock);
	const TSharedPtr<FEntry>* CacheEntry = Cache.FindAndTouch(LicenseKeyInfo->URI);
	if (CacheEntry)
	{
		return (*CacheEntry)->LicenseKeyData;
	}
	else
	{
		return nullptr;
	}
}

void FLicenseKeyCacheHLS::AddLicenseKey(const TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe>& LicenseKey, const TSharedPtr<const FManifestHLSInternal::FMediaStream::FDRMKeyInfo, ESPMode::ThreadSafe>& LicenseKeyInfo, const FTimeValue& ExpiresAtUTC)
{
	TSharedPtr<FEntry> Entry = MakeShared<FEntry>();
	Entry->LicenseKeyData  = LicenseKey;
	Entry->LicenseKeyInfo  = *LicenseKeyInfo;
	Entry->ExpiresAt	   = ExpiresAtUTC;
	FScopeLock ScopeLock(&Lock);
	Cache.Add(LicenseKeyInfo->URI, Entry);
}


} // namespace Electra

