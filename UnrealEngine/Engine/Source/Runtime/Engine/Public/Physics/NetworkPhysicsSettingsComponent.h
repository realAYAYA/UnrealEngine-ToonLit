// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	NetworkPhysicsSettingsComponent.h
	Manage networked physics settings per actor through ActorComponent and the subsequent physics-thread data flow for the settings.
=============================================================================*/

#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Chaos/SimCallbackObject.h"

#include "NetworkPhysicsSettingsComponent.generated.h"


class FNetworkPhysicsSettingsComponentAsync;

//  Alias
using FPredictiveInterpolationSettings	= FNetworkPhysicsSettingsPredictiveInterpolation;
using FResimulationSettings				= FNetworkPhysicsSettingsResimulation;
/*
using FRewindSettings					= FNetworkPhysicsSettingsRewindData;
using FRenderInterpolationSettings		= FNetworkPhysicsSettingsRenderInterpolation;
*/

namespace PhysicsReplicationCVars
{
	namespace PredictiveInterpolationCVars
	{
		extern float PosCorrectionTimeBase;
		extern float PosCorrectionTimeMin;
		extern float PosCorrectionTimeMultiplier;
		extern float RotCorrectionTimeBase;
		extern float RotCorrectionTimeMin;
		extern float RotCorrectionTimeMultiplier;
		extern float PosInterpolationTimeMultiplier;
		extern float RotInterpolationTimeMultiplier;
		extern float SoftSnapPosStrength;
		extern float SoftSnapRotStrength;
		extern bool bSoftSnapToSource;
		extern bool bSkipVelocityRepOnPosEarlyOut;
		extern bool bPostResimWaitForUpdate;
		extern bool bDisableSoftSnap;
	}

	namespace ResimulationCVars
	{
		extern bool bRuntimeCorrectionEnabled;
		extern bool bRuntimeVelocityCorrection;
		extern float PosStabilityMultiplier;
		extern float RotStabilityMultiplier;
		extern float VelStabilityMultiplier;
		extern float AngVelStabilityMultiplier;
	}
}


USTRUCT()
struct FNetworkPhysicsSettings
{
	GENERATED_BODY()

	UPROPERTY(config, EditDefaultsOnly, Category = "Overrides", Meta = (InlineEditConditionToggle))
	uint32 bOverrideSimProxyRepMode : 1;
	// Override the EPhysicsReplicationMode if Actor is ENetRole::ROLE_SimulatedProxy and has EPhysicsReplicationMode::Resimulation set
	UPROPERTY(config, EditDefaultsOnly, Category = "Overrides", Meta = (EditCondition = "bOverrideSimProxyRepMode"))
	EPhysicsReplicationMode SimProxyRepMode = EPhysicsReplicationMode::PredictiveInterpolation;
};


USTRUCT()
struct FNetworkPhysicsSettingsPredictiveInterpolation
{
	GENERATED_BODY()

	// np2.PredictiveInterpolation.PosCorrectionTimeBase
	UPROPERTY(config, EditDefaultsOnly, Category = "Overrides", Meta = (InlineEditConditionToggle))
	uint32 bOverridePosCorrectionTimeBase : 1;
	UPROPERTY(config, EditDefaultsOnly, Category = "Overrides", Meta = (EditCondition = "bOverridePosCorrectionTimeBase"))
	float PosCorrectionTimeBase = PhysicsReplicationCVars::PredictiveInterpolationCVars::PosCorrectionTimeBase;
	float GetPosCorrectionTimeBase() { return bOverridePosCorrectionTimeBase ? PosCorrectionTimeBase : PhysicsReplicationCVars::PredictiveInterpolationCVars::PosCorrectionTimeBase; }

	// np2.PredictiveInterpolation.PosCorrectionTimeMin
	UPROPERTY(config, EditDefaultsOnly, Category = "Overrides", Meta = (InlineEditConditionToggle))
	uint32 bOverridePosCorrectionTimeMin : 1;
	UPROPERTY(config, EditDefaultsOnly, Category = "Overrides", Meta = (EditCondition = "bOverridePosCorrectionTimeMin"))
	float PosCorrectionTimeMin = PhysicsReplicationCVars::PredictiveInterpolationCVars::PosCorrectionTimeMin;
	float GetPosCorrectionTimeMin() { return bOverridePosCorrectionTimeMin ? PosCorrectionTimeMin : PhysicsReplicationCVars::PredictiveInterpolationCVars::PosCorrectionTimeMin; }

