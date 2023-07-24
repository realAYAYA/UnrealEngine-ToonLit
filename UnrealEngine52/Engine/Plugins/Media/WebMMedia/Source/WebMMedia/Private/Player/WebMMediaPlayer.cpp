// Copyright Epic Games, Inc. All Rights Reserved.

#include "WebMMediaPlayer.h"

#if WITH_WEBM_LIBS

#include "WebMMediaPrivate.h"

#include "IMediaEventSink.h"
#include "MediaSamples.h"

#include "WebMVideoDecoder.h"
#include "WebMAudioDecoder.h"
#include "WebMMediaFrame.h"
#include "WebMMediaTextureSample.h"
#include "WebMMediaAudioSample.h"

#define LOCTEXT_NAMESPACE "FWebMMediaPlayer"

FWebMMediaPlayer::FWebMMediaPlayer(IMediaEventSink& InEventSink)
	: EventSink(InEventSink)
	, Samples(MakeShared<FMediaSamples, ESPMode::ThreadSafe>())
	, MkvCurrentCluster(nullptr)
	, MkvCurrentBlockEntry(nullptr)
	, CurrentState(EMediaState::Closed)
	, SelectedAudioTrack(INDEX_NONE)
	, SelectedVideoTrack(INDEX_NONE)
	, CurrentTime(0)
	, bNoMoreToRead(false)
	, bLooping(false)
{
}

FWebMMediaPlayer::~FWebMMediaPlayer()
{
	Close();
}

void FWebMMediaPlayer::Close()
{
	if (CurrentState == EMediaState::Closed)
	{
		return;
	}

	VideoDecoder.Reset();
	AudioDecoder.Reset();
	VideoTracks.Empty();
	AudioTracks.Empty();
	MkvReader.Reset();
	MkvSegment.Reset();
	MkvCurrentCluster = nullptr;
	MkvCurrentBlockEntry = nullptr;
	MediaUrl.Empty();
	CurrentState = EMediaState::Closed;
	SelectedAudioTrack = INDEX_NONE;
	SelectedVideoTrack = INDEX_NONE;
	CurrentTime = 0;
	bNoMoreToRead = false;

	// notify listeners
	OutEvents.Push(EMediaEvent::TracksChanged);
	OutEvents.Push(EMediaEvent::MediaClosed);
}

IMediaCache& FWebMMediaPlayer::GetCache()
{
	return *this;
}

IMediaControls& FWebMMediaPlayer::GetControls()
{
	return *this;
}

FString FWebMMediaPlayer::GetInfo() const
{
	return TEXT("WebMMedia information not implemented yet");
}

FGuid FWebMMediaPlayer::GetPlayerPluginGUID() const
{
	static FGuid PlayerPluginGUID(0xdfbb4e57, 0x07dc4b4a, 0xa25b5cba, 0x0f963ac3);
	return PlayerPluginGUID;
}

IMediaSamples& FWebMMediaPlayer::GetSamples()
{
	return *Samples.Get();
}

FString FWebMMediaPlayer::GetStats() const
{
	return TEXT("WebMMedia stats information not implemented yet");
}

IMediaTracks& FWebMMediaPlayer::GetTracks()
{
	return *this;
}

FString FWebMMediaPlayer::GetUrl() const
{
	return MediaUrl;
}

IMediaView& FWebMMediaPlayer::GetView()
{
	return *this;
}

