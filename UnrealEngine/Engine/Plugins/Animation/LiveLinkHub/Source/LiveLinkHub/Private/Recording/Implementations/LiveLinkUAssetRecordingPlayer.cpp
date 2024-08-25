// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkUAssetRecordingPlayer.h"

#include "LiveLinkHubLog.h"
#include "Recording/LiveLinkRecording.h"


class FLiveLinkPlaybackTrackIterator
{
public:
	virtual ~FLiveLinkPlaybackTrackIterator() = default;

	FLiveLinkPlaybackTrackIterator(FLiveLinkPlaybackTrack& InTrack, int32 InInitialIndex)
		: Track(InTrack)
		, FrameIndex(InInitialIndex)
	{
	}

	/** Advances to the next frame */
	void operator++()
	{
		Advance();
	}

	/* @Return True if there are more frames in this track. */
	explicit operator bool() const
	{
		return HasMoreFrames();
	}

	double FrameTimestamp() const
	{
		return FrameIndex >= 0 && FrameIndex < Track.Timestamps.Num() ? Track.Timestamps[FrameIndex] : 0.f;
	}

	const FInstancedStruct& FrameData() const
	{
		return Track.FrameData[FrameIndex];
	}

	int32 CurrentIndex() const
	{
		return FrameIndex;
	}

protected:
	/* @Return True if there are more vertices on the component */
	virtual bool HasMoreFrames() const = 0;

	/* Advances to the next frame */
	virtual void Advance() = 0;

protected:
	/** Track that's currently being iterated. */
	FLiveLinkPlaybackTrack& Track;
	/** "Playhead" for this track */
	int32 FrameIndex = 0;
};

class FLiveLinkPlaybackTrackForwardIterator : public FLiveLinkPlaybackTrackIterator
{
public:
	FLiveLinkPlaybackTrackForwardIterator(FLiveLinkPlaybackTrack& InTrack, int32 InInitialIndex)
		: FLiveLinkPlaybackTrackIterator(InTrack, InInitialIndex)
	{
	}

private:
	/* @Return True if there are more vertices on the component */
	virtual bool HasMoreFrames() const override
	{
		return FrameIndex < Track.Timestamps.Num() && FrameIndex < Track.FrameData.Num();
	}

	/* Advances to the next frame */
	virtual void Advance() override
	{
		++FrameIndex;
	}
};

class FLiveLinkPlaybackTrackReverseIterator : public FLiveLinkPlaybackTrackIterator
{
public:
	FLiveLinkPlaybackTrackReverseIterator(FLiveLinkPlaybackTrack& InTrack, int32 InInitialIndex)
		: FLiveLinkPlaybackTrackIterator(InTrack, InInitialIndex)
	{
	}

private:
	/* @Return True if there are more vertices on the component */
	virtual bool HasMoreFrames() const override
	{
		return FrameIndex >= 0;
	}

	/* Advances to the next frame */
	virtual void Advance() override
	{
		--FrameIndex;
	}
};

void FLiveLinkPlaybackTrack::GetFramesUntil(double InPlayhead, TArray<FLiveLinkRecordedFrame>& OutFrames)
{
	for (FLiveLinkPlaybackTrackForwardIterator It = FLiveLinkPlaybackTrackForwardIterator(*this, LastReadIndex + 1); It; ++It)
	{
		if (It.FrameTimestamp() > InPlayhead)
		{
			break;
		}

		LastReadIndex = It.CurrentIndex();

		FLiveLinkRecordedFrame FrameToPlay;
		FrameToPlay.Data = It.FrameData();
		FrameToPlay.SubjectKey = SubjectKey;
		FrameToPlay.LiveLinkRole = LiveLinkRole;
		FrameToPlay.FrameIndex = LastReadIndex;

		OutFrames.Add(MoveTemp(FrameToPlay));
	}
}

void FLiveLinkPlaybackTrack::GetFramesUntilReverse(double InPlayhead, TArray<FLiveLinkRecordedFrame>& OutFrames)
{
	if (LastReadIndex == INDEX_NONE)
	{
		LastReadIndex = FrameData.Num();
	}

	// We need to look up what the last frame would be if this was running forward, and then end on that frame.
	// Since we iterate in reverse, but all other operations like GoToFrame use forward look ahead, it's possible the time stamp comparison
	// will differ by a frame with a reverse look up. There's probably a better way of handling this.
	const int32 FinalFrameIndex = PlayheadToFrameIndex(InPlayhead);
	
	for (FLiveLinkPlaybackTrackReverseIterator It = FLiveLinkPlaybackTrackReverseIterator(*this, LastReadIndex - 1); It; ++It)
	{
		if (FinalFrameIndex == LastReadIndex)
		{
			break;
		}

		LastReadIndex = It.CurrentIndex();

		FLiveLinkRecordedFrame FrameToPlay;
		FrameToPlay.Data = It.FrameData();
		FrameToPlay.SubjectKey = SubjectKey;
		FrameToPlay.LiveLinkRole = LiveLinkRole;
		FrameToPlay.FrameIndex = LastReadIndex;

		OutFrames.Add(MoveTemp(FrameToPlay));
	}
}

bool FLiveLinkPlaybackTrack::TryGetFrame(int32 InIndex, FLiveLinkRecordedFrame& OutFrame)
{
	if (InIndex >= 0 && InIndex < FrameData.Num())
	{
		LastReadIndex = InIndex;
		
		FLiveLinkRecordedFrame FrameToPlay;
		FrameToPlay.Data = FrameData[InIndex];
		FrameToPlay.SubjectKey = SubjectKey;
		FrameToPlay.LiveLinkRole = LiveLinkRole;
		FrameToPlay.FrameIndex = LastReadIndex;

		OutFrame = MoveTemp(FrameToPlay);
		return true;
	}
	
	return false;
}