	// np2.PredictiveInterpolation.PosCorrectionTimeMultiplier
	UPROPERTY(config, EditDefaultsOnly, Category = "Overrides", Meta = (InlineEditConditionToggle))
	uint32 bOverridePosCorrectionTimeMultiplier : 1;
	UPROPERTY(config, EditDefaultsOnly, Category = "Overrides", Meta = (EditCondition = "bOverridePosCorrectionTimeMultiplier"))
	float PosCorrectionTimeMultiplier = PhysicsReplicationCVars::PredictiveInterpolationCVars::PosCorrectionTimeMultiplier;
	float GetPosCorrectionTimeMultiplier() { return bOverridePosCorrectionTimeMultiplier ? PosCorrectionTimeMultiplier : PhysicsReplicationCVars::PredictiveInterpolationCVars::PosCorrectionTimeMultiplier; }

	// np2.PredictiveInterpolation.RotCorrectionTimeBase
	UPROPERTY(config, EditDefaultsOnly, Category = "Overrides", Meta = (InlineEditConditionToggle))
	uint32 bOverrideRotCorrectionTimeBase : 1;
	UPROPERTY(config, EditDefaultsOnly, Category = "Overrides", Meta = (EditCondition = "bOverrideRotCorrectionTimeBase"))
	float RotCorrectionTimeBase = PhysicsReplicationCVars::PredictiveInterpolationCVars::RotCorrectionTimeBase;
	float GetRotCorrectionTimeBase() { return bOverrideRotCorrectionTimeBase ? RotCorrectionTimeBase : PhysicsReplicationCVars::PredictiveInterpolationCVars::RotCorrectionTimeBase; }

	// np2.PredictiveInterpolation.RotCorrectionTimeMin
	UPROPERTY(config, EditDefaultsOnly, Category = "Overrides", Meta = (InlineEditConditionToggle))
	uint32 bOverrideRotCorrectionTimeMin : 1;
	UPROPERTY(config, EditDefaultsOnly, Category = "Overrides", Meta = (EditCondition = "bOverrideRotCorrectionTimeMin"))
	float RotCorrectionTimeMin = PhysicsReplicationCVars::PredictiveInterpolationCVars::RotCorrectionTimeMin;
	float GetRotCorrectionTimeMin() { return bOverrideRotCorrectionTimeMin ? RotCorrectionTimeMin : PhysicsReplicationCVars::PredictiveInterpolationCVars::RotCorrectionTimeMin; }

	// np2.PredictiveInterpolation.RotCorrectionTimeMultiplier
	UPROPERTY(config, EditDefaultsOnly, Category = "Overrides", Meta = (InlineEditConditionToggle))
	uint32 bOverrideRotCorrectionTimeMultiplier : 1;
	UPROPERTY(config, EditDefaultsOnly, Category = "Overrides", Meta = (EditCondition = "bOverrideRotCorrectionTimeMultiplier"))
	float RotCorrectionTimeMultiplier = PhysicsReplicationCVars::PredictiveInterpolationCVars::RotCorrectionTimeMultiplier;
	float GetRotCorrectionTimeMultiplier() { return bOverrideRotCorrectionTimeMultiplier ? RotCorrectionTimeMultiplier : PhysicsReplicationCVars::PredictiveInterpolationCVars::RotCorrectionTimeMultiplier; }

