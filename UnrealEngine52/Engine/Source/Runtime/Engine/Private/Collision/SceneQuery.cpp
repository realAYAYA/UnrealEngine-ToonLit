// Copyright Epic Games, Inc. All Rights Reserved.

#include "Engine/World.h"
#include "CollisionDebugDrawingPublic.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "Physics/PhysicsInterfaceUtils.h"
#include "Collision/CollisionConversions.h"
#include "PhysicsEngine/CollisionQueryFilterCallback.h"
#include "PhysicsEngine/ScopedSQHitchRepeater.h"

#include "Collision/CollisionDebugDrawing.h"

float DebugLineLifetime = 2.f;

#include "PhysicsEngine/CollisionAnalyzerCapture.h"

#include "Physics/Experimental/ChaosInterfaceWrapper.h"
#include "PBDRigidsSolver.h"

CSV_DEFINE_CATEGORY(SceneQuery, false);

enum class ESingleMultiOrTest : uint8
{
	Single,
	Multi,
	Test
};

enum class ESweepOrRay : uint8
{
	Raycast,
	Sweep,
};

struct FGeomSQAdditionalInputs
{
	FGeomSQAdditionalInputs(const FCollisionShape& InCollisionShape, const FQuat& InGeomRot)
		: ShapeAdapter(InGeomRot, InCollisionShape)
		, CollisionShape(InCollisionShape)
	{
	}

	const FPhysicsGeometry* GetGeometry() const
	{
		return &ShapeAdapter.GetGeometry();
	}

	const FQuat* GetGeometryOrientation() const
	{
		return &ShapeAdapter.GetGeomOrientation();
	}


	const FCollisionShape* GetCollisionShape() const
	{
		return &CollisionShape;
	}

	FPhysicsShapeAdapter ShapeAdapter;
	const FCollisionShape& CollisionShape;
};

struct FGeomCollectionSQAdditionalInputs
{
	FGeomCollectionSQAdditionalInputs(const FPhysicsGeometryCollection& InCollection, const FQuat& InGeomRot)
	: Collection(InCollection)
	, GeomRot(InGeomRot)
	{
	}

	const FPhysicsGeometry* GetGeometry() const
	{
		return &Collection.GetGeometry();
	}

	const FQuat* GetGeometryOrientation() const
	{
		return &GeomRot;
	}

	const FPhysicsGeometryCollection* GetCollisionShape() const
	{
		return &Collection;
	}

	const FPhysicsGeometryCollection& Collection;
	const FQuat& GeomRot;
};

struct FRaycastSQAdditionalInputs
{
	const FPhysicsGeometry* GetGeometry() const
	{
		return nullptr;
	}

	const FQuat* GetGeometryOrientation() const
	{
		return nullptr;
	}

	const FCollisionShape* GetCollisionShape() const
	{
		return nullptr;
	}
};

void LowLevelRaycast(FPhysScene& Scene, const FVector& Start, const FVector& Dir, float DeltaMag, FPhysicsHitCallback<FHitRaycast>& HitBuffer, EHitFlags OutputFlags, FQueryFlags QueryFlags, const FCollisionFilterData& Filter, const ChaosInterface::FQueryFilterData& QueryFilterData, ICollisionQueryFilterCallbackBase* QueryCallback, const ChaosInterface::FQueryDebugParams& DebugParams);
void LowLevelSweep(FPhysScene& Scene, const FPhysicsGeometry& Geom, const FTransform& StartTM, const FVector& Dir, float DeltaMag, FPhysicsHitCallback<FHitSweep>& HitBuffer, EHitFlags OutputFlags, FQueryFlags QueryFlags, const FCollisionFilterData& Filter, const ChaosInterface::FQueryFilterData& QueryFilterData, ICollisionQueryFilterCallbackBase* QueryCallback, const ChaosInterface::FQueryDebugParams& DebugParams);
void LowLevelOverlap(FPhysScene& Scene, const FPhysicsGeometry& Geom, const FTransform& GeomPose, FPhysicsHitCallback<FHitOverlap>& HitBuffer, FQueryFlags QueryFlags, const FCollisionFilterData& Filter, const ChaosInterface::FQueryFilterData& QueryFilterData, ICollisionQueryFilterCallbackBase* QueryCallback, const ChaosInterface::FQueryDebugParams& DebugParams);

template <typename InHitType, ESweepOrRay InGeometryQuery, ESingleMultiOrTest InSingleMultiOrTest>
struct TSQTraits
{
	static const ESingleMultiOrTest SingleMultiOrTest = InSingleMultiOrTest;
	static const ESweepOrRay GeometryQuery = InGeometryQuery;
	using THitType = InHitType;
	using TOutHits = typename TChooseClass<InSingleMultiOrTest == ESingleMultiOrTest::Multi, TArray<FHitResult>, FHitResult>::Result;
	using THitBuffer = typename TChooseClass<InSingleMultiOrTest == ESingleMultiOrTest::Multi, FDynamicHitBuffer<InHitType>, FSingleHitBuffer<InHitType>>::Result;

