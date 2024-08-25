// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "IRewindDebuggerTrackCreator.h"
#include "RewindDebuggerTrack.h"
#include "Textures/SlateIcon.h"
#include "SEventTimelineView.h"
#include "IDetailsView.h"
#include "VLogDetailsObject.h"
#include "UObject/WeakObjectPtrTemplates.h"

namespace RewindDebugger
{

class FVisualLogCategoryTrack : public FRewindDebuggerTrack
{
public:
	FVisualLogCategoryTrack(uint64 InObjectId, const FName& Category);
	virtual ~FVisualLogCategoryTrack();
	const FName GetCategory() const { return Category; }
private:
	virtual bool UpdateInternal() override;
	TSharedPtr<SWidget> GetDetailsViewInternal() override;
	virtual TSharedPtr<SWidget> GetTimelineViewInternal() override;
	virtual FSlateIcon GetIconInternal() override { return Icon; }
	virtual FText GetDisplayNameInternal() const override { return TrackName; }
	virtual uint64 GetObjectIdInternal() const override { return ObjectId; }
	virtual FName GetNameInternal() const override { return Category; }

	UVLogDetailsObject* InitializeDetailsObject();

	TSharedPtr<SEventTimelineView::FTimelineEventData> GetEventData() const;
	
	mutable TSharedPtr<SEventTimelineView::FTimelineEventData> EventData;
	mutable int EventUpdateRequested = 0;
	
	uint64 ObjectId;
	FName Category;
	FSlateIcon Icon;
	FText TrackName;
	TSharedPtr<IDetailsView> DetailsView;
	TWeakObjectPtr<UVLogDetailsObject> DetailsObjectWeakPtr;

	double PreviousScrubTime = 0.0f;
	
	enum class EFilterType
	{
		VisualLog,
		VisualLogState,
		SyncMarker,
	};
};



// Parent track 
class FVisualLogTrack : public FRewindDebuggerTrack
{
public:
	FVisualLogTrack(uint64 InObjectId);

private:
	virtual bool UpdateInternal() override;
	virtual TSharedPtr<SWidget> GetDetailsViewInternal() override;

	virtual FSlateIcon GetIconInternal() override { return Icon; }
	virtual FName GetNameInternal() const override { return "Visual Logging"; }
	virtual FText GetDisplayNameInternal() const override { return NSLOCTEXT("RewindDebugger", "VisualLoggerTrackName", "Visual Logging"); }
	virtual uint64 GetObjectIdInternal() const override { return ObjectId; }
	virtual void IterateSubTracksInternal(TFunction<void(TSharedPtr<FRewindDebuggerTrack> SubTrack)> IteratorFunction) override;

	FSlateIcon Icon;
	uint64 ObjectId;

	TArray<TSharedPtr<FVisualLogCategoryTrack>> Children;
};


class FVisualLogTrackCreator : public IRewindDebuggerTrackCreator
{
private:
	virtual FName GetTargetTypeNameInternal() const override;
	virtual FName GetNameInternal() const override;
	virtual void GetTrackTypesInternal(TArray<FRewindDebuggerTrackType>& Types) const override;
	virtual TSharedPtr<FRewindDebuggerTrack> CreateTrackInternal(uint64 ObjectId) const override;
	virtual bool HasDebugInfoInternal(uint64 ObjectId) const override;
};

}