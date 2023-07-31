// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "WidgetAnimationPlayCallbackProxy.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FWidgetAnimationResult);

UCLASS(MinimalAPI)
class UWidgetAnimationPlayCallbackProxy : public UObject
{
	GENERATED_UCLASS_BODY()

	// Called when animation has been completed
	UPROPERTY(BlueprintAssignable)
	FWidgetAnimationResult Finished;

	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", DisplayName = "Play Animation with Finished event", ShortToolTip = "Play Animation and trigger event on Finished",  ToolTip="Play Animation on widget and trigger Finish event when the animation is done."), Category = "User Interface|Animation")
	static UWidgetAnimationPlayCallbackProxy* CreatePlayAnimationProxyObject(class UUMGSequencePlayer*& Result, class UUserWidget* Widget, class UWidgetAnimation* InAnimation, float StartAtTime = 0.0f, int32 NumLoopsToPlay = 1, EUMGSequencePlayMode::Type PlayMode = EUMGSequencePlayMode::Forward, float PlaybackSpeed = 1.0f);

	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", DisplayName = "Play Animation Time Range with Finished event", ShortToolTip = "Play Animation Time Range and trigger event on Finished", ToolTip = "Play Animation Time Range on widget and trigger Finish event when the animation is done."), Category = "User Interface|Animation")
	static UWidgetAnimationPlayCallbackProxy* CreatePlayAnimationTimeRangeProxyObject(class UUMGSequencePlayer*& Result, class UUserWidget* Widget, class UWidgetAnimation* InAnimation, float StartAtTime = 0.0f, float EndAtTime = 0.0f, int32 NumLoopsToPlay = 1, EUMGSequencePlayMode::Type PlayMode = EUMGSequencePlayMode::Forward, float PlaybackSpeed = 1.0f);

private:
	class UUMGSequencePlayer* ExecutePlayAnimation(class UUserWidget* Widget, class UWidgetAnimation* InAnimation, float StartAtTime, int32 NumLoopsToPlay, EUMGSequencePlayMode::Type PlayMode, float PlaybackSpeed);
	class UUMGSequencePlayer* ExecutePlayAnimationTimeRange(class UUserWidget* Widget, class UWidgetAnimation* InAnimation, float StartAtTime, float EndAtTime, int32 NumLoopsToPlay, EUMGSequencePlayMode::Type PlayMode, float PlaybackSpeed);
	void OnSequenceFinished(class UUMGSequencePlayer& Player);
	bool OnAnimationFinished(float DeltaTime);

	FDelegateHandle OnFinishedHandle;
};