	// GetNumHits - multi
	template <ESingleMultiOrTest T = SingleMultiOrTest>
	static typename TEnableIf<T == ESingleMultiOrTest::Multi, int32>::Type GetNumHits(const THitBuffer& HitBuffer)
	{
		return HitBuffer.GetNumHits();
	}

	// GetNumHits - single/test
	template <ESingleMultiOrTest T = SingleMultiOrTest>
	static typename TEnableIf<T != ESingleMultiOrTest::Multi, int32>::Type GetNumHits(const THitBuffer& HitBuffer)
	{
		return GetHasBlock(HitBuffer) ? 1 : 0;
	}

	//GetHits - multi
	template <ESingleMultiOrTest T = SingleMultiOrTest>
	static typename TEnableIf<T == ESingleMultiOrTest::Multi, THitType*>::Type GetHits(THitBuffer& HitBuffer)
	{
		return HitBuffer.GetHits();
	}

	//GetHits - single/test
	template <ESingleMultiOrTest T = SingleMultiOrTest>
	static typename TEnableIf<T != ESingleMultiOrTest::Multi, THitType*>::Type GetHits(THitBuffer& HitBuffer)
	{
		return GetBlock(HitBuffer);
	}

	//SceneTrace - ray
	template <typename TGeomInputs, ESweepOrRay T = GeometryQuery>
	static typename TEnableIf<T == ESweepOrRay::Raycast, void>::Type SceneTrace(FPhysScene& Scene, const TGeomInputs& GeomInputs, const FVector& Dir, float DeltaMag, const FTransform& StartTM, THitBuffer& HitBuffer, EHitFlags OutputFlags, EQueryFlags QueryFlags, const FCollisionFilterData& FilterData, const FCollisionQueryParams& Params, ICollisionQueryFilterCallbackBase* QueryCallback)
	{
		using namespace ChaosInterface;
		FQueryFilterData QueryFilterData = MakeQueryFilterData(FilterData, QueryFlags, Params);
		FQueryDebugParams DebugParams;
#if !(UE_BUILD_TEST || UE_BUILD_SHIPPING)
		DebugParams.bDebugQuery = Params.bDebugQuery;
#endif
		LowLevelRaycast(Scene, StartTM.GetLocation(), Dir, DeltaMag, HitBuffer, OutputFlags, QueryFlags, FilterData, QueryFilterData, QueryCallback, DebugParams);	//todo(ocohen): namespace?
	}

	//SceneTrace - sweep
	template <typename TGeomInputs, ESweepOrRay T = GeometryQuery>
	static typename TEnableIf<T == ESweepOrRay::Sweep, void>::Type SceneTrace(FPhysScene& Scene, const TGeomInputs& GeomInputs, const FVector& Dir, float DeltaMag, const FTransform& StartTM, THitBuffer& HitBuffer, EHitFlags OutputFlags, EQueryFlags QueryFlags, const FCollisionFilterData& FilterData, const FCollisionQueryParams& Params, ICollisionQueryFilterCallbackBase* QueryCallback)
	{
		using namespace ChaosInterface;
		FQueryFilterData QueryFilterData = MakeQueryFilterData(FilterData, QueryFlags, Params);
		FQueryDebugParams DebugParams;
#if !(UE_BUILD_TEST || UE_BUILD_SHIPPING)
		DebugParams.bDebugQuery = Params.bDebugQuery;
#endif
		LowLevelSweep(Scene, *GeomInputs.GetGeometry(), StartTM, Dir, DeltaMag, HitBuffer, OutputFlags, QueryFlags, FilterData, QueryFilterData, QueryCallback, DebugParams);	//todo(ocohen): namespace?
	}

	static void ResetOutHits(TArray<FHitResult>& OutHits, const FVector& Start, const FVector& End)
	{
		OutHits.Reset();
	}

	static void ResetOutHits(FHitResult& OutHit, const FVector& Start, const FVector& End)
	{
		OutHit = FHitResult();
		OutHit.TraceStart = Start;
		OutHit.TraceEnd = End;
	}

	static void DrawTraces(const UWorld* World, const FVector& Start, const FVector& End, const FPhysicsGeometry* PGeom, const FQuat* PGeomRot, const TArray<FHitResult>& Hits)
	{
		if (IsRay())
		{
			DrawLineTraces(World, Start, End, Hits, DebugLineLifetime);
		}
		else
		{
			DrawGeomSweeps(World, Start, End, *PGeom, *PGeomRot, Hits, DebugLineLifetime);
		}
	}

	static void DrawTraces(const UWorld* World, const FVector& Start, const FVector& End, const FPhysicsGeometry* PGeom, const FQuat* GeomRotation, const FHitResult& Hit)
	{
		TArray<FHitResult> Hits;
		Hits.Add(Hit);

		DrawTraces(World, Start, End, PGeom, GeomRotation, Hits);
	}