	// np2.PredictiveInterpolation.InterpolationTimeMultiplier
	UPROPERTY(config, EditDefaultsOnly, Category = "Overrides", Meta = (InlineEditConditionToggle))
	uint32 bOverridePosInterpolationTimeMultiplier : 1;
	UPROPERTY(config, EditDefaultsOnly, Category = "Overrides", Meta = (EditCondition = "bOverridePosInterpolationTimeMultiplier"))
	float PosInterpolationTimeMultiplier = PhysicsReplicationCVars::PredictiveInterpolationCVars::PosInterpolationTimeMultiplier;
	float GetPosInterpolationTimeMultiplier() { return bOverridePosInterpolationTimeMultiplier ? PosInterpolationTimeMultiplier : PhysicsReplicationCVars::PredictiveInterpolationCVars::PosInterpolationTimeMultiplier; }

	// np2.PredictiveInterpolation.RotInterpolationTimeMultiplier
	UPROPERTY(config, EditDefaultsOnly, Category = "Overrides", Meta = (InlineEditConditionToggle))
	uint32 bOverrideRotInterpolationTimeMultiplier : 1;
	UPROPERTY(config, EditDefaultsOnly, Category = "Overrides", Meta = (EditCondition = "bOverrideRotInterpolationTimeMultiplier"))
	float RotInterpolationTimeMultiplier = PhysicsReplicationCVars::PredictiveInterpolationCVars::RotInterpolationTimeMultiplier;
	float GetRotInterpolationTimeMultiplier() { return bOverrideRotInterpolationTimeMultiplier ? RotInterpolationTimeMultiplier : PhysicsReplicationCVars::PredictiveInterpolationCVars::RotInterpolationTimeMultiplier; }

	// np2.PredictiveInterpolation.SoftSnapPosStrength
	UPROPERTY(config, EditDefaultsOnly, Category = "Overrides", Meta = (InlineEditConditionToggle))
	uint32 bOverrideSoftSnapPosStrength : 1;
	UPROPERTY(config, EditDefaultsOnly, Category = "Overrides", Meta = (EditCondition = "bOverrideSoftSnapPosStrength"))
	float SoftSnapPosStrength = PhysicsReplicationCVars::PredictiveInterpolationCVars::SoftSnapPosStrength;
	float GetSoftSnapPosStrength() { return bOverrideSoftSnapPosStrength ? SoftSnapPosStrength : PhysicsReplicationCVars::PredictiveInterpolationCVars::SoftSnapPosStrength; }

	// np2.PredictiveInterpolation.SoftSnapRotStrength
	UPROPERTY(config, EditDefaultsOnly, Category = "Overrides", Meta = (InlineEditConditionToggle))
	uint32 bOverrideSoftSnapRotStrength : 1;
	UPROPERTY(config, EditDefaultsOnly, Category = "Overrides", Meta = (EditCondition = "bOverrideSoftSnapRotStrength"))
	float SoftSnapRotStrength = PhysicsReplicationCVars::PredictiveInterpolationCVars::SoftSnapRotStrength;
	float GetSoftSnapRotStrength() { return bOverrideSoftSnapRotStrength ? SoftSnapRotStrength : PhysicsReplicationCVars::PredictiveInterpolationCVars::SoftSnapRotStrength; }

	// np2.PredictiveInterpolation.SoftSnapToSource
	UPROPERTY(config, EditDefaultsOnly, Category = "Overrides", Meta = (InlineEditConditionToggle))
	uint32 bOverrideSoftSnapToSource : 1;
	UPROPERTY(config, EditDefaultsOnly, Category = "Overrides", Meta = (EditCondition = "bOverrideSoftSnapToSource"))
	bool bSoftSnapToSource = PhysicsReplicationCVars::PredictiveInterpolationCVars::bSoftSnapToSource;
	bool GetSoftSnapToSource() { return bOverrideSoftSnapToSource ? bSoftSnapToSource : PhysicsReplicationCVars::PredictiveInterpolationCVars::bSoftSnapToSource; }

