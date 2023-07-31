// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "PlayerCore.h"
#include "PlayerTime.h"
#include "StreamTypes.h"
#include "StreamAccessUnitBuffer.h"
#include "HTTP/HTTPManager.h"
#include "Player/PlaybackTimeline.h"
#include "Player/AdaptiveStreamingPlayerMetrics.h"
#include "Player/PlayerSessionServices.h"
#include "ParameterDictionary.h"


namespace Electra
{

	class IStreamReader;

	enum class EMediaFormatType
	{
		Unknown,
		ISOBMFF,					// mp4
		HLS,						// Apple HLS (HTTP Live Streaming)
		DASH						// MPEG DASH
	};


	struct FPlayStartOptions
	{
		FPlayStartOptions()
		{
			PlaybackRange.Start = FTimeValue::GetZero();
			PlaybackRange.End = FTimeValue::GetPositiveInfinity();
		}
		FTimeRange		PlaybackRange;
		bool			bFrameAccuracy = false;
	};

	struct FPlayStartPosition
	{
		FTimeValue			Time;
		FPlayStartOptions	Options;
	};

	struct FLowLatencyDescriptor
	{
		struct FLatency
		{
			int64 ReferenceID = -1;
			FTimeValue Target;
			FTimeValue Min;
			FTimeValue Max;
		};
		struct FPlayRate
		{
			FTimeValue Min;
			FTimeValue Max;
		};
		FLatency Latency;
		FPlayRate PlayRate;

		FTimeValue GetLatencyMin() const
		{ return Latency.Min; }
		FTimeValue GetLatencyMax() const
		{ return Latency.Max; }
		FTimeValue GetLatencyTarget() const
		{ return Latency.Target; }
		FTimeValue GetPlayrateMin() const
		{ return PlayRate.Min; }
		FTimeValue GetPlayrateMax() const
		{ return PlayRate.Max; }
	};

	struct IProducerReferenceTimeInfo
	{
		enum class EType
		{
			Encoder,
			Captured
		};
		virtual FTimeValue GetWallclockTime() const = 0;
		virtual uint64 GetPresentationTime() const = 0;
		virtual uint32 GetID() const = 0;
		virtual EType GetType() const = 0;
		virtual bool GetIsInband() const = 0;
	};



	class IStreamSegment : public TSharedFromThis<IStreamSegment, ESPMode::ThreadSafe>
	{
	public:
		virtual ~IStreamSegment() = default;

		/**
		 * Sets a playback sequence ID. This is useful to check the ID later in asynchronously received messages
		 * to validate the request still being valid or having become outdated.
		 */
		virtual void SetPlaybackSequenceID(uint32 PlaybackSequenceID) = 0;
		/**
		 * Returns the playback sequence ID. See SetPlaybackSequenceID()
		 */
		virtual uint32 GetPlaybackSequenceID() const = 0;

		virtual void SetExecutionDelay(const FTimeValue& UTCNow, const FTimeValue& ExecutionDelay) = 0;

		virtual FTimeValue GetExecuteAtUTCTime() const = 0;

		virtual EStreamType GetType() const = 0;

		/**
		 * Returns a list of dependent streams.
		 */
		virtual void GetDependentStreams(TArray<TSharedPtrTS<IStreamSegment>>& OutDependentStreams) const = 0;

		/**
		 * Returns a list of requested streams. This must be the initial request that has a list of
		 * all the segments of the streams selected for playback.
		 */
		virtual void GetRequestedStreams(TArray<TSharedPtrTS<IStreamSegment>>& OutRequestedStreams) = 0;

		/**
		 * Returns a list of streams, primary or dependent, that are already at EOS in this request.
		 */
		virtual void GetEndedStreams(TArray<TSharedPtrTS<IStreamSegment>>& OutAlreadyEndedStreams) = 0;

		//! Returns the first PTS value as indicated by the media timeline. This should correspond to the actual absolute PTS of the sample.
		virtual FTimeValue GetFirstPTS() const = 0;

		virtual int32 GetQualityIndex() const = 0;
		virtual int32 GetBitrate() const = 0;

		virtual void GetDownloadStats(Metrics::FSegmentDownloadStats& OutStats) const = 0;

		virtual bool GetStartupDelay(FTimeValue& OutStartTime, FTimeValue& OutTimeIntoSegment, FTimeValue& OutSegmentDuration) const = 0;
	};