	template <typename TGeomInputs>
	static void CaptureTraces(const UWorld* World, const FVector& Start, const FVector& End, const TGeomInputs& GeomInputs, ECollisionChannel TraceChannel, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParams, const FCollisionObjectQueryParams& ObjectParams, const TArray<FHitResult>& Hits, bool bHaveBlockingHit, double StartTime)
	{
#if ENABLE_COLLISION_ANALYZER
		ECAQueryMode::Type QueryMode = IsMulti() ? ECAQueryMode::Multi : (IsSingle() ? ECAQueryMode::Single : ECAQueryMode::Test);
		if (IsRay())
		{
			CAPTURERAYCAST(World, Start, End, QueryMode, TraceChannel, Params, ResponseParams, ObjectParams, Hits);
		}
		else
		{
			CAPTUREGEOMSWEEP(World, Start, End, *GeomInputs.GetGeometryOrientation(), QueryMode, *GeomInputs.GetCollisionShape(), TraceChannel, Params, ResponseParams, ObjectParams, Hits);
		}
#endif
	}

	template <typename TGeomInputs>
	static void CaptureTraces(const UWorld* World, const FVector& Start, const FVector& End, const TGeomInputs& GeomInputs, ECollisionChannel TraceChannel, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParams, const FCollisionObjectQueryParams& ObjectParams, const FHitResult& Hit, bool bHaveBlockingHit, double StartTime)
	{
		TArray<FHitResult> Hits;
		if (bHaveBlockingHit)
		{
			Hits.Add(Hit);
		}
		CaptureTraces(World, Start, End, GeomInputs, TraceChannel, Params, ResponseParams, ObjectParams, Hits, bHaveBlockingHit, StartTime);
	}

	static EHitFlags GetHitFlags()
	{
		if (IsTest())
		{
			return EHitFlags::None;
		}
		else
		{
			if (IsRay())
			{
				return EHitFlags::Position | EHitFlags::Normal | EHitFlags::Distance | EHitFlags::MTD | EHitFlags::FaceIndex;
			}
			else
			{
				if (IsSingle())
				{
					return EHitFlags::Position | EHitFlags::Normal | EHitFlags::Distance | EHitFlags::MTD;
				}
				else
				{
					return EHitFlags::Position | EHitFlags::Normal | EHitFlags::Distance | EHitFlags::MTD | EHitFlags::FaceIndex;
				}
			}
		}
	}

	static EQueryFlags GetQueryFlags()
	{
		if (IsRay())
		{
			return (IsTest() ? (EQueryFlags::PreFilter | EQueryFlags::AnyHit) : EQueryFlags::PreFilter);
		}
		else
		{
			if (IsTest())
			{
				return (EQueryFlags::PreFilter | EQueryFlags::PostFilter | EQueryFlags::AnyHit);
			}
			else if (IsSingle())
			{
				return EQueryFlags::PreFilter;
			}
			else
			{
				return (EQueryFlags::PreFilter | EQueryFlags::PostFilter);
			}
		}
	}

	CA_SUPPRESS(6326);
	constexpr static bool IsSingle() { return SingleMultiOrTest == ESingleMultiOrTest::Single;  }

	CA_SUPPRESS(6326);
	constexpr static bool IsTest() { return SingleMultiOrTest == ESingleMultiOrTest::Test;  }

	CA_SUPPRESS(6326);
	constexpr static bool IsMulti() { return SingleMultiOrTest == ESingleMultiOrTest::Multi;  }

	CA_SUPPRESS(6326);
	constexpr static bool IsRay() { return GeometryQuery == ESweepOrRay::Raycast;  }

	CA_SUPPRESS(6326);
	constexpr static bool IsSweep() { return GeometryQuery == ESweepOrRay::Sweep;  }
};

enum class EThreadQueryContext
{
	GTData,		//use interpolated GT data
	PTDataWithGTObjects,	//use pt data, but convert back to GT when possible
	PTOnlyData,	//use only the PT data and don't try to convert anything back to GT
};

EThreadQueryContext GetThreadQueryContext(const Chaos::FPhysicsSolver& Solver)
{
	if (Solver.IsGameThreadFrozen())
	{
		//If the game thread is frozen the solver is currently in fixed tick mode (i.e. fixed tick callbacks are being executed on GT)
		if (IsInGameThread() || IsInParallelGameThread())
		{
			//Since we are on GT or parallel GT we must be in fixed tick, so use PT data and convert back to GT where possible
			return EThreadQueryContext::PTDataWithGTObjects;
		}
		else
		{
			//The solver can't be running since it's calling fixed tick callbacks on gt, so it must be an unrelated thread task (audio, animation, etc...) so just use interpolated gt data
			return EThreadQueryContext::GTData;
		}
	}
	else
	{
		//TODO: need a way to know we are on a physics thread task (this isn't supported yet)
		//For now just use interpolated data
		return EThreadQueryContext::GTData;
	}
}