	// np2.PredictiveInterpolation.SkipVelocityRepOnPosEarlyOut
	UPROPERTY(config, EditDefaultsOnly, Category = "Overrides", Meta = (InlineEditConditionToggle))
	uint32 bOverrideSkipVelocityRepOnPosEarlyOut : 1;
	UPROPERTY(config, EditDefaultsOnly, Category = "Overrides", Meta = (EditCondition = "bOverrideSkipVelocityRepOnPosEarlyOut"))
	bool bSkipVelocityRepOnPosEarlyOut = PhysicsReplicationCVars::PredictiveInterpolationCVars::bSkipVelocityRepOnPosEarlyOut;
	bool GetSkipVelocityRepOnPosEarlyOut() { return bOverrideSkipVelocityRepOnPosEarlyOut ? bSkipVelocityRepOnPosEarlyOut : PhysicsReplicationCVars::PredictiveInterpolationCVars::bSkipVelocityRepOnPosEarlyOut; }

	// np2.PredictiveInterpolation.PostResimWaitForUpdate
	UPROPERTY(config, EditDefaultsOnly, Category = "Overrides", Meta = (InlineEditConditionToggle))
	uint32 bOverridePostResimWaitForUpdate : 1;
	UPROPERTY(config, EditDefaultsOnly, Category = "Overrides", Meta = (EditCondition = "bOverridePostResimWaitForUpdate"))
	bool bPostResimWaitForUpdate = PhysicsReplicationCVars::PredictiveInterpolationCVars::bPostResimWaitForUpdate;
	bool GetPostResimWaitForUpdate() { return bOverridePostResimWaitForUpdate ? bPostResimWaitForUpdate : PhysicsReplicationCVars::PredictiveInterpolationCVars::bPostResimWaitForUpdate; }

	// np2.PredictiveInterpolation.DisableSoftSnap
	UPROPERTY(config, EditDefaultsOnly, Category = "Overrides", Meta = (InlineEditConditionToggle))
	uint32 bOverrideDisableSoftSnap : 1;
	UPROPERTY(config, EditDefaultsOnly, Category = "Overrides", Meta = (EditCondition = "bOverrideDisableSoftSnap"))
	bool bDisableSoftSnap = PhysicsReplicationCVars::PredictiveInterpolationCVars::bDisableSoftSnap;
	bool GetDisableSoftSnap() { return bOverrideDisableSoftSnap ? bDisableSoftSnap : PhysicsReplicationCVars::PredictiveInterpolationCVars::bDisableSoftSnap; }
};

USTRUCT()
struct FNetworkPhysicsSettingsResimulation
{
	GENERATED_BODY()

	// Override how many inputs to synchronize each sync to cover packet loss.
	UPROPERTY(config, EditDefaultsOnly, Category = "Overrides", Meta = (InlineEditConditionToggle))
	uint32 bOverrideRedundantInputs : 1;
	UPROPERTY(config, EditDefaultsOnly, Category = "Overrides", Meta = (EditCondition = "bOverrideRedundantInputs"))
	uint8 RedundantInputs = 3;

	// Override how many states to synchronize each sync to cover packet loss.
	UPROPERTY(config, EditDefaultsOnly, Category = "Overrides", Meta = (InlineEditConditionToggle))
	uint32 bOverrideRedundantStates : 1;
	UPROPERTY(config, EditDefaultsOnly, Category = "Overrides", Meta = (EditCondition = "bOverrideRedundantStates"))
	uint8 RedundantStates = 1;

	// np2.Resim.RuntimeCorrectionEnabled
	UPROPERTY(config, EditDefaultsOnly, Category = "Overrides", Meta = (InlineEditConditionToggle))
	uint32 bOverrideRuntimeCorrectionEnabled : 1;
	UPROPERTY(config, EditDefaultsOnly, Category = "Overrides", Meta = (EditCondition = "bOverrideRuntimeCorrectionEnabled"))
	bool bRuntimeCorrectionEnabled = PhysicsReplicationCVars::ResimulationCVars::bRuntimeCorrectionEnabled;
	bool GetRuntimeCorrectionEnabled() { return bOverrideRuntimeCorrectionEnabled ? bRuntimeCorrectionEnabled : PhysicsReplicationCVars::ResimulationCVars::bRuntimeCorrectionEnabled; }

