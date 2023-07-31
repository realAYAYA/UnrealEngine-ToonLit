// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Templates/SharedPointer.h"
#include "Modules/ModuleInterface.h"
#include "Misc/Timespan.h"
#include "IMediaOverlaySample.h"

/**
 * Extension of the media overlay sample to carry Electra
 * specific values.
 */
class IElectraSubtitleSample : public IMediaOverlaySample
{
public:
	static FGuid GetSampleTypeGUID()
	{
		static FGuid SampleTypeGUID(0xC94C9B6F, 0x07DF4B88, 0xACE0B08A, 0x9EF6AA39);
		return SampleTypeGUID;
	}

	virtual ~IElectraSubtitleSample() = default;

	virtual FTimespan GetDuration() const = 0;
	virtual TOptional<FVector2D> GetPosition() const = 0;
	virtual FText GetText() const = 0;
	virtual FMediaTimeStamp GetTime() const = 0;
	//virtual TOptional<FTimecode> GetTimecode() const { return TOptional<FTimecode>(); }
	virtual EMediaOverlaySampleType GetType() const = 0;

	virtual FGuid GetGUID() const = 0;

};
