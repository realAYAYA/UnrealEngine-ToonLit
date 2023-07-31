// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Player/Manifest.h"

namespace Electra
{
struct FManifestHLSInternal;
class IPlaylistReaderHLS;


class FManifestHLS : public IManifest
{
public:
	static TSharedPtrTS<FManifestHLS> Create(IPlayerSessionServices* SessionServices, IPlaylistReaderHLS* PlaylistReader, TSharedPtrTS<FManifestHLSInternal> Manifest);

	virtual ~FManifestHLS();
	EType GetPresentationType() const override;
	TSharedPtrTS<const FLowLatencyDescriptor> GetLowLatencyDescriptor() const override;
	FTimeValue GetAnchorTime() const override;
	FTimeRange GetTotalTimeRange() const override;
	FTimeRange GetSeekableTimeRange() const override;
	FTimeRange GetPlaybackRange() const override;
	void GetSeekablePositions(TArray<FTimespan>& OutPositions) const override;
	FTimeValue GetDuration() const override;
	FTimeValue GetDefaultStartTime() const override;
	void ClearDefaultStartTime() override;
	FTimeValue GetMinBufferTime() const override;
	FTimeValue GetDesiredLiveLatency() const override;
	TSharedPtrTS<IProducerReferenceTimeInfo> GetProducerReferenceTimeInfo(int64 ID) const override;
	void GetTrackMetadata(TArray<FTrackMetadata>& OutMetadata, EStreamType StreamType) const override;
	void UpdateDynamicRefetchCounter() override;
	void TriggerClockSync(EClockSyncType InClockSyncType) override;
	void TriggerPlaylistRefresh() override;
	IStreamReader *CreateStreamReaderHandler() override;

	FResult FindPlayPeriod(TSharedPtrTS<IPlayPeriod>& OutPlayPeriod, const FPlayStartPosition& StartPosition, ESearchType SearchType) override;
	FResult FindNextPlayPeriod(TSharedPtrTS<IPlayPeriod>& OutPlayPeriod, TSharedPtrTS<const IStreamSegment> CurrentSegment) override;

private:
	FManifestHLS(IPlayerSessionServices* SessionServices, IPlaylistReaderHLS* PlaylistReader, TSharedPtrTS<FManifestHLSInternal> Manifest);

	TSharedPtrTS<FManifestHLSInternal>			InternalManifest;
	IPlayerSessionServices* 					SessionServices;
	IPlaylistReaderHLS*							PlaylistReader;

};



} // namespace Electra


