// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PlayerCore.h"
#include "StreamTypes.h"
#include "InfoLog.h"
#include "Player/AdaptiveStreamingPlayerABR_State.h"
#include "Player/Playlist.h"
#include "ElectraHTTPStream.h"

namespace Electra
{

namespace Metrics
{
	enum class ESegmentType
	{
		Init,
		Media
	};
	static const TCHAR * const GetSegmentTypeString(ESegmentType SegmentType)
	{
		switch(SegmentType)
		{
			case ESegmentType::Init:
				return TEXT("Init");
			case ESegmentType::Media:
				return TEXT("Media");
		}
		return TEXT("n/a");
	}

	enum class EBufferingReason
	{
		Initial,
		Seeking,
		Rebuffering,
	};
	static const TCHAR * const GetBufferingReasonString(EBufferingReason BufferingReason)
	{
		switch(BufferingReason)
		{
			case EBufferingReason::Initial:
				return TEXT("Initial");
			case EBufferingReason::Seeking:
				return TEXT("Seeking");
			case EBufferingReason::Rebuffering:
				return TEXT("Rebuffering");
		}
		return TEXT("n/a");
	}

	enum class ETimeJumpReason
	{
		UserSeek,
		FellOffTimeline,
		FellBehindWallclock,
		Looping
	};
	static const TCHAR * const GetTimejumpReasonString(ETimeJumpReason TimejumpReason)
	{
		switch(TimejumpReason)
		{
			case ETimeJumpReason::UserSeek:
				return TEXT("User seek");
			case ETimeJumpReason::FellOffTimeline:
				return TEXT("Fell off timeline");
			case ETimeJumpReason::FellBehindWallclock:
				return TEXT("Fell behind wallclock");
			case ETimeJumpReason::Looping:
				return TEXT("Looping");
		}
		return TEXT("n/a");
	}

	struct FBufferStats
	{
		FBufferStats()
		{
			BufferType  		 = EStreamType::Video;
			MaxDurationInSeconds = 0.0;
			DurationInUse   	 = 0.0;
			MaxByteCapacity 	 = 0;
			BytesInUse  		 = 0;
		}
		EStreamType		BufferType;
		double			MaxDurationInSeconds;
		double			DurationInUse;
		int64			MaxByteCapacity;
		int64			BytesInUse;
	};

	struct FPlaylistDownloadStats
	{
		FPlaylistDownloadStats()
		{
			ListType	   = Playlist::EListType::Master;
			LoadType	   = Playlist::ELoadType::Initial;
			HTTPStatusCode = 0;
			RetryNumber    = 0;
			bWasSuccessful = false;
		}

		FString					URL;
		//FString				CDN;							// do not have this yet
		FString					FailureReason;					//!< Human readable failure reason. Only for display purposes.
		Playlist::EListType		ListType;
		Playlist::ELoadType		LoadType;
		int32					HTTPStatusCode;					//!< HTTP status code (0 if not connected to server yet)
		int32					RetryNumber;
		bool					bWasSuccessful;
	};

	struct FSegmentDownloadStats
	{
		// Inputs from stream request
		EStreamType		StreamType = EStreamType::Video;	//!< Type of stream
		ESegmentType	SegmentType = ESegmentType::Media;	//!< Type of segment (init or media)
		FString			URL;								//!< Effective URL used to download from
		FString			Range;								//!< Range used to download
		FString			CDN;								//!< CDN
		FString			MediaAssetID;
		FString			AdaptationSetID;
		FString			RepresentationID;
		double			PresentationTime = 0.0;				//!< Presentation time on media timeline
		double			Duration = 0.0;						//!< Duration of segment as specified in manifest
		int32			Bitrate = 0;						//!< Stream bitrate as specified in manifest
		int32			RetryNumber = 0;
		bool			bIsMissingSegment = false;			//!< true if the segment was not actually downloaded because it is missing on the timeline.

