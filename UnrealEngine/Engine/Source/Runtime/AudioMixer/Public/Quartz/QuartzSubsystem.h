// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Subsystems/WorldSubsystem.h"
#include "Quartz/AudioMixerClockManager.h"
#include "Sound/QuartzQuantizationUtilities.h"

#include "QuartzSubsystem.generated.h"


// forwards
namespace Audio
{
	class FMixerDevice;
	class FQuartzClockManager;

	template<class ListenerType>
	class TQuartzShareableCommandQueue;

}

class FQuartzTickableObject;
class UQuartzClockHandle;
using MetronomeCommandQueuePtr = TSharedPtr<Audio::TQuartzShareableCommandQueue<FQuartzTickableObject>, ESPMode::ThreadSafe>;



struct AUDIOMIXER_API FQuartzTickableObjectsManager : public FQuartLatencyTracker
{
public:
	void Tick(float DeltaTime);
	bool IsTickable() const;
	void SubscribeToQuartzTick(FQuartzTickableObject* InObjectToTick);
	void UnsubscribeFromQuartzTick(FQuartzTickableObject* InObjectToTick);

private:
	// list of objects needing to be ticked by Quartz
	TArray<FQuartzTickableObject *> QuartzTickSubscribers;

	// index to track the next subscriber to tick (if updates are being amortized across multiple UObject Ticks)
	int32 UpdateIndex{ 0 };
};