	// np2.Resim.RuntimeVelocityCorrection
	UPROPERTY(config, EditDefaultsOnly, Category = "Overrides", Meta = (InlineEditConditionToggle))
	uint32 bOverrideRuntimeVelocityCorrection : 1;
	UPROPERTY(config, EditDefaultsOnly, Category = "Overrides", Meta = (EditCondition = "bOverrideRuntimeVelocityCorrection"))
	bool bRuntimeVelocityCorrection = PhysicsReplicationCVars::ResimulationCVars::bRuntimeVelocityCorrection;
	bool GetRuntimeVelocityCorrectionEnabled() { return bOverrideRuntimeVelocityCorrection ? bRuntimeVelocityCorrection : PhysicsReplicationCVars::ResimulationCVars::bRuntimeVelocityCorrection; }

	// np2.Resim.PosStabilityMultiplier
	UPROPERTY(config, EditDefaultsOnly, Category = "Overrides", Meta = (InlineEditConditionToggle))
	uint32 bOverridePosStabilityMultiplier : 1;
	UPROPERTY(config, EditDefaultsOnly, Category = "Overrides", Meta = (EditCondition = "bOverridePosStabilityMultiplier"))
	float PosStabilityMultiplier = PhysicsReplicationCVars::ResimulationCVars::PosStabilityMultiplier;
	float GetPosStabilityMultiplier() { return bOverridePosStabilityMultiplier ? PosStabilityMultiplier : PhysicsReplicationCVars::ResimulationCVars::PosStabilityMultiplier; }

	// np2.Resim.RotStabilityMultiplier
	UPROPERTY(config, EditDefaultsOnly, Category = "Overrides", Meta = (InlineEditConditionToggle))
	uint32 bOverrideRotStabilityMultiplier : 1;
	UPROPERTY(config, EditDefaultsOnly, Category = "Overrides", Meta = (EditCondition = "bOverrideRotStabilityMultiplier"))
	float RotStabilityMultiplier = PhysicsReplicationCVars::ResimulationCVars::RotStabilityMultiplier;
	float GetRotStabilityMultiplier() { return bOverrideRotStabilityMultiplier ? RotStabilityMultiplier : PhysicsReplicationCVars::ResimulationCVars::RotStabilityMultiplier; }

	// np2.Resim.VelStabilityMultiplier
	UPROPERTY(config, EditDefaultsOnly, Category = "Overrides", Meta = (InlineEditConditionToggle))
	uint32 bOverrideVelStabilityMultiplier : 1;
	UPROPERTY(config, EditDefaultsOnly, Category = "Overrides", Meta = (EditCondition = "bOverrideVelStabilityMultiplier"))
	float VelStabilityMultiplier = PhysicsReplicationCVars::ResimulationCVars::VelStabilityMultiplier;
	float GetVelStabilityMultiplier() { return bOverrideVelStabilityMultiplier ? VelStabilityMultiplier : PhysicsReplicationCVars::ResimulationCVars::VelStabilityMultiplier; }

	// np2.Resim.AngVelStabilityMultiplier
	UPROPERTY(config, EditDefaultsOnly, Category = "Overrides", Meta = (InlineEditConditionToggle))
	uint32 bOverrideAngVelStabilityMultiplier : 1;
	UPROPERTY(config, EditDefaultsOnly, Category = "Overrides", Meta = (EditCondition = "bOverrideAngVelStabilityMultiplier"))
	float AngVelStabilityMultiplier = PhysicsReplicationCVars::ResimulationCVars::AngVelStabilityMultiplier;
	float GetAngVelStabilityMultiplier() { return bOverrideAngVelStabilityMultiplier ? AngVelStabilityMultiplier : PhysicsReplicationCVars::ResimulationCVars::AngVelStabilityMultiplier; }

	// Project Settings -> Physics -> Replication -> Physics Prediction -> Resimulation Error Threshold
	UPROPERTY(config, EditDefaultsOnly, Category = "Overrides", Meta = (InlineEditConditionToggle))
	uint32 bOverrideResimulationErrorThreshold : 1;
	UPROPERTY(config, EditDefaultsOnly, Category = "Overrides", Meta = (EditCondition = "bOverrideResimulationErrorThreshold"))
	uint32 ResimulationErrorThreshold = 10;
	uint32 GetResimulationErrorThreshold(uint32 DefaultValue) { return bOverrideRotStabilityMultiplier ? ResimulationErrorThreshold : DefaultValue; }
	