template <typename Traits, typename TGeomInputs>
bool TSceneCastCommonImp(const UWorld* World, typename Traits::TOutHits& OutHits, const TGeomInputs& GeomInputs, const FVector Start, const FVector End, ECollisionChannel TraceChannel, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParams, const struct FCollisionObjectQueryParams& ObjectParams)
{
	using namespace ChaosInterface;

	FScopeCycleCounter Counter(Params.StatId);
	STARTQUERYTIMER();

	if (!Traits::IsTest())
	{
		Traits::ResetOutHits(OutHits, Start, End);
	}

	// Track if we get any 'blocking' hits
	bool bHaveBlockingHit = false;

	FVector Delta = End - Start;
	float DeltaSize = Delta.Size();
	float DeltaMag = FMath::IsNearlyZero(DeltaSize) ? 0.f : DeltaSize;
	float MinBlockingDistance = DeltaMag;
	if (Traits::IsSweep() || DeltaMag > 0.f)
	{
		// Create filter data used to filter collisions 
		CA_SUPPRESS(6326);
		FCollisionFilterData Filter = CreateQueryFilterData(TraceChannel, Params.bTraceComplex, ResponseParams.CollisionResponse, Params, ObjectParams, Traits::SingleMultiOrTest == ESingleMultiOrTest::Multi);
		
		CA_SUPPRESS(6326);
		FCollisionQueryFilterCallback QueryCallback(Params, Traits::GeometryQuery == ESweepOrRay::Sweep);

		CA_SUPPRESS(6326);
		if (Traits::SingleMultiOrTest != ESingleMultiOrTest::Multi)
		{
			QueryCallback.bIgnoreTouches = true;
		}

		typename Traits::THitBuffer HitBufferSync;

		bool bBlockingHit = false;
		const FVector Dir = DeltaMag > 0.f ? (Delta / DeltaMag) : FVector(1, 0, 0);
		const FTransform StartTM = Traits::IsRay() ? FTransform(Start) : FTransform(*GeomInputs.GetGeometryOrientation(), Start);

		// Enable scene locks, in case they are required
		FPhysScene& PhysScene = *World->GetPhysicsScene();
		
		FScopedSceneReadLock SceneLocks(PhysScene);
		{
			FScopedSQHitchRepeater<decltype(HitBufferSync)> HitchRepeater(HitBufferSync, QueryCallback, FHitchDetectionInfo(Start, End, TraceChannel, Params));
			do
			{
				Traits::SceneTrace(PhysScene, GeomInputs, Dir, DeltaMag, StartTM, HitchRepeater.GetBuffer(), Traits::GetHitFlags(), Traits::GetQueryFlags(), Filter, Params, &QueryCallback);
			} while (HitchRepeater.RepeatOnHitch());
		}


		const int32 NumHits = Traits::GetNumHits(HitBufferSync);

		if(NumHits > 0 && GetHasBlock(HitBufferSync))
		{
			bBlockingHit = true;
			MinBlockingDistance = GetDistance(Traits::GetHits(HitBufferSync)[NumHits - 1]);
		}

		if (NumHits > 0 && !Traits::IsTest())
		{
			bool bSuccess = ConvertTraceResults(bBlockingHit, World, NumHits, Traits::GetHits(HitBufferSync), DeltaMag, Filter, OutHits, Start, End, *GeomInputs.GetGeometry(), StartTM, MinBlockingDistance, Params.bReturnFaceIndex, Params.bReturnPhysicalMaterial) == EConvertQueryResult::Valid;

			if (!bSuccess)
			{
				// We don't need to change bBlockingHit, that's done by ConvertTraceResults if it removed the blocking hit.
				UE_LOG(LogCollision, Error, TEXT("%s%s resulted in a NaN/INF in PHit!"), Traits::IsRay() ? TEXT("Raycast") : TEXT("Sweep"), Traits::IsMulti() ? TEXT("Multi") : (Traits::IsSingle() ? TEXT("Single") : TEXT("Test")));
#if ENABLE_NAN_DIAGNOSTIC
				UE_LOG(LogCollision, Error, TEXT("--------TraceChannel : %d"), (int32)TraceChannel);
				UE_LOG(LogCollision, Error, TEXT("--------Start : %s"), *Start.ToString());
				UE_LOG(LogCollision, Error, TEXT("--------End : %s"), *End.ToString());
				if (Traits::IsSweep())
				{
					UE_LOG(LogCollision, Error, TEXT("--------GeomRotation : %s"), *(GeomInputs.GetGeometryOrientation()->ToString()));
				}
				UE_LOG(LogCollision, Error, TEXT("--------%s"), *Params.ToString());
#endif
			}
		}

		bHaveBlockingHit = bBlockingHit;

	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (World->DebugDrawSceneQueries(Params.TraceTag))
	{
		Traits::DrawTraces(World, Start, End, GeomInputs.GetGeometry(), GeomInputs.GetGeometryOrientation(), OutHits);
	}
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)

#if ENABLE_COLLISION_ANALYZER
	Traits::CaptureTraces(World, Start, End, GeomInputs, TraceChannel, Params, ResponseParams, ObjectParams, OutHits, bHaveBlockingHit, StartTime);
#endif

	return bHaveBlockingHit;
}

