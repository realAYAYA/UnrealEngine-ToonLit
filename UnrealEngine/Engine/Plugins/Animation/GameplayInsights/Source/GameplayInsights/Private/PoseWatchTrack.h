// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#if WITH_EDITOR

#include "IAnimationProvider.h"
#include "IRewindDebuggerTrackCreator.h"
#include "RewindDebuggerTrack.h"
#include "Textures/SlateIcon.h"
#include "SSegmentedTimelineView.h"

class UPoseWatchPoseElement;
class UAnimBlueprintGeneratedClass;

namespace RewindDebugger
{

class FPoseWatchTrack : public FRewindDebuggerTrack
{
public:
	struct FPoseWatchTrackId
	{
		uint64 NameId;

		bool operator == (const FPoseWatchTrackId& Other) const
		{
			return NameId == Other.NameId;
		}

		bool operator <(const FPoseWatchTrack::FPoseWatchTrackId& Other) const
		{
			return NameId < Other.NameId; 
		}		
	};

	FPoseWatchTrack(uint64 InObjectId, const FPoseWatchTrackId& TrackId);
	const FPoseWatchTrackId& GetPoseWatchTrackId() const { return PostWatchTrackId; }

private:
	virtual bool UpdateInternal() override;
	virtual TSharedPtr<SWidget> GetTimelineViewInternal() override;

	virtual FSlateIcon GetIconInternal() override { return Icon; }
	virtual FText GetDisplayNameInternal() const override;
	virtual uint64 GetObjectIdInternal() const override { return ObjectId; }
	
	TSharedPtr<SSegmentedTimelineView::FSegmentData> GetSegmentData() const;
	
	uint64 ObjectId;
	FPoseWatchTrackId PostWatchTrackId;
	FSlateIcon Icon;
	FText TrackName;

	const UPoseWatchPoseElement* PoseWatchOwner;
	TSharedPtr<SSegmentedTimelineView::FSegmentData> EnabledSegments;
};


// Parent track of all the pose watches
class FPoseWatchesTrack : public FRewindDebuggerTrack
{
public:
	FPoseWatchesTrack(uint64 InObjectId);

private:
	virtual bool UpdateInternal() override;
	virtual void IterateSubTracksInternal(TFunction<void(TSharedPtr<FRewindDebuggerTrack> SubTrack)> IteratorFunction) override;

	virtual FName GetNameInternal() const override { return "PoseWatches"; }
	virtual FText GetDisplayNameInternal() const override { return NSLOCTEXT("RewindDebugger", "PoseWatchesTrackName", "Pose Watches"); }
	virtual uint64 GetObjectIdInternal() const override { return ObjectId; }
	virtual FSlateIcon GetIconInternal() override { return Icon; }

	FSlateIcon Icon;
	uint64 ObjectId;

	class UAnimBlueprintGeneratedClass* AnimBPGenClass;

	TArray<TSharedPtr<FPoseWatchTrack>> Children;
};


class FPoseWatchesTrackCreator : public IRewindDebuggerTrackCreator
{
private:
	virtual FName GetTargetTypeNameInternal() const;
	virtual FName GetNameInternal() const override;
	virtual TSharedPtr<FRewindDebuggerTrack> CreateTrackInternal(uint64 ObjectId) const override;
	virtual bool HasDebugInfoInternal(uint64 ObjectId) const override;
};

}

#endif