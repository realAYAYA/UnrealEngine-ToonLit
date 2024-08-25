// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IRewindDebuggerView.h"
#include "Textures/SlateIcon.h"

namespace TraceServices
{
	class IAnalysisSession;
}

class IRewindDebugger;

namespace RewindDebugger
{
	
class FRewindDebuggerTrack;

struct FRewindDebuggerTrackType
{
	FName Name;
	FText DisplayName;
};

// Interface class which creates tracks
class REWINDDEBUGGERINTERFACE_API IRewindDebuggerTrackCreator : public IModularFeature
{
	public:
	virtual ~IRewindDebuggerTrackCreator() {};
	
	static const FName ModularFeatureName;

	// returns the type of UObject this track creator can create tracks for
	FName GetTargetTypeName() const
	{
		return GetTargetTypeNameInternal();
	}

	// returns an identifying Name for this type of track
	FName GetName() const
	{
		return GetNameInternal();
	}

	// returns an integer.  Higher values will show higher in the track list (default is 0)
	int32 GetSortOrderPriority() const
	{
		return GetSortOrderPriorityInternal();
	}

	// optional additional filter, to prevent debug views from being listed if they have no data
	bool HasDebugInfo(uint64 ObjectId) const
	{
		return HasDebugInfoInternal(ObjectId);
	};
	
	// Create a track which will be shown in the timeline view and tree view, as a child track of the Object
	TSharedPtr<RewindDebugger::FRewindDebuggerTrack> CreateTrack(uint64 ObjectId) const
	{
		return CreateTrackInternal(ObjectId);
	}
	
	void GetTrackTypes(TArray<FRewindDebuggerTrackType>& Types) const
	{
		return GetTrackTypesInternal(Types);
	};

	private:

	// get the UObject type name this Creator will create child tracks for
	virtual FName GetTargetTypeNameInternal() const { return "Object"; }

	// get the Name (unique identifier) for this Creator
	virtual FName GetNameInternal() const { return FName(); }

	// An integer to override sort order for tracks created by this Creator (Higher priority will make tracks appear higher in the list)
	virtual int32 GetSortOrderPriorityInternal() const { return 0; }

	// Add track types that this Creator will create to the track type list
	virtual void GetTrackTypesInternal(TArray<FRewindDebuggerTrackType>& Types) const { };

	// Returns true if this creator's track should be shown for this Object, false if there is no data and they should be hidden.
	virtual bool HasDebugInfoInternal(uint64 ObjectId) const
	{
		return true;
	};
	
	virtual TSharedPtr<FRewindDebuggerTrack> CreateTrackInternal(uint64 ObjectId) const
	{
		return TSharedPtr<FRewindDebuggerTrack>();
	}
};
	
}