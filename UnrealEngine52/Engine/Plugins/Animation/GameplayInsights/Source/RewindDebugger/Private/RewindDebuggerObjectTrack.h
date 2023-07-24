// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "RewindDebuggerTrack.h"
#include "SSegmentedTimelineView.h"

namespace RewindDebugger
{

class FRewindDebuggerObjectTrack : public FRewindDebuggerTrack
{
public:

	FRewindDebuggerObjectTrack(uint64 InObjectId, const FString& InObjectName, bool bInAddController = false)
		: ObjectName(InObjectName)
		, ObjectId(InObjectId)
		, bAddController(bInAddController)
		, bDisplayNameValid(false)
	{
		ExistenceRange = MakeShared<SSegmentedTimelineView::FSegmentData>();
		ExistenceRange->Segments.SetNumUninitialized(1);
	}

	TSharedPtr<SSegmentedTimelineView::FSegmentData> GetExistenceRange() const { return ExistenceRange; }

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
	TSharedPtr<SSegmentedTimelineView::FSegmentData> ExistenceRange;
	uint64 ObjectId;
	TArray<TSharedPtr<FRewindDebuggerTrack>> Children;

	bool bAddController;
	mutable bool bDisplayNameValid;
};

}