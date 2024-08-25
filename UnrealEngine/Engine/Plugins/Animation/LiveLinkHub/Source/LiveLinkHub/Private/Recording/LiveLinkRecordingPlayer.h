// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LiveLinkRole.h"
#include "LiveLinkTypes.h"
#include "StructView.h"
#include "Templates/SubclassOf.h"

class ULiveLinkRecording;

/** Frame that was read by the recording player*/
struct FLiveLinkRecordedFrame
{
	/** Recorded frame or static data. */
	FConstStructView Data;
	/** Subject that originally sent the data. */
	FLiveLinkSubjectKey SubjectKey;
	/** Role used to interpret the data (Only present with recorded static data). */
	TSubclassOf<ULiveLinkRole> LiveLinkRole;
	/** The frame index of this frame within a track. */
	int32 FrameIndex;
};

/** Object responsible for reading a livelink recording and providing the frames to the LiveLinkPlaybackController. */
class ILiveLinkRecordingPlayer
{
public:
	virtual ~ILiveLinkRecordingPlayer() = default;

	/** Initialize internal structures needed for playback of the recorded data. */
	virtual void PreparePlayback(const ULiveLinkRecording* Recording) = 0;

	/** Fetch next frames at the provided playhead position. */
	virtual TArray<FLiveLinkRecordedFrame> FetchNextFramesAtTimestamp(double Playhead) = 0;

	/** Fetch previous frames at the provided playhead position. */
	virtual TArray<FLiveLinkRecordedFrame> FetchPreviousFramesAtTimestamp(double Playhead) = 0;
	
	/** Fetch next frames at the provided frame index. */
	virtual TArray<FLiveLinkRecordedFrame> FetchNextFramesAtIndex(int32 FrameIndex) = 0;

	/** Convert the playhead to a frame index. */
	virtual int32 PlayheadToFrameIndex(double InPlayhead, bool bReverse) = 0;

	/** Convert the frame index to a timstamp. */
	virtual double FrameIndexToPlayhead(int32 InIndex) = 0;

	/** Restart the recording from the beginning. */
	virtual void RestartPlayback(int32 InIndex = INDEX_NONE) = 0;

	/** Retrieve the first frame's frame rate information. */
	virtual FFrameRate GetInitialFramerate() = 0;
};
