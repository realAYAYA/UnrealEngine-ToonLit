// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Features/IModularFeatures.h"


// IRewindDebuggerExtension 
//
// interface class for extensions which add functionality to the rewind debugger
// these will get updated on scrub/playback to handle things like updating the world state to match recorded data from that time for a particular system
//

class IRewindDebugger;

class REWINDDEBUGGERINTERFACE_API IRewindDebuggerExtension : public IModularFeature
{
public:
	static const FName ModularFeatureName;

	// called while scrubbing, playing back, or paused
	virtual void Update(float DeltaTime, IRewindDebugger* RewindDebugger) {};

	// called when recording has started
	virtual void RecordingStarted(IRewindDebugger* RewindDebugger) {};

	// called when recording has ended
	virtual void RecordingStopped(IRewindDebugger* RewindDebugger) {};
};
