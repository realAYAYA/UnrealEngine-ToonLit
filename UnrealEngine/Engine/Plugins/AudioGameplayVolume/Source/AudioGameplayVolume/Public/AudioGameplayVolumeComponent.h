// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AudioGameplayComponent.h"
#include "Interfaces/IAudioGameplayVolumeInteraction.h"
#include "AudioGameplayVolumeComponent.generated.h"

// Forward Declarations 
class UAudioGameplayVolumeProxy;
class UAudioGameplayVolumeSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAudioGameplayVolumeProxyStateChange);

/**
 *  UAudioGameplayVolumeComponent - Component used to drive interaction with AudioGameplayVolumeSubsystem.
 *   NOTE: Do not inherit from this class, use UAudioGameplayVolumeComponentBase or UAudioGameplayVolumeMutator to create extendable functionality
 */
UCLASS(Config = Game, ClassGroup = ("AudioGameplay"), meta = (BlueprintSpawnableComponent, IsBlueprintBase = false, DisplayName = "Volume Proxy"))
class AUDIOGAMEPLAYVOLUME_API UAudioGameplayVolumeComponent final : public UAudioGameplayComponent
{
	GENERATED_UCLASS_BODY()

public:

	virtual ~UAudioGameplayVolumeComponent() = default;

	void SetProxy(UAudioGameplayVolumeProxy* NewProxy);
	UAudioGameplayVolumeProxy* GetProxy() const { return Proxy; }

	/** Called by a component on same actor to notify our proxy may need updating */
	void OnComponentDataChanged();

	/** Called when the proxy is 'entered' - This is when the proxy goes from zero listeners to at least one. */
	void EnterProxy() const;

	/** Called when the proxy is 'exited' - This is when the proxy goes from at least one listeners to zero. */
	void ExitProxy() const;

	/** Blueprint event for proxy enter */
	UPROPERTY(BlueprintAssignable, Category = Events)
	FOnAudioGameplayVolumeProxyStateChange OnProxyEnter;

	/** Blueprint event for proxy exit */
	UPROPERTY(BlueprintAssignable, Category = Events)
	FOnAudioGameplayVolumeProxyStateChange OnProxyExit;

protected:

	// A representation of this volume for the audio thread
	UPROPERTY(Instanced, EditAnywhere, BlueprintReadWrite, Category = "AudioGameplay", Meta = (ShowOnlyInnerProperties, AllowPrivateAccess = "true"))
	TObjectPtr<UAudioGameplayVolumeProxy> Proxy = nullptr;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR

	//~ Begin UActorComponent Interface
	virtual void OnUnregister() override;
	//~ End UActorComponent Interface

	//~ Begin UAudioGameplayComponent Interface
	virtual void Enable() override;
	virtual void Disable() override;
	//~ End UAudioGameplayComponent Interface

	void AddProxy() const;
	void RemoveProxy() const;
	void UpdateProxy() const;

	UAudioGameplayVolumeSubsystem* GetSubsystem() const;
};

/**
 *  UAudioGameplayVolumeComponentBase - Blueprintable component used to craft custom functionality with AudioGameplayVolumes.
 *   NOTE: Inherit from this class to get easy access to OnListenerEnter and OnListenerExit Blueprint Events
 */
UCLASS(Blueprintable, ClassGroup = ("AudioGameplay"), hidecategories = (Tags, Collision), meta = (BlueprintSpawnableComponent))
class AUDIOGAMEPLAYVOLUME_API UAudioGameplayVolumeComponentBase : public UAudioGameplayComponent
																, public IAudioGameplayVolumeInteraction
{
	GENERATED_UCLASS_BODY()

public:

	virtual ~UAudioGameplayVolumeComponentBase() = default;
};