template <typename Traits, typename PTTraits, typename TGeomInputs>
bool TSceneCastCommon(const UWorld* World, typename Traits::TOutHits& OutHits, const TGeomInputs& GeomInputs, const FVector Start, const FVector End, ECollisionChannel TraceChannel, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParams, const struct FCollisionObjectQueryParams& ObjectParams)
{
	if ((World == NULL) || (World->GetPhysicsScene() == NULL))
	{
		return false;
	}

	const EThreadQueryContext ThreadContext = GetThreadQueryContext(*World->GetPhysicsScene()->GetSolver());
	if(ThreadContext == EThreadQueryContext::GTData)
	{
		return TSceneCastCommonImp<Traits, TGeomInputs>(World, OutHits, GeomInputs, Start, End, TraceChannel, Params, ResponseParams, ObjectParams);
	}
	else
	{
		return TSceneCastCommonImp<PTTraits, TGeomInputs>(World, OutHits, GeomInputs, Start, End, TraceChannel, Params, ResponseParams, ObjectParams);
	}
}

//////////////////////////////////////////////////////////////////////////
// RAYCAST

bool FGenericPhysicsInterface::RaycastTest(const UWorld* World, const FVector Start, const FVector End, ECollisionChannel TraceChannel, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParams, const struct FCollisionObjectQueryParams& ObjectParams)
{
	using namespace ChaosInterface;

	SCOPE_CYCLE_COUNTER(STAT_Collision_SceneQueryTotal);
	SCOPE_CYCLE_COUNTER(STAT_Collision_RaycastAny);
	CSV_SCOPED_TIMING_STAT(SceneQuery, RaycastTest);

	using TCastTraits = TSQTraits<FHitRaycast, ESweepOrRay::Raycast, ESingleMultiOrTest::Test>;
	using TPTCastTraits = TSQTraits<FPTRaycastHit, ESweepOrRay::Raycast, ESingleMultiOrTest::Test>;
	FHitResult DummyHit(NoInit);

	return TSceneCastCommon<TCastTraits, TPTCastTraits>(World, DummyHit, FRaycastSQAdditionalInputs(), Start, End, TraceChannel, Params, ResponseParams, ObjectParams);
}

bool FGenericPhysicsInterface::RaycastSingle(const UWorld* World, struct FHitResult& OutHit, const FVector Start, const FVector End, ECollisionChannel TraceChannel, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParams, const struct FCollisionObjectQueryParams& ObjectParams)
{
	using namespace ChaosInterface;

	SCOPE_CYCLE_COUNTER(STAT_Collision_SceneQueryTotal);
	SCOPE_CYCLE_COUNTER(STAT_Collision_RaycastSingle);
	CSV_SCOPED_TIMING_STAT(SceneQuery, RaycastSingle);

	using TCastTraits = TSQTraits<FRaycastHit, ESweepOrRay::Raycast, ESingleMultiOrTest::Single>;
	using TPTCastTraits = TSQTraits<FPTRaycastHit, ESweepOrRay::Raycast, ESingleMultiOrTest::Single>;
	return TSceneCastCommon<TCastTraits, TPTCastTraits>(World, OutHit, FRaycastSQAdditionalInputs(), Start, End, TraceChannel, Params, ResponseParams, ObjectParams);
}



bool FGenericPhysicsInterface::RaycastMulti(const UWorld* World, TArray<struct FHitResult>& OutHits, const FVector& Start, const FVector& End, ECollisionChannel TraceChannel, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParams, const struct FCollisionObjectQueryParams& ObjectParams)
{
	using namespace ChaosInterface;

	SCOPE_CYCLE_COUNTER(STAT_Collision_SceneQueryTotal);
	SCOPE_CYCLE_COUNTER(STAT_Collision_RaycastMultiple);
	CSV_SCOPED_TIMING_STAT(SceneQuery, RaycastMultiple);

	using TCastTraits = TSQTraits<FHitRaycast, ESweepOrRay::Raycast, ESingleMultiOrTest::Multi>;
	using TPTCastTraits = TSQTraits<FPTRaycastHit, ESweepOrRay::Raycast, ESingleMultiOrTest::Multi>;
	return TSceneCastCommon<TCastTraits, TPTCastTraits> (World, OutHits, FRaycastSQAdditionalInputs(), Start, End, TraceChannel, Params, ResponseParams, ObjectParams);
}

//////////////////////////////////////////////////////////////////////////
// GEOM SWEEP

