// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Recording/LiveLinkRecordingPlayer.h"

#include "Recording/Implementations/LiveLinkUAssetRecording.h"

/** Playback track that holds recorded data for a given subject. */
struct FLiveLinkPlaybackTrack
{
	/** Retrieve all frames from last read index to the new playhead, forward looking. */
	void GetFramesUntil(double InPlayhead, TArray<FLiveLinkRecordedFrame>& OutFrames);

	/** Retrieve all frames from last read index to the new playhead, reverse looking. */
	void GetFramesUntilReverse(double InPlayhead, TArray<FLiveLinkRecordedFrame>& OutFrames);
	
	/** Retrieve the frame at the read index. */
	bool TryGetFrame(int32 InIndex, FLiveLinkRecordedFrame& OutFrame);

	/**
	 * Convert the playhead time to a frame index.
	 * @param InPlayhead The play head to convert to a frame index.
	 * @return The frame index or INDEX_NONE.
	 */
	int32 PlayheadToFrameIndex(double InPlayhead);

	/**
	 * Convert the frameindex to a playhead.
	 * @return The frame index or INDEX_NONE.
	 */
	double FrameIndexToPlayhead(int32 InIndex);

	/** Reset the LastReadIndex. */
	void Restart(int32 NewIndex = INDEX_NONE)
	{
		LastReadIndex = NewIndex < FrameData.Num() && NewIndex < Timestamps.Num() ? NewIndex : INDEX_NONE;
	}

	/** Frame data to read. */
	TConstArrayView<struct FInstancedStruct> FrameData;
	/** Timestamps for the frames in the track. */
	TConstArrayView<double> Timestamps;
	/** Used for static data. */
	TSubclassOf<ULiveLinkRole> LiveLinkRole;
	/** Subject key. */
	FLiveLinkSubjectKey SubjectKey;
	/** Index of the last frame that was read by the GetFrames method. */
	int32 LastReadIndex = -1;

	friend class FLiveLinkPlaybackTrackIterator;
};

/** Reorganized recording data to facilitate playback. */
struct FLiveLinkPlaybackTracks
{
	/** Get the next frames */
	TArray<FLiveLinkRecordedFrame> FetchNextFrames(double Playhead);

	/** Get the previous frames as if going in reverse */
	TArray<FLiveLinkRecordedFrame> FetchPreviousFrames(double Playhead);
	
	/** Get the next frame(s) at the index */
	TArray<FLiveLinkRecordedFrame> FetchNextFramesAtIndex(int32 FrameIndex);

	/** Convert the playhead to a frame index */
	int32 PlayheadToFrameIndex(double InPlayhead);

	/** Convert the index to a playhead */
	double FrameIndexToPlayhead(int32 InIndex);
	
	void Restart(int32 InIndex);

	/** Retrieve the framerate of the first frame */
	FFrameRate GetInitialFrameRate() const;

public:
	/** LiveLink tracks to playback. */
	TArray<FLiveLinkPlaybackTrack> Tracks;
};

class FLiveLinkUAssetRecordingPlayer : public ILiveLinkRecordingPlayer
{
public:
	void PreparePlayback(const class ULiveLinkRecording* CurrentRecording);

	virtual TArray<FLiveLinkRecordedFrame> FetchNextFramesAtTimestamp(double Playhead) override
	{
		return CurrentRecordingPlayback.FetchNextFrames(Playhead);
	}

	virtual TArray<FLiveLinkRecordedFrame> FetchPreviousFramesAtTimestamp(double Playhead) override
	{
		return CurrentRecordingPlayback.FetchPreviousFrames(Playhead);
	}

	virtual TArray<FLiveLinkRecordedFrame> FetchNextFramesAtIndex(int32 FrameIndex) override
	{
		return CurrentRecordingPlayback.FetchNextFramesAtIndex(FrameIndex);
	}

	virtual int32 PlayheadToFrameIndex(double InPlayhead, bool bReverse) override
	{
		return CurrentRecordingPlayback.PlayheadToFrameIndex(InPlayhead);
	}

	virtual double FrameIndexToPlayhead(int32 InIndex) override
	{
		return CurrentRecordingPlayback.FrameIndexToPlayhead(InIndex);
	}

	virtual void RestartPlayback(int32 InIndex) override
	{
		CurrentRecordingPlayback.Restart(InIndex);
	}

	virtual FFrameRate GetInitialFramerate() override
	{
		return CurrentRecordingPlayback.GetInitialFrameRate();
	}
	
private:
	/** All tracks for the current recording. */
	FLiveLinkPlaybackTracks CurrentRecordingPlayback;
};