bool FWebMMediaPlayer::Open(const FString& Url, const IMediaOptions* /*Options*/)
{
	if (CurrentState == EMediaState::Error)
	{
		return false;
	}

	Close();

	if ((Url.IsEmpty()))
	{
		return false;
	}

	MediaUrl = Url;

	// open the media
	if (!Url.StartsWith(TEXT("file://")))
	{
		UE_LOG(LogWebMMedia, Error, TEXT("Not supported URL: %s"), *Url);
		return false;
	}

	FString FilePath = Url.RightChop(7);
	FPaths::NormalizeFilename(FilePath);

	MkvReader.Reset(new FMkvFileReader());
	if (!MkvReader->Open(*FilePath))
	{
		UE_LOG(LogWebMMedia, Error, TEXT("Failed opening video file: %s"), *FilePath);
		return false;
	}

	if (!MkvRead())
	{
		MkvReader.Reset();

		UE_LOG(LogWebMMedia, Error, TEXT("Error parsing matroska file: %s"), *FilePath);
		return false;
	}

	VideoDecoder.Reset(new FWebMVideoDecoder(*this));
	AudioDecoder.Reset(new FWebMAudioDecoder(*this));

	if (SelectedAudioTrack != INDEX_NONE)
	{
		const mkvparser::AudioTrack* AudioTrack = AudioTracks[SelectedAudioTrack];

		size_t PrivateDataSize;
		const uint8* PrivateData = AudioTrack->GetCodecPrivate(PrivateDataSize);

		AudioDecoder->Initialize(AudioTrack->GetCodecId(), AudioTrack->GetSamplingRate(), AudioTrack->GetChannels(), PrivateData, PrivateDataSize);
	}

	if (SelectedVideoTrack != INDEX_NONE)
	{
		const mkvparser::VideoTrack* VideoTrack = VideoTracks[SelectedVideoTrack];

		VideoDecoder->Initialize(VideoTrack->GetCodecId());
	}

	CurrentState = EMediaState::Stopped;
	bNoMoreToRead = false;

	// notify listeners
	OutEvents.Push(EMediaEvent::TracksChanged);
	OutEvents.Push(EMediaEvent::MediaOpened);

	return true;
}

bool FWebMMediaPlayer::Open(const TSharedRef<FArchive, ESPMode::ThreadSafe>& /*Archive*/, const FString& /*OriginalUrl*/, const IMediaOptions* /*Options*/)
{
	// TODO! We don't support opening URLs for now.
	return false;
}

void FWebMMediaPlayer::TickFetch(FTimespan DeltaTime, FTimespan Timecode)
{
	if (CurrentState != EMediaState::Playing)
	{
		return;
	}

	CurrentTime += DeltaTime;

	if (bNoMoreToRead)
	{
		// Check if we finished playing
		if (Samples->NumVideoSamples() == 0 && Samples->NumAudio() == 0)
		{
			if (bLooping)
			{
				Seek(FTimespan::FromSeconds(0));
			}
			else
			{
				CurrentState = EMediaState::Stopped;
				CurrentTime = FTimespan::FromMicroseconds((double) MkvSegment->GetDuration() / 1000.0);
				OutEvents.Push(EMediaEvent::PlaybackEndReached);
				OutEvents.Push(EMediaEvent::PlaybackSuspended);
			}
		}

		return;
	}

	FTimespan ReadBufferLength = FTimespan::FromSeconds(1.0);
	FTimespan CurrentReadTime = CurrentTime;

	TArray<TSharedPtr<FWebMFrame>> VideoFrames;
	TArray<TSharedPtr<FWebMFrame>> AudioFrames;

	if (CurrentReadTime < CurrentTime - ReadBufferLength / 2)
	{
		UE_LOG(LogWebMMedia, Warning, TEXT("Playback is very behind, try to catchup and reset sync"));
		Samples->FlushSamples();
	}

	// Read frames up to 1 secs in the future
	while (CurrentReadTime < CurrentTime + ReadBufferLength)
	{
		MkvSeekToNextValidBlock();

		if (bNoMoreToRead)
		{
			break;
		}

		CurrentReadTime = FTimespan::FromMicroseconds(MkvCurrentBlockEntry->GetBlock()->GetTime(MkvCurrentCluster) / 1000.0);

		for (int32 i = 0; i < MkvCurrentBlockEntry->GetBlock()->GetFrameCount(); ++i)
		{
			const mkvparser::Block::Frame& MkvFrame = MkvCurrentBlockEntry->GetBlock()->GetFrame(i);

			TSharedPtr<FWebMFrame> Frame = MakeShared<FWebMFrame>();
			Frame->Time = CurrentReadTime;
			Frame->Data.SetNumUninitialized(MkvFrame.len);
			MkvFrame.Read(MkvReader.Get(), Frame->Data.GetData());

			int32 TrackNumber = MkvCurrentBlockEntry->GetBlock()->GetTrackNumber();

			if (SelectedVideoTrack != INDEX_NONE && VideoTracks[SelectedVideoTrack]->GetNumber() == TrackNumber)
			{
				Frame->Duration = FMkvFileReader::GetVideoFrameDuration(*VideoTracks[SelectedVideoTrack]);
				VideoFrames.Add(Frame);
				CurrentReadTime += Frame->Duration;
			}
			else if (SelectedAudioTrack != INDEX_NONE && AudioTracks[SelectedAudioTrack]->GetNumber() == TrackNumber)
			{
				// We will set duration after decompression.
				AudioFrames.Add(Frame);
			}
		}

		// Make sure we're not decoding too much at once as it is GPU memory intensive.
		if (VideoFrames.Num() > 20)
		{
			break;
		}
	}

	// Trigger video decoding
	if (VideoFrames.Num() > 0)
	{
		VideoDecoder->DecodeVideoFramesAsync(VideoFrames);
	}

	// Trigger audio decoding
	if (AudioFrames.Num() > 0)
	{
		AudioDecoder->DecodeAudioFramesAsync(AudioFrames);
	}
}

