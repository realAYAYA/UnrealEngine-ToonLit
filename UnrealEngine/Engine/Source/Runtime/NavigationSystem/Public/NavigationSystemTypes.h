// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NavFilters/NavigationQueryFilter.h"
#include "AI/NavigationSystemConfig.h"
#include "AI/Navigation/NavLinkDefinition.h"
#include "Math/GenericOctreePublic.h"
#include "AI/NavigationModifier.h"

#define NAVSYS_DEBUG (0 && UE_BUILD_DEBUG)

#define RECAST_INTERNAL_DEBUG_DATA (!UE_BUILD_SHIPPING)

class UBodySetup;
class UNavCollision;
struct FKAggregateGeom;
class FNavigationOctree;
class UNavigationPath;
class ANavigationData;

// LWC_TODO_AI: A lot of the floats in this file should be FVector::FReal. Not until after 5.0!

struct NAVIGATIONSYSTEM_API FPathFindingQueryData
{
	TWeakObjectPtr<const UObject> Owner;
	FVector StartLocation;
	FVector EndLocation;
	FSharedConstNavQueryFilter QueryFilter;

	/** cost limit of nodes allowed to be added to the open list */
	float CostLimit;
	
	/** additional flags passed to navigation data handling request */
	int32 NavDataFlags;

	/** if set, allow partial paths as a result */
	uint32 bAllowPartialPaths : 1;

	FPathFindingQueryData() : StartLocation(FNavigationSystem::InvalidLocation), EndLocation(FNavigationSystem::InvalidLocation), CostLimit(FLT_MAX), NavDataFlags(0), bAllowPartialPaths(true) {}

	FPathFindingQueryData(const UObject* InOwner, const FVector& InStartLocation, const FVector& InEndLocation, FSharedConstNavQueryFilter InQueryFilter = nullptr, int32 InNavDataFlags = 0, bool bInAllowPartialPaths = true, const float InCostLimit = FLT_MAX) :
		Owner(InOwner), StartLocation(InStartLocation), EndLocation(InEndLocation), QueryFilter(InQueryFilter), CostLimit(InCostLimit), NavDataFlags(InNavDataFlags), bAllowPartialPaths(bInAllowPartialPaths) {}
};

struct NAVIGATIONSYSTEM_API FPathFindingQuery : public FPathFindingQueryData
{
	TWeakObjectPtr<const ANavigationData> NavData;
	FNavPathSharedPtr PathInstanceToFill;
	FNavAgentProperties NavAgentProperties;

	FPathFindingQuery() : FPathFindingQueryData() {}
	FPathFindingQuery(const FPathFindingQuery& Source);
	FPathFindingQuery(const UObject* InOwner, const ANavigationData& InNavData, const FVector& Start, const FVector& End, FSharedConstNavQueryFilter SourceQueryFilter = NULL, FNavPathSharedPtr InPathInstanceToFill = NULL, const float CostLimit = FLT_MAX);
	FPathFindingQuery(const INavAgentInterface& InNavAgent, const ANavigationData& InNavData, const FVector& Start, const FVector& End, FSharedConstNavQueryFilter SourceQueryFilter = NULL, FNavPathSharedPtr InPathInstanceToFill = NULL, const float CostLimit = FLT_MAX);

	explicit FPathFindingQuery(FNavPathSharedRef PathToRecalculate, const ANavigationData* NavDataOverride = NULL);

	FPathFindingQuery& SetPathInstanceToUpdate(FNavPathSharedPtr InPathInstanceToFill) { PathInstanceToFill = InPathInstanceToFill; return *this; }
	FPathFindingQuery& SetAllowPartialPaths(bool bAllow) { bAllowPartialPaths = bAllow; return *this; }
	FPathFindingQuery& SetNavAgentProperties(const FNavAgentProperties& InNavAgentProperties) { NavAgentProperties = InNavAgentProperties; return *this; }

	/** utility function to compute a cost limit using an Euclidean heuristic, an heuristic scale and a cost limit factor
	*	CostLimitFactor: multiplier used to compute the cost limit value from the initial heuristic
	*	MinimumCostLimit: minimum clamping value used to prevent low cost limit for short path query */
	float ComputeCostLimitFromHeuristic(const FVector& StartPos, const FVector& EndPos, const float HeuristicScale, const float CostLimitFactor, const float MinimumCostLimit) const;
};

namespace EPathFindingMode
{
	enum Type
	{
		Regular,
		Hierarchical,
	};
};

////////////////////////////////////////////////////////////////////////////
//// Custom path following data
//
///** Custom data passed to movement requests. */
struct NAVIGATIONSYSTEM_API FMoveRequestCustomData
{
};

typedef TSharedPtr<FMoveRequestCustomData, ESPMode::ThreadSafe> FCustomMoveSharedPtr;
typedef TWeakPtr<FMoveRequestCustomData, ESPMode::ThreadSafe> FCustomMoveWeakPtr;

//----------------------------------------------------------------------//
// Active tiles 
//----------------------------------------------------------------------//
struct FNavigationInvokerRaw
{
	FVector Location;
	float RadiusMin;
	float RadiusMax;

