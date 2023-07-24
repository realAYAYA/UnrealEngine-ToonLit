// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/WidgetAnimationPlayCallbackProxy.h"
#include "Animation/UMGSequencePlayer.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Types/WidgetActiveTimerDelegate.h"
#include "Containers/Ticker.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WidgetAnimationPlayCallbackProxy)

#define LOCTEXT_NAMESPACE "UMG"

UWidgetAnimationPlayCallbackProxy* UWidgetAnimationPlayCallbackProxy::CreatePlayAnimationProxyObject(class UUMGSequencePlayer*& Result, class UUserWidget* Widget, UWidgetAnimation* InAnimation, float StartAtTime, int32 NumLoopsToPlay, EUMGSequencePlayMode::Type PlayMode, float PlaybackSpeed)
{
	UWidgetAnimationPlayCallbackProxy* Proxy = NewObject<UWidgetAnimationPlayCallbackProxy>();
	Proxy->SetFlags(RF_StrongRefOnFrame);
	Result = Proxy->ExecutePlayAnimation(Widget, InAnimation, StartAtTime, NumLoopsToPlay, PlayMode, PlaybackSpeed);
	return Proxy;
}

UWidgetAnimationPlayCallbackProxy* UWidgetAnimationPlayCallbackProxy::CreatePlayAnimationTimeRangeProxyObject(class UUMGSequencePlayer*& Result, class UUserWidget* Widget, UWidgetAnimation* InAnimation, float StartAtTime, float EndAtTime, int32 NumLoopsToPlay, EUMGSequencePlayMode::Type PlayMode, float PlaybackSpeed)
{
	UWidgetAnimationPlayCallbackProxy* Proxy = NewObject<UWidgetAnimationPlayCallbackProxy>();
	Proxy->SetFlags(RF_StrongRefOnFrame);
	Result = Proxy->ExecutePlayAnimationTimeRange(Widget, InAnimation, StartAtTime, EndAtTime, NumLoopsToPlay, PlayMode, PlaybackSpeed);
	return Proxy;
}

UWidgetAnimationPlayCallbackProxy::UWidgetAnimationPlayCallbackProxy(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

class UUMGSequencePlayer* UWidgetAnimationPlayCallbackProxy::ExecutePlayAnimation(class UUserWidget* Widget, UWidgetAnimation* InAnimation, float StartAtTime, int32 NumLoopsToPlay, EUMGSequencePlayMode::Type PlayMode, float PlaybackSpeed)
{
	if (!Widget)
	{
		return nullptr;
	}

	UUMGSequencePlayer* Player = Widget->PlayAnimation(InAnimation, StartAtTime, NumLoopsToPlay, PlayMode, PlaybackSpeed);
	if (Player)
	{
		Player->OnSequenceFinishedPlaying().AddUObject(this, &UWidgetAnimationPlayCallbackProxy::OnSequenceFinished);
	}

	return Player;
}

class UUMGSequencePlayer* UWidgetAnimationPlayCallbackProxy::ExecutePlayAnimationTimeRange(class UUserWidget* Widget, UWidgetAnimation* InAnimation, float StartAtTime, float EndAtTime, int32 NumLoopsToPlay, EUMGSequencePlayMode::Type PlayMode, float PlaybackSpeed)
{
	if (!Widget)
	{
		return nullptr;
	}

	UUMGSequencePlayer* Player = Widget->PlayAnimationTimeRange(InAnimation, StartAtTime, EndAtTime, NumLoopsToPlay, PlayMode, PlaybackSpeed);
	if (Player)
	{
		OnFinishedHandle = Player->OnSequenceFinishedPlaying().AddUObject(this, &UWidgetAnimationPlayCallbackProxy::OnSequenceFinished);
	}

	return Player;
}

void UWidgetAnimationPlayCallbackProxy::OnSequenceFinished(class UUMGSequencePlayer& Player)
{
	Player.OnSequenceFinishedPlaying().Remove(OnFinishedHandle);

	// We delay the Finish broadcast to next frame.
	FTSTicker::FDelegateHandle TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UWidgetAnimationPlayCallbackProxy::OnAnimationFinished));
}


bool UWidgetAnimationPlayCallbackProxy::OnAnimationFinished(float /*DeltaTime*/)
{
	Finished.Broadcast();

	// Returning false, disable the ticker.
	return false;
}
#undef LOCTEXT_NAMESPACE

