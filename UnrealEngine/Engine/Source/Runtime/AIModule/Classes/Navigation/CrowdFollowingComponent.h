// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/WeakInterfacePtr.h"
#include "EngineDefines.h"
#include "AITypes.h"
#include "Navigation/PathFollowingComponent.h"
#include "Navigation/CrowdAgentInterface.h"
#include "AI/Navigation/NavigationAvoidanceTypes.h"
#include "CrowdFollowingComponent.generated.h"

class INavLinkCustomInterface;
class IRVOAvoidanceInterface;
class UCrowdManager;

namespace ECrowdAvoidanceQuality
{
	enum Type
	{
		Low,
		Medium,
		Good,
		High,
	};
}

enum class ECrowdSimulationState : uint8
{
	Enabled,
	ObstacleOnly	UMETA(DisplayName="Disabled, avoided by others"),
	Disabled		UMETA(DisplayName="Disabled, ignored by others"),
};

UCLASS(BlueprintType)
class AIMODULE_API UCrowdFollowingComponent : public UPathFollowingComponent, public ICrowdAgentInterface
{
	GENERATED_UCLASS_BODY()

	virtual void BeginDestroy() override;

	// ICrowdAgentInterface BEGIN
	virtual FVector GetCrowdAgentLocation() const override;
	virtual FVector GetCrowdAgentVelocity() const override;
	virtual void GetCrowdAgentCollisions(float& CylinderRadius, float& CylinderHalfHeight) const override;
	virtual float GetCrowdAgentMaxSpeed() const override;
	virtual int32 GetCrowdAgentAvoidanceGroup() const override;
	virtual int32 GetCrowdAgentGroupsToAvoid() const override;
	virtual int32 GetCrowdAgentGroupsToIgnore() const override;
	// ICrowdAgentInterface END

	// PathFollowingComponent BEGIN
	virtual void Initialize() override;
	virtual void Cleanup() override;
	virtual void AbortMove(const UObject& Instigator, FPathFollowingResultFlags::Type AbortFlags, FAIRequestID RequestID = FAIRequestID::CurrentRequest, EPathFollowingVelocityMode VelocityMode = EPathFollowingVelocityMode::Reset) override;
	virtual void PauseMove(FAIRequestID RequestID = FAIRequestID::CurrentRequest, EPathFollowingVelocityMode VelocityMode = EPathFollowingVelocityMode::Reset) override;
	virtual void ResumeMove(FAIRequestID RequestID = FAIRequestID::CurrentRequest) override;
	virtual FVector GetMoveFocus(bool bAllowStrafe) const override;
	virtual void OnLanded() override;
	virtual void FinishUsingCustomLink(INavLinkCustomInterface* CustomNavLink) override;
	virtual void OnPathFinished(const FPathFollowingResult& Result) override;
	virtual void OnPathUpdated() override;
	virtual void OnPathfindingQuery(FPathFindingQuery& Query) override;
	virtual int32 GetCurrentPathElement() const override { return LastPathPolyIndex; }
	virtual void OnNavigationInitDone() override;
	// PathFollowingComponent END

	/** update params in crowd manager */
	void UpdateCrowdAgentParams() const;

	/** pass agent velocity to movement component */
	virtual void ApplyCrowdAgentVelocity(const FVector& NewVelocity, const FVector& DestPathCorner, bool bTraversingLink, bool bIsNearEndOfPath);
	
	/** pass desired position to movement component (after resolving collisions between crowd agents) */
	virtual void ApplyCrowdAgentPosition(const FVector& NewPosition);

	/** main switch for crowd steering & avoidance */
	UFUNCTION(BlueprintCallable, Category = "Crowd")
	virtual void SuspendCrowdSteering(bool bSuspend);

	/** switch between crowd simulation and parent implementation (following path segments) */
	virtual void SetCrowdSimulationState(ECrowdSimulationState NewState);

	/** called when agent moved to next nav node (poly) */
	virtual void OnNavNodeChanged(NavNodeRef NewPolyRef, NavNodeRef PrevPolyRef, int32 CorridorSize);

