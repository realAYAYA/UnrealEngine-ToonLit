// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IRewindDebugger.h"
#include "Widgets/SWidget.h"
#include "Textures/SlateIcon.h"

namespace TraceServices
{
	class IAnalysisSession;
}

struct FToolMenuSection;

namespace RewindDebugger
{
	
class FRewindDebuggerTrack
{
public:
	FRewindDebuggerTrack()
	{
	}

	virtual ~FRewindDebuggerTrack()
	{
	}

	bool GetIsExpanded()
	{
		return bExpanded;
	}

	void SetIsExpanded(bool bIsExpanded)
	{
		bExpanded = bIsExpanded;
	}
	
	bool GetIsSelected()
	{
		return bSelected;
	}

	void SetIsSelected(bool bIsSelected)
	{
		bSelected = bIsSelected;
	}
    		
	bool GetIsTreeHovered()
	{
		return bTreeHovered;
	}
	
	void SetIsTreeHovered(bool bIsHovered)
   	{
   		bTreeHovered = bIsHovered;
   	}	
		
	bool GetIsTrackHovered()
	{
		return bTrackHovered;
	}
	
	void SetIsTrackHovered(bool bIsHovered)
   	{
   		bTrackHovered = bIsHovered;
   	}
	
	bool GetIsHovered()
	{
		return bTrackHovered || bTreeHovered;
	}

	// Update should do work to compute children etc for the current time range.  Return true if children have changed.
	bool Update()
	{
		return UpdateInternal();
	}

	// Get a widget to show in the timeline view for this track
	TSharedPtr<SWidget> GetTimelineView()
	{
		return GetTimelineViewInternal();
	}

	// Get a widget to show in the details tab, when this track is selected
	TSharedPtr<SWidget> GetDetailsView()
	{
		return GetDetailsViewInternal();
	}
	
	// unique name for track (must match creator name if track is created by an IRewindDebuggerViewCreator) 
	FName GetName() const
	{
		return GetNameInternal();
	}

	// icon to display in the tree view
	FSlateIcon GetIcon()
	{
		return GetIconInternal();
	}

	// display name for track in Tree View
	FText GetDisplayName() const
	{
		return GetDisplayNameInternal();
	}

	// insights object id for an object associated with this track
	uint64 GetObjectId() const
	{
		return GetObjectIdInternal();
	}

	// iterate over all sub-tracks of this track and call Iterator function
	void IterateSubTracks(TFunction<void(TSharedPtr<FRewindDebuggerTrack> SubTrack)> IteratorFunction)
	{
		IterateSubTracksInternal(IteratorFunction);
	}

	// returns true for tracks that contain debug data (used for filtering out parts of the hierarchy with no useful information in them)
	bool HasDebugData() const
	{
		return HasDebugDataInternal();
	}

	// Called when a track is double clicked.  Returns true if the track handled the double click
	bool HandleDoubleClick()
	{
		return HandleDoubleClickInternal();
	}

	bool IsVisible() const { return bVisible; }
	void SetIsVisible(bool bInIsVisible) { bVisible = bInIsVisible; }

	// Called to generate context menu for the current selected track
	virtual void BuildContextMenu(FToolMenuSection& InMenuSection) {};

private:

	virtual bool UpdateInternal() { return false; }
	virtual TSharedPtr<SWidget> GetTimelineViewInternal() { return TSharedPtr<SWidget>(); }
	virtual TSharedPtr<SWidget> GetDetailsViewInternal() { return TSharedPtr<SWidget>(); }
	virtual FSlateIcon GetIconInternal() { return FSlateIcon(); }
	virtual FName GetNameInternal() const { return ""; }
	virtual FText GetDisplayNameInternal() const { return FText(); }
	virtual uint64 GetObjectIdInternal() const { return 0; }
	virtual bool HasDebugDataInternal() const { return true; }
	virtual void IterateSubTracksInternal(TFunction<void(TSharedPtr<FRewindDebuggerTrack> SubTrack)> IteratorFunction) { }

	virtual bool HandleDoubleClickInternal()
	{
		IRewindDebugger::Instance()->OpenDetailsPanel();
		return true;
	};

	bool bSelected : 1 = false;
	bool bTrackHovered : 1 = false;
	bool bTreeHovered : 1 = false;
	bool bExpanded : 1 = true;
	bool bVisible : 1 = true;
};
	
}