bool FGenericPhysicsInterface::GeomSweepTest(const UWorld* World, const struct FCollisionShape& CollisionShape, const FQuat& Rot, FVector Start, FVector End, ECollisionChannel TraceChannel, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParams, const struct FCollisionObjectQueryParams& ObjectParams)
{
	using namespace ChaosInterface;

	SCOPE_CYCLE_COUNTER(STAT_Collision_SceneQueryTotal);
	SCOPE_CYCLE_COUNTER(STAT_Collision_GeomSweepAny);
	CSV_SCOPED_TIMING_STAT(SceneQuery, GeomSweepTest);

	using TCastTraits = TSQTraits<FHitSweep, ESweepOrRay::Sweep, ESingleMultiOrTest::Test>;
	using TPTCastTraits = TSQTraits<FPTSweepHit, ESweepOrRay::Sweep, ESingleMultiOrTest::Test>;
	FHitResult DummyHit(NoInit);

	return TSceneCastCommon<TCastTraits, TPTCastTraits>(World, DummyHit, FGeomSQAdditionalInputs(CollisionShape, Rot), Start, End, TraceChannel, Params, ResponseParams, ObjectParams);
}

bool FGenericPhysicsInterface::GeomSweepSingle(const UWorld* World, const struct FCollisionShape& CollisionShape, const FQuat& Rot, FHitResult& OutHit, FVector Start, FVector End, ECollisionChannel TraceChannel, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParams, const struct FCollisionObjectQueryParams& ObjectParams)
{
	using namespace ChaosInterface;

	SCOPE_CYCLE_COUNTER(STAT_Collision_SceneQueryTotal);
	SCOPE_CYCLE_COUNTER(STAT_Collision_GeomSweepSingle);
	CSV_SCOPED_TIMING_STAT(SceneQuery, GeomSweepSingle);

	using TCastTraits = TSQTraits<FSweepHit, ESweepOrRay::Sweep, ESingleMultiOrTest::Single>;
	using TPTCastTraits = TSQTraits<FPTSweepHit, ESweepOrRay::Sweep, ESingleMultiOrTest::Single>;
	return TSceneCastCommon<TCastTraits, TPTCastTraits>(World, OutHit, FGeomSQAdditionalInputs(CollisionShape, Rot), Start, End, TraceChannel, Params, ResponseParams, ObjectParams);
}

template<>
bool FGenericPhysicsInterface::GeomSweepMulti(const UWorld* World, const FPhysicsGeometryCollection& InGeom, const FQuat& InGeomRot, TArray<FHitResult>& OutHits, FVector Start, FVector End, ECollisionChannel TraceChannel, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParams, const FCollisionObjectQueryParams& ObjectParams /*= FCollisionObjectQueryParams::DefaultObjectQueryParam*/)
{
	using namespace ChaosInterface;

	SCOPE_CYCLE_COUNTER(STAT_Collision_SceneQueryTotal);
	SCOPE_CYCLE_COUNTER(STAT_Collision_GeomSweepMultiple);
	CSV_SCOPED_TIMING_STAT(SceneQuery, GeomSweepMultiple);

	using TCastTraits = TSQTraits<FHitSweep, ESweepOrRay::Sweep, ESingleMultiOrTest::Multi>;
	using TPTCastTraits = TSQTraits<FPTSweepHit, ESweepOrRay::Sweep, ESingleMultiOrTest::Multi>;
	return TSceneCastCommon<TCastTraits, TPTCastTraits>(World, OutHits, FGeomCollectionSQAdditionalInputs(InGeom, InGeomRot), Start, End, TraceChannel, Params, ResponseParams, ObjectParams);
}

template<>
bool FGenericPhysicsInterface::GeomSweepMulti(const UWorld* World, const FCollisionShape& InGeom, const FQuat& InGeomRot, TArray<FHitResult>& OutHits, FVector Start, FVector End, ECollisionChannel TraceChannel, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParams, const FCollisionObjectQueryParams& ObjectParams /*= FCollisionObjectQueryParams::DefaultObjectQueryParam*/)
{
	using namespace ChaosInterface;

	SCOPE_CYCLE_COUNTER(STAT_Collision_SceneQueryTotal);
	SCOPE_CYCLE_COUNTER(STAT_Collision_GeomSweepMultiple);
	CSV_SCOPED_TIMING_STAT(SceneQuery, GeomSweepMultiple);

	using TCastTraits = TSQTraits<FHitSweep, ESweepOrRay::Sweep, ESingleMultiOrTest::Multi>;
	using TPTCastTraits = TSQTraits<FPTSweepHit, ESweepOrRay::Sweep, ESingleMultiOrTest::Multi>;
	return TSceneCastCommon<TCastTraits, TPTCastTraits>(World, OutHits, FGeomSQAdditionalInputs(InGeom, InGeomRot), Start, End, TraceChannel, Params, ResponseParams, ObjectParams);
}


//////////////////////////////////////////////////////////////////////////
// GEOM OVERLAP

namespace EQueryInfo
{
	//This is used for templatizing code based on the info we're trying to get out.
	enum Type
	{
		GatherAll,		//get all data and actually return it
		IsBlocking,		//is any of the data blocking? only return a bool so don't bother collecting
		IsAnything		//is any of the data blocking or touching? only return a bool so don't bother collecting
	};
}

