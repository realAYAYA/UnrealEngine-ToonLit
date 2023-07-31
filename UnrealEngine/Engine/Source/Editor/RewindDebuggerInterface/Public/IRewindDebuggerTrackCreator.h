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

	private:

	virtual FName GetTargetTypeNameInternal() const { return "Object"; }
	
	virtual FName GetNameInternal() const { return FName(); }
	
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