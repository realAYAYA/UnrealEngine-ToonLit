// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PlayerCore.h"
#include "Containers/List.h"
#include "Player/PlaybackTimeline.h"
#include "Player/AdaptiveStreamingPlayerABR.h"
#include "Player/PlaylistReader.h"
#include "Player/PlayerSessionServices.h"
#include "StreamTypes.h"
#include "ErrorDetail.h"
#include "Parser.h"


namespace Electra
{
class IInitSegmentCacheHLS;
class ILicenseKeyCacheHLS;
class FTimelineMediaAssetHLS;

struct FPlaylistLoadRequestHLS
{
	enum class ELoadType
	{
		Master,
		Initial,
		First,
		Update
	};
	FPlaylistLoadRequestHLS() : LoadType(ELoadType::Master), InternalUniqueID(0), LastUpdateCRC32(~uint32(0)) {}
	FString			URL;
	FTimeValue		RequestedAtTime;
	ELoadType		LoadType;
	uint32			InternalUniqueID;
	uint32			LastUpdateCRC32;
	FString			AdaptationSetUniqueID;
	FString			RepresentationUniqueID;
	FString			CDN;
};


struct FManifestHLSInternal
{
	struct FMediaStream
	{
		enum class EPlaylistType
		{
			Live,
			Event,
			VOD
		};

		struct FByteRange
		{
			FByteRange() : Start(-1), End(-1)
			{
			}
			int64		Start;
			int64		End;
			bool IsSet() const
			{
				return Start >=0 && End >= 0;
			}
			int64 GetStart() const
			{
				return Start;
			}
			int64 GetEnd() const
			{
				return End;
			}
			int64 GetNumBytes() const
			{
				return IsSet() ? End - Start + 1 : 0;
			}
		};

		struct FDRMKeyInfo
		{
			enum class EMethod
			{
				None,
				AES128,
				SampleAES
			};
			FDRMKeyInfo() : Method(EMethod::None) {}
			EMethod				Method;
			FString				URI;
			FString				IV;
			//FString		Keyformat;
			//FString		KeyformatVersions;
		};

		struct FInitSegmentInfo
		{
			FString											URI;
			FByteRange										ByteRange;
			TSharedPtr<FDRMKeyInfo, ESPMode::ThreadSafe>	DRMKeyInfo;			//!< If set the init segment is encrypted with the specified parameters.
		};

		struct FMediaSegment
		{
			FString											URI;
			FByteRange										ByteRange;
			FTimeValue										Duration;
			FTimeValue										RelativeStartTime;	//!< Accumulated durations to give a total start time of this segment relative to the beginning of the playlist.
			FTimeValue										AbsoluteDateTime;	//!< If valid this gives the absolute time of the first sample
			TSharedPtr<FDRMKeyInfo, ESPMode::ThreadSafe>	DRMKeyInfo;			//!< If not set the segment is not encrypted. Otherwise it is with the specified parameters.
			TSharedPtrTS<FInitSegmentInfo>					InitSegmentInfo;	//!< If not set the segment contains the necessary init data. Otherwise this points to the init segment.
			int64											SequenceNumber;
			int64											DiscontinuityCount;
			bool											bIsPrefetch;		//!< Whether or not this is an EXT-X-PREFETCH segment
		};

		struct FStartTime
		{
			FStartTime() : bPrecise(false) {}
			FTimeValue			Offset;
			bool				bPrecise;
		};

		FMediaStream()
			: PlaylistType(EPlaylistType::Live)
			, MediaSequence(0)
			, DiscontinuitySequence(0)
			, bHasListEnd(false)
			, bIsIFramesOnly(false)
			, bHasIndependentSegments(false)
			, bHasEncryptedSegments(false)
		{
		}

	// TODO: replace with a scattered vector!!!!
		TArray<FMediaSegment>			SegmentList;
		FStartTime						StartTime;
		EPlaylistType					PlaylistType;
		FTimeValue						TargetDuration;
		int64							MediaSequence;
		int64							DiscontinuitySequence;
		bool							bHasListEnd;
		bool							bIsIFramesOnly;
		bool							bHasIndependentSegments;
		bool							bHasEncryptedSegments;

		// Internal use
		FTimeRange						SeekableRange;						//!< Current seekable range within the stream.
		FTimeRange						TimelineRange;						//!< Current timeline range of the stream.
		FTimeValue						TotalAccumulatedSegmentDuration;	//!< Accumulated duration of all segments.
		TArray<FTimespan>				SeekablePositions;					//!< List of segment absolute start times
	};


