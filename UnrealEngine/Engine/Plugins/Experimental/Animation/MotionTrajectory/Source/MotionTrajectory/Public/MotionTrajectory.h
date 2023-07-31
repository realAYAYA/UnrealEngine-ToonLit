// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Animation/MotionTrajectoryTypes.h"
#include "Components/ActorComponent.h"
#include "Containers/RingBuffer.h"
#include "Modules/ModuleManager.h"
#include "UObject/ObjectMacros.h"

#include "MotionTrajectory.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogMotionTrajectory, Log, All);

class APawn;

class FMotionTrajectoryModule : public IModuleInterface
{
public: 

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

// Specifies the chosen domain parameters for trajectory sample retention and creation
USTRUCT(BlueprintType, Category="Motion Trajectory")
struct MOTIONTRAJECTORY_API FMotionTrajectorySettings
{
	GENERATED_BODY()

	// Trajectory mask for specifying a time and/or distance domain
	// Default: Time Domain of 2.0 seconds
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings", meta=(Bitmask, BitmaskEnum="/Script/Engine.ETrajectorySampleDomain"))
	int32 Domain = 1; // ETrajectorySampleDomain::Time

	// Sample time horizon
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings", meta=(ClampMin="0.0"))
	float Seconds = 2.f;

	// Sample distance horizon
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings", meta=(ClampMin="0.0"))
	float Distance = 0.f;
};

// Abstract component interface for the minimum Motion Trajectory prediction and history API
UCLASS(ClassGroup=Movement, abstract, BlueprintType, Category="Motion Trajectory", Experimental)
class MOTIONTRAJECTORY_API UMotionTrajectoryComponent : public UActorComponent
{
	GENERATED_BODY()

private:

	// Previous frame world-space transform
	FTransform PreviousWorldTransform = {};

	// Previous frame world game time
	float PreviousWorldGameTime = 0.f;

	// A superceding "effective" time domain applied to distance and time domain history samples, which guarantees uniform decay
	float EffectiveTimeDomain = 0.f;

	// Component-driven tick which decays and evicts trajectory history outside of the defined domain horizons
	void TickHistoryEvictionPolicy();

protected:

	// Retained trajectory history samples
	TRingBuffer<FTrajectorySample> SampleHistory = {};

	// Retained instantaneous/present trajectory in world space
	FTrajectorySample PresentTrajectorySampleWS = {};

	// Retained instantaneous/present trajectory in actor space
	FTrajectorySample PresentTrajectorySampleLS = {};

	// Gets the instantaneous/present trajectory sample of the current frame
	virtual FTrajectorySample CalcWorldSpacePresentTrajectorySample(float DeltaTime) const;

	// Ticks the trajectory. Call this to tick before ::TickComponent when necessary
	virtual void TickTrajectory(float DeltaTime);

	// Gets the Pawn from the owning Actor of this component
	const APawn* TryGetOwnerPawn() const;

	// Combines all trajectory samples in the past, present, and future into a unifed trajectory range
	FTrajectorySampleRange CombineHistoryPresentPrediction(bool bIncludeHistory, const FTrajectorySampleRange& Prediction) const;


	// Forcefully evicts all trajectory history and resets internal history tracking state
	void FlushHistory();

public:

	UMotionTrajectoryComponent(const FObjectInitializer& ObjectInitializer);

	// Begin UActorComponent Interface
	virtual void OnComponentCreated() override;
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime
		, enum ELevelTick TickType
		, FActorComponentTickFunction* ThisTickFunction) override;
	// End UActorComponent Interface

	// Interface for computing a trajectory prediction
	UFUNCTION(BlueprintPure, Category="Motion Trajectory")
	virtual FTrajectorySampleRange GetTrajectory() const;

	// Interface for computing a trajectory prediction with overridden component settings
	UFUNCTION(BlueprintPure, Category="Motion Trajectory", meta=(AutoCreateRefTerm="Settings", DisplayName="Get Trajectory (With Settings)"))
	virtual FTrajectorySampleRange GetTrajectoryWithSettings(const FMotionTrajectorySettings& Settings, bool bIncludeHistory) const;
	
	// Changes the history and prediction sample rate for the component
	UFUNCTION(BlueprintCallable, Category="Motion Trajectory")
	void SetSampleRate(int32 Rate);

	// Retrieves the historical trajectory
	UFUNCTION(BlueprintCallable, Category="Motion Trajectory")
	FTrajectorySampleRange GetHistory() const;

	// Prediction trajectory simulation settings
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Settings")
	FMotionTrajectorySettings PredictionSettings;

	// Historical trajectory settings
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Settings")
	FMotionTrajectorySettings HistorySettings;

	// The trajectory sampling rate for both prediction and history
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Settings", meta=(ClampMin="5", ClampMax="120"))
	int32 SampleRate = 30;

	// The maximum number of trajectory samples retained by the internal history buffer
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Settings", meta=(ClampMin="0", ClampMax="3600"))
	int32 MaxSamples = 1000;

	// Determines if GetTrajectory() will automatically combine history into the returned trajectory prediction (true)
	// Consider disabling this option when historical trajectory samples are never used
	// Example: No Motion Matching historical sample times or distances are defined in the Pose Search Schema
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Settings")
	bool bPredictionIncludesHistory = true;

	// Determines if history will be sampled by the specified Sample Rate (true) or the Component Tick Rate (false)
	// Consider disabling this option when the engine frame rate and component sample rate are the same
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Settings")
	bool bUniformSampledHistory = true;

	// History samples will use Centripetal Catmull–Rom spline interpolation (true) (slow, accurate) instead of linear interpolation (false) (fast, approximate)
	// Consider enabling this option when accurate trajectory sample speed at low tick rates (high interpolation usage) is important
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings")
	bool bSmoothInterpolation = false;

#if WITH_EDITORONLY_DATA
	// Debug drawing options for the complete trajectory range
	// a.MotionTrajectory.Debug (0/1) to enable or disable
	// a.MotionTrajectory.Options (0-6) to enable additional sample information
	// a.MotionTrajectory.Stride (*) to stride sample information display by a specified modulo
	UPROPERTY(EditAnywhere, Category="Debug")
	bool bDebugDrawTrajectory = false;
#endif
};