void FWebMMediaPlayer::TickInput(FTimespan DeltaTime, FTimespan /*Timecode*/)
{
	// Send out events
	for (const auto& Event : OutEvents)
	{
		EventSink.ReceiveMediaEvent(Event);
	}
	OutEvents.Empty();
}

/* IMediaTracks interface
 *****************************************************************************/

bool FWebMMediaPlayer::GetAudioTrackFormat(int32 TrackIndex, int32 FormatIndex, FMediaAudioTrackFormat& OutFormat) const
{
	if (FormatIndex != 0)
	{
		// We only support one format per track
		return false;
	}

	if (TrackIndex == INDEX_NONE || TrackIndex >= AudioTracks.Num())
	{
		return false;
	}

	OutFormat.BitsPerSample = AudioTracks[TrackIndex]->GetBitDepth();
	OutFormat.NumChannels = AudioTracks[TrackIndex]->GetChannels();
	OutFormat.SampleRate = AudioTracks[TrackIndex]->GetSamplingRate();
	OutFormat.TypeName = UTF8_TO_TCHAR(AudioTracks[TrackIndex]->GetCodecNameAsUTF8());

	return true;
}

int32 FWebMMediaPlayer::GetNumTracks(EMediaTrackType TrackType) const
{
	if (TrackType == EMediaTrackType::Audio)
	{
		return AudioTracks.Num();
	}
	else if (TrackType == EMediaTrackType::Video)
	{
		return VideoTracks.Num();
	}
	else
	{
		// We support only video and audio tracks
		return 0;
	}
}

int32 FWebMMediaPlayer::GetNumTrackFormats(EMediaTrackType TrackType, int32 TrackIndex) const
{
	// We only support one format per track
	return 1;
}

int32 FWebMMediaPlayer::GetSelectedTrack(EMediaTrackType TrackType) const
{
	if (TrackType == EMediaTrackType::Audio)
	{
		return SelectedAudioTrack;
	}
	else if (TrackType == EMediaTrackType::Video)
	{
		return SelectedVideoTrack;
	}
	else
	{
		return INDEX_NONE;
	}
}

FText FWebMMediaPlayer::GetTrackDisplayName(EMediaTrackType TrackType, int32 TrackIndex) const
{
	return FText::FromString(GetTrackName(TrackType, TrackIndex));
}

int32 FWebMMediaPlayer::GetTrackFormat(EMediaTrackType TrackType, int32 TrackIndex) const
{
	// We only support one format per track
	return 0;
}

FString FWebMMediaPlayer::GetTrackLanguage(EMediaTrackType TrackType, int32 TrackIndex) const
{
	// TODO! We support only default language
	static FString Language(TEXT("Default"));
	return Language;
}

