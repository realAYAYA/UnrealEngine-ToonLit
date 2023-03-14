// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassProcessor.h"
#include "MassCommonTypes.h"
#include "MassSignalProcessorBase.h"
#include "MassLookAtProcessors.generated.h"

class UMassNavigationSubsystem;
class UZoneGraphSubsystem;
struct FMassLookAtFragment;
struct FMassZoneGraphLaneLocationFragment;
struct FMassLookAtTrajectoryFragment;
struct FMassZoneGraphShortPathFragment;
struct FMassMoveTargetFragment;

/**
 * Processor to choose and assign LookAt configurations  
 */
UCLASS()
class MASSAIBEHAVIOR_API UMassLookAtProcessor : public UMassProcessor
{
	GENERATED_BODY()

	UMassLookAtProcessor();

protected:

	virtual void ConfigureQueries() override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	/** Selects a nearby target if possible or use a random fixed direction */
	void FindNewGazeTarget(const UMassNavigationSubsystem& MassNavSystem, const FMassEntityManager& EntitySubsystem, const float CurrentTime, const FTransform& Transform, FMassLookAtFragment& LookAt) const;

	/** Updates look direction based on look at trajectory. */
	void UpdateLookAtTrajectory(const FTransform& Transform, const FMassZoneGraphLaneLocationFragment& ZoneGraphLocation,
								const FMassLookAtTrajectoryFragment& LookAtTrajectory, const bool bDisplayDebug, FMassLookAtFragment& LookAt) const;

	/** Updates look at based on tracked entity. */
	void UpdateLookAtTrackedEntity(const FMassEntityManager& EntitySubsystem, const FTransform& Transform, const bool bDisplayDebug, FMassLookAtFragment& LookAt) const;

	/** Updates gaze based on tracked entity. */
	bool UpdateGazeTrackedEntity(const FMassEntityManager& EntitySubsystem, const FTransform& Transform, const bool bDisplayDebug, FMassLookAtFragment& LookAt) const;

	/** Builds look at trajectory along the current path. */
	void BuildTrajectory(const UZoneGraphSubsystem& ZoneGraphSubsystem, const FMassZoneGraphLaneLocationFragment& LaneLocation, const FMassZoneGraphShortPathFragment& ShortPath,
							const FMassEntityHandle Entity, const bool bDisplayDebug, FMassLookAtTrajectoryFragment& LookAtTrajectory);

	/** Size of the query to find potential targets */
	UPROPERTY(EditDefaultsOnly, Category = LookAt, config, meta = (UIMin = 0.0, ClampMin = 0.0))
	float QueryExtent = 0.f;

	/** Time an entity must use a random look at. */
	UPROPERTY(EditDefaultsOnly, Category = LookAt, config, meta = (UIMin = 0.0, ClampMin = 0.0))
	float Duration = 0.f;
	
	/** Variation applied to a random look at duration [Duration-Variation : Duration+Variation] */
	UPROPERTY(EditDefaultsOnly, Category = LookAt, config, meta = (UIMin = 0.0, ClampMin = 0.0))
	float DurationVariation = 0.f;

	/** Height offset that will be added for debug draw of the look at vector. Useful to bring arrow near character's eyes */
	UPROPERTY(EditDefaultsOnly, Category = LookAt, config, meta = (UIMin = 0.0, ClampMin = 0.0, DisplayName="Debug draw Z offset (cm)"))
	float DebugZOffset = 0.f;

	/** Tolerance in degrees between the forward direction and the look at duration to track an entity */
	UPROPERTY(EditDefaultsOnly, Category = LookAt, config, meta = (UIMin = 0.0, ClampMin = 0.0, UIMax = 180.0, ClampMax = 180.0, DisplayName="Angle Threshold (degrees)"))
	float AngleThresholdInDegrees = 0.f;

	FMassEntityQuery EntityQuery_Conditional;
};