	class IManifest : public TMediaNoncopyable<IManifest>
	{
	public:
		enum class EType
		{
			OnDemand,						//!< An on-demand presentation
			Live,							//!< A live presentation
		};

		enum class ESearchType
		{
			Closest,				//!< Find closest match
			After,					//!< Find match only for fragment times >= target time
			Before,					//!< Find match only for fragment times <= target time
			StrictlyAfter,			//!< Match must be strictly after (>). Used to locate the next segment.
			StrictlyBefore,			//!< Match must be strictly before (<). Used to locate the previous segment.
			Same,					//!< Match must be for the same fragment
		};

		class FResult
		{
		public:
			enum class EType
			{
				Found,				//!< Found
				NotFound,			//!< Not found
				PastEOS,			//!< Time is beyond the duration
				BeforeStart,		//!< Time is before the start time
				TryAgainLater,		//!< Not found at the moment. Playlist load may be pending
				NotLoaded,			//!< Not loaded (playlist has not been requested)
			};
			FResult(EType InType = EType::NotFound)
				: Type(InType)
			{
			}
			FResult& RetryAfterMilliseconds(int32 Milliseconds)
			{
				Type = EType::TryAgainLater;
				RetryAgainAtTime = MEDIAutcTime::Current() + FTimeValue().SetFromMilliseconds(Milliseconds);
				return *this;
			}
			FResult& SetErrorDetail(const FErrorDetail& InErrorDetail)
			{
				ErrorDetail = InErrorDetail;
				return *this;
			}
			EType GetType() const
			{
				return Type;
			}
			bool IsSuccess() const
			{
				return GetType() == EType::Found;
			}
			const FTimeValue& GetRetryAgainAtTime() const
			{
				return RetryAgainAtTime;
			}
			const FErrorDetail& GetErrorDetail() const
			{
				return ErrorDetail;
			}
			static const TCHAR* GetTypeName(EType s)
			{
				switch(s)
				{
					case EType::Found:
						return TEXT("Found");
					case EType::NotFound:
						return TEXT("Not found");
					case EType::PastEOS:
						return TEXT("Behind EOF");
					case EType::BeforeStart:
						return TEXT("Before start");
					case EType::TryAgainLater:
						return TEXT("Try again later");
					case EType::NotLoaded:
						return TEXT("Not loaded");
					default:
						return TEXT("undefined");
				}
			}
		protected:
			EType			Type;
			FTimeValue		RetryAgainAtTime;
			FErrorDetail	ErrorDetail;
		};



		virtual ~IManifest() = default;

		//-------------------------------------------------------------------------
		// Presentation related functions
		//
		//! Returns the type of this presentation, either on-demand or live.
		virtual EType GetPresentationType() const = 0;

		//! Returns the low-latency descriptor, if any. May return nullptr if there is none.
		virtual TSharedPtrTS<const FLowLatencyDescriptor> GetLowLatencyDescriptor() const = 0;


		//-------------------------------------------------------------------------
		// Presentation and play time related functions.
		//

		/**
		 * Returns the time value the timeline is anchored at.
		 * Usually this is a UTC timestamp at which the presentation began but it could
		 * literally be anything.
		 * All time values are absolute and have this anchor added in.
		 *
		 * @return The time this presentation timeline is anchored at.
		 */
		virtual FTimeValue GetAnchorTime() const = 0;

		/**
		 * Returns the total time range of the timeline, including the end time of the last sample
		 * to which the player will not be able to seek to.
		 *
		 * @return Time range of assets on the timeline.
		 */
		virtual FTimeRange GetTotalTimeRange() const = 0;

		/**
		 * Returns the seekable range of the timeline, which is a subset of the total
		 * time range. Playback can start at any point in the seekable range with
		 * additional constraints imposed by the format.
		 *
		 * @return Time range in which playback can start.
		 */
		virtual FTimeRange GetSeekableTimeRange() const = 0;

		/**
		 * Returns the playback range on the timeline, which is a subset of the total
		 * time range. This may be set through manifest internal means or by URL fragment
		 * parameters where permissable (eg. example.mp4#t=22,50).
		 * If start or end are not specified they will be set to invalid.
		 * 
		 * @return Optionally set time range to which playback is restricted.
		 */
		virtual FTimeRange GetPlaybackRange() const = 0;

		/**
		 * Returns the timestamps of the segments from the video or audio track (if no video is present).
		 * Segments are required to start with a keyframe and can thus be used to start playback with.
		 */
		virtual void GetSeekablePositions(TArray<FTimespan>& OutPositions) const = 0;