FString FWebMMediaPlayer::GetTrackName(EMediaTrackType TrackType, int32 TrackIndex) const
{
	FString TrackName(TEXT("None"));

	if (TrackIndex != INDEX_NONE)
	{
		if (TrackType == EMediaTrackType::Audio && TrackIndex < AudioTracks.Num())
		{
			TrackName = AudioTracks[TrackIndex]->GetNameAsUTF8() ? UTF8_TO_TCHAR(AudioTracks[TrackIndex]->GetNameAsUTF8()) : FString::Printf(TEXT("Track %d"), TrackIndex);
		}
		else if (TrackType == EMediaTrackType::Video && TrackIndex < VideoTracks.Num())
		{
			TrackName = VideoTracks[TrackIndex]->GetNameAsUTF8() ? UTF8_TO_TCHAR(VideoTracks[TrackIndex]->GetNameAsUTF8()) : FString::Printf(TEXT("Track %d"), TrackIndex);
		}
	}

	return TrackName;
}

bool FWebMMediaPlayer::GetVideoTrackFormat(int32 TrackIndex, int32 FormatIndex, FMediaVideoTrackFormat& OutFormat) const
{
	if (FormatIndex != 0)
	{
		// we support only one format
		return false;
	}

	if (TrackIndex == INDEX_NONE || TrackIndex >= VideoTracks.Num())
	{
		return false;
	}

	OutFormat.Dim = FIntPoint(VideoTracks[TrackIndex]->GetWidth(), VideoTracks[TrackIndex]->GetHeight());
	OutFormat.FrameRate = VideoTracks[TrackIndex]->GetFrameRate();
	OutFormat.TypeName = VideoTracks[TrackIndex]->GetCodecNameAsUTF8();
	OutFormat.FrameRates = TRange<float>(OutFormat.FrameRate, OutFormat.FrameRate);

	return true;
}

bool FWebMMediaPlayer::SelectTrack(EMediaTrackType TrackType, int32 TrackIndex)
{
	bool bHasChanged = false;

	if (TrackType == EMediaTrackType::Audio)
	{
		bHasChanged = SelectedAudioTrack != TrackIndex;
		SelectedAudioTrack = TrackIndex;
	}
	else if (TrackType == EMediaTrackType::Video)
	{
		bHasChanged = SelectedVideoTrack != TrackIndex;
		SelectedVideoTrack = TrackIndex;
	}

	if (bHasChanged)
	{
		if (CurrentState == EMediaState::Playing)
		{
			Seek(CurrentTime);
		}

		if (CurrentState != EMediaState::Closed)
		{
			if (SelectedAudioTrack != INDEX_NONE)
			{
				const mkvparser::AudioTrack* AudioTrack = AudioTracks[SelectedAudioTrack];

				size_t PrivateDataSize;
				const uint8* PrivateData = AudioTrack->GetCodecPrivate(PrivateDataSize);

				AudioDecoder->Initialize(AudioTrack->GetCodecId(), AudioTrack->GetSamplingRate(), AudioTrack->GetChannels(), PrivateData, PrivateDataSize);
			}

			if (SelectedVideoTrack != INDEX_NONE)
			{
				const mkvparser::VideoTrack* VideoTrack = VideoTracks[SelectedVideoTrack];

				VideoDecoder->Initialize(VideoTrack->GetCodecId());
			}
		}
	}

	return true;
}

bool FWebMMediaPlayer::SetTrackFormat(EMediaTrackType TrackType, int32 TrackIndex, int32 FormatIndex)
{
	if (FormatIndex == 0)
	{
		return true;
	}
	else
	{
		// We only support one track format
		return false;
	}
}

/* IMediaControls interface
 *****************************************************************************/

bool FWebMMediaPlayer::CanControl(EMediaControl Control) const
{
	if (Control == EMediaControl::Pause)
	{
		return (CurrentState == EMediaState::Playing);
	}

	if (Control == EMediaControl::Resume)
	{
		return ((CurrentState == EMediaState::Playing) || (CurrentState == EMediaState::Stopped));
	}

	if (Control == EMediaControl::Seek)
	{
		return ((CurrentState != EMediaState::Closed) && (CurrentState != EMediaState::Error));
	}

	return false;
}

FTimespan FWebMMediaPlayer::GetDuration() const
{
	if (CurrentState == EMediaState::Error || CurrentState == EMediaState::Closed)
	{
		return FTimespan::Zero();
	}

	return FTimespan::FromMicroseconds((double) MkvSegment->GetDuration() / 1000.0);
}