UCLASS(DisplayName = "Quartz")
class AUDIOMIXER_API UQuartzSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	// ctor/dtor
	UQuartzSubsystem() = default;
	virtual ~UQuartzSubsystem() override = default;

	//~ Begin UWorldSubsystem Interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;
	void virtual BeginDestroy() override;
	//~ End UWorldSubsystem Interface

	//~ Begin FTickableGameObject Interface
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;
	//~ End FTickableGameObject Interface

	// these calls are forwarded to the internal FQuartzTickableObjectsManager
	void SubscribeToQuartzTick(FQuartzTickableObject* InObjectToTick);
	void UnsubscribeFromQuartzTick(FQuartzTickableObject* InObjectToTick);

	// get C++ handle (proxy) to a clock if it exists
	Audio::FQuartzClockProxy GetProxyForClock(FName ClockName) const;

	// allow an external clock (not ticked by the Audio Mixer or QuartzSubsystem) to be accessible via this subsystem
	void AddProxyForExternalClock(const Audio::FQuartzClockProxy& InProxy);

	// static methods
	static UQuartzSubsystem* Get(const UWorld* const World);

	// Helper functions for initializing quantized command initialization struct (to consolidate eyesore)
	static Audio::FQuartzQuantizedRequestData CreateRequestDataForTickRateChange(UQuartzClockHandle* InClockHandle, const FOnQuartzCommandEventBP& InDelegate, const Audio::FQuartzClockTickRate& InNewTickRate, const FQuartzQuantizationBoundary& InQuantizationBoundary);
	static Audio::FQuartzQuantizedRequestData CreateRequestDataForTransportReset(UQuartzClockHandle* InClockHandle, const FQuartzQuantizationBoundary& InQuantizationBoundary, const FOnQuartzCommandEventBP& InDelegate);
	static Audio::FQuartzQuantizedRequestData CreateRequestDataForStartOtherClock(UQuartzClockHandle* InClockHandle, FName InClockToStart, const FQuartzQuantizationBoundary& InQuantizationBoundary, const FOnQuartzCommandEventBP& InDelegate);
	static Audio::FQuartzQuantizedRequestData CreateRequestDataForSchedulePlaySound(UQuartzClockHandle* InClockHandle, const FOnQuartzCommandEventBP& InDelegate, const FQuartzQuantizationBoundary& InQuantizationBoundary);

	// DEPRECATED HELPERS: non-static versions of the above CreateDataFor...() functions
	UE_DEPRECATED(5.1, "Use the static (CreateRequestDataFor) version of this function instead")
	Audio::FQuartzQuantizedRequestData CreateDataForTickRateChange(UQuartzClockHandle* InClockHandle, const FOnQuartzCommandEventBP& InDelegate, const Audio::FQuartzClockTickRate& InNewTickRate, const FQuartzQuantizationBoundary& InQuantizationBoundary);

	UE_DEPRECATED(5.1, "Use the static (CreateRequestDataFor) version of this function instead")
	Audio::FQuartzQuantizedRequestData CreateDataForTransportReset(UQuartzClockHandle* InClockHandle, const FQuartzQuantizationBoundary& InQuantizationBoundary, const FOnQuartzCommandEventBP& InDelegate);
	
	UE_DEPRECATED(5.1, "Use the static (CreateRequestDataFor) version of this function instead")
	Audio::FQuartzQuantizedRequestData CreateDataForStartOtherClock(UQuartzClockHandle* InClockHandle, FName InClockToStart, const FQuartzQuantizationBoundary& InQuantizationBoundary, const FOnQuartzCommandEventBP& InDelegate);

	UE_DEPRECATED(5.1, "Use the static (CreateRequestDataFor) version of this function instead")
	Audio::FQuartzQuantizedRequestData CreateDataDataForSchedulePlaySound(UQuartzClockHandle* InClockHandle, const FOnQuartzCommandEventBP& InDelegate, const FQuartzQuantizationBoundary& InQuantizationBoundary);

	UFUNCTION(BlueprintCallable, Category = "Quartz Clock Handle")
	bool IsQuartzEnabled();

	// Clock Creation
	// create a new clock (or return handle if clock already exists)
	UFUNCTION(BlueprintCallable, Category = "Quartz Clock Handle", meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "bUseAudioEngineClockManager"))
	UQuartzClockHandle* CreateNewClock(const UObject* WorldContextObject, FName ClockName, FQuartzClockSettings InSettings, bool bOverrideSettingsIfClockExists = false, bool bUseAudioEngineClockManager = true);

	// delete an existing clock given its name
	UFUNCTION(BlueprintCallable, Category = "Quartz Clock Handle", meta = (WorldContext = "WorldContextObject"))
	void DeleteClockByName(const UObject* WorldContextObject, FName ClockName);

	// delete an existing clock given its clock handle
	UFUNCTION(BlueprintCallable, Category = "Quartz Clock Handle", meta = (WorldContext = "WorldContextObject"))
	void DeleteClockByHandle(const UObject* WorldContextObject, UPARAM(ref) UQuartzClockHandle*& InClockHandle);

	// get handle for existing clock
	UFUNCTION(BlueprintCallable, Category = "Quartz Clock Handle", meta = (WorldContext = "WorldContextObject"))
	UQuartzClockHandle* GetHandleForClock(const UObject* WorldContextObject, FName ClockName);

	// returns true if the clock exists
	UFUNCTION(BlueprintCallable, Category = "Quartz Clock Handle", meta = (WorldContext = "WorldContextObject"))
	bool DoesClockExist(const UObject* WorldContextObject, FName ClockName);

	// returns true if the clock is running
	UFUNCTION(BlueprintCallable, Category = "Quartz Clock Handle", meta = (WorldContext = "WorldContextObject"))
	bool IsClockRunning(const UObject* WorldContextObject, FName ClockName);

	// Returns the duration in seconds of the given Quantization Type
	UFUNCTION(BlueprintCallable, Category = "Quartz Clock Handle", meta = (WorldContext = "WorldContextObject"))
	float GetDurationOfQuantizationTypeInSeconds(const UObject* WorldContextObject, FName ClockName, const EQuartzCommandQuantization& QuantizationType, float Multiplier = 1.0f);

	// Retrieves a timestamp for the clock
	UFUNCTION(BlueprintCallable, Category = "Quartz Clock Handle", meta = (WorldContext = "WorldContextObject"))
	FQuartzTransportTimeStamp GetCurrentClockTimestamp(const UObject* WorldContextObject, const FName& InClockName);

	// Returns the amount of time, in seconds, the clock has been running. Caution: due to latency, this will not be perfectly accurate
	UFUNCTION(BlueprintCallable, Category = "Quartz Clock Handle", meta = (WorldContext = "WorldContextObject"))
	float GetEstimatedClockRunTime(const UObject* WorldContextObject, const FName& InClockName);

	// latency data (Game thread -> Audio Render Thread)
	UFUNCTION(BlueprintCallable, Category = "Quartz Subsystem", meta = (WorldContext = "WorldContextObject"))
	float GetGameThreadToAudioRenderThreadAverageLatency(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Quartz Subsystem", meta = (WorldContext = "WorldContextObject"))
	float GetGameThreadToAudioRenderThreadMinLatency(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Quartz Subsystem", meta = (WorldContext = "WorldContextObject"))
	float GetGameThreadToAudioRenderThreadMaxLatency(const UObject* WorldContextObject);

	// latency data (Audio Render Thread -> Game thread)
	UFUNCTION(BlueprintCallable, Category = "Quartz Subsystem")
	float GetAudioRenderThreadToGameThreadAverageLatency();

	UFUNCTION(BlueprintCallable, Category = "Quartz Subsystem")
	float GetAudioRenderThreadToGameThreadMinLatency();

	UFUNCTION(BlueprintCallable, Category = "Quartz Subsystem")
	float GetAudioRenderThreadToGameThreadMaxLatency();

	// latency data (Round trip)
	UFUNCTION(BlueprintCallable, Category = "Quartz Subsystem", meta = (WorldContext = "WorldContextObject"))
	float GetRoundTripAverageLatency(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Quartz Subsystem", meta = (WorldContext = "WorldContextObject"))
	float GetRoundTripMinLatency(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Quartz Subsystem", meta = (WorldContext = "WorldContextObject"))
	float GetRoundTripMaxLatency(const UObject* WorldContextObject);

	UE_DEPRECATED(5.1, "Obtain and use a UQuartzClockHandle / FQuartzClockProxy instead")
	void AddCommandToClock(const UObject* WorldContextObject, Audio::FQuartzQuantizedCommandInitInfo& InQuantizationCommandInitInfo, FName ClockName);

	// sharable to allow non-UObjects to un-subscribe if the Subsystem is going to outlive them
	TWeakPtr<FQuartzTickableObjectsManager> GetTickableObjectManager() const;

private:
	// deletes proxies to clocks that no longer exists
	void PruneStaleProxies();
	static void PruneStaleProxiesInternal(TArray<Audio::FQuartzClockProxy>& ContainerToPrune);


	// sharable tickable object manager to allow for non-UObject subscription / un-subscription
	TSharedPtr<FQuartzTickableObjectsManager> TickableObjectManagerPtr { MakeShared<FQuartzTickableObjectsManager>() };

	// Clock manager/proxy-related data that lives on the AudioDevice for persistence.
	TSharedPtr<Audio::FPersistentQuartzSubsystemData> ClockManagerDataPtr { nullptr };

	// helpers
	Audio::FQuartzClockProxy* FindProxyByName(const FName& ClockName);
	Audio::FQuartzClockProxy const* FindProxyByName(const FName& ClockName) const;
	Audio::FQuartzClockManager* GetClockManager(const UObject* WorldContextObject, bool bUseAudioEngineClockManager = true);

}; // class UQuartzGameSubsystem