		/**
		 * Returns the duration of the assets on the timeline.
		 * Typically this is the difference of the end and start values of the total time range
		 * unless this is a Live presentation for which the duration will be set to infinite.
		 *
		 * @return Duration of the timeline.
		 */
		virtual FTimeValue GetDuration() const = 0;

		/**
		 * Returns the playback start time as defined by the presentation itself.
		 * If the presentation has no preferred start time an invalid value is returned.
		 */
		virtual FTimeValue GetDefaultStartTime() const = 0;

		/**
		 * Clears the internal default start time so it will not be used again.
		 */
		virtual void ClearDefaultStartTime() = 0;

		//! Returns track metadata. For period based presentations the streams can be different per period in which case the metadata of the first period is returned.
		virtual void GetTrackMetadata(TArray<FTrackMetadata>& OutMetadata, EStreamType StreamType) const = 0;

		//
		virtual FTimeValue GetMinBufferTime() const = 0;

		//
		virtual TSharedPtrTS<IProducerReferenceTimeInfo> GetProducerReferenceTimeInfo(int64 ID) const = 0;

		virtual FTimeValue GetDesiredLiveLatency() const = 0;

		//! Needs to be called when the user has explicitly triggered a seek, including a programmatic loop back to the beginning.
		//! For presentations with dynamic content changes (eg. DASH xlink:onRequest Periods) the content may need to be updated
		//! again. This is different to internal seeking for retry purposes where content will not be re-resolved.
		virtual void UpdateDynamicRefetchCounter() = 0;


		enum class EClockSyncType
		{
			Recommended,
			Required
		};
		virtual void TriggerClockSync(EClockSyncType InClockSyncType) = 0;

		virtual void TriggerPlaylistRefresh() = 0;

		//-------------------------------------------------------------------------
		// Stream fragment reader
		//
		//! Create a stream reader handler suitable for streams from this manifest.
		virtual IStreamReader* CreateStreamReaderHandler() = 0;



		/**
		 * Interface to a playback period.
		 */
		class IPlayPeriod : private TMediaNoncopyable<IPlayPeriod>
		{
		public:
			virtual ~IPlayPeriod() = default;

			virtual void SetStreamPreferences(EStreamType ForStreamType, const FStreamSelectionAttributes& StreamAttributes) = 0;

			enum class EReadyState
			{
				NotLoaded,
				Loading,
				Loaded,
				Preparing,
				IsReady,
			};
			virtual EReadyState GetReadyState() = 0;
			virtual void Load() = 0;
			virtual void PrepareForPlay() = 0;

			//! Returns the bitrate of the default stream (usually the first one specified).
			virtual int64 GetDefaultStartingBitrate() const = 0;

			/**
			 * Returns the selected stream's buffer source information for the specified stream type
			 * after setting the stream preferences via SetStreamPreferences() and a following PrepareToPlay().
			 * Valid only when GetReadyState() returns IsReady.
			 * If a nullptr is returned there is no stream for this type.
			 */
			virtual TSharedPtrTS<FBufferSourceInfo> GetSelectedStreamBufferSourceInfo(EStreamType StreamType) = 0;

			virtual FString GetSelectedAdaptationSetID(EStreamType StreamType) = 0;

			enum class ETrackChangeResult
			{
				Changed,
				NotChanged,
				NewPeriodNeeded
			};

			/**
			 * Changes the specified track over to one of the passed attributes.
			 * If Changed is returned the new selection has been made active and the new FBufferSourceInfo can be fetched
			 * with GetSelectedStreamBufferSourceInfo(). If NotChanged is returned no change has been made, either because
			 * the track is already selected or no (better) match could be made.
			 * If NewPeriodNeeded is returned this play period cannot perform track changing on the fly and a new period
			 * with new stream preferences must be created instead.
			 */
			virtual ETrackChangeResult ChangeTrackStreamPreference(EStreamType ForStreamType, const FStreamSelectionAttributes& StreamAttributes) = 0;

			/**
			 * Returns the media asset for this play period.
			 *
			 * @return Pointer to the media asset.
			 */
			virtual TSharedPtrTS<ITimelineMediaAsset> GetMediaAsset() const = 0;

			/**
			 * Selects a specific stream to be used for all next segment downloads.
			 *
			 * @param AdaptationSetID
			 * @param RepresentationID
			 */
			virtual void SelectStream(const FString& AdaptationSetID, const FString& RepresentationID) = 0;