template <typename TOverlapHit, EQueryInfo::Type InfoType, typename TCollisionAnalyzerType>
bool GeomOverlapMultiImp(const UWorld* World, const FPhysicsGeometry& Geom, const TCollisionAnalyzerType& CollisionAnalyzerType, const FTransform& GeomPose, TArray<FOverlapResult>& OutOverlaps, ECollisionChannel TraceChannel, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParams, const struct FCollisionObjectQueryParams& ObjectParams)
{
	using namespace ChaosInterface;

	FScopeCycleCounter Counter(Params.StatId);

	if ((World == NULL) || (World->GetPhysicsScene() == NULL))
	{
		return false;
	}

	STARTQUERYTIMER();

	bool bHaveBlockingHit = false;

	// overlapMultiple only supports sphere/capsule/box
	const ECollisionShapeType GeomType = GetType(Geom);
	if (GeomType == ECollisionShapeType::Sphere || GeomType == ECollisionShapeType::Capsule || GeomType == ECollisionShapeType::Box || GeomType == ECollisionShapeType::Convex)
	{
		// Create filter data used to filter collisions
		FCollisionFilterData Filter = CreateQueryFilterData(TraceChannel, Params.bTraceComplex, ResponseParams.CollisionResponse, Params, ObjectParams, InfoType != EQueryInfo::IsAnything);
		FCollisionQueryFilterCallback QueryCallback(Params, false);
		QueryCallback.bIgnoreTouches |= (InfoType == EQueryInfo::IsBlocking); // pre-filter to ignore touches and only get blocking hits, if that's what we're after.
		QueryCallback.bIsOverlapQuery = true;

		EQueryFlags QueryFlags = InfoType == EQueryInfo::GatherAll ? EQueryFlags::PreFilter : (EQueryFlags::PreFilter | EQueryFlags::AnyHit);
		if (Params.bSkipNarrowPhase)
		{
			QueryFlags = QueryFlags | EQueryFlags::SkipNarrowPhase;
		}
		FDynamicHitBuffer<TOverlapHit> OverlapBuffer;

		FQueryDebugParams DebugParams;
#if !(UE_BUILD_TEST || UE_BUILD_SHIPPING)
		DebugParams.bDebugQuery = Params.bDebugQuery;
#endif

		// Enable scene locks, in case they are required
		FPhysScene& PhysScene = *World->GetPhysicsScene();

		FPhysicsCommand::ExecuteRead(&PhysScene, [&]()
		{
			{
				FScopedSQHitchRepeater<typename TRemoveReference<decltype(OverlapBuffer)>::Type> HitchRepeater(OverlapBuffer, QueryCallback, FHitchDetectionInfo(GeomPose, TraceChannel, Params));
				do
				{
					LowLevelOverlap(PhysScene, Geom, GeomPose, HitchRepeater.GetBuffer(), QueryFlags, Filter, MakeQueryFilterData(Filter, QueryFlags, Params), &QueryCallback, DebugParams);
				} while(HitchRepeater.RepeatOnHitch());

				if(GetHasBlock(OverlapBuffer) && InfoType != EQueryInfo::GatherAll)	//just want true or false so don't bother gathering info
				{
					bHaveBlockingHit = true;
				}
			}

			if(InfoType == EQueryInfo::GatherAll)	//if we are gathering all we need to actually convert to UE format
			{
				const int32 NumHits = OverlapBuffer.GetNumHits();

				if(NumHits > 0)
				{
					bHaveBlockingHit = ConvertOverlapResults(NumHits, OverlapBuffer.GetHits(), Filter, OutOverlaps);
				}
			}

		});
	}
	else
	{
		UE_LOG(LogCollision, Log, TEXT("GeomOverlapMulti : unsupported shape - only supports sphere, capsule, box"));
	}

#if ENABLE_COLLISION_ANALYZER
	if (GCollisionAnalyzerIsRecording)
	{
		// Determine query mode ('single' doesn't really exist for overlaps)
		ECAQueryMode::Type QueryMode = (InfoType == EQueryInfo::GatherAll) ? ECAQueryMode::Multi : ECAQueryMode::Test;

		CAPTUREGEOMOVERLAP(World, CollisionAnalyzerType, GeomPose, QueryMode, TraceChannel, Params, ResponseParams, ObjectParams, OutOverlaps);
	}
#endif // ENABLE_COLLISION_ANALYZER

	return bHaveBlockingHit;
}

