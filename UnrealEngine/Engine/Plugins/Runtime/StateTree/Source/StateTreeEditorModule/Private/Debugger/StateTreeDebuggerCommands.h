// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_STATETREE_DEBUGGER

#include "Framework/Commands/Commands.h"

/**
 * StateTree Debugger command set.
 */
class FStateTreeDebuggerCommands : public TCommands<FStateTreeDebuggerCommands>
{
public:
	FStateTreeDebuggerCommands();

	// TCommands<> overrides
	virtual void RegisterCommands() override;

	TSharedPtr<FUICommandInfo> EnableOnEnterStateBreakpoint;
	TSharedPtr<FUICommandInfo> EnableOnExitStateBreakpoint;
	TSharedPtr<FUICommandInfo> StartRecording;
	TSharedPtr<FUICommandInfo> StopRecording;
	TSharedPtr<FUICommandInfo> PreviousFrameWithStateChange;
	TSharedPtr<FUICommandInfo> PreviousFrameWithEvents;
	TSharedPtr<FUICommandInfo> NextFrameWithEvents;
	TSharedPtr<FUICommandInfo> NextFrameWithStateChange;
	TSharedPtr<FUICommandInfo> ResumeDebuggerAnalysis;
	TSharedPtr<FUICommandInfo> ResetTracks;
};

#endif // WITH_STATETREE_DEBUGGER