// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LearningAgentsDebug.h"

#include "Templates/SharedPointer.h"
#include "UObject/Object.h"
#include "Components/SplineComponent.h" // Required for ESplineCoordinateSpace::World

#include "LearningAgentsHelpers.generated.h"

class ULearningAgentsManagerComponent;
class UMeshComponent;

//------------------------------------------------------------------

/**
 * The base class for all helpers. Helpers are additional objects that can be used in getting or setting observations,
 * actions, rewards, and completions.
 */
UCLASS(Abstract, BlueprintType)
class LEARNINGAGENTS_API ULearningAgentsHelper : public UObject
{
	GENERATED_BODY()

public:

	/** Reference to the Manager Component this helper is associated with. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "LearningAgents")
	TObjectPtr<ULearningAgentsManagerComponent> ManagerComponent;

public:

	/**
	 * Called whenever agents are added to the associated ULearningAgentsManagerComponent object.
	 * @param AgentIds Array of agent ids which have been added
	 */
	virtual void OnAgentsAdded(const TArray<int32>& AgentIds);

	/**
	 * Called whenever agents are removed from the associated ULearningAgentsManagerComponent object.
	 * @param AgentIds Array of agent ids which have been removed
	 */
	virtual void OnAgentsRemoved(const TArray<int32>& AgentIds);

	/**
	 * Called whenever agents are reset on the associated ULearningAgentsManagerComponent object.
	 * @param AgentIds Array of agent ids which have been reset
	 */
	virtual void OnAgentsReset(const TArray<int32>& AgentIds);

public:
#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	/** Color used to draw this helper in the visual log */
	FLinearColor VisualLogColor = FColor::Magenta;
#endif
};

//------------------------------------------------------------------

/** A helper for computing various properties from a SplineComponent. */
UCLASS()
class LEARNINGAGENTS_API USplineComponentHelper : public ULearningAgentsHelper
{
	GENERATED_BODY()

public:

	/**
	 * Adds a new spline component helper to the given manager component.
	 * @param InManagerComponent The manager component to add this helper to (such as an Interactor or Trainer).
	 * @param Name The name of this new helper. Used for debugging.
	 * @return The newly created helper.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (DefaultToSelf = "InManagerComponent"))
	static USplineComponentHelper* AddSplineComponentHelper(ULearningAgentsManagerComponent* InManagerComponent, const FName Name = NAME_None);

	/**
	 * Gets the position on a spline closest to the provided position.
	 * @param AgentId The agent id to run the helper for.
	 * @param SplineComponent The spline component to use.
	 * @param Position The position to find the closest position to.
	 * @param CoordinateSpace The coordinate space to use for the spline.
	 * @return The closest position on the spline.
	 */
	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AgentId = "-1"))
	FVector GetNearestPositionOnSpline(const int32 AgentId, const USplineComponent* SplineComponent, const FVector Position, const ESplineCoordinateSpace::Type CoordinateSpace = ESplineCoordinateSpace::World) const;

	/**
	 * Gets the distance along a spline closest to the provided position.
	 * @param AgentId The agent id to run the helper for.
	 * @param SplineComponent The spline component to use.
	 * @param Position The position to find the closest position to.
	 * @param CoordinateSpace The coordinate space to use for the spline.
	 * @return The distance along the spline of the closest position.
	 */
	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AgentId = "-1"))
	float GetDistanceAlongSplineAtPosition(const int32 AgentId, const USplineComponent* SplineComponent, const FVector Position, const ESplineCoordinateSpace::Type CoordinateSpace = ESplineCoordinateSpace::World) const;

	/**
	 * Gets the position along a spline at the given distance.
	 * @param AgentId The agent id to run the helper for.
	 * @param SplineComponent The spline component to use.
	 * @param DistanceAlongSpline The distance along the spline to use.
	 * @param CoordinateSpace The coordinate space to use for the spline.
	 * @return The position at the given distance along the spline.
	 */
	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AgentId = "-1"))
	FVector GetPositionAtDistanceAlongSpline(const int32 AgentId, const USplineComponent* SplineComponent, const float DistanceAlongSpline, const ESplineCoordinateSpace::Type CoordinateSpace = ESplineCoordinateSpace::World) const;

	/**
	 * Gets the direction along a spline at the given distance.
	 * @param AgentId The agent id to run the helper for.
	 * @param SplineComponent The spline component to use.
	 * @param DistanceAlongSpline The distance along the spline to use.
	 * @param CoordinateSpace The coordinate space to use for the spline.
	 * @return The direction at the given distance along the spline.
	 */
	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AgentId = "-1"))
	FVector GetDirectionAtDistanceAlongSpline(const int32 AgentId, const USplineComponent* SplineComponent, const float DistanceAlongSpline, const ESplineCoordinateSpace::Type CoordinateSpace = ESplineCoordinateSpace::World) const;

	/**
	 * Gets the proportion along a spline in the range 0-1 for a given distance.
	 * @param AgentId The agent id to run the helper for.
	 * @param SplineComponent The spline component to use.
	 * @param DistanceAlongSpline The distance along the spline to use.
	 * @return The proportion along the spline in the range 0-1.
	 */
	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AgentId = "-1"))
	float GetProportionAlongSpline(const int32 AgentId, const USplineComponent* SplineComponent, const float DistanceAlongSpline) const;

	/**
	 * Gets the proportion along a spline encoded as an angle between -180 and 180 degrees for a given distance. 
	 * This should be used for looped splines.
	 * @param AgentId The agent id to run the helper for.
	 * @param SplineComponent The spline component to use.
	 * @param DistanceAlongSpline The distance along the spline to use.
	 * @return The proportion along the spline encoded as an angle between -180 and 180 degrees
	 */
	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AgentId = "-1"))
	float GetProportionAlongSplineAsAngle(const int32 AgentId, const USplineComponent* SplineComponent, const float DistanceAlongSpline) const;

	/**
	 * Gets an array of positions sampled along a spline between some starting and stopping distance. Deals properly
	 * for splines which are looped.
	 * @param OutPositions Output array of spline positions.
	 * @param AgentId The agent id to run the helper for.
	 * @param SplineComponent The spline component to use.
	 * @param PositionNum The number of positions to sample along the spline.
	 * @param StartDistanceAlongSpline The starting distance along the spline.
	 * @param StopDistanceAlongSpline The stopping distance along the spline.
	 * @param CoordinateSpace The coordinate space to use for the spline.
	 */
	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AgentId = "-1"))
	void GetPositionsAlongSpline(
		TArray<FVector>& OutPositions,
		const int32 AgentId,
		const USplineComponent* SplineComponent,
		const int32 PositionNum,
		const float StartDistanceAlongSpline,
		const float StopDistanceAlongSpline,
		const ESplineCoordinateSpace::Type CoordinateSpace = ESplineCoordinateSpace::World) const;

	/**
	 * Gets the velocity a point is traveling along a spline.
	 * @param AgentId The agent id to run the helper for.
	 * @param SplineComponent The spline component to use.
	 * @param Position The position of the point.
	 * @param Velocity The velocity of the point.
	 * @param FiniteDifferenceDelta The finite difference delta used to estimate the velocity along the spline.
	 * @param CoordinateSpace The coordinate space to use for the spline.
	 * @return The scalar velocity the point is traveling along the spline.
	 */
	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AgentId = "-1"))
	float GetVelocityAlongSpline(
		const int32 AgentId,
		const USplineComponent* SplineComponent,
		const FVector Position,
		const FVector Velocity,
		const float FiniteDifferenceDelta = 10.0f,
		const ESplineCoordinateSpace::Type CoordinateSpace = ESplineCoordinateSpace::World) const;
};

//------------------------------------------------------------------

/** A helper for projecting onto surfaces. */
UCLASS()
class LEARNINGAGENTS_API UProjectionHelper : public ULearningAgentsHelper
{
	GENERATED_BODY()

public:

	/**
	 * Adds a new projection helper to the given manager component.
	 * @param InManagerComponent The manager component to add this helper to (such as an Interactor or Trainer).
	 * @param Name The name of this new helper. Used for debugging.
	 * @return The newly created helper.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (DefaultToSelf = "InManagerComponent"))
	static UProjectionHelper* AddProjectionHelper(ULearningAgentsManagerComponent* InManagerComponent, const FName Name = NAME_None);

	/**
	 * Projects a transform onto the zero height ground plane resulting in translation only in XY and rotation only around the Z.
	 * @param AgentId The agent id to run the helper for.
	 * @param Transform The transform to project.
	 * @param LocalForwardVector The local forward direction.
	 * @return The transform projected onto the ground plane.
	 */
	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AgentId = "-1"))
	FTransform ProjectTransformOntoGroundPlane(const int32 AgentId, const FTransform Transform, const FVector LocalForwardVector = FVector::ForwardVector) const;

	/**
	 * Projects a position and rotation onto the zero height ground plane resulting in translation only in XY and rotation only around the Z.
	 * @param OutPosition The output projected position.
	 * @param OutRotation The output projected rotation.
	 * @param AgentId The agent id to run the helper for.
	 * @param InPosition The input position.
	 * @param InRotation The input rotation.
	 * @param LocalForwardVector The local forward direction.
	 */
	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AgentId = "-1"))
	void ProjectPositionRotationOntoGroundPlane(
		FVector& OutPosition, 
		FRotator& OutRotation, 
		const int32 AgentId, 
		const FVector InPosition, 
		const FRotator InRotation, 
		const FVector LocalForwardVector = FVector::ForwardVector) const;
};

//------------------------------------------------------------------

/** A helper for getting various properties from a MeshComponent. */
UCLASS()
class LEARNINGAGENTS_API UMeshComponentHelper : public ULearningAgentsHelper
{
	GENERATED_BODY()

public:

	/**
	 * Adds a new mesh component helper to the given manager component.
	 * @param InManagerComponent The manager component to add this helper to (such as an Interactor or Trainer).
	 * @param Name The name of this new helper. Used for debugging.
	 * @return The newly created helper.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (DefaultToSelf = "InManagerComponent"))
	static UMeshComponentHelper* AddMeshComponentHelper(ULearningAgentsManagerComponent* InManagerComponent, const FName Name = NAME_None);

	/**
	 * Gets the bone positions for a set of bones of the mesh component in world space.
	 * @param OutBonePositions The output array of bone positions.
	 * @param AgentId The agent id to run the helper for.
	 * @param MeshComponent The mesh component to use.
	 * @param BoneNames The names of the bones to use.
	 */
	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AgentId = "-1"))
	void GetMeshBonePositions(TArray<FVector>& OutBonePositions, const int32 AgentId, const UMeshComponent* MeshComponent, const TArray<FName>& BoneNames) const;
};

//------------------------------------------------------------------

/** A helper for performing various kinds of ray cast. */
UCLASS()
class LEARNINGAGENTS_API URayCastHelper : public ULearningAgentsHelper
{
	GENERATED_BODY()

public:

	/**
	 * Adds a new ray cast helper to the given manager component.
	 * @param InManagerComponent The manager component to add this helper to (such as an Interactor or Trainer).
	 * @param Name The name of this new helper. Used for debugging.
	 * @return The newly created helper.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (DefaultToSelf = "InManagerComponent"))
	static URayCastHelper* AddRayCastHelper(ULearningAgentsManagerComponent* InManagerComponent, const FName Name = NAME_None);

	/**
	 * Samples a grid of heights from above.
	 * @param OutHeights The output array of heights for each ray in the grid.
	 * @param AgentId The agent id to run the helper for.
	 * @param Position The central position of the grid.
	 * @param Rotation The orientation of the grid.
	 * @param RowNum The number of rows in the grid.
	 * @param ColNum The number of columns in the grid.
	 * @param RowWidth The width of grid rows.
	 * @param ColWidth The width of grid columns.
	 * @param MaxHeight The height at which to start ray casts from.
	 * @param MinHeight The height at which to send ray casts to.
	 * @param CollisionChannel The collision channel to use for the ray-casts.
	 */
	UFUNCTION(BlueprintPure=false, Category = "LearningAgents", meta = (AgentId = "-1"))
	void RayCastGridHeights(
		TArray<float>& OutHeights,
		const int32 AgentId,
		const FVector Position,
		const FRotator Rotation,
		const int32 RowNum = 5,
		const int32 ColNum = 5,
		const float RowWidth = 1000.0f,
		const float ColWidth = 1000.0f,
		const float MaxHeight = 10000.0f,
		const float MinHeight = -10000.0f,
		const ECollisionChannel CollisionChannel = ECollisionChannel::ECC_WorldStatic) const;

	/**
	 * Casts rays out in a radius from a given position.
	 * @param OutDistances The output array of distances for each ray from the start position.
	 * @param AgentId The agent id to run the helper for.
	 * @param Position The central position of the ray cast.
	 * @param Rotation The orientation of the ray cast.
	 * @param RayNum The number of rays to cast.
	 * @param MinAngle The minimum angle to spread the rays.
	 * @param MaxAngle The maximum angle to spread the rays.
	 * @param MaxRayDist The maximum distance at which to shoot rays.
	 * @param CollisionChannel The collision channel to use for the ray-casts.
	 */
	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AgentId = "-1"))
	void RayCastRadial(
		TArray<float>& OutDistances,
		const int32 AgentId,
		const FVector Position,
		const FRotator Rotation,
		const int32 RayNum = 7,
		const float MinAngle = -90.0f,
		const float MaxAngle = 90.0f,
		const float MaxRayDist = 500.0f,
		const FVector LocalForward = FVector::ForwardVector,
		const ECollisionChannel CollisionChannel = ECollisionChannel::ECC_WorldStatic) const;

};

//------------------------------------------------------------------

/**
 * A helper for monitoring collisions between components.
 *
 * This object works by monitoring for collisions between a given component and any other components with a given tag.
 * To indicate which component to monitor for collisions `SetComponent` should be called (generally on the 
 * `ULearningAgentsManagerComponent::AgentsAdded` event) with the corresponding agent id.
 * 
 * If any collisions occur `GetCollisionOccurred` will report true until it is reset to false either by calling 
 * `ResetCollisionOccurred` or `GetAndResetCollisionOccurred`. It will also be automatically reset when the agent is 
 * reset.
 */
UCLASS()
class LEARNINGAGENTS_API UCollisionMonitorHelper : public ULearningAgentsHelper
{
	GENERATED_BODY()

public:

	/**
	 * Adds a new collision monitor helper to the given manager component.
	 * @param InManagerComponent The manager component to add this helper to (such as an Interactor or Trainer).
	 * @param Name The name of this new helper. Used for debugging.
	 * @return The newly created helper.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (DefaultToSelf = "InManagerComponent"))
	static UCollisionMonitorHelper* AddCollisionMonitorHelper(
		ULearningAgentsManagerComponent* InManagerComponent, 
		const FName Name = NAME_None);

	/**
	 * Sets the component to monitor for collisions. Generally this should be called on the 
	 * `ULearningAgentsManagerComponent::AgentsAdded` event.
	 * @param AgentId The agent id to associate this component with.
	 * @param Component The component to monitor for collisions.
	 * @param OtherComponentTag The tag other components must have to trigger collisions.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (AgentId = "-1"))
	void SetComponent(const int32 AgentId, UPrimitiveComponent* Component, const FName OtherComponentTag = NAME_None);

	/**
	 * Gets if a collision has occurred for this agent.
	 * @param AgentId The agent id to check for collisions for.
	 */
	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (AgentId = "-1"))
	bool GetCollisionOccurred(const int32 AgentId) const;
	
	/**
	 * Gets if a collision has occurred for this agent and resets the state of if a collision has occurred back to false.
	 * @param AgentId The agent id to check for collisions for.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (AgentId = "-1"))
	bool GetAndResetCollisionOccurred(const int32 AgentId);

	/**
	 * Resets the state of if a collision has occurred back to false.
	 * @param AgentId The agent id to reset the state for.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (AgentId = "-1"))
	void ResetCollisionOccurred(const int32 AgentId);

public:

	/** Initialize the internal state for a given maximum number of agents */
	void Init(const int32 MaxAgentNum);

	//~ Begin ULearningAgentsHelper Interface
	virtual void OnAgentsRemoved(const TArray<int32>& AgentIds) override;
	virtual void OnAgentsReset(const TArray<int32>& AgentIds) override;
	//~ End ULearningAgentsHelper Interface

private:

	TLearningArray<1, UPrimitiveComponent*, TInlineAllocator<32>> Components;
	TLearningArray<1, FName, TInlineAllocator<32>> OtherComponentTags;
	TLearningArray<1, bool, TInlineAllocator<32>> CollisionsOccured;

private:

	UFUNCTION()
	void HandleOnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);
};