	// np2.Resim.CompareStateToTriggerRewind
	UPROPERTY(config, EditDefaultsOnly, Category = "Overrides", Meta = (InlineEditConditionToggle))
	uint32 bOverrideCompareStateToTriggerRewind : 1;
	UPROPERTY(config, EditDefaultsOnly, Category = "Overrides", Meta = (EditCondition = "bOverrideCompareStateToTriggerRewind"))
	bool bCompareStateToTriggerRewind = false;
	bool GetCompareStateToTriggerRewind(bool DefaultValue) { return bOverrideCompareStateToTriggerRewind ? bCompareStateToTriggerRewind : DefaultValue; }

	// np2.Resim.CompareInputToTriggerRewind
	UPROPERTY(config, EditDefaultsOnly, Category = "Overrides", Meta = (InlineEditConditionToggle))
	uint32 bOverrideCompareInputToTriggerRewind : 1;
	UPROPERTY(config, EditDefaultsOnly, Category = "Overrides", Meta = (EditCondition = "bOverrideCompareInputToTriggerRewind"))
	bool bCompareInputToTriggerRewind = false;
	bool GetCompareInputToTriggerRewind(bool DefaultValue) { return bOverrideCompareInputToTriggerRewind ? bCompareInputToTriggerRewind : DefaultValue; }

};

/*
USTRUCT()
struct FNetworkPhysicsSettingsRewindData
{
	GENERATED_BODY()
};

USTRUCT()
struct FNetworkPhysicsSettingsRenderInterpolation
{
	GENERATED_BODY()
};
*/

UCLASS(BlueprintType, meta = (BlueprintSpawnableComponent))
class UNetworkPhysicsSettingsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNetworkPhysicsSettingsComponent();

	virtual void InitializeComponent() override;
	virtual void UninitializeComponent() override;

	virtual void BeginPlay() override;


public:
	UPROPERTY(EditDefaultsOnly, Category = "Networked Physics Settings")
	FNetworkPhysicsSettings GeneralSettings;
	
	UPROPERTY(EditDefaultsOnly, Category = "Networked Physics Settings")
	FNetworkPhysicsSettingsPredictiveInterpolation PredictiveInterpolationSettings;
	
	UPROPERTY(EditDefaultsOnly, Category = "Networked Physics Settings")
	FNetworkPhysicsSettingsResimulation ResimulationSettings;
	
	/*
	UPROPERTY(EditDefaultsOnly, Category = "Networked Physics Settings")
	FNetworkPhysicsSettingsRewindData RewindSettings;
	
	UPROPERTY(EditDefaultsOnly, Category = "Networked Physics Settings")
	FNetworkPhysicsSettingsRenderInterpolation RenderInterpolationSettings;
	*/

private:
	FNetworkPhysicsSettingsComponentAsync* NetworkPhysicsSettingsAsync;
};






#pragma region // FNetworkPhysicsSettingsComponentAsync

struct FNetworkPhysicsSettingsAsync
{
	FNetworkPhysicsSettings GeneralSettings;
	FNetworkPhysicsSettingsResimulation ResimulationSettings;
	FNetworkPhysicsSettingsPredictiveInterpolation PredictiveInterpolationSettings;
};

struct FNetworkPhysicsSettingsAsyncInput : public Chaos::FSimCallbackInput
{
	Chaos::FConstPhysicsObjectHandle PhysicsObject;
	FNetworkPhysicsSettingsAsync Settings;

	void Reset()
	{
		Settings = FNetworkPhysicsSettingsAsync();
	}
};

class FNetworkPhysicsSettingsComponentAsync : public Chaos::TSimCallbackObject<FNetworkPhysicsSettingsAsyncInput>
{
public:
	virtual void OnPostInitialize_Internal() override;
	virtual void OnPreSimulate_Internal() override { };

public:
	FNetworkPhysicsSettingsAsync Settings;
};

#pragma endregion // FNetworkPhysicsSettingsComponentAsync
