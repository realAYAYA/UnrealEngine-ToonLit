// Copyright Epic Games, Inc. All Rights Reserved.

#include "Streaming/LevelStreamingDelegates.h"

TMulticastDelegate<void(
	UWorld*, 
	const ULevelStreaming*, 
	ULevel* LevelIfLoaded,
	ELevelStreamingState Current,
	ELevelStreamingTargetState PrevTarget, 
	ELevelStreamingTargetState NewTarget)> FLevelStreamingDelegates::OnLevelStreamingTargetStateChanged;

TMulticastDelegate<void(
	UWorld*, 
	const ULevelStreaming*,
	ULevel* LevelIfLoaded,
	ELevelStreamingState PreviousState,
	ELevelStreamingState NewState
	)> FLevelStreamingDelegates::OnLevelStreamingStateChanged;

TMulticastDelegate<void(UWorld*, const ULevelStreaming*, ULevel* LoadedLevel)> FLevelStreamingDelegates::OnLevelBeginMakingVisible;
TMulticastDelegate<void(UWorld*, const ULevelStreaming*, ULevel* LoadedLevel)> FLevelStreamingDelegates::OnLevelBeginMakingInvisible;

