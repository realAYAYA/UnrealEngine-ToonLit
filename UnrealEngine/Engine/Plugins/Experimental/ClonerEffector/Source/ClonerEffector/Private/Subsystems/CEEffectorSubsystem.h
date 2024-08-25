// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/World.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "CEEffectorSubsystem.generated.h"

class ACEEffectorActor;
class UNiagaraDataChannelAsset;

UCLASS()
class UCEEffectorSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnSubsystemInitialized, const UWorld*)
	static FOnSubsystemInitialized OnSubsystemInitializedDelegate;

	/** Broadcast when this effector identifier changed to update linked cloners */
	DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnEffectorIdentifierChanged, ACEEffectorActor* /** InEffectorActor */, int32 /** OldIdentifier */, int32 /** NewIdentifier */)
	static FOnEffectorIdentifierChanged OnEffectorIdentifierChangedDelegate;

	static inline constexpr TCHAR DataChannelAssetPath[] = TEXT("/Script/Niagara.NiagaraDataChannelAsset'/ClonerEffector/Channels/NDC_Effector.NDC_Effector'");

	/** Get this subsystem instance */
	static UCEEffectorSubsystem* Get(const UWorld* InWorld = GWorld);

	//~ Begin USubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void PostInitialize() override;
	//~ End USubsystem

	/** Registers an effector actor to use it within a effector channel */
	bool RegisterChannelEffector(ACEEffectorActor* InEffector);

	/** Unregister an effector actor used within a effector channel */
	bool UnregisterChannelEffector(ACEEffectorActor* InEffector);

protected:
	//~ Begin FTickableGameObject interface
	virtual bool IsTickableInEditor() const override;
	virtual TStatId GetStatId() const override;
	virtual void Tick(float InDeltaTime) override;
	//~ End FTickableGameObject interface

	/** Updates all registered effectors */
	void UpdateEffectorChannel();

	/** Ordered effectors included in this channel */
	UPROPERTY()
	TArray<TWeakObjectPtr<ACEEffectorActor>> EffectorsWeak;

	/** This represents the data channel structure for effector */
	UPROPERTY()
	TObjectPtr<UNiagaraDataChannelAsset> EffectorDataChannelAsset;
};