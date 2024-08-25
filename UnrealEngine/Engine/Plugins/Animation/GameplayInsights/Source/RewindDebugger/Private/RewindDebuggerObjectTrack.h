// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "RewindDebuggerTrack.h"
#include "SEventTimelineView.h"


namespace RewindDebugger
{
	
class IRewindDebuggerTrackCreator;

struct FTrackCreatorAndTrack
{
	const IRewindDebuggerTrackCreator* Creator;
	TSharedPtr<FRewindDebuggerTrack> Track;
};

class FRewindDebuggerObjectTrack : public FRewindDebuggerTrack
{
public:

	FRewindDebuggerObjectTrack(uint64 InObjectId, const FString& InObjectName, bool bInAddController = false);

	TSharedPtr<SEventTimelineView::FTimelineEventData> GetExistenceRange() const { return ExistenceRange; }

private:
	virtual TSharedPtr<SWidget> GetTimelineViewInternal() override;
	virtual bool UpdateInternal() override;
	virtual void IterateSubTracksInternal(TFunction<void(TSharedPtr<FRewindDebuggerTrack> SubTrack)> IteratorFunction) override;
	
	virtual FName GetNameInternal() const override { return ""; }
	virtual FSlateIcon GetIconInternal() override { return Icon; }
	virtual FText GetDisplayNameInternal() const override;
	virtual uint64 GetObjectIdInternal() const override { return ObjectId; }
	virtual bool HasDebugDataInternal() const override { return false; }
	virtual bool HandleDoubleClickInternal() override;
	
	mutable FText DisplayName;
	FString ObjectName;
	FSlateIcon Icon;
	TSharedPtr<SEventTimelineView::FTimelineEventData> ExistenceRange;
	uint64 ObjectId;
	TArray<FTrackCreatorAndTrack> TrackChildren;
	TArray<TSharedPtr<FRewindDebuggerTrack>> Children;

	bool bAddController;
	mutable bool bDisplayNameValid;
	
};

}