int32 FLiveLinkPlaybackTrack::PlayheadToFrameIndex(double InPlayhead)
{
	int32 CurrentIndex = 0;

	for (int32 Idx = 0; Idx < Timestamps.Num(); ++Idx)
	{
		if (Timestamps[Idx] > InPlayhead)
		{
			break;
		}

		CurrentIndex = Idx;
	}

	return CurrentIndex;
}

double FLiveLinkPlaybackTrack::FrameIndexToPlayhead(int32 InIndex)
{
	if (InIndex >= 0 && InIndex < Timestamps.Num())
	{
		return Timestamps[InIndex];
	}

	return INDEX_NONE;
}

TArray<FLiveLinkRecordedFrame> FLiveLinkPlaybackTracks::FetchNextFrames(double Playhead)
{
	TArray<FLiveLinkRecordedFrame> NextFrames;

	if (Tracks.Num())
	{
		// todo: sort frames by timestamp
		for (FLiveLinkPlaybackTrack& Track : Tracks)
		{
			Track.GetFramesUntil(Playhead, NextFrames);
		}
	}

	return NextFrames;
}

TArray<FLiveLinkRecordedFrame> FLiveLinkPlaybackTracks::FetchPreviousFrames(double Playhead)
{
	TArray<FLiveLinkRecordedFrame> PreviousFrames;

	if (Tracks.Num())
	{
		// todo: sort frames by timestamp
		for (FLiveLinkPlaybackTrack& Track : Tracks)
		{
			Track.GetFramesUntilReverse(Playhead, PreviousFrames);
		}
	}

	return PreviousFrames;
}

TArray<FLiveLinkRecordedFrame> FLiveLinkPlaybackTracks::FetchNextFramesAtIndex(int32 FrameIndex)
{
	TArray<FLiveLinkRecordedFrame> NextFrames;

	if (FrameIndex >= 0)
	{
		for (FLiveLinkPlaybackTrack& Track : Tracks)
		{
			FLiveLinkRecordedFrame Frame;
			if (Track.TryGetFrame(FrameIndex, Frame))
			{
				NextFrames.Add(MoveTemp(Frame));
			}
		}
	}

	return NextFrames;
}

int32 FLiveLinkPlaybackTracks::PlayheadToFrameIndex(double InPlayhead)
{
	for (FLiveLinkPlaybackTrack& Track : Tracks)
	{
		// todo: Is this the best way to determine if this is keyframe data and not static data?
		if (Track.LiveLinkRole == nullptr)
		{
			return Track.PlayheadToFrameIndex(InPlayhead);
		}
	}

	return INDEX_NONE;
}

double FLiveLinkPlaybackTracks::FrameIndexToPlayhead(int32 InIndex)
{
	for (FLiveLinkPlaybackTrack& Track : Tracks)
	{
		// todo: Is this the best way to determine if this is keyframe data and not static data?
		if (Track.LiveLinkRole == nullptr)
		{
			return Track.FrameIndexToPlayhead(InIndex);
		}
	}

	return INDEX_NONE;
}

void FLiveLinkPlaybackTracks::Restart(int32 InIndex)
{
	for (FLiveLinkPlaybackTrack& Track : Tracks)
	{
		Track.Restart(InIndex);
	}
}

FFrameRate FLiveLinkPlaybackTracks::GetInitialFrameRate() const
{
	for (const FLiveLinkPlaybackTrack& Track : Tracks)
	{
		if (Track.LiveLinkRole == nullptr && Track.FrameData.Num() > 0)
		{
			FLiveLinkFrameDataStruct FrameDataStruct;
			FrameDataStruct.InitializeWith(Track.FrameData[0].GetScriptStruct(), (FLiveLinkBaseFrameData*)Track.FrameData[0].GetMemory());

			return FrameDataStruct.GetBaseData()->MetaData.SceneTime.Rate;
		}
	}

	UE_LOG(LogLiveLinkHub, Warning, TEXT("Could not find an initial framerate for the recording. Using the default value."));
	
	return FFrameRate(30, 1);
}

void FLiveLinkUAssetRecordingPlayer::PreparePlayback(const ULiveLinkRecording* CurrentRecording)
{
	const ULiveLinkUAssetRecording* UAssetRecording = CastChecked<ULiveLinkUAssetRecording>(CurrentRecording);

	FLiveLinkPlaybackTracks RecordingPlayback;

	for (const TPair<FLiveLinkSubjectKey, FLiveLinkRecordingStaticDataContainer>& Pair : UAssetRecording->RecordingData.StaticData)
	{
		FLiveLinkPlaybackTrack PlaybackTrack;
		PlaybackTrack.FrameData = TConstArrayView<FInstancedStruct>(Pair.Value.RecordedData);
		PlaybackTrack.Timestamps = TConstArrayView<double>(Pair.Value.Timestamps);
		PlaybackTrack.LiveLinkRole = Pair.Value.Role;
		PlaybackTrack.SubjectKey = Pair.Key;
		RecordingPlayback.Tracks.Add(PlaybackTrack);
	}

	for (const TPair<FLiveLinkSubjectKey, FLiveLinkRecordingBaseDataContainer>& Pair : UAssetRecording->RecordingData.FrameData)
	{
		FLiveLinkPlaybackTrack PlaybackTrack;
		PlaybackTrack.FrameData = TConstArrayView<FInstancedStruct>(Pair.Value.RecordedData);
		PlaybackTrack.Timestamps = TConstArrayView<double>(Pair.Value.Timestamps);
		PlaybackTrack.SubjectKey = Pair.Key;
		RecordingPlayback.Tracks.Add(PlaybackTrack);
	}

	CurrentRecordingPlayback = MoveTemp(RecordingPlayback);
}
