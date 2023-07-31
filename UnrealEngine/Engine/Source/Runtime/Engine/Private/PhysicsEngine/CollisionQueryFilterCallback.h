// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CollisionQueryFilterCallbackCore.h"
#include "CollisionQueryParams.h"
#include "Physics/PhysicsInterfaceTypes.h"

#define ENABLE_PREFILTER_LOGGING 0

struct FBodyInstance;

/** TArray typedef of components to ignore. */
typedef FCollisionQueryParams::IgnoreComponentsArrayType FilterIgnoreComponentsArrayType;

/** TArray typedef of actors to ignore. */
typedef FCollisionQueryParams::IgnoreActorsArrayType FilterIgnoreActorsArrayType;

class FCollisionQueryFilterCallback : public ICollisionQueryFilterCallbackBase
{
public:
	/** Result of PreFilter callback. */
	ECollisionQueryHitType PreFilterReturnValue;

	/** List of ComponentIds for this query to ignore */
	const FilterIgnoreComponentsArrayType& IgnoreComponents;

	/** List of ActorIds for this query to ignore */
	const FilterIgnoreActorsArrayType& IgnoreActors;

	/** Whether we are doing an overlap query. This is needed to ensure physx results are never blocking (even if they are in terms of unreal)*/
	bool bIsOverlapQuery;

	/** Whether to ignore touches (convert an eTOUCH result to eNONE). */
	bool bIgnoreTouches;

	/** Whether to ignore blocks (convert an eBLOCK result to eNONE). */
	bool bIgnoreBlocks;

	FCollisionQueryFilterCallback(const FCollisionQueryParams& InQueryParams, bool bInIsSweep)
		: IgnoreComponents(InQueryParams.GetIgnoredComponents())
		, IgnoreActors(InQueryParams.GetIgnoredActors())
#if DETECT_SQ_HITCHES
		, bRecordHitches(false)
#endif
		, bIsSweep(bInIsSweep)
	{
		PreFilterReturnValue = ECollisionQueryHitType::None;
		bIsOverlapQuery = false;
		bIgnoreTouches = InQueryParams.bIgnoreTouches;
		bIgnoreBlocks = InQueryParams.bIgnoreBlocks;
		bDiscardInitialOverlaps = !InQueryParams.bFindInitialOverlaps;
	}
	~FCollisionQueryFilterCallback() {}


	static ECollisionQueryHitType CalcQueryHitType(const FCollisionFilterData& QueryFilter, const FCollisionFilterData& ShapeFilter, bool bPreFilter = false);

	ECollisionQueryHitType PreFilterImp(const FCollisionFilterData& FilterData, const FCollisionFilterData& ShapeFilterData, uint32 ComponentID, const FBodyInstance* BodyInstance);

	ECollisionQueryHitType PostFilterImp(const FCollisionFilterData& FilterData, bool bIsOverlap);

	ECollisionQueryHitType PreFilterImp(const FCollisionFilterData& FilterData, const Chaos::FPerShapeData& Shape, const Chaos::FGeometryParticle& Actor);
	ECollisionQueryHitType PreFilterImp(const FCollisionFilterData& FilterData, const Chaos::FPerShapeData& Shape, const Chaos::FGeometryParticleHandle& Actor);

	ECollisionQueryHitType PostFilterImp(const FCollisionFilterData& FilterData, const ChaosInterface::FQueryHit& Hit);
	ECollisionQueryHitType PostFilterImp(const FCollisionFilterData& FilterData, const ChaosInterface::FPTQueryHit& Hit);

	virtual ECollisionQueryHitType PostFilter(const FCollisionFilterData& FilterData, const ChaosInterface::FQueryHit& Hit) override
	{
		return PostFilterImp(FilterData, Hit);
	}

	virtual ECollisionQueryHitType PostFilter(const FCollisionFilterData& FilterData, const ChaosInterface::FPTQueryHit& Hit) override
	{
		return PostFilterImp(FilterData, Hit);
	}

	virtual ECollisionQueryHitType PreFilter(const FCollisionFilterData& FilterData, const Chaos::FPerShapeData& Shape, const Chaos::FGeometryParticle& Actor) override
	{
		return PreFilterImp(FilterData, Shape, Actor);
	}

	virtual ECollisionQueryHitType PreFilter(const FCollisionFilterData& FilterData, const Chaos::FPerShapeData& Shape, const Chaos::FGeometryParticleHandle& Actor) override
	{
		return PreFilterImp(FilterData, Shape, Actor);
	}

#if DETECT_SQ_HITCHES
	// Util struct to record what preFilter was called with
	struct FPreFilterRecord
	{
		FString OwnerComponentReadableName;
		ECollisionQueryHitType Result;
	};

	TArray<FPreFilterRecord> PreFilterHitchInfo;
	bool bRecordHitches;
#endif
	bool bDiscardInitialOverlaps;
	bool bIsSweep;

private:
	template <typename TParticle>
	ECollisionQueryHitType PreFilterBaseImp(const FCollisionFilterData& FilterData, const Chaos::FPerShapeData& Shape, const TParticle& Actor);
};