	void SetCrowdAnticipateTurns(bool bEnable, bool bUpdateAgent = true);
	void SetCrowdObstacleAvoidance(bool bEnable, bool bUpdateAgent = true);
	void SetCrowdSeparation(bool bEnable, bool bUpdateAgent = true);
	void SetCrowdOptimizeVisibility(bool bEnable, bool bUpdateAgent = true);
	void SetCrowdOptimizeTopology(bool bEnable, bool bUpdateAgent = true);
	void SetCrowdPathOffset(bool bEnable, bool bUpdateAgent = true);
	void SetCrowdSlowdownAtGoal(bool bEnable, bool bUpdateAgent = true);
	void SetCrowdSeparationWeight(float Weight, bool bUpdateAgent = true);
	void SetCrowdCollisionQueryRange(float Range, bool bUpdateAgent = true);
	void SetCrowdPathOptimizationRange(float Range, bool bUpdateAgent = true);
	void SetCrowdAvoidanceQuality(ECrowdAvoidanceQuality::Type Quality, bool bUpdateAgent = true);
	void SetCrowdAvoidanceRangeMultiplier(float Multiplier, bool bUpdateAgent = true);
	void SetCrowdAffectFallingVelocity(bool bEnable);
	void SetCrowdRotateToVelocity(bool bEnable);
	void SetAvoidanceGroup(int32 GroupFlags, bool bUpdateAgent = true);
	void SetGroupsToAvoid(int32 GroupFlags, bool bUpdateAgent = true);
	void SetGroupsToIgnore(int32 GroupFlags, bool bUpdateAgent = true);

	FORCEINLINE bool IsCrowdSimulationEnabled() const { return SimulationState == ECrowdSimulationState::Enabled; }
	FORCEINLINE bool IsCrowdSimulatioSuspended() const { return bSuspendCrowdSimulation; }
	FORCEINLINE bool IsCrowdAnticipateTurnsEnabled() const { return bEnableAnticipateTurns; }
	FORCEINLINE bool IsCrowdObstacleAvoidanceEnabled() const { return bEnableObstacleAvoidance; }
	FORCEINLINE bool IsCrowdSeparationEnabled() const { return bEnableSeparation; }
	FORCEINLINE bool IsCrowdOptimizeVisibilityEnabled() const { return bEnableOptimizeVisibility; /** don't check suspend here! */ }
	FORCEINLINE bool IsCrowdOptimizeTopologyEnabled() const { return bEnableOptimizeTopology; }
	FORCEINLINE bool IsCrowdPathOffsetEnabled() const { return bEnablePathOffset; }
	FORCEINLINE bool IsCrowdSlowdownAtGoalEnabled() const { return bEnableSlowdownAtGoal; }
	FORCEINLINE bool IsCrowdAffectFallingVelocityEnabled() const { return bAffectFallingVelocity; }
	FORCEINLINE bool IsCrowdRotateToVelocityEnabled() const { return bRotateToVelocity; }

	FORCEINLINE ECrowdSimulationState GetCrowdSimulationState() const { return SimulationState; }
	FORCEINLINE bool IsCrowdSimulationActive() const { return IsCrowdSimulationEnabled() && !IsCrowdSimulatioSuspended(); }
	/** checks if bEnableAnticipateTurns is set to true, and if crowd simulation is not suspended */
	FORCEINLINE bool IsCrowdAnticipateTurnsActive() const { return IsCrowdAnticipateTurnsEnabled() && !IsCrowdSimulatioSuspended(); }
	/** checks if bEnableObstacleAvoidance is set to true, and if crowd simulation is not suspended */
	FORCEINLINE bool IsCrowdObstacleAvoidanceActive() const { return IsCrowdObstacleAvoidanceEnabled() && !IsCrowdSimulatioSuspended(); }
	/** checks if bEnableSeparation is set to true, and if crowd simulation is not suspended */
	FORCEINLINE bool IsCrowdSeparationActive() const { return IsCrowdSeparationEnabled() && !IsCrowdSimulatioSuspended(); }
	/** checks if bEnableOptimizeTopology is set to true, and if crowd simulation is not suspended */
	FORCEINLINE bool IsCrowdOptimizeTopologyActive() const { return IsCrowdOptimizeTopologyEnabled() && !IsCrowdSimulatioSuspended(); }

	FORCEINLINE float GetCrowdSeparationWeight() const { return SeparationWeight; }
	FORCEINLINE float GetCrowdCollisionQueryRange() const { return CollisionQueryRange; }
	FORCEINLINE float GetCrowdPathOptimizationRange() const { return PathOptimizationRange; }
	FORCEINLINE ECrowdAvoidanceQuality::Type GetCrowdAvoidanceQuality() const { return AvoidanceQuality; }
	FORCEINLINE float GetCrowdAvoidanceRangeMultiplier() const { return AvoidanceRangeMultiplier; }
	int32 GetAvoidanceGroup() const;
	int32 GetGroupsToAvoid() const;
	int32 GetGroupsToIgnore() const;

	virtual void GetDebugStringTokens(TArray<FString>& Tokens, TArray<EPathFollowingDebugTokens::Type>& Flags) const override;
#if ENABLE_VISUAL_LOG
	virtual void DescribeSelfToVisLog(struct FVisualLogEntry* Snapshot) const override;
#endif // ENABLE_VISUAL_LOG

	UE_DEPRECATED_FORGAME(4.16, "Use ApplyCrowdAgentVelocity function with bIsNearEndOfPath param instead.")
	virtual void ApplyCrowdAgentVelocity(const FVector& NewVelocity, const FVector& DestPathCorner, bool bTraversingLink) {}

	void UpdateDestinationForMovingGoal(const FVector& NewDestination);

protected:

	TWeakInterfacePtr<IRVOAvoidanceInterface> AvoidanceInterface;

public:
	UPROPERTY()
	FVector CrowdAgentMoveDirection;

protected:
#if WITH_EDITORONLY_DATA
	UE_DEPRECATED(4.26, "This member is deprecated and no longer used. Use AvoidanceInterface instead.")
	UPROPERTY(transient)
	TObjectPtr<class UCharacterMovementComponent> CharacterMovement;

	/** DEPRECATED: Group mask for this agent - use IRVOAvoidanceInterface instead */
	UPROPERTY()
	FNavAvoidanceMask AvoidanceGroup_DEPRECATED;

	/** DEPRECATED: Will avoid other agents if they are in one of specified groups - use IRVOAvoidanceInterface instead */
	UPROPERTY()
	FNavAvoidanceMask GroupsToAvoid_DEPRECATED;

	/** DEPRECATED: Will NOT avoid other agents if they are in one of specified groups, higher priority than GroupsToAvoid - use IRVOAvoidanceInterface instead */
	UPROPERTY()
	FNavAvoidanceMask GroupsToIgnore_DEPRECATED;
#endif

	/** if set, velocity will be updated even if agent is falling */
	uint8 bAffectFallingVelocity : 1;

	/** if set, move focus will match velocity direction */
	uint8 bRotateToVelocity : 1;

	/** if set, move velocity will be updated in every tick */
	uint8 bUpdateDirectMoveVelocity : 1;

	/** set when agent is registered in crowd simulation (either controlled or an obstacle) */
	uint8 bRegisteredWithCrowdSimulation : 1;

	/** if set, avoidance and steering will be suspended (used for direct move requests) */
	uint8 bSuspendCrowdSimulation : 1;

	uint8 bEnableAnticipateTurns : 1;
	uint8 bEnableObstacleAvoidance : 1;
	uint8 bEnableSeparation : 1;
	uint8 bEnableOptimizeVisibility : 1;
	uint8 bEnableOptimizeTopology : 1;
	uint8 bEnablePathOffset : 1;
	uint8 bEnableSlowdownAtGoal : 1;

	/** if set, agent if moving on final path part, skip further updates (runtime flag) */
	uint8 bFinalPathPart : 1;

	/** if set, destination overshot can be tested */
	uint8 bCanCheckMovingTooFar : 1;

	/** if set, path parts can be switched in UpdatePathSegment, based on distance */
	uint8 bCanUpdatePathPartInTick : 1;

	/** if set, movement will be finished when velocity is opposite to path direction (runtime flag) */
	uint8 bCheckMovementAngle : 1;

	uint8 bEnableSimulationReplanOnResume : 1;

	TEnumAsByte<ECrowdAvoidanceQuality::Type> AvoidanceQuality;
	ECrowdSimulationState SimulationState;

	float SeparationWeight;
	float CollisionQueryRange;
	float PathOptimizationRange;

	/** multiplier for avoidance samples during detection, doesn't affect actual velocity */
	float AvoidanceRangeMultiplier;

	/** start index of current path part */
	int32 PathStartIndex;

	/** last visited poly on path */
	int32 LastPathPolyIndex;

	// PathFollowingComponent BEGIN
	virtual int32 DetermineStartingPathPoint(const FNavigationPath* ConsideredPath) const override;
	virtual void SetMoveSegment(int32 SegmentStartIndex) override;
	virtual void UpdatePathSegment() override;
	virtual void FollowPathSegment(float DeltaTime) override;
	virtual bool ShouldCheckPathOnResume() const override;
	virtual bool IsOnPath() const override;
	virtual bool UpdateMovementComponent(bool bForce) override;
	virtual void Reset() override;
	// PathFollowingComponent END

	void SwitchToNextPathPart();
	bool ShouldSwitchPathPart(int32 CorridorSize) const;
	bool HasMovedDuringPause() const;
	void UpdateCachedDirections(const FVector& NewVelocity, const FVector& NextPathCorner, bool bTraversingLink);

	virtual bool ShouldTrackMovingGoal(FVector& OutGoalLocation) const;
	
	void RegisterCrowdAgent();

	friend UCrowdManager;
};
