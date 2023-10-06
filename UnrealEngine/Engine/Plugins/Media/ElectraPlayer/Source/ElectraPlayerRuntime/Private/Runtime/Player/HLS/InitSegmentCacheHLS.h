// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PlayerCore.h"
#include "Player/Manifest.h"
#include "ManifestBuilderHLS.h"

namespace Electra
{
class IPlayerSessionServices;
class IParserISO14496_12;



class IInitSegmentCacheHLS
{
public:
	static IInitSegmentCacheHLS* Create(IPlayerSessionServices* SessionServices);

	virtual ~IInitSegmentCacheHLS() = default;

	virtual TSharedPtrTS<const IParserISO14496_12> GetInitSegmentFor(const TSharedPtrTS<const FManifestHLSInternal::FMediaStream::FInitSegmentInfo>& InitSegmentInfo) = 0;

	virtual void AddInitSegment(const TSharedPtrTS<const IParserISO14496_12>& InitSegment, const TSharedPtrTS<const FManifestHLSInternal::FMediaStream::FInitSegmentInfo>& InitSegmentInfo, const FTimeValue& ExpiresAtUTC) = 0;
};


} // namespace Electra

