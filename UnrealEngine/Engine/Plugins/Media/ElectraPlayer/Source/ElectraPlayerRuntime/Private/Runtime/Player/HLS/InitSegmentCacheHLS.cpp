// Copyright Epic Games, Inc. All Rights Reserved.

#include "InitSegmentCacheHLS.h"
#include "Player/PlayerSessionServices.h"
#include "Demuxer/ParserISO14496-12.h"
#include "Containers/LruCache.h"

namespace Electra
{


class FInitSegmentCacheHLS : public IInitSegmentCacheHLS
{
public:
	FInitSegmentCacheHLS(IPlayerSessionServices* SessionServices);
	virtual ~FInitSegmentCacheHLS();
	virtual TSharedPtrTS<const IParserISO14496_12> GetInitSegmentFor(const TSharedPtrTS<const FManifestHLSInternal::FMediaStream::FInitSegmentInfo>& InitSegmentInfo) override;
	virtual void AddInitSegment(const TSharedPtrTS<const IParserISO14496_12>& InitSegment, const TSharedPtrTS<const FManifestHLSInternal::FMediaStream::FInitSegmentInfo>& InitSegmentInfo, const FTimeValue& ExpiresAtUTC) override;
private:

	static const uint32 kMaxCacheEntries = 32;

	IPlayerSessionServices* SessionServices;

	struct FEntry
	{
		FManifestHLSInternal::FMediaStream::FInitSegmentInfo	SegmentInfo;
		TSharedPtrTS<const IParserISO14496_12>					ParsedData;
		FTimeValue												ExpiresAt;
	};

	FCriticalSection						Lock;
	TLruCache<FString, TSharedPtr<FEntry>> 	Cache;
};

/***************************************************************************************************************************************************/
/***************************************************************************************************************************************************/
/***************************************************************************************************************************************************/

IInitSegmentCacheHLS* IInitSegmentCacheHLS::Create(IPlayerSessionServices* SessionServices)
{
	return new FInitSegmentCacheHLS(SessionServices);
}

/***************************************************************************************************************************************************/
/***************************************************************************************************************************************************/
/***************************************************************************************************************************************************/

FInitSegmentCacheHLS::FInitSegmentCacheHLS(IPlayerSessionServices* InSessionServices)
	: SessionServices(InSessionServices)
	, Cache(kMaxCacheEntries)
{
}

FInitSegmentCacheHLS::~FInitSegmentCacheHLS()
{
	FScopeLock ScopeLock(&Lock);
	Cache.Empty();
}

TSharedPtrTS<const IParserISO14496_12> FInitSegmentCacheHLS::GetInitSegmentFor(const TSharedPtrTS<const FManifestHLSInternal::FMediaStream::FInitSegmentInfo>& InitSegmentInfo)
{
	FScopeLock ScopeLock(&Lock);
	const TSharedPtr<FEntry>* CacheEntry = Cache.FindAndTouch(InitSegmentInfo->URI);
	if (CacheEntry)
	{
		return (*CacheEntry)->ParsedData;
	}
	else
	{
		return TSharedPtrTS<const IParserISO14496_12>();
	}

}

void FInitSegmentCacheHLS::AddInitSegment(const TSharedPtrTS<const IParserISO14496_12>& InitSegment, const TSharedPtrTS<const FManifestHLSInternal::FMediaStream::FInitSegmentInfo>& InitSegmentInfo, const FTimeValue& ExpiresAtUTC)
{
	TSharedPtr<FEntry> Entry = MakeShared<FEntry>();
	Entry->ParsedData  = InitSegment;
	Entry->SegmentInfo = *InitSegmentInfo;
	Entry->ExpiresAt   = ExpiresAtUTC;
	FScopeLock ScopeLock(&Lock);
	Cache.Add(InitSegmentInfo->URI, Entry);
}


} // namespace Electra