float FWebMMediaPlayer::GetRate() const
{
	return (CurrentState == EMediaState::Playing) ? 1.0f : 0.0f;
}

EMediaState FWebMMediaPlayer::GetState() const
{
	return CurrentState;
}

EMediaStatus FWebMMediaPlayer::GetStatus() const
{
	return EMediaStatus::None;
}

TRangeSet<float> FWebMMediaPlayer::GetSupportedRates(EMediaRateThinning /*Thinning*/) const
{
	TRangeSet<float> Result;

	Result.Add(TRange<float>(0.0f));
	Result.Add(TRange<float>(1.0f));

	return Result;
}

FTimespan FWebMMediaPlayer::GetTime() const
{
	if ((CurrentState == EMediaState::Closed) || (CurrentState == EMediaState::Error))
	{
		return FTimespan::Zero();
	}

	return CurrentTime;
}

bool FWebMMediaPlayer::IsLooping() const
{
	return bLooping;
}

bool FWebMMediaPlayer::Seek(const FTimespan& Time)
{
	if ((CurrentState == EMediaState::Closed) ||
		(CurrentState == EMediaState::Error) ||
		(CurrentState == EMediaState::Preparing))
	{
		UE_LOG(LogWebMMedia, Warning, TEXT("Cannot seek while closed or in error state"));
		return false;
	}

	MkvSeekToTime(Time);

	if (SelectedVideoTrack != INDEX_NONE)
	{
		const mkvparser::VideoTrack* VideoTrack = VideoTracks[SelectedVideoTrack];

		VideoDecoder->Initialize(VideoTrack->GetCodecId());
	}

	if (SelectedAudioTrack != INDEX_NONE)
	{
		const mkvparser::AudioTrack* AudioTrack = AudioTracks[SelectedAudioTrack];

		size_t PrivateDataSize;
		const uint8* PrivateData = AudioTrack->GetCodecPrivate(PrivateDataSize);

		AudioDecoder->Initialize(AudioTrack->GetCodecId(), AudioTrack->GetSamplingRate(), AudioTrack->GetChannels(), PrivateData, PrivateDataSize);
	}

	CurrentTime = Time;
	bNoMoreToRead = false;

	EventSink.ReceiveMediaEvent(EMediaEvent::SeekCompleted);

	return true;
}

bool FWebMMediaPlayer::SetLooping(bool Looping)
{
	bLooping = Looping;

	return true;
}

bool FWebMMediaPlayer::SetRate(float Rate)
{
	if (Rate == 0.0f)
	{
		Pause();
		return true;
	}
	else if (Rate == 1.0f)
	{
		Resume();
		return true;
	}
	else
	{
		return false;
	}
}

bool FWebMMediaPlayer::MkvRead()
{
	int64 FilePosition = 0;
	if (mkvparser::EBMLHeader().Parse(MkvReader.Get(), FilePosition) != 0)
	{
		return false;
	}

	mkvparser::Segment* Segment;
	if (mkvparser::Segment::CreateInstance(MkvReader.Get(), FilePosition, Segment) != 0)
	{
		return false;
	}

	MkvSegment.Reset(Segment);

	if (MkvSegment->Load() < 0)
	{
		return false;
	}

	// Read all the tracks

	const mkvparser::Tracks* Tracks = MkvSegment->GetTracks();
	int32 NumOfTracks = Tracks->GetTracksCount();

	if (NumOfTracks == 0)
	{
		UE_LOG(LogWebMMedia, Warning, TEXT("File doesn't have any tracks"));
		return false;
	}

	for (int32 i = 0; i < NumOfTracks; ++i)
	{
		const mkvparser::Track* Track = Tracks->GetTrackByIndex(i);
		check(Track);

		if (Track->GetType() == mkvparser::Track::kVideo)
		{
			if (FCStringAnsi::Strcmp(Track->GetCodecId(), "V_VP8") == 0 || FCStringAnsi::Strcmp(Track->GetCodecId(), "V_VP9") == 0)
			{
				VideoTracks.Add(static_cast<const mkvparser::VideoTrack*>(Track));
			}
			else
			{
				UE_LOG(LogWebMMedia, Warning, TEXT("File contains unsupported video track %d: %s"), i, UTF8_TO_TCHAR(Track->GetCodecId()));
				continue;
			}
		}
		else if (Track->GetType() == mkvparser::Track::kAudio)
		{
			if (FCStringAnsi::Strcmp(Track->GetCodecId(), "A_OPUS") == 0 || FCStringAnsi::Strcmp(Track->GetCodecId(), "A_VORBIS") == 0)
			{
				AudioTracks.Add(static_cast<const mkvparser::AudioTrack*>(Track));
			}
			else
			{
				UE_LOG(LogWebMMedia, Warning, TEXT("File contains unsupported audio track %d: %s"), i, UTF8_TO_TCHAR(Track->GetCodecId()));
				continue;
			}
		}
		else
		{
			UE_LOG(LogWebMMedia, Warning, TEXT("File contains unsupported track %d: %s"), i, UTF8_TO_TCHAR(Track->GetCodecId()));
			continue;
		}
	}

	if (VideoTracks.Num() == 0 || AudioTracks.Num() == 0)
	{
		UE_LOG(LogWebMMedia, Warning, TEXT("File doesn't have video or audio. Right now only files with both are supported"));
		return false;
	}

	return true;
}