	FNavigationInvokerRaw(const FVector& InLocation, float Min, float Max)
		: Location(InLocation), RadiusMin(Min), RadiusMax(Max)
	{}
};

struct FNavigationInvoker
{
	TWeakObjectPtr<AActor> Actor;

	/** tiles GenerationRadius away or close will be generated if they're not already present */
	float GenerationRadius;
	/** tiles over RemovalRadius will get removed.
	*	@Note needs to be >= GenerationRadius or will get clampped */
	float RemovalRadius;

	FNavigationInvoker();
	FNavigationInvoker(AActor& InActor, float InGenerationRadius, float InRemovalRadius);
};

namespace NavigationHelper
{
	void GatherCollision(UBodySetup* RigidBody, TNavStatArray<FVector>& OutVertexBuffer, TNavStatArray<int32>& OutIndexBuffer, const FTransform& ComponentToWorld = FTransform::Identity);
	void GatherCollision(UBodySetup* RigidBody, UNavCollision* NavCollision);

	/** gather collisions from aggregated geom, convex and tri mesh elements are not supported - use override with full UBodySetup param instead */
	void GatherCollision(const FKAggregateGeom& AggGeom, UNavCollision& NavCollision);
}

namespace FNavigationSystem
{
	enum ECreateIfMissing
	{
		Invalid = -1,
		DontCreate = 0,
		Create = 1,
	};

	typedef ECreateIfMissing ECreateIfEmpty;
}

//----------------------------------------------------------------------//
// 
//----------------------------------------------------------------------//

namespace NavigationHelper
{
	struct NAVIGATIONSYSTEM_API FNavLinkOwnerData
	{
		const AActor* Actor;
		FTransform LinkToWorld;

		FNavLinkOwnerData() : Actor(nullptr) {}
		FNavLinkOwnerData(const AActor& InActor);
		FNavLinkOwnerData(const USceneComponent& InComponent);
	};

	DECLARE_DELEGATE_ThreeParams(FNavLinkProcessorDelegate, FCompositeNavModifier*, const AActor*, const TArray<FNavigationLink>&);
	DECLARE_DELEGATE_ThreeParams(FNavLinkSegmentProcessorDelegate, FCompositeNavModifier*, const AActor*, const TArray<FNavigationSegmentLink>&);

	DECLARE_DELEGATE_ThreeParams(FNavLinkProcessorDataDelegate, FCompositeNavModifier*, const FNavLinkOwnerData&, const TArray<FNavigationLink>&);
	DECLARE_DELEGATE_ThreeParams(FNavLinkSegmentProcessorDataDelegate, FCompositeNavModifier*, const FNavLinkOwnerData&, const TArray<FNavigationSegmentLink>&);

	/** Set new implementation of nav link processor, a function that will be
	*	be used to process/transform links before adding them to CompositeModifier.
	*	This function is supposed to be called once during the engine/game
	*	setup phase. Not intended to be toggled at runtime */
	NAVIGATIONSYSTEM_API void SetNavLinkProcessorDelegate(const FNavLinkProcessorDataDelegate& NewDelegate);
	NAVIGATIONSYSTEM_API void SetNavLinkSegmentProcessorDelegate(const FNavLinkSegmentProcessorDataDelegate& NewDelegate);

	/** called to do any necessary processing on NavLinks and put results in CompositeModifier */
	NAVIGATIONSYSTEM_API void ProcessNavLinkAndAppend(FCompositeNavModifier* OUT CompositeModifier, const AActor* Actor, const TArray<FNavigationLink>& IN NavLinks);
	NAVIGATIONSYSTEM_API void ProcessNavLinkAndAppend(FCompositeNavModifier* OUT CompositeModifier, const FNavLinkOwnerData& OwnerData, const TArray<FNavigationLink>& IN NavLinks);

	/** called to do any necessary processing on NavLinks and put results in CompositeModifier */
	NAVIGATIONSYSTEM_API void ProcessNavLinkSegmentAndAppend(FCompositeNavModifier* OUT CompositeModifier, const AActor* Actor, const TArray<FNavigationSegmentLink>& IN NavLinks);
	NAVIGATIONSYSTEM_API void ProcessNavLinkSegmentAndAppend(FCompositeNavModifier* OUT CompositeModifier, const FNavLinkOwnerData& OwnerData, const TArray<FNavigationSegmentLink>& IN NavLinks);

	NAVIGATIONSYSTEM_API void DefaultNavLinkProcessorImpl(FCompositeNavModifier* OUT CompositeModifier, const FNavLinkOwnerData& OwnerData, const TArray<FNavigationLink>& IN NavLinks);
	NAVIGATIONSYSTEM_API void DefaultNavLinkSegmentProcessorImpl(FCompositeNavModifier* OUT CompositeModifier, const FNavLinkOwnerData& OwnerData, const TArray<FNavigationSegmentLink>& IN NavLinks);

	NAVIGATIONSYSTEM_API bool IsBodyNavigationRelevant(const UBodySetup& IN BodySetup);
}