		// Outputs from stream reader
		uint32			StatsID = 0;						//!< ID uniquely identifying this download
		FString			FailureReason;						//!< Human readable failure reason. Only for display purposes.
		double			AvailibilityDelay = 0.0;			//!< Time the download had to wait for the segment to enter its availability window.
		double			DurationDownloaded = 0.0;			//!< Duration of content successfully downloaded. May be less than Duration in case of errors.
		double			DurationDelivered = 0.0;			//!< Duration of content delivered to buffer. If larger than DurationDownloaded indicates dummy data was inserted into buffer.
		double			TimeToFirstByte = 0.0;				//!< Time in seconds until first data byte was received
		double			TimeToDownload = 0.0;				//!< Total time in seconds for entire download
		int64			ByteSize = 0;						//!< Content-Length, may be -1 if unknown (either on error or chunked transfer)
		int64			NumBytesDownloaded = 0;				//!< Number of bytes successfully downloaded.
		int32			HTTPStatusCode = 0 ;				//!< HTTP status code (0 if not connected to server yet)
		bool			bWasSuccessful = false;				//!< true if download was successful, false if not
		bool			bWasAborted = false;				//!< true if download was aborted by ABR (not by playback!)
		bool			bDidTimeout = false;				//!< true if a timeout occurred. Only set if timeouts are enabled. Usually the ABR will monitor and abort.
		bool			bParseFailure = false;				//!< true if the segment could not be parsed
		bool			bInsertedFillerData = false;
		bool			bIsCachedResponse = false;

		// Chunk timing
		struct FMovieChunkInfo
		{
			int64 HeaderOffset = 0;
			int64 PayloadStartOffset = 0;
			int64 PayloadEndOffset = 0;
			int64 NumKeyframeBytes = 0;
			FTimeValue ContentDuration;
			FMovieChunkInfo() { ContentDuration = FTimeValue::GetZero(); }
		};
		TArray<IElectraHTTPStreamResponse::FTimingTrace> TimingTraces;
		TArray<FMovieChunkInfo> MovieChunkInfos;
	};

	struct FLicenseKeyStats
	{
		FLicenseKeyStats()
		{
			bWasSuccessful = false;
		}
		FString			URL;
		FString			FailureReason;					//!< Human readable failure reason. Only for display purposes.
		bool			bWasSuccessful;
	};

	struct FDataAvailabilityChange
	{
		enum class EAvailability
		{
			DataAvailable,
			DataNotAvailable
		};
		FDataAvailabilityChange()
			: StreamType(EStreamType::Unsupported), Availability(EAvailability::DataNotAvailable)
		{ }

		EStreamType		StreamType;						//!< Type of stream
		EAvailability	Availability;
	};

} // namespace Metrics


/**
 *
**/
class IAdaptiveStreamingPlayerMetrics
{
public:
	virtual ~IAdaptiveStreamingPlayerMetrics() = default;

	//=================================================================================================================
	// Methods called from the media player.
	//

	/**
	 * Called when the source will be opened.
	 */
	virtual void ReportOpenSource(const FString& URL) = 0;

	/**
	 * Called when the source's master playlist has beed loaded successfully.
	 */
	virtual void ReportReceivedMasterPlaylist(const FString& EffectiveURL) = 0;

	/**
	 * Called when the dependent child playlists have been loaded successfully.
	 */
	virtual void ReportReceivedPlaylists() = 0;

	/**
	 * Called when the available tracks or their properties have changed.
	 */
	virtual void ReportTracksChanged() = 0;

	/**
	 * Called at the end of every downloaded playlist.
	 */
	virtual void ReportPlaylistDownload(const Metrics::FPlaylistDownloadStats& PlaylistDownloadStats) = 0;

	/**
	 * Called when the player starts over from a clean state.
	 */
	virtual void ReportCleanStart() = 0;

	/**
	 * Called when buffering of data begins.
	 */
	virtual void ReportBufferingStart(Metrics::EBufferingReason BufferingReason) = 0;

