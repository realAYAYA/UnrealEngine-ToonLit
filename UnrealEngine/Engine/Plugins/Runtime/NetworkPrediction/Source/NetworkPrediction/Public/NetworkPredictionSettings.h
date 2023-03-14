// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "NetworkPredictionConfig.h"
#include "NetworkPredictionReplicatedManager.h"
#include "Templates/SubclassOf.h"

#include "NetworkPredictionSettings.generated.h"

USTRUCT(meta=(ShowOnlyInnerProperties))
struct FNetworkPredictionSettings
{
	GENERATED_BODY()

	// Which ticking policy to use in cases where both are supported by the underlying simulation.
	// Leave this on Fixed if you intend to use physics based simulations.
	UPROPERTY(config, EditAnywhere, Category = Global)
	ENetworkPredictionTickingPolicy PreferredTickingPolicy = ENetworkPredictionTickingPolicy::Fixed;

	// Replicated Manager class
	UPROPERTY(config, EditAnywhere, Category = Global)
	TSubclassOf<ANetworkPredictionReplicatedManager> ReplicatedManagerClassOverride;

	// ------------------------------------------------------------------------------------------

	// Frame rate to use when running Fixed Tick simulations. Note: Engine::FixedFrameRate will take precedence if manually set.
	UPROPERTY(config, EditAnywhere, Category = FixedTick)
	int32 FixedTickFrameRate = 60;

	// Forces the engine to run in fixed tick mode when a NP physics simulation is running.
	// This is the same as settings UEngine::bUseFixedFrameRate / FixedFrameRate manually.
	UPROPERTY(config, EditAnywhere, Category = FixedTick)
	bool bForceEngineFixTickForcePhysics = true;

	// Default NetworkLOD for simulated proxy simulations.
	UPROPERTY(config, EditAnywhere, Category = FixedTick)
	ENetworkLOD SimulatedProxyNetworkLOD = ENetworkLOD::ForwardPredict;

	// ------------------------------------------------------------------------------------------

	// How much buffered time to keep for fixed ticking interpolated sims (client only).
	UPROPERTY(config, EditAnywhere, Category = Interpolation)
	int32 FixedTickInterpolationBufferedMS = 100;

	// How much buffered time to keep for fixed independent interpolated sims (client only).
	UPROPERTY(config, EditAnywhere, Category = Interpolation)
	int32 IndependentTickInterpolationBufferedMS = 100;

	// Max buffered time to keep for fixed independent interpolated sims (client only).
	UPROPERTY(config, EditAnywhere, Category = Interpolation)
	int32 IndependentTickInterpolationMaxBufferedMS = 250;
};

USTRUCT(meta=(ShowOnlyInnerProperties))
struct FNetworkPredictionDevHUDItem
{
	GENERATED_BODY();

	UPROPERTY(config, EditAnywhere, Category = DevHUD)
	FString DisplayName;

	UPROPERTY(config, EditAnywhere, Category = DevHUD)
	FString ExecCommand;

	// Return to to level HUD menu after selecting this
	UPROPERTY(config, EditAnywhere, Category = DevHUD)
	bool bAutoBack = true;

	// only works in PIE
	UPROPERTY(config, EditAnywhere, Category = DevHUD)
	bool bRequirePIE = false;

	// only works in non PIE
	UPROPERTY(config, EditAnywhere, Category = DevHUD)
	bool bRequireNotPIE = false;
};

USTRUCT(meta=(ShowOnlyInnerProperties))
struct FNetworkPredictionDevHUD
{
	GENERATED_BODY()

	UPROPERTY(config, EditAnywhere, Category = DevHUD)
	FString HUDName;

	UPROPERTY(config, EditAnywhere, Category = DevHUD, meta=(ShowOnlyInnerProperties))
	TArray<FNetworkPredictionDevHUDItem> Items;

	// only works in PIE
	UPROPERTY(config, EditAnywhere, Category = DevHUD)
	bool bRequirePIE = false;

	// only works in non PIE
	UPROPERTY(config, EditAnywhere, Category = DevHUD)
	bool bRequireNotPIE = false;
};


UCLASS(config=NetworkPrediction, defaultconfig, meta=(DisplayName="Network Prediction"))
class UNetworkPredictionSettingsObject : public UObject
{
	GENERATED_BODY()

public:

	UPROPERTY(config, EditAnywhere, Category = "Network Prediction", meta=(ShowOnlyInnerProperties))
	FNetworkPredictionSettings Settings;

	UPROPERTY(config, EditAnywhere, Category = DevHUD, meta=(ShowOnlyInnerProperties))
	TArray<FNetworkPredictionDevHUD> DevHUDs;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};