// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "UObject/ObjectPtr.h"
#include "UObject/SoftObjectPtr.h"
#include "AvaPlayableGroupManager.generated.h"

class UAvaGameInstance;
class UAvaPlayableGroup;
class UAvaPlayableGroupManager;

/**
 * Manager for the shared playable groups per channel.
 */
UCLASS()
class UAvaPlayableGroupChannelManager : public UObject
{
	GENERATED_BODY()

public:
	UAvaPlayableGroup* GetOrCreateSharedLevelGroup(bool bInIsRemoteProxy);

	UAvaPlayableGroupManager* GetPlayableGroupManager() const;
	
protected:
	//~ Begin UObject
	virtual void BeginDestroy() override;
	//~ End UObject

	void Shutdown();
	
protected:
	FName ChannelName;

	/**
	 * This is the shared playable group for this channel.
	 * 
	 * For now all the levels go in the same group for a given channel.
	 * This a weak ptr because it is only a cache. The ownership
	 * of the group is with the playable objects.
	 */
	TWeakObjectPtr<UAvaPlayableGroup> SharedLevelGroupWeak;

	/**
	 * For remote proxy playables, we need a specialized group
	 * that doesn't implement the local game instance but still
	 * provides the correct logic to emulate functionality.
	 */
	TWeakObjectPtr<UAvaPlayableGroup> SharedRemoteProxyLevelGroupWeak;

	friend class UAvaPlayableGroupManager;
};

/**
 * Manager for the shared playable groups.
 * The scope of this manager is either global (in the global playback manager)
 * or for a given playback manager.
 */
UCLASS()
class UAvaPlayableGroupManager : public UObject
{
	GENERATED_BODY()
	
public:
	void Init();

	void Shutdown();

	void Tick(double InDeltaSeconds);

	UAvaPlayableGroupChannelManager* FindChannelManager(const FName& InChannelName) const
	{
		const TObjectPtr<UAvaPlayableGroupChannelManager>* ChannelManager = ChannelManagers.Find(InChannelName);
		return ChannelManager ? *ChannelManager : nullptr;
	}

	UAvaPlayableGroupChannelManager* FindOrAddChannelManager(const FName& InChannelName);

	UAvaPlayableGroup* GetOrCreateSharedLevelGroup(const FName& InChannelName, bool bInIsRemoteProxy)
	{
		UAvaPlayableGroupChannelManager* ChannelManager = FindOrAddChannelManager(InChannelName);
		return ChannelManager ? ChannelManager->GetOrCreateSharedLevelGroup(bInIsRemoteProxy) : nullptr;
	}

	void RegisterForLevelStreamingUpdate(UAvaPlayableGroup* InPlayableGroup);
	void UnregisterFromLevelStreamingUpdate(UAvaPlayableGroup* InPlayableGroup);

	void RegisterForTransitionTicking(UAvaPlayableGroup* InPlayableGroup);
	void UnregisterFromTransitionTicking(UAvaPlayableGroup* InPlayableGroup);

protected:
	//~ Begin UObject
	virtual void BeginDestroy() override;
	//~ End UObject

	void OnGameInstanceEndPlay(UAvaGameInstance* InGameInstance, FName InChannelName);

	void UpdateLevelStreaming();
	
	void TickTransitions(double InDeltaSeconds);
	
protected:
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UAvaPlayableGroupChannelManager>> ChannelManagers;

	bool bIsUpdatingStreaming = false;
	TSet<TWeakObjectPtr<UAvaPlayableGroup>> GroupsToUpdateStreaming;

	bool bIsTickingTransitions = false;
	TSet<TWeakObjectPtr<UAvaPlayableGroup>> GroupsToTickTransitions;
};