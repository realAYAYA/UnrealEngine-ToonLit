// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Playable/Transition/AvaPlayableTransition.h"
#include "AvaPlayableRemoteTransition.generated.h"

class IAvaPlaybackClient;

namespace UE::AvaPlaybackClient::Delegates
{
	struct FPlaybackTransitionEventArgs;
}

UCLASS()
class UAvaPlayableRemoteTransition : public UAvaPlayableTransition
{
	GENERATED_BODY()
	
public:
	virtual ~UAvaPlayableRemoteTransition() override;

	void SetChannelName(const FName& InChannelName) { ChannelName = InChannelName; }
	
	//~ Begin UAvaPlayableTransition
	virtual bool Start() override;
	virtual void Stop() override;
	virtual bool IsRunning() const override;
	//~ End UAvaPlayableTransition

protected:
	void HandlePlaybackTransitionEvent(IAvaPlaybackClient& InPlaybackClient,
		const UE::AvaPlaybackClient::Delegates::FPlaybackTransitionEventArgs& InArgs);

	void RegisterToPlaybackClientDelegates();
	void UnregisterFromPlaybackClientDelegates() const;

protected:
	FGuid TransitionId;
	FName ChannelName;
};