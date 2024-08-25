// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Playable/AvaPlayable.h"
#include "AvaPlayableRemoteProxy.generated.h"

class IAvaPlaybackClient;

namespace UE::AvaPlaybackClient::Delegates
{
	struct FPlaybackSequenceEventArgs;
}

UCLASS(NotBlueprintable, BlueprintType, ClassGroup = "Motion Design Playable",
	meta = (DisplayName = "Motion Design Remote Proxy Playable"))
class UAvaPlayableRemoteProxy : public UAvaPlayable
{
	GENERATED_BODY()
public:
	FName GetPlayingChannelFName() const { return PlayingChannelFName; }
	
	//~ Begin UAvaPlayable
	virtual bool LoadAsset(const FAvaSoftAssetPtr& InSourceAsset, bool bInInitiallyVisible) override;
	virtual bool UnloadAsset() override;
	virtual const FSoftObjectPath& GetSourceAssetPath() const override { return SourceAssetPath; }
	virtual EAvaPlayableStatus GetPlayableStatus() const override;
	virtual IAvaSceneInterface* GetSceneInterface() const override;
	virtual EAvaPlayableCommandResult ExecuteAnimationCommand(EAvaPlaybackAnimAction InAnimAction, const FAvaPlaybackAnimPlaySettings& InAnimPlaySettings) override;
	virtual EAvaPlayableCommandResult UpdateRemoteControlCommand(const TSharedRef<FAvaPlayableRemoteControlValues>& InRemoteControlValues) override;
	virtual bool ApplyCamera() override;
	virtual bool IsRemoteProxy() const override { return true; }
	virtual void SetUserData(const FString& InUserData) override;

protected:
	virtual bool InitPlayable(const FPlayableCreationInfo& InPlayableInfo) override;
	virtual void OnPlay() override;
	virtual void OnEndPlay() override;
	//~ End UAvaPlayable

	//~ Begin UObject
	virtual void BeginDestroy() override;
	//~ End UObject

	void RegisterClientEventHandlers();
	void UnregisterClientEventHandlers() const;

	static TArray<FString> GetOnlineServerForChannel(const FName& InChannelName);
	
	void HandleAvaPlaybackSequenceEvent(IAvaPlaybackClient& InPlaybackClient,
		const UE::AvaPlaybackClient::Delegates::FPlaybackSequenceEventArgs& InEventArgs);
	
protected:
	/** Channel name this playable is playing on. */
	FName PlayingChannelFName;
	FString PlayingChannelName;

	FSoftObjectPath SourceAssetPath;
};