	struct FDenylist
	{
		TSharedPtrTS<HTTP::FRetryInfo>					PreviousAttempts;
		FTimeValue										BecomesAvailableAgainAtUTC;
		IAdaptiveStreamSelector::FDenylistedStream		AssetIDs;
	};


	struct FPlaylistBase
	{
		FPlaylistBase() = default;
		virtual ~FPlaylistBase() = default;

		virtual bool IsVariantStream() const = 0;

		virtual const FString& GetURL() const = 0;

		virtual int32 GetBitrate() const = 0;

		// Internal use.
		struct FInternal
		{
			enum class ELoadState
			{
				NotLoaded,
				Pending,
				Loaded
			};
			FInternal() : LoadState(ELoadState::NotLoaded), UniqueID(0), bReloadTriggered(false), bNewlySelected(true), bHasVideo(false), bHasAudio(false) {}
			FPlaylistLoadRequestHLS			PlaylistLoadRequest;
			TSharedPtrTS<FMediaStream>		MediaStream;
			TSharedPtrTS<FDenylist>		Denylisted;
			FTimeValue						ExpiresAtTime;							//!< Synchronized UTC time (from session service GetSynchronizedUTCTime()) at which this list expires. Set to infinite if it does not.
			ELoadState						LoadState;
			uint32							UniqueID;
			bool							bReloadTriggered;
			bool							bNewlySelected;
			FString							AdaptationSetUniqueID;
			FString							RepresentationUniqueID;
			FString							CDN;
			bool							bHasVideo;
			bool							bHasAudio;
		};
		FInternal			Internal;
	};

	// 4.3.4.1. EXT-X-MEDIA
	struct FRendition : public FPlaylistBase
	{
		FRendition()
			: FPlaylistBase(), Bitrate(0), bDefault(false), bAutoSelect(false), bForced(false)
		{
		}
		virtual bool IsVariantStream() const override
		{
			return false;
		}
		virtual const FString& GetURL() const override
		{
			return URI;
		}
		virtual int32 GetBitrate() const override
		{
			return Bitrate;
		}

		FString		Type;
		FString		GroupID;
		FString		URI;
		FString		Language;
		FString		AssocLanguage;
		FString		Name;
		FString		InStreamID;
		FString		Characteristics;
		FString		Channels;
		int32		Bitrate;
		bool		bDefault;
		bool		bAutoSelect;
		bool		bForced;
	};

	// 4.3.4.2. EXT-X-STREAM-INF
	struct FVariantStream : public FPlaylistBase
	{
		FVariantStream()
			: FPlaylistBase(), Bandwidth(0), AverageBandwidth(0), HDCPLevel(0)
		{
		}
		virtual bool IsVariantStream() const override
		{
			return true;
		}
		virtual const FString& GetURL() const override
		{
			return URI;
		}
		virtual int32 GetBitrate() const override
		{
			return Bandwidth;
		}

		TArray<FStreamCodecInformation>			StreamCodecInformationList;
		FString									URI;
		FString									VideoGroupID;
		FString									AudioGroupID;
		FString									SubtitleGroupID;
		FString									ClosedCaptionGroupID;
		int32									Bandwidth;
		int32									AverageBandwidth;
		int32									HDCPLevel;
	};


	// Internal use
	struct FMasterPlaylistVars
	{
		FMasterPlaylistVars() : PresentationType(IManifest::EType::Live) {}
		FPlaylistLoadRequestHLS								PlaylistLoadRequest;				//!< The master playlist load request. This holds the URL for resolving child playlists.
		IManifest::EType									PresentationType;					//!< Type of the presentation (Live or VOD).
		FTimeValue											PresentationDuration;				//!< Current duration of the presentation. Will be reset when a playlist is refreshed.
		FTimeRange											SeekableRange;						//!< Current seekable range within the presentation. Will be updated when a playlist is refreshed.
		FTimeRange											TimelineRange;						//!< Current media timeline range of the presentation. Will be updated when a playlist is refreshed.
		TArray<FTimespan>									SeekablePositions;					//!< Segment start times.
	};

	FManifestHLSInternal()
		: bHasIndependentSegments(false)
	{
	}


	TMultiMap<FString, TSharedPtrTS<FRendition>>			VideoRenditions;
	TMultiMap<FString, TSharedPtrTS<FRendition>>			AudioRenditions;
	TMultiMap<FString, TSharedPtrTS<FRendition>>			SubtitleRenditions;
	TMultiMap<FString, TSharedPtrTS<FRendition>>			ClosedCaptionRenditions;
	// TODO: if provided we can put the EXT-X-START here
	// TODO: For future DRM support we can put the EXT-X-SESSION-KEY here.

	TArray<TSharedPtrTS<FVariantStream>>					VariantStreams;
	TArray<TSharedPtrTS<FVariantStream>>					AudioOnlyStreams;				//!< Variants that are audio-only (have only audio codec specified in master playlist)
	TArray<TSharedPtrTS<FVariantStream>>					IFrameOnlyStreams;

	TMap<uint32, TWeakPtrTS<FPlaylistBase>>					PlaylistIDMap;
	TMap<int32, int32>										BandwidthToQualityIndex;

	bool													bHasIndependentSegments;

	FMasterPlaylistVars										MasterPlaylistVars;


	FMediaCriticalSection									VariantPlaylistAccessMutex;			//!< This mutex is used to access any(!) of the variant and rendition playlists. The master playlist is immutable.
	TSet<uint32>											ActivelyReferencedStreamIDs;		//!< Unique IDs of streams from which segments are being fetched.
	TSharedPtrTS<IInitSegmentCacheHLS>						InitSegmentCache;
	TSharedPtrTS<ILicenseKeyCacheHLS>						LicenseKeyCache;

	TArray<FTrackMetadata>									TrackMetadataVideo;
	TArray<FTrackMetadata>									TrackMetadataAudio;

	TSharedPtrTS<FTimelineMediaAssetHLS>					CurrentMediaAsset;



	TSharedPtrTS<FPlaylistBase> GetPlaylistForUniqueID(uint32 UniqueID) const
	{
		const TWeakPtrTS<FPlaylistBase>* PlaylistID = PlaylistIDMap.Find(UniqueID);
		if (PlaylistID != nullptr)
		{
			return PlaylistID->Pin();
		}
		return TSharedPtrTS<FManifestHLSInternal::FPlaylistBase>();
	}


	void SelectActiveStreamID(uint32 NewActiveStreamID, uint32 NowInactiveStreamID)
	{
		VariantPlaylistAccessMutex.Lock();
		ActivelyReferencedStreamIDs.Remove(NowInactiveStreamID);

		// Add to selected list only if not deselected (the new stream ID is not 0)
		if (NewActiveStreamID)
		{
			ActivelyReferencedStreamIDs.Add(NewActiveStreamID);

			TSharedPtrTS<FPlaylistBase> Playlist = PlaylistIDMap[NewActiveStreamID].Pin();
			if (Playlist.IsValid())
			{
				Playlist->Internal.bNewlySelected = true;
			}
		}
		VariantPlaylistAccessMutex.Unlock();
	}

	void GetActiveStreams(TArray<TSharedPtrTS<FPlaylistBase>>& OutActiveStreams)
	{
		VariantPlaylistAccessMutex.Lock();
		for(TSet<uint32>::TConstIterator StreamID=ActivelyReferencedStreamIDs.CreateConstIterator(); StreamID; ++StreamID)
		{
			TSharedPtrTS<FPlaylistBase> Playlist = PlaylistIDMap[*StreamID].Pin();
			if (Playlist.IsValid())
			{
				OutActiveStreams.Push(Playlist);
			}
		}
		VariantPlaylistAccessMutex.Unlock();
	}



	void LockPlaylists()
	{
		VariantPlaylistAccessMutex.Lock();
	}
	void UnlockPlaylists()
	{
		VariantPlaylistAccessMutex.Unlock();
	}
	struct ScopedLockPlaylists
	{
		ScopedLockPlaylists(TSharedPtrTS<FManifestHLSInternal> InManifest)
			: Manifest(InManifest)
		{
			if (Manifest.IsValid())
			{
				Manifest->LockPlaylists();
			}
		}
		~ScopedLockPlaylists()
		{
			if (Manifest.IsValid())
			{
				Manifest->UnlockPlaylists();
			}
		}
	private:
		TSharedPtrTS<FManifestHLSInternal> Manifest;
	};
};






class IManifestBuilderHLS
{
public:
	static IManifestBuilderHLS* Create(IPlayerSessionServices* PlayerSessionServices);

	virtual ~IManifestBuilderHLS() = default;


	/**
	 * Builds a new internal manifest from a HLS master playlist.
	 *
	 * @param OutHLSPlaylist
	 * @param Playlist
	 * @param SourceRequest
	 * @param ConnectionInfo
	 *
	 * @return
	 */
	virtual FErrorDetail BuildFromMasterPlaylist(TSharedPtrTS<FManifestHLSInternal>& OutHLSPlaylist, const HLSPlaylistParser::FPlaylist& Playlist, const FPlaylistLoadRequestHLS& SourceRequest, const HTTP::FConnectionInfo* ConnectionInfo) = 0;