void FWebMMediaPlayer::MkvSeekToNextValidBlock()
{
	for (;;)
	{
		if (!MkvCurrentCluster)
		{
			MkvCurrentCluster = MkvSegment->GetFirst();
			MkvCurrentBlockEntry = nullptr;
			check(MkvCurrentCluster);
		}

		if (!MkvCurrentBlockEntry || MkvCurrentBlockEntry->EOS())
		{
			if (MkvCurrentCluster->GetFirst(MkvCurrentBlockEntry) != 0)
			{
				UE_LOG(LogWebMMedia, Warning, TEXT("Something went wrong while seeking"));
				bNoMoreToRead = true;
				return;
			}
		}
		else
		{
			if(MkvCurrentCluster->GetNext(MkvCurrentBlockEntry, MkvCurrentBlockEntry) != 0)
			{
				UE_LOG(LogWebMMedia, Warning, TEXT("Something went wrong while seeking"));
				bNoMoreToRead = true;
				return;
			}
		}

		if (!MkvCurrentBlockEntry || MkvCurrentBlockEntry->EOS())
		{
			MkvCurrentBlockEntry = nullptr;
			MkvCurrentCluster = MkvSegment->GetNext(MkvCurrentCluster);
			check(MkvCurrentCluster);

			if (MkvCurrentCluster->EOS())
			{
				bNoMoreToRead = true;
				return;
			}
			else
			{
				continue;
			}
		}

		return;
	}
}

void FWebMMediaPlayer::MkvSeekToTime(const FTimespan& Time)
{
	// TODO: for more precise seeking, we should use CUEs

	uint64 TimeInNs = (uint64_t) (Time.GetTotalMicroseconds() * 1000.0);

	MkvCurrentCluster = MkvSegment->FindCluster(TimeInNs);
	MkvCurrentBlockEntry = nullptr;

	Samples->FlushSamples();
}

void FWebMMediaPlayer::Resume()
{
	CurrentState = EMediaState::Playing;
	OutEvents.Push(EMediaEvent::PlaybackResumed);
}

void FWebMMediaPlayer::Pause()
{
	CurrentState = EMediaState::Paused;
	OutEvents.Push(EMediaEvent::PlaybackSuspended);
}

void FWebMMediaPlayer::Stop()
{
	Pause();
}

void FWebMMediaPlayer::AddVideoSampleFromDecodingThread(TSharedRef<FWebMMediaTextureSample, ESPMode::ThreadSafe> Sample)
{
	if (Sample->GetTime() < CurrentTime)
	{
		// We don't care about expired samples
		return;
	}

	Samples->AddVideo(Sample);
}

void FWebMMediaPlayer::AddAudioSampleFromDecodingThread(TSharedRef<FWebMMediaAudioSample, ESPMode::ThreadSafe> Sample)
{
	if (Sample->GetTime() < CurrentTime)
	{
		// We don't care about expired samples
		return;
	}

	Samples->AddAudio(Sample);
}

#undef LOCTEXT_NAMESPACE

#endif // WITH_WEBM_LIBS
