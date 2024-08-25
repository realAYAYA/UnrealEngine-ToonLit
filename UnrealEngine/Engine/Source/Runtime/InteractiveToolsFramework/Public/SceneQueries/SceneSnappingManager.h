// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "Misc/Optional.h"
#include "Math/Vector.h"
#include "Math/Ray.h"
#include "Engine/HitResult.h"
#include "SceneSnappingManager.generated.h"

class AActor;
class UActorComponent;
class UPrimitiveComponent;
class UInteractiveToolManager;
class UInteractiveGizmoManager;



struct FSceneQueryVisibilityFilter
{
	/** Optional: components to consider invisible even if they aren't. */
	const TArray<const UPrimitiveComponent*>* ComponentsToIgnore = nullptr;

	/** Optional: components to consider visible even if they aren't. */
	const TArray<const UPrimitiveComponent*>* InvisibleComponentsToInclude = nullptr;

	/** @return true if the Component is currently configured as visible (does not consider ComponentsToIgnore or InvisibleComponentsToInclude lists) */
	INTERACTIVETOOLSFRAMEWORK_API bool IsVisible(const UPrimitiveComponent* Component) const;
};


/**
* Configuration variables for a USceneSnappingManager hit query request.
*/
struct FSceneHitQueryRequest
{
	/** scene query ray */
	FRay3d WorldRay;

	bool bWantHitGeometryInfo = false;

	FSceneQueryVisibilityFilter VisibilityFilter;
};


/**
* Computed result of a USceneSnappingManager hit query request
*/
struct FSceneHitQueryResult
{
	/** Actor that owns hit target */
	AActor* TargetActor = nullptr;
	/** Component that owns hit target */
	UPrimitiveComponent* TargetComponent = nullptr;

	/** hit position*/
	FVector3d Position = FVector3d::Zero();
	/** hit normal */
	FVector3d Normal = FVector3d::UnitZ();

	/** integer ID of triangle that was hit */
	int HitTriIndex = -1;
	/** Vertices of triangle that was hit (for debugging, may not be set) */
	FVector3d TriVertices[3];

	FHitResult HitResult;

	INTERACTIVETOOLSFRAMEWORK_API void InitializeHitResult(const FSceneHitQueryRequest& FromRequest);
};




/** Types of Snap Queries that a USceneSnappingManager may support */
UENUM()
enum class ESceneSnapQueryType : uint8
{
	/** snapping a position */
	Position = 1,
	Rotation = 2
};


/** Types of Snap Targets that a caller may want to run snap queries against. */
UENUM()
enum class ESceneSnapQueryTargetType : uint8
{
	None = 0,
	/** Consider any mesh vertex */
	MeshVertex = 1,
	/** Consider any mesh edge */
	MeshEdge = 2,
	/** Grid Snapping */
	Grid = 4,

	All = MeshVertex | MeshEdge | Grid
};
ENUM_CLASS_FLAGS(ESceneSnapQueryTargetType);


/**
 * Configuration variables for a USceneSnappingManager snap query request.
 */
struct FSceneSnapQueryRequest
{
	/** What type of snap query geometry is this */
	ESceneSnapQueryType RequestType = ESceneSnapQueryType::Position;
	/** What does caller want to try to snap to */
	ESceneSnapQueryTargetType TargetTypes = ESceneSnapQueryTargetType::Grid;

	/** Optional explicitly specified position grid */
	TOptional<FVector> GridSize{};

	/** Optional explicitly specified rotation grid */
	TOptional<FRotator> RotGridSize{};

	/** Snap input position */
	FVector Position = FVector::ZeroVector;

	/**
	 *  When considering if one point is close enough to another point for snapping purposes, they
	 *  must deviate less than this number of degrees (in visual angle) to be considered an acceptable snap position.
	 */
	float VisualAngleThresholdDegrees = 15.0;

	/** Snap input rotation delta */
	FQuat DeltaRotation = FQuat(EForceInit::ForceInitToZero);

	/** Optional: components to consider invisible even if they aren't. */
	const TArray<const UPrimitiveComponent*>* ComponentsToIgnore = nullptr;
	/** Optional: components to consider visible even if they aren't. */
	const TArray<const UPrimitiveComponent*>* InvisibleComponentsToInclude = nullptr;
};


/**
 * Computed result of a USceneSnappingManager snap query request
 */
struct FSceneSnapQueryResult
{
	/** Actor that owns snap target */
	AActor* TargetActor = nullptr;
	/** Component that owns snap target */
	UActorComponent* TargetComponent = nullptr;
	/** What kind of geometric element was snapped to */
	ESceneSnapQueryTargetType TargetType = ESceneSnapQueryTargetType::None;

	/** Snap position (may not be set depending on query types) */
	FVector Position;
	/** Snap normal (may not be set depending on query types) */
	FVector Normal;
	/** Snap rotation delta (may not be set depending on query types) */
	FQuat   DeltaRotation;

	/** Vertices of triangle that contains result (for debugging, may not be set) */
	FVector TriVertices[3];
	/** Vertex/Edge index we snapped to in triangle */
	int TriSnapIndex;

};


/**
 * USceneSnappingManager is intended to be used as a base class for a Snapping implementation
 * stored in the ContextObjectStore of an InteractiveToolsContext. ITF classes like Tools and Gizmos
 * can then access this object and run snap queries via the various API functions.
 * 
 * USceneSnappingManager::Find() can be used to look up a registered USceneSnappingManager, if one is available
 * 
 * See UModelingSceneSnappingManager for a sample implementation.
 */
UCLASS(MinimalAPI)
class USceneSnappingManager : public UObject
{
	GENERATED_BODY()
public:

	/**
	* Try to find a Hit Object in the scene that satisfies the Hit Query
	* @param Request hit query configuration
	* @param Results hit query result
	* @return true if any valid hit target was found
	* @warning implementations are not required (and may not be able) to support hit testing
	*/
	virtual bool ExecuteSceneHitQuery(const FSceneHitQueryRequest& Request, FSceneHitQueryResult& ResultOut) const
	{
		return false;
	}

	/**
	* Try to find Snap Targets/Results in the scene that satisfy the Snap Query.
	* @param Request snap query configuration
	* @param Results list of potential snap results
	* @return true if any valid snap target/result was found
	* @warning implementations are not required (and may not be able) to support snapping
	*/
	virtual bool ExecuteSceneSnapQuery(const FSceneSnapQueryRequest& Request, TArray<FSceneSnapQueryResult>& ResultsOut) const
	{
		return false;
	}


public:
	/**
	 * @return existing USceneSnappingManager registered in context store via the ToolManager, or nullptr if not found
	 */
	static INTERACTIVETOOLSFRAMEWORK_API USceneSnappingManager* Find(UInteractiveToolManager* ToolManager);

	/**
	 * @return existing USceneSnappingManager registered in context store via the ToolManager, or nullptr if not found
	 */
	static INTERACTIVETOOLSFRAMEWORK_API USceneSnappingManager* Find(UInteractiveGizmoManager* GizmoManager);
};