	/**
	 * Returns the list of variant and rendition playlists that must be fetched on play start.
	 *
	 * @param OutRequests
	 * @param Manifest
	 *
	 * @return
	 */
	virtual FErrorDetail GetInitialPlaylistLoadRequests(TArray<FPlaylistLoadRequestHLS>& OutRequests, TSharedPtrTS<FManifestHLSInternal> Manifest) = 0;

	/**
	 * Updates an initial playlist load request that failed to load or parse with an
	 * alternative request from a lower bitrate if possible.
	 *
	 * @param InOutFailedRequest
	 *                 Failed request for which an alterative shall be returned.
	 * @param ConnectionInfo
	 * @param PreviousAttempts
	 * @param DenylistUntilUTC
	 * @param Manifest
	 *
	 * @return UEMEDIA_ERROR_OK if an alternative was found or UEMEDIA_ERROR_END_OF_STREAM if no further alternatives exist.
	 */
	virtual UEMediaError UpdateFailedInitialPlaylistLoadRequest(FPlaylistLoadRequestHLS& InOutFailedRequest, const HTTP::FConnectionInfo* ConnectionInfo, TSharedPtrTS<HTTP::FRetryInfo> PreviousAttempts, const FTimeValue& DenylistUntilUTC, TSharedPtrTS<FManifestHLSInternal> Manifest) = 0;


	/**
	 * Updates the internal manifest with a new/refreshed variant playlist.
	 *
	 * @param InOutHLSPlaylist
	 * @param VariantPlaylist
	 * @param SourceRequest
	 * @param ConnectionInfo
	 * @param ResponseCRC
	 *
	 * @return
	 */
	virtual FErrorDetail UpdateFromVariantPlaylist(TSharedPtrTS<FManifestHLSInternal> InOutHLSPlaylist, const HLSPlaylistParser::FPlaylist& VariantPlaylist, const FPlaylistLoadRequestHLS& SourceRequest, const HTTP::FConnectionInfo* ConnectionInfo, uint32 ResponseCRC) = 0;


	/**
	 * Marks a variant as failed.
	 *
	 * @param InHLSPlaylist
	 * @param SourceRequest
	 * @param ConnectionInfo
	 * @param PreviousAttempts
	 * @param DenylistUntilUTC
	 */
	virtual void SetVariantPlaylistFailure(TSharedPtrTS<FManifestHLSInternal> InHLSPlaylist, const FPlaylistLoadRequestHLS& SourceRequest, const HTTP::FConnectionInfo* ConnectionInfo, TSharedPtrTS<HTTP::FRetryInfo> PreviousAttempts, const FTimeValue& DenylistUntilUTC) = 0;
};



class FPlaybackAssetRepresentationHLS : public IPlaybackAssetRepresentation
{
public:
	FPlaybackAssetRepresentationHLS()
	{ }
	virtual ~FPlaybackAssetRepresentationHLS()
	{ }
	FString GetUniqueIdentifier() const override
	{
		return UniqueIdentifier;
	}
	const FStreamCodecInformation& GetCodecInformation() const override
	{
		return CodecInformation;
	}
	int32 GetBitrate() const override
	{
		return Bitrate;
	}
	int32 GetQualityIndex() const override
	{ 
		return QualityIndex;
	}
	bool CanBePlayed() const override
	{
		return true;
	}
	FStreamCodecInformation	CodecInformation;
	FString					UniqueIdentifier;
	int32					Bitrate = 0;
	int32					QualityIndex = 0;
};

class FPlaybackAssetAdaptationSetHLS : public IPlaybackAssetAdaptationSet
{
public:
	virtual ~FPlaybackAssetAdaptationSetHLS()
	{
	}
	FString GetUniqueIdentifier() const override
	{
		return UniqueIdentifier;
	}
	FString GetListOfCodecs() const override
	{
		return Codecs;
	}
	FString GetLanguage() const override
	{
		return Language;
	}
	int32 GetNumberOfRepresentations() const override
	{
		return Representations.Num();
	}
	bool IsLowLatencyEnabled() const override
	{
		return false;
	}
	TSharedPtrTS<IPlaybackAssetRepresentation> GetRepresentationByIndex(int32 RepresentationIndex) const override
	{
		check(RepresentationIndex < Representations.Num());
		if (RepresentationIndex < Representations.Num())
		{
			return Representations[RepresentationIndex];
		}
		return TSharedPtrTS<IPlaybackAssetRepresentation>();
	}
	TSharedPtrTS<IPlaybackAssetRepresentation> GetRepresentationByUniqueIdentifier(const FString& InUniqueIdentifier) const override
	{
		for(int32 i=0, iMax=Representations.Num(); i<iMax; ++i)
		{
			if (Representations[i]->GetUniqueIdentifier() == InUniqueIdentifier)
			{
				return Representations[i];
			}
		}
		return TSharedPtrTS<IPlaybackAssetRepresentation>();
	}

	TArray<TSharedPtrTS<IPlaybackAssetRepresentation>>		Representations;
	FString													UniqueIdentifier;
	FString													Language;
	FString													Codecs;
	FTrackMetadata											Metadata;
};


class FTimelineMediaAssetHLS : public ITimelineMediaAsset
{
public:
	virtual ~FTimelineMediaAssetHLS()
	{
	}
	FTimeRange GetTimeRange() const override
	{
		return TimeRange;
	}
	FTimeValue GetDuration() const override
	{
		return Duration;
	}
	FString GetAssetIdentifier() const override
	{
		return AssetIdentifier;
	}
	FString GetUniqueIdentifier() const override
	{
		return UniqueIdentifier;
	}
	int32 GetNumberOfAdaptationSets(EStreamType OfStreamType) const override
	{
		switch(OfStreamType)
		{
			case EStreamType::Video:
				return VideoAdaptationSet.IsValid() ? 1 : 0;
			case EStreamType::Audio:
				return (int32) AudioAdaptationSets.Num();
			default:
				return 0;
		}
	}
	TSharedPtrTS<IPlaybackAssetAdaptationSet> GetAdaptationSetByTypeAndIndex(EStreamType OfStreamType, int32 AdaptationSetIndex) const override
	{
		switch(OfStreamType)
		{
			case EStreamType::Video:
			{
				check(AdaptationSetIndex == 0);
				return AdaptationSetIndex == 0 ? VideoAdaptationSet : TSharedPtrTS<FPlaybackAssetAdaptationSetHLS>();
			}
			case EStreamType::Audio:
			{
				check(AdaptationSetIndex < AudioAdaptationSets.Num());
				return AdaptationSetIndex < AudioAdaptationSets.Num() ? AudioAdaptationSets[AdaptationSetIndex] : TSharedPtrTS<FPlaybackAssetAdaptationSetHLS>();
			}
			default:
			{
				return TSharedPtrTS<FPlaybackAssetAdaptationSetHLS>();
			}
		}
	}

	void GetMetaData(TArray<FTrackMetadata>& OutMetadata, EStreamType OfStreamType) const override
	{
		switch(OfStreamType)
		{
			case EStreamType::Video:
			{
				if (VideoAdaptationSet.IsValid())
				{
					OutMetadata.Emplace(VideoAdaptationSet->Metadata);
				}
				break;
			}
			case EStreamType::Audio:
			{
				for(int32 i=0, iMax=AudioAdaptationSets.Num(); i<iMax; ++i)
				{
					OutMetadata.Emplace(AudioAdaptationSets[i]->Metadata);
				}
				break;
			}
			default:
			{
				break;
			}
		}
	}

	TSharedPtrTS<IPlaybackAssetAdaptationSet> GetAdaptationSetByTypeAndUniqueIdentifier(EStreamType OfStreamType, const FString& InUniqueIdentifier) const
	{
		switch(OfStreamType)
		{
			case EStreamType::Video:
			{
				return VideoAdaptationSet;
			}
			case EStreamType::Audio:
			{
				for(int32 i=0, iMax=AudioAdaptationSets.Num(); i<iMax; ++i)
				{
					if (AudioAdaptationSets[i]->GetUniqueIdentifier() == InUniqueIdentifier)
					{
						return AudioAdaptationSets[i];
					}
				}
				return TSharedPtrTS<FPlaybackAssetAdaptationSetHLS>();
			}
			default:
			{
				return TSharedPtrTS<FPlaybackAssetAdaptationSetHLS>();
			}
		}
	}

	TSharedPtrTS<FPlaybackAssetAdaptationSetHLS>			VideoAdaptationSet;
	TArray<TSharedPtrTS<FPlaybackAssetAdaptationSetHLS>>	AudioAdaptationSets;
	FString													UniqueIdentifier;
	FString													AssetIdentifier;
	FCriticalSection										UpdateLock;
	FTimeRange												TimeRange;
	FTimeValue												Duration;
	FTimeRange												SeekableTimeRange;
	TArray<FTimespan>										SeekablePositions;
};



} // namespace Electra