			struct FInitSegmentPreload
			{
				FString AdaptationSetID;
				FString RepresentationID;
			};
			/**
			 * Triggers pre-loading of initialization segments.
			 * This may be called multiple times with different streams. The implementation needs to keep track
			 * of which init segments have already been loaded.
			 */
			virtual void TriggerInitSegmentPreload(const TArray<FInitSegmentPreload>& InitSegmentsToPreload) = 0;

			/**
			 * Sets up a starting segment request to begin playback at the specified time.
			 * The streams selected through SelectStream() will be used.
			 *
			 * @param OutSegment
			 * @param InSequenceState
			 * @param StartPosition
			 * @param SearchType
			 *
			 * @return
			 */
			virtual FResult GetStartingSegment(TSharedPtrTS<IStreamSegment>& OutSegment, const FPlayerSequenceState& InSequenceState, const FPlayStartPosition& StartPosition, ESearchType SearchType) = 0;


			/**
			 * Same as GetStartingSegment() except this is for a specific stream (video, audio, ...) only.
			 * To be used when a track (language) change is made and a new segment is needed at the current playback position.
			 */
			virtual FResult GetContinuationSegment(TSharedPtrTS<IStreamSegment>& OutSegment, EStreamType StreamType, const FPlayerSequenceState& SequenceState, const FPlayStartPosition& StartPosition, ESearchType SearchType) = 0;

			/**
			 * Sets up a starting segment request to loop playback to.
			 * The streams selected through SelectStream() will be used.
			 *
			 * @param OutSegment
			 * @param InSequenceState
			 * @param StartPosition
			 * @param SearchType
			 *
			 * @return
			 */
			virtual FResult GetLoopingSegment(TSharedPtrTS<IStreamSegment>& OutSegment, const FPlayerSequenceState& InSequenceState, const FPlayStartPosition& StartPosition, ESearchType SearchType) = 0;

			/**
			 * Gets the segment request for the segment following the specified earlier request.
			 *
			 * @param OutSegment
			 * @param CurrentSegment
			 * @param Options
			 *
			 * @return
			 */
			virtual FResult GetNextSegment(TSharedPtrTS<IStreamSegment>& OutSegment, TSharedPtrTS<const IStreamSegment> CurrentSegment, const FPlayStartOptions& Options) = 0;

			/**
			 * Gets the segment request for the same segment on a different quality level or CDN.
			 *
			 * @param OutSegment
			 * @param CurrentSegment
			 * @param Options
			 * @param bReplaceWithFillerData
			 *
			 * @return
			 */
			virtual FResult GetRetrySegment(TSharedPtrTS<IStreamSegment>& OutSegment, TSharedPtrTS<const IStreamSegment> CurrentSegment, const FPlayStartOptions& Options, bool bReplaceWithFillerData) = 0;

			/**
			 * Called by the ABR to increase the delay in fetching the next segment in case the segment returned a 404 when fetched at
			 * the announced availability time. This may reduce 404's on the next segment fetches.
			 *
			 * @param IncreaseAmount
			 */
			virtual void IncreaseSegmentFetchDelay(const FTimeValue& IncreaseAmount) = 0;

			struct FSegmentInformation
			{
				FTimeValue	Duration;
				int64		ByteSize = 0;
			};

			/**
			 * Obtains information on the stream segmentation of a particular stream starting at a given current reference segment (optional, if not given returns suitable default values).
			 *
			 * @param OutSegmentInformation
			 * @param OutAverageSegmentDuration
			 * @param CurrentSegment
			 * @param LookAheadTime
			 * @param AdaptationSetID
			 * @param RepresentationID
			 */
			virtual void GetSegmentInformation(TArray<FSegmentInformation>& OutSegmentInformation, FTimeValue& OutAverageSegmentDuration, TSharedPtrTS<const IStreamSegment> CurrentSegment, const FTimeValue& LookAheadTime, const FString& AdaptationSetID, const FString& RepresentationID) = 0;
		};

		//! Finds the playback period the specified start time falls into.
		virtual FResult FindPlayPeriod(TSharedPtrTS<IPlayPeriod>& OutPlayPeriod, const FPlayStartPosition& StartPosition, ESearchType SearchType) = 0;

		//! Locates the period following the given segment.
		virtual FResult FindNextPlayPeriod(TSharedPtrTS<IPlayPeriod>& OutPlayPeriod, TSharedPtrTS<const IStreamSegment> CurrentSegment) = 0;
	};



} // namespace Electra