template <EQueryInfo::Type InfoType, typename TCollisionAnalyzerType>
bool GeomOverlapMultiHelper(const UWorld* World, const FPhysicsGeometry& Geom, const TCollisionAnalyzerType& CollisionAnalyzerType, const FTransform& GeomPose, TArray<FOverlapResult>& OutOverlaps, ECollisionChannel TraceChannel, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParams, const struct FCollisionObjectQueryParams& ObjectParams)
{
	using namespace ChaosInterface;

	if ((World == NULL) || (World->GetPhysicsScene() == NULL))
	{
		return false;
	}

	const EThreadQueryContext ThreadContext = GetThreadQueryContext(*World->GetPhysicsScene()->GetSolver());
	if (ThreadContext == EThreadQueryContext::GTData)
	{
		return GeomOverlapMultiImp<FOverlapHit, InfoType, TCollisionAnalyzerType>(World, Geom, CollisionAnalyzerType, GeomPose, OutOverlaps, TraceChannel, Params, ResponseParams, ObjectParams);
	}
	else
	{
		return GeomOverlapMultiImp<FPTOverlapHit, InfoType, TCollisionAnalyzerType>(World, Geom, CollisionAnalyzerType, GeomPose, OutOverlaps, TraceChannel, Params, ResponseParams, ObjectParams);
	}
}

bool FGenericPhysicsInterface::GeomOverlapBlockingTest(const UWorld* World, const struct FCollisionShape& CollisionShape, const FVector& Pos, const FQuat& Rot, ECollisionChannel TraceChannel, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParams, const struct FCollisionObjectQueryParams& ObjectParams)
{
	SCOPE_CYCLE_COUNTER(STAT_Collision_SceneQueryTotal);
	SCOPE_CYCLE_COUNTER(STAT_Collision_GeomOverlapBlocking);
	CSV_SCOPED_TIMING_STAT(SceneQuery, GeomOverlapBlocking);

	TArray<FOverlapResult> Overlaps;	//needed only for template shared code
	FTransform GeomTransform(Rot, Pos);
	FPhysicsShapeAdapter Adaptor(GeomTransform.GetRotation(), CollisionShape);
	return GeomOverlapMultiHelper<EQueryInfo::IsBlocking>(World, Adaptor.GetGeometry(), CollisionShape, Adaptor.GetGeomPose(GeomTransform.GetTranslation()), Overlaps, TraceChannel, Params, ResponseParams, ObjectParams);
}

bool FGenericPhysicsInterface::GeomOverlapAnyTest(const UWorld* World, const struct FCollisionShape& CollisionShape, const FVector& Pos, const FQuat& Rot, ECollisionChannel TraceChannel, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParams, const struct FCollisionObjectQueryParams& ObjectParams)
{
	SCOPE_CYCLE_COUNTER(STAT_Collision_SceneQueryTotal);
	SCOPE_CYCLE_COUNTER(STAT_Collision_GeomOverlapAny);
	CSV_SCOPED_TIMING_STAT(SceneQuery, GeomOverlapAny);

	TArray<FOverlapResult> Overlaps;	//needed only for template shared code
	FTransform GeomTransform(Rot, Pos);
	FPhysicsShapeAdapter Adaptor(GeomTransform.GetRotation(), CollisionShape);
	return GeomOverlapMultiHelper<EQueryInfo::IsAnything>(World, Adaptor.GetGeometry(), CollisionShape, Adaptor.GetGeomPose(GeomTransform.GetTranslation()), Overlaps, TraceChannel, Params, ResponseParams, ObjectParams);
}

template<>
bool FGenericPhysicsInterface::GeomOverlapMulti(const UWorld* World, const FPhysicsGeometryCollection& InGeom, const FVector& InPosition, const FQuat& InRotation, TArray<FOverlapResult>& OutOverlaps, ECollisionChannel TraceChannel, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParams, const FCollisionObjectQueryParams& ObjectParams)
{
	SCOPE_CYCLE_COUNTER(STAT_Collision_SceneQueryTotal);
	SCOPE_CYCLE_COUNTER(STAT_Collision_GeomOverlapMultiple);
	CSV_SCOPED_TIMING_STAT(SceneQuery, GeomOverlapMultiple);

	FTransform GeomTransform(InRotation, InPosition);
	return GeomOverlapMultiHelper<EQueryInfo::GatherAll>(World, InGeom.GetGeometry(), InGeom, GeomTransform, OutOverlaps, TraceChannel, Params, ResponseParams, ObjectParams);
}

template<>
bool FGenericPhysicsInterface::GeomOverlapMulti(const UWorld* World, const FCollisionShape& InGeom, const FVector& InPosition, const FQuat& InRotation, TArray<FOverlapResult>& OutOverlaps, ECollisionChannel TraceChannel, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParams, const FCollisionObjectQueryParams& ObjectParams)
{
	SCOPE_CYCLE_COUNTER(STAT_Collision_SceneQueryTotal);
	SCOPE_CYCLE_COUNTER(STAT_Collision_GeomOverlapMultiple);
	CSV_SCOPED_TIMING_STAT(SceneQuery, GeomOverlapMultiple);

	FTransform GeomTransform(InRotation, InPosition);
	FPhysicsShapeAdapter Adaptor(GeomTransform.GetRotation(), InGeom);
	return GeomOverlapMultiHelper<EQueryInfo::GatherAll>(World, Adaptor.GetGeometry(), InGeom, Adaptor.GetGeomPose(GeomTransform.GetTranslation()), OutOverlaps, TraceChannel, Params, ResponseParams, ObjectParams);
}
