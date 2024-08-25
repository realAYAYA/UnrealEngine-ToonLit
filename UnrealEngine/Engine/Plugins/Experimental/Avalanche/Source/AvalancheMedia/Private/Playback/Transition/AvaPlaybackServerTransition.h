// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Playable/AvaPlayable.h"
#include "Playback/Transition/AvaPlaybackTransition.h"
#include "UObject/Object.h"
#include "AvaPlaybackServerTransition.generated.h"

class FAvaPlaybackInstance;
class FAvaPlaybackServer;
class UAvaPlayableTransition;
class UAvaPlaybackGraph;

/**
 * Class for creating and tracking playback graph instance transitions on the server.
 * It is responsible for creating the playable transition object when requested from
 * the playback graphs.
 *
 * This class handles each playback graph instance as a single playable. It is
 * meant to be used by the playback server primarily.
 */
UCLASS()
class UAvaPlaybackServerTransition : public UAvaPlaybackTransition
{
	GENERATED_BODY()
	
public:
	void SetChannelName(const FName& InChannelName) { ChannelName = InChannelName; }
	void SetTransitionId(const FGuid& InTransitionId) { TransitionId = InTransitionId; }
	void SetClientName(const FString& InClientName) { ClientName = InClientName; }
	void SetUnloadDiscardedInstances(bool bInUnloadDiscardedInstances) { bUnloadDiscardedInstances = bInUnloadDiscardedInstances; }
	void SetTransitionFlags(EAvaPlayableTransitionFlags InTransitionFlags) { TransitionFlags = InTransitionFlags; }
	void SetEnterInstanceIds(const TArray<FGuid>& InInstanceIds) { EnterInstanceIds = InInstanceIds; }
	void SetEnterValues(const TArray<FAvaPlayableRemoteControlValues>& InEnterValues);
	bool AddEnterInstance(const TSharedPtr<FAvaPlaybackInstance>& InPlaybackInstance);
	bool AddPlayingInstance(const TSharedPtr<FAvaPlaybackInstance>& InPlaybackInstance);
	bool AddExitInstance(const TSharedPtr<FAvaPlaybackInstance>& InPlaybackInstance);

	void TryResolveInstances(const FAvaPlaybackServer& InPlaybackServer);
	
	//~ Begin IAvaPlayableVisibilityConstraint
	virtual bool IsVisibilityConstrained(const UAvaPlayable* InPlayable) const override;
	//~ End IAvaPlayableVisibilityConstraint
	
	//~ Begin UAvaPlaybackTransition
	virtual bool CanStart(bool& bOutShouldDiscard) const override;
	virtual void Start() override;
	virtual void Stop() override;
	virtual bool IsRunning() const override;
	//~ End UAvaPlaybackTransition

	/** Returns the channel this transition is happening in. A transition can only have instances within the same channel. */
	FName GetChannelName() const { return ChannelName; }

	FString GetPrettyTransitionInfo() const;
	FString GetBriefTransitionDescription() const;
	
protected:
	TSharedPtr<FAvaPlaybackInstance> FindInstanceForPlayable(const UAvaPlayable* InPlayable);

	void OnTransitionEvent(UAvaPlayable* InPlayable, UAvaPlayableTransition* InTransition, EAvaPlayableTransitionEventFlags InTransitionFlags);
	void OnPlayableCreated(UAvaPlaybackGraph* InPlayback, UAvaPlayable* InPlayable);
	
	void MakePlayableTransition();

	void LogDetailedTransitionInfo() const;

	void RegisterToPlayableTransitionEvent();
	void UnregisterFromPlayableTransitionEvent() const;

	bool AddPlaybackInstance(const TSharedPtr<FAvaPlaybackInstance>& InPlaybackInstance, TArray<TWeakPtr<FAvaPlaybackInstance>>& OutPlaybackInstancesWeak);	
	void UpdateChannelName(const FAvaPlaybackInstance* InPlaybackInstance);

protected:
	FString ClientName;
	FName ChannelName;
	FGuid TransitionId;
	bool bUnloadDiscardedInstances = false;
	EAvaPlayableTransitionFlags TransitionFlags = EAvaPlayableTransitionFlags::None;
	
	TArray<FGuid> EnterInstanceIds;

	TArray<TWeakPtr<FAvaPlaybackInstance>> EnterPlaybackInstancesWeak;
	TArray<TWeakPtr<FAvaPlaybackInstance>> PlayingPlaybackInstancesWeak;
	TArray<TWeakPtr<FAvaPlaybackInstance>> ExitPlaybackInstancesWeak;
	TArray<TSharedPtr<FAvaPlayableRemoteControlValues>> EnterValues;
	
	UPROPERTY(Transient)
	TObjectPtr<UAvaPlayableTransition> PlayableTransition;
};