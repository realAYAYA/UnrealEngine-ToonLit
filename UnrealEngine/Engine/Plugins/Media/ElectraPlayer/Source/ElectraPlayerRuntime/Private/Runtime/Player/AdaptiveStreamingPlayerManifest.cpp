// Copyright Epic Games, Inc. All Rights Reserved.

#include "PlayerCore.h"
#include "Misc/Paths.h"
#include "Player/AdaptiveStreamingPlayerInternal.h"
#include "Player/AdaptivePlayerOptionKeynames.h"
#include "Player/HLS/PlaylistReaderHLS.h"
#include "Player/mp4/PlaylistReaderMP4.h"
#include "Player/DASH/PlaylistReaderDASH.h"
#include "Player/mkv/PlaylistReaderMKV.h"
#include "Utilities/Utilities.h"
#include "Utilities/StringHelpers.h"
#include "Utilities/URLParser.h"
#include "ParameterDictionary.h"


namespace Electra
{

namespace Playlist
{
static const FString MIMETypeMP4(TEXT("video/mp4"));
static const FString MIMETypeMP4A(TEXT("audio/mp4"));
static const FString MIMETypeQuickTime(TEXT("video/quicktime"));
static const FString MIMETypeHLS(TEXT("application/vnd.apple.mpegURL"));
static const FString MIMETypeDASH(TEXT("application/dash+xml"));
static const FString MIMETypeMKV(TEXT("video/x-matroska"));
static const FString MIMETypeMKA(TEXT("audio/x-matroska"));



/**
 * Fix common mistakes in a file:// URL like backslashes instead of forward slashes
 * and spaces that are not escaped.
 */
FString FixLocalFileSchemeURL(const FString& InURL)
{
	FString URL(InURL);
	if (URL.StartsWith("file:", ESearchCase::IgnoreCase))
	{
		URL.ReplaceCharInline(TCHAR('\\'), TCHAR('/'));
		URL.ReplaceInline(TEXT(" "), TEXT("%20"));
	}
	return URL;
}

/**
 * Returns the MIME type of the media playlist pointed to by the given URL.
 * This only parses the URL for known extensions or scheme.
 * If the MIME type cannot be precisely determined an empty string is returned.
 *
 * @param URL    Media playlist URL
 *
 * @return MIME type or empty string if it cannot be determined
 */
FString GetMIMETypeForURL(const FString& URL)
{
	FString MimeType;
	FURL_RFC3986 UrlParser;
	if (!UrlParser.Parse(URL))
	{
		return MimeType;
	}

	//	FString PathOnly = UrlParser->GetPath();
	TArray<FString> PathComponents;
	UrlParser.GetPathComponents(PathComponents);
	if (PathComponents.Num())
	{
		FString LowerCaseExtension = FPaths::GetExtension(PathComponents.Last().ToLower());

		// Check for known extensions.
		static const FString kTextMP4(TEXT("mp4"));
		static const FString kTextMP4V(TEXT("m4v"));
		static const FString kTextMP4A(TEXT("m4a"));
		static const FString kTextMOV(TEXT("mov"));
		static const FString kTextMPD(TEXT("mpd"));
		static const FString kTextM3U8(TEXT("m3u8"));
		static const FString kTextMKV(TEXT("mkv"));
		static const FString kTextMKA(TEXT("mka"));
		static const FString kTextWEBM(TEXT("webm"));
		if (LowerCaseExtension == kTextMP4 || LowerCaseExtension == kTextMP4V)
		{
			MimeType = MIMETypeMP4;
		}
		else if (LowerCaseExtension == kTextMP4A)
		{
			MimeType = MIMETypeMP4A;
		}
		else if (LowerCaseExtension == kTextMOV)
		{
			MimeType = MIMETypeQuickTime;
		}
		else if (LowerCaseExtension == kTextMPD)
		{
			MimeType = MIMETypeDASH;
		}
		else if (LowerCaseExtension == kTextM3U8)
		{
			MimeType = MIMETypeHLS;
		}
		else if (LowerCaseExtension == kTextMKV || LowerCaseExtension == kTextWEBM)
		{
			MimeType = MIMETypeMKV;
		}
		else if (LowerCaseExtension == kTextMKA)
		{
			MimeType = MIMETypeMKA;
		}
	}

	return MimeType;
}


} // namespace Playlist







void FAdaptiveStreamingPlayer::OnManifestGetMimeTypeComplete(TSharedPtrTS<FHTTPResourceRequest> InRequest)
{
	if (!InRequest->GetWasCanceled())
	{
		FString mimeType(TEXT("application/octet-stream"));
		if (InRequest->GetError() == 0)
		{
			const HTTP::FConnectionInfo* ci = InRequest->GetConnectionInfo();
			if (ci)
			{
				// Get the content type, if it exists.
				if (ci->ContentType.Len())
				{
					mimeType = ci->ContentType;
				}
				// Use the effective URL after potential redirections.
				if (ci->EffectiveURL.Len())
				{
					ManifestURL = ci->EffectiveURL;
				}
			}
		}
		WorkerThread.SendLoadManifestMessage(ManifestURL, mimeType);
	}
}


void FAdaptiveStreamingPlayer::InternalCancelLoadManifest()
{
	if (ManifestMimeTypeRequest.IsValid())
	{
		ManifestMimeTypeRequest->Cancel();
		ManifestMimeTypeRequest.Reset();
	}
}


void FAdaptiveStreamingPlayer::InternalCloseManifestReader()
{
	if (ManifestReader.IsValid())
	{
		ManifestReader->Close();
		ManifestReader.Reset();
	}
}


//-----------------------------------------------------------------------------
/**
 * Starts asynchronous loading and parsing of a manifest.
 *
 * @param URL
 * @param MimeType
 */
void FAdaptiveStreamingPlayer::InternalLoadManifest(const FString& InURL, const FString& MimeType)
{
	// Fix potential errors in a local file URL.
	FString URL = Playlist::FixLocalFileSchemeURL(InURL);
	// Remember the original request URL since we may lose the fragment part in requests.
	ManifestURL = URL;
	ManifestMimeTypeRequest.Reset();
	ManifestType = EMediaFormatType::Unknown;
	if (CurrentState == EPlayerState::eState_Idle)
	{
		FString mimeType = MimeType;
		if (PlayerOptions.HaveKey(OptionKeyMimeType))
		{
			mimeType = PlayerOptions.GetValue(OptionKeyMimeType).GetFString();
		}
		else if (mimeType.IsEmpty())
		{
			mimeType = Playlist::GetMIMETypeForURL(URL);
		}
		// If there is no mime type we need to issue a HTTP HEAD request in order to get the "Content-Type" header.
		if (mimeType.IsEmpty() && (URL.StartsWith("https://", ESearchCase::CaseSensitive) || URL.StartsWith("http://", ESearchCase::CaseSensitive)))
		{
			ManifestMimeTypeRequest = MakeSharedTS<FHTTPResourceRequest>();
			ManifestMimeTypeRequest->URL(URL);
			ManifestMimeTypeRequest->Verb(TEXT("HEAD"));
			ManifestMimeTypeRequest->Callback().BindThreadSafeSP(AsShared(), &FAdaptiveStreamingPlayer::OnManifestGetMimeTypeComplete);
			ManifestMimeTypeRequest->StartGet(this);
			return;
		}
		DispatchEvent(FMetricEvent::ReportOpenSource(URL));
		if (mimeType.Len())
		{
			check(!ManifestReader.IsValid());

			CurrentState = EPlayerState::eState_ParsingManifest;
			if (mimeType == Playlist::MIMETypeHLS)
			{
				ManifestReader = IPlaylistReaderHLS::Create(this);
				ManifestType = EMediaFormatType::HLS;
			}
			else if (mimeType == Playlist::MIMETypeMP4 || mimeType == Playlist::MIMETypeMP4A || mimeType == Playlist::MIMETypeQuickTime)
			{
				ManifestReader = IPlaylistReaderMP4::Create(this);
				ManifestType = EMediaFormatType::ISOBMFF;
			}
			else if (mimeType == Playlist::MIMETypeDASH)
			{
				ManifestReader = IPlaylistReaderDASH::Create(this);
				ManifestType = EMediaFormatType::DASH;
			}
			else if (mimeType == Playlist::MIMETypeMKV || mimeType == Playlist::MIMETypeMKA)
			{
				ManifestReader = IPlaylistReaderMKV::Create(this);
				ManifestType = EMediaFormatType::MKV;
			}
			else
			{
				FErrorDetail err;
				err.SetFacility(Facility::EFacility::Player);
				err.SetMessage(TEXT("Unsupported stream MIME type"));
				err.SetCode(INTERR_UNSUPPORTED_FORMAT);
				PostError(err);
			}

			if (ManifestReader.IsValid())
			{
				ManifestReader->LoadAndParse(URL);
			}
		}
		else
		{
			FErrorDetail err;
			err.SetFacility(Facility::EFacility::Player);
			err.SetMessage(TEXT("Could not determine stream MIME type"));
			err.SetCode(INTERR_UNSUPPORTED_FORMAT);
			PostError(err);
		}
	}
	else
	{
// TODO: Error: Not idle
	}
}


void FAdaptiveStreamingPlayer::InternalHandleManifestReader()
{
	if (ManifestReader.IsValid())
	{
		ManifestReader->HandleOnce();
	}
}



//-----------------------------------------------------------------------------
/**
 * Selects the internal presentation for playback, setting the initial metadata and
 * updating options that may appear in the playlist.
 *
 * @return
 */
bool FAdaptiveStreamingPlayer::SelectManifest()
{
	if (ManifestReader.IsValid())
	{
		check(Manifest == nullptr);
		if (ManifestType != EMediaFormatType::Unknown)
		{
			TArray<FTimespan> SeekablePositions;
			TSharedPtrTS<IManifest> NewPresentation = ManifestReader->GetManifest();
			check(NewPresentation.IsValid());

			PlaybackState.SetSeekableRange(NewPresentation->GetSeekableTimeRange());
			NewPresentation->GetSeekablePositions(SeekablePositions);
			PlaybackState.SetSeekablePositions(SeekablePositions);
			PlaybackState.SetTimelineRange(NewPresentation->GetTotalTimeRange());
			PlaybackState.SetDuration(NewPresentation->GetDuration());

			// Check for playback range restriction. This is currently assumed to come from URL fragment parameters
			// like example.mp4#r=10.8,18.4
			// These do not override any user defined range values!
			FPlaybackRange PlaybackRange;
			FTimeRange RestrictedPlaybackRange;
			RestrictedPlaybackRange = NewPresentation->GetPlaybackRange(IManifest::EPlaybackRangeType::LockedPlaybackRange);
			PlaybackState.GetPlayRange(PlaybackRange);
			if (RestrictedPlaybackRange.Start.IsValid() && !PlaybackRange.Start.Get(FTimeValue()).IsValid())
			{
				PlaybackRange.Start = RestrictedPlaybackRange.Start;
			}
			if (RestrictedPlaybackRange.End.IsValid() && !PlaybackRange.End.Get(FTimeValue()).IsValid())
			{
				PlaybackRange.End = RestrictedPlaybackRange.End;
			}
			// Set the new range, but say it did not change. This is the first start and we do not need to issue a seek.
			PlaybackState.SetPlayRange(PlaybackRange);
			PlaybackState.ActivateNewPlayRange(nullptr);

			TArray<FTrackMetadata> VideoTrackMetadata;
			TArray<FTrackMetadata> AudioTrackMetadata;
			TArray<FTrackMetadata> SubtitleTrackMetadata;
			NewPresentation->GetTrackMetadata(VideoTrackMetadata, EStreamType::Video);
			NewPresentation->GetTrackMetadata(AudioTrackMetadata, EStreamType::Audio);
			NewPresentation->GetTrackMetadata(SubtitleTrackMetadata, EStreamType::Subtitle);
			PlaybackState.SetTrackMetadata(VideoTrackMetadata, AudioTrackMetadata, SubtitleTrackMetadata);
			PlaybackState.SetHaveMetadata(true);
			// Get the supported playback rates for thinned and unthinned playback.
			PlaybackState.SetPlaybackRates(EPlaybackRateType::Unthinned, NewPresentation->GetPossiblePlaybackRates(IManifest::EPlayRateType::UnthinnedRate));
			PlaybackState.SetPlaybackRates(EPlaybackRateType::Thinned, NewPresentation->GetPossiblePlaybackRates(IManifest::EPlayRateType::ThinnedRate));

			Manifest = NewPresentation;

			CurrentState = EPlayerState::eState_Ready;

			double minBufTimeMPD = Manifest->GetMinBufferTime().GetAsSeconds();
			PlayerConfig.InitialBufferMinTimeAvailBeforePlayback = Utils::Min(minBufTimeMPD, PlayerConfig.InitialBufferMinTimeAvailBeforePlayback);
			PlayerConfig.SeekBufferMinTimeAvailBeforePlayback    = Utils::Min(minBufTimeMPD, PlayerConfig.SeekBufferMinTimeAvailBeforePlayback);
			PlayerConfig.RebufferMinTimeAvailBeforePlayback 	 = Utils::Min(minBufTimeMPD, PlayerConfig.RebufferMinTimeAvailBeforePlayback);

			// For an mp4 or mkv stream we can now get rid of the manifest reader. It is no longer needed and we don't need to have it linger.
			if (ManifestType == EMediaFormatType::ISOBMFF || ManifestType == EMediaFormatType::MKV)
			{
				InternalCloseManifestReader();
			}

			// Let the ABR know the format as well.
			StreamSelector->SetFormatType(ManifestType);

			// Live streams must not use any cache!
			if (NewPresentation->GetPresentationType() == IManifest::EType::Live && HttpResponseCache.IsValid())
			{
				HttpResponseCache->Disable();
			}

			return true;
		}
		else
		{
			// Handle other types of playlist here.
			FErrorDetail err;
			err.SetFacility(Facility::EFacility::Player);
			err.SetMessage(TEXT("Unsupported playlist/manifest type"));
			err.SetCode(INTERR_UNSUPPORTED_FORMAT);
			PostError(err);
		}
	}

	return false;
}

void FAdaptiveStreamingPlayer::UpdateManifest()
{
	if (Manifest.IsValid())
	{
		TArray<FTimespan> SeekablePositions;
		PlaybackState.SetSeekableRange(Manifest->GetSeekableTimeRange());
		Manifest->GetSeekablePositions(SeekablePositions);
		PlaybackState.SetSeekablePositions(SeekablePositions);
		PlaybackState.SetTimelineRange(Manifest->GetTotalTimeRange());
		PlaybackState.SetDuration(Manifest->GetDuration());
	}
}


bool FAdaptiveStreamingPlayer::FMediaMetadataUpdate::Handle(const FTimeValue& InAtTime)
{
	TSharedPtrTS<UtilsMP4::FMetadataParser> NextMetadata;
	FTimeValue NextActiveTime;
	while(NextEntries.Num())
	{
		// Make the first metadata available right away if there is none yet and the time is not valid either.
		if (!InAtTime.IsValid() && !ActiveMetadata.IsValid())
		{
			NextMetadata = NextEntries[0].Metadata;
			NextActiveTime = NextEntries[0].ValidFrom;
			break;
		}
		else
		{
			const int64 s1 = NextEntries[0].ValidFrom.GetSequenceIndex();
			const int64 s2 = InAtTime.GetSequenceIndex();
			// Sequence count of metadata less or equal to the one of the current time?
			if (s1 <= s2)
			{
				if (NextEntries[0].ValidFrom <= InAtTime)
				{
					// Is this metadata older than the one we used before? This happens when there is metadata in the
					// init segment, which is sent for every media segment so that no metadata is missed when seeking.
					// Also, when stream switches occur the metadata of the init segment from the new stream is sent
					// as well.
					if (!ActiveSince.IsValid() || NextEntries[0].ValidFrom.GetSequenceIndex() > ActiveSince.GetSequenceIndex() || NextEntries[0].ValidFrom > ActiveSince)
					{
						NextMetadata = NextEntries[0].Metadata;
						NextActiveTime = NextEntries[0].ValidFrom;
					}
					NextEntries.RemoveAt(0);
				}
				else
				{
					break;
				}
			}
			else
			{
				break;
			}
		}
	}
	if (NextMetadata.IsValid())
	{
		bool bChanged = !ActiveMetadata.IsValid() || ActiveMetadata->IsDifferentFrom(*NextMetadata);
		ActiveMetadata = MoveTemp(NextMetadata);
		ActiveSince = NextActiveTime;
		return bChanged;
	}
	return false;
}


} // namespace Electra