	/**
	 * Called when buffering of data ends.
	 * This does not necessarily coincide with a segment download as buffering ends as soon as
	 * sufficient data has been received to start/resume playback.
	 */
	virtual void ReportBufferingEnd(Metrics::EBufferingReason BufferingReason) = 0;

	/**
	 * Called at the end of each downloaded video segment.
	 * The order will be ReportSegmentDownload() followed by ReportBandwidth()
	 */
	virtual void ReportBandwidth(int64 EffectiveBps, int64 ThroughputBps, double LatencyInSeconds) = 0;

	/**
	 * Called before and after a segment download.
	 */
	virtual void ReportBufferUtilization(const Metrics::FBufferStats& BufferStats) = 0;

	/**
	 * Called at the end of each downloaded segment.
	 */
	virtual void ReportSegmentDownload(const Metrics::FSegmentDownloadStats& SegmentDownloadStats) = 0;

	/**
	 * Called for license key events.
	 */
	virtual void ReportLicenseKey(const Metrics::FLicenseKeyStats& LicenseKeyStats) = 0;

	/**
	 * Called when a new video stream segment is fetched at a different bitrate than before.
	 * A drastic change is one where quality _drops_ more than _one_ level.
	 */
	virtual void ReportVideoQualityChange(int32 NewBitrate, int32 PreviousBitrate, bool bIsDrasticDownswitch) = 0;

	/**
	 * Called when stream data availability changes when feeding the decoder.
	 */
	virtual void ReportDataAvailabilityChange(const Metrics::FDataAvailabilityChange& DataAvailability) = 0;

	/**
	 * Called when the format of the stream being decoded changes in some way.
	 */
	virtual void ReportDecodingFormatChange(const FStreamCodecInformation& NewDecodingFormat) = 0;

	/**
	 * Called when decoders start to decode first data to pre-roll the pipeline.
	 */
	virtual void ReportPrerollStart() = 0;

	/**
	 * Called when enough initial data has been decoded to pre-roll the pipeline.
	 */
	virtual void ReportPrerollEnd() = 0;

	/**
	 * Called when playback starts for the first time. This is not called after resuming a paused playback.
	 * If playback has ended(!) and is then begun again by seeking back to an earlier point in time this
	 * callback will be triggered again. Seeking during playback will not cause this callback.
	 */
	virtual void ReportPlaybackStart() = 0;

	/**
	 * Called when the player enters pause mode, either through user request or an internal state change.
	 */
	virtual void ReportPlaybackPaused() = 0;

	/**
	 * Called when the player resumes from pause mode, either through user request or an internal state change.
	 */
	virtual void ReportPlaybackResumed() = 0;

	/**
	 * Called when playback has reached the end. See ReportPlaybackStart().
	 */
	virtual void ReportPlaybackEnded() = 0;

	/**
	 * Called when the play position jumps either because of a user induced seek or because the play position fell off the timeline
	 * or because a wallclock synchronized Live playback fell too far behind the actual time.
	 */
	virtual void ReportJumpInPlayPosition(const FTimeValue& ToNewTime, const FTimeValue& FromTime, Metrics::ETimeJumpReason TimejumpReason) = 0;

	/**
	 * Called when playback is terminally stopped.
	 */
	virtual void ReportPlaybackStopped() = 0;

	/**
	 * Called when a seek has completed such that the first new data is ready.
	 * Future data may still be buffering.
	 */
	virtual void ReportSeekCompleted() = 0;

	/**
	 * Called when an error occurs. Errors always result in termination of playback.
	 */
	virtual void ReportError(const FString& ErrorReason) = 0;

	/**
	 * Called to output a log message. The log level is a player internal level.
	 */
	virtual void ReportLogMessage(IInfoLog::ELevel LogLevel, const FString& LogMessage, int64 PlayerWallclockMilliseconds) = 0;


	//=================================================================================================================
	// Methods called from the renderers.
	//

	virtual void ReportDroppedVideoFrame() = 0;
	virtual void ReportDroppedAudioFrame() = 0;
};


} // namespace Electra


