// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Delegates/DelegateCombinations.h"
#include "IVirtualCameraController.h"
#include "Subsystems/EngineSubsystem.h"
#include "VirtualCameraSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnStreamStarted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnStreamStopped);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSelectedAnyActor, AActor*, SelectedActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSelectedViewportActor, AActor*, SelectedActor);

UCLASS(BlueprintType, Category = "VirtualCamera", DisplayName = "VirtualCameraSubsystem")
class VIRTUALCAMERA_API UVirtualCameraSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()
	
public:

	UVirtualCameraSubsystem();

public:
	UFUNCTION(BlueprintCallable, Category = "VirtualCamera | Streaming")
	bool StartStreaming();

	UFUNCTION(BlueprintCallable, Category = "VirtualCamera | Streaming")
	bool StopStreaming();

	UFUNCTION(BlueprintCallable, Category = "VirtualCamera | Streaming")
	bool IsStreaming() const;

#if WITH_EDITOR
	UFUNCTION()
	void HandleSelectionChangedEvent(UObject* SelectedObject);

	UFUNCTION()
	void HandleSelectObjectEvent(UObject* SelectedObject);
#endif

	UFUNCTION(BlueprintCallable, Category = "VirtualCamera")
	TScriptInterface<IVirtualCameraController> GetVirtualCameraController() const;
	
	UFUNCTION(BlueprintCallable, Category = "VirtualCamera")
	void SetVirtualCameraController(TScriptInterface<IVirtualCameraController> VirtualCamera);

	UPROPERTY(BlueprintReadOnly, Category = "VirtualCamera")
	TObjectPtr<ULevelSequencePlaybackController> SequencePlaybackController;

	UPROPERTY(BlueprintAssignable, Category = "VirtualCamera | Streaming")
	FOnStreamStarted OnStreamStartedDelegate;

	UPROPERTY(BlueprintAssignable, Category = "VirtualCamera | Streaming")
	FOnStreamStopped OnStreamStoppedDelegate;
	
	UPROPERTY(BlueprintAssignable, Category = "VirtualCamera | Streaming")
	FOnSelectedAnyActor OnSelectedAnyActorDelegate; 

	UPROPERTY(BlueprintAssignable, Category = "VirtualCamera | Streaming")
	FOnSelectedViewportActor OnSelectedActorInViewportDelegate;



private:

	UPROPERTY(Transient) 
	TScriptInterface<IVirtualCameraController> ActiveCameraController;

	bool bIsStreaming;
};
