// Copyright Epic Games, Inc. All Rights Reserved.

#include "SQAccelerator.h"
#include "CollisionQueryFilterCallbackCore.h"
#include "PhysicsInterfaceUtilsCore.h"

#include "SceneQueryChaosImp.h"

#include "ChaosInterfaceWrapperCore.h"
#include "Chaos/CastingUtilities.h"
#include "Chaos/ISpatialAcceleration.h"
#include "Chaos/PBDCollisionConstraints.h"
#include "Chaos/GeometryParticles.h"
#include "Chaos/GeometryQueries.h"
#include "Chaos/DebugDrawQueue.h"

#if CHAOS_DEBUG_DRAW
int32 ChaosSQDrawDebugVisitorQueries = 0;
FAutoConsoleVariableRef CVarChaosSQDrawDebugQueries(TEXT("p.Chaos.SQ.DrawDebugVisitorQueries"), ChaosSQDrawDebugVisitorQueries, TEXT("Draw bounds of objects visited by visitors in scene queries."));
#endif

void FSQAcceleratorUnion::Raycast(const FVector& Start, const FVector& Dir, const float DeltaMagnitude, FPhysicsHitCallback<FHitRaycast>& HitBuffer, EHitFlags OutputFlags, const FQueryFilterData& QueryFilterData, ICollisionQueryFilterCallbackBase& QueryCallback) const
{
	for (const ISQAccelerator* Accelerator : Accelerators)
	{
		Accelerator->Raycast(Start, Dir, DeltaMagnitude, HitBuffer, OutputFlags, QueryFilterData, QueryCallback);
	}
}

void FSQAcceleratorUnion::Sweep(const FPhysicsGeometry& QueryGeom, const FTransform& StartTM, const FVector& Dir, const float DeltaMagnitude, FPhysicsHitCallback<FHitSweep>& HitBuffer, EHitFlags OutputFlags, const FQueryFilterData& QueryFilterData, ICollisionQueryFilterCallbackBase& QueryCallback) const
{
	for (const ISQAccelerator* Accelerator : Accelerators)
	{
		Accelerator->Sweep(QueryGeom, StartTM, Dir, DeltaMagnitude, HitBuffer, OutputFlags, QueryFilterData, QueryCallback);
	}
}

void FSQAcceleratorUnion::Overlap(const FPhysicsGeometry& QueryGeom, const FTransform& GeomPose, FPhysicsHitCallback<FHitOverlap>& HitBuffer, const FQueryFilterData& QueryFilterData, ICollisionQueryFilterCallbackBase& QueryCallback) const
{
	for (const ISQAccelerator* Accelerator : Accelerators)
	{
		Accelerator->Overlap(QueryGeom, GeomPose, HitBuffer, QueryFilterData, QueryCallback);
	}
}

void FSQAcceleratorUnion::AddSQAccelerator(ISQAccelerator* InAccelerator)
{
	Accelerators.AddUnique(InAccelerator);
}

void FSQAcceleratorUnion::RemoveSQAccelerator(ISQAccelerator* AcceleratorToRemove)
{
	Accelerators.RemoveSingleSwap(AcceleratorToRemove);	//todo(ocohen): probably want to order these in some optimal way
}

FChaosSQAccelerator::FChaosSQAccelerator(const Chaos::ISpatialAcceleration<Chaos::FAccelerationStructureHandle, Chaos::FReal, 3>& InSpatialAcceleration)
	: SpatialAcceleration(InSpatialAcceleration)
{}

struct FPreFilterInfo
{
	const Chaos::FImplicitObject* Geom;
	int32 ActorIdx;
};

template <typename TLocationHit>
void FillHitHelperImp(TLocationHit& Hit, const Chaos::FReal Distance, const FVector& WorldPosition, const FVector& WorldNormal, int32 FaceIdx, bool bComputeMTD)
{
	Hit.Distance = Distance;
	Hit.WorldPosition = WorldPosition;
	Hit.WorldNormal = WorldNormal;
	Hit.Flags = Distance > 0.f || bComputeMTD ? EHitFlags::Distance | EHitFlags::Normal | EHitFlags::Position : EHitFlags::Distance | EHitFlags::FaceIndex;
	Hit.FaceIndex = FaceIdx;
}

void FillHitHelper(ChaosInterface::FLocationHit& Hit, const Chaos::FReal Distance, const FVector& WorldPosition, const FVector& WorldNormal, int32 FaceIdx, bool bComputeMTD)
{
	FillHitHelperImp(Hit, Distance, WorldPosition, WorldNormal, FaceIdx, bComputeMTD);
}

void FillHitHelper(ChaosInterface::FPTLocationHit& Hit, const Chaos::FReal Distance, const FVector& WorldPosition, const FVector& WorldNormal, int32 FaceIdx, bool bComputeMTD)
{
	FillHitHelperImp(Hit, Distance, WorldPosition, WorldNormal, FaceIdx, bComputeMTD);
}

void FillHitHelper(ChaosInterface::FOverlapHit& Hit, const Chaos::FReal Distance, const FVector& WorldPosition, const FVector& WorldNormal, int32 FaceIdx, bool bComputeMTD)
{
}

void FillHitHelper(ChaosInterface::FPTOverlapHit& Hit, const Chaos::FReal Distance, const FVector& WorldPosition, const FVector& WorldNormal, int32 FaceIdx, bool bComputeMTD)
{
}

template <typename QueryGeometryType, typename TPayload, typename THitType, bool bGTData = true>
struct TSQVisitor : public Chaos::ISpatialVisitor<TPayload, Chaos::FReal>
{
	using TGeometryType = typename TChooseClass<bGTData, Chaos::FGeometryParticle, Chaos::FGeometryParticleHandle>::Result;
	TSQVisitor(const FVector& InStartPoint, const FVector& InDir, ChaosInterface::FSQHitBuffer<THitType>& InHitBuffer, EHitFlags InOutputFlags,
		const FQueryFilterData& InQueryFilterData, ICollisionQueryFilterCallbackBase& InQueryCallback, const FQueryDebugParams& InDebugParams)
		: StartPoint(InStartPoint)
		, Dir(InDir)
		, HalfExtents(0)
		, OutputFlags(InOutputFlags)
		, bAnyHit(false)
		, DebugParams(InDebugParams)
		, HitBuffer(InHitBuffer)
		, QueryFilterData(InQueryFilterData)
		, QueryFilterDataConcrete(C2UFilterData(QueryFilterData.data))
		, QueryCallback(InQueryCallback)
	{
		bAnyHit = QueryFilterData.flags & FChaosQueryFlag::eANY_HIT;
	}

	TSQVisitor(const FTransform& InStartTM, const FVector& InDir, ChaosInterface::FSQHitBuffer<THitType>& InHitBuffer, EHitFlags InOutputFlags,
		const FQueryFilterData& InQueryFilterData, ICollisionQueryFilterCallbackBase& InQueryCallback, const QueryGeometryType& InQueryGeom, const FQueryDebugParams& InDebugParams)
		: Dir(InDir)
		, HalfExtents(InQueryGeom.BoundingBox().Extents() * 0.5)
		, OutputFlags(InOutputFlags)
		, bAnyHit(false)
		, DebugParams(InDebugParams)
		, HitFaceNormal(Chaos::FVec3::ZeroVector)
		, HitBuffer(InHitBuffer)
		, QueryFilterData(InQueryFilterData)
		, QueryFilterDataConcrete(C2UFilterData(QueryFilterData.data))
		, QueryGeom(&InQueryGeom)
		, QueryCallback(InQueryCallback)
		, StartTM(InStartTM)
	{
		bAnyHit = QueryFilterData.flags & FChaosQueryFlag::eANY_HIT;
		//todo: check THitType is sweep
	}

	TSQVisitor(const FTransform& InWorldTM, ChaosInterface::FSQHitBuffer<THitType>& InHitBuffer,
		const FQueryFilterData& InQueryFilterData, ICollisionQueryFilterCallbackBase& InQueryCallback, const QueryGeometryType& InQueryGeom, const FQueryDebugParams& InDebugParams)
		: HalfExtents(InQueryGeom.BoundingBox().Extents() * 0.5)
		, bAnyHit(false)
		, DebugParams(InDebugParams)
		, HitFaceNormal(Chaos::FVec3::ZeroVector)
		, HitBuffer(InHitBuffer)
		, QueryFilterData(InQueryFilterData)
		, QueryFilterDataConcrete(C2UFilterData(QueryFilterData.data))
		, QueryGeom(&InQueryGeom)
		, QueryCallback(InQueryCallback)
		, StartTM(InWorldTM)
	{
		bAnyHit = QueryFilterData.flags & FChaosQueryFlag::eANY_HIT;
		//todo: check THitType is overlap
	}

	virtual bool Raycast(const Chaos::TSpatialVisitorData<TPayload>& Instance, Chaos::FQueryFastData& CurData) override
	{
		return Visit<ESQType::Raycast>(Instance, &CurData);
	}

	virtual bool Sweep(const Chaos::TSpatialVisitorData<TPayload>& Instance, Chaos::FQueryFastData& CurData) override
	{
		return Visit<ESQType::Sweep>(Instance, &CurData);
	}

	virtual bool Overlap(const Chaos::TSpatialVisitorData<TPayload>& Instance) override
	{
		return Visit<ESQType::Overlap>(Instance, nullptr);
	}

	virtual const void* GetQueryData() const override
	{
		return &QueryFilterDataConcrete;
	}

	virtual const void* GetSimData() const override
	{
		return nullptr;
	}

	/** Return a pointer to the payload on which we are querying the acceleration structure */
	virtual const void* GetQueryPayload() const override
	{
		return nullptr;
	}

	virtual bool HasBlockingHit() const override
	{ 
		return HitBuffer.HasBlockingHit();
	}

private:

	enum class ESQType
	{
		Raycast,
		Sweep,
		Overlap
	};

	auto GetPayloadForThread(const TPayload& Payload)
	{
		if constexpr(bGTData)
		{
			return Payload.GetExternalGeometryParticle_ExternalThread();
		}
		else
		{
			return Payload.GetGeometryParticleHandle_PhysicsThread();
		}
	}

	template <ESQType SQ>
	bool Visit(const Chaos::TSpatialVisitorData<TPayload>& Instance, Chaos::FQueryFastData* CurData)
	{
		//QUICK_SCOPE_CYCLE_COUNTER(SQVisit);
		TPayload Payload = Instance.Payload;

		//todo: add a check to ensure hitbuffer matches SQ type
		using namespace Chaos;
		TGeometryType* GeometryParticle = GetPayloadForThread(Payload);

		if(!GeometryParticle)
		{
			// This case handles particles created by the physics simulation without the main thread
			// being made aware of their creation. We have a PT particle but no external particle
			ensure(bGTData);

			return true;
		}

		const FShapesArray& Shapes = GeometryParticle->ShapesArray();

		const bool bTestShapeBounds =  Shapes.Num() > 1;
		bool bContinue = true;

		const FRigidTransform3 ActorTM(GeometryParticle->X(), GeometryParticle->R());
		const TAABB<FReal, 3> QueryGeomWorldBounds = QueryGeom ? QueryGeom->CalculateTransformedBounds(StartTM) : TAABB<FReal, 3>(-HalfExtents, HalfExtents);

#if CHAOS_DEBUG_DRAW
		bool bAllShapesIgnoredInPrefilter = true;
		bool bHitBufferIncreased = false;
#endif

		for (const auto& Shape : Shapes)
		{
			const FImplicitObject* Geom = Shape->GetGeometry().Get();

			if (bTestShapeBounds)
			{
				FAABB3 InflatedWorldBounds;
				if (SQ == ESQType::Raycast)
				{
					InflatedWorldBounds = Shape->GetWorldSpaceInflatedShapeBounds();
				}
				else
				{
					// Transform to world bounds and get the proper half extent.
					const FVec3 WorldHalfExtent = QueryGeom ? QueryGeomWorldBounds.Extents() * 0.5f : FVec3(HalfExtents);

					InflatedWorldBounds = FAABB3(Shape->GetWorldSpaceInflatedShapeBounds().Min() - WorldHalfExtent, Shape->GetWorldSpaceInflatedShapeBounds().Max() + WorldHalfExtent);
				}
				if (SQ != ESQType::Overlap)
				{
					//todo: use fast raycast
					Chaos::FReal TmpTime, TmpExitTime;
					const FVec3 InflatedBoundsTraceStart = SQ == ESQType::Raycast ? StartPoint : QueryGeomWorldBounds.Center();
					if (!InflatedWorldBounds.RaycastFast(InflatedBoundsTraceStart, CurData->Dir, CurData->InvDir, CurData->bParallel, CurData->CurrentLength, CurData->InvCurrentLength, TmpTime, TmpExitTime))
					{
						continue;
					}
				}
				else
				{
					const FVec3 QueryCenter = QueryGeom ? QueryGeomWorldBounds.Center() : StartTM.GetLocation();
					if (!InflatedWorldBounds.Contains(QueryCenter))
					{
						continue;
					}
				}
			}

			//TODO: use gt particles directly
			ECollisionQueryHitType HitType = QueryFilterData.flags & FChaosQueryFlag::ePREFILTER ? QueryCallback.PreFilter(QueryFilterDataConcrete, *Shape, *GeometryParticle) : ECollisionQueryHitType::Block;

			if (HitType != ECollisionQueryHitType::None)
			{
#if CHAOS_DEBUG_DRAW
				bAllShapesIgnoredInPrefilter = false;
#endif

				//QUICK_SCOPE_CYCLE_COUNTER(SQNarrow);
				THitType Hit;
				Hit.Actor = GeometryParticle;
				Hit.Shape = Shape.Get();

				bool bHit = false;

				FVec3 WorldPosition, WorldNormal;
				Chaos::FReal Distance = 0;	//not needed but fixes compiler warning for overlap
				int32 FaceIdx = INDEX_NONE;	//not needed but fixes compiler warning for overlap
				FVec3 FaceNormal = FVec3::ZeroVector;
				const bool bComputeMTD = !!((uint16)(OutputFlags.HitFlags & EHitFlags::MTD));

				if (SQ == ESQType::Raycast)
				{
					FVec3 LocalNormal;
					FVec3 LocalPosition;

					const FVec3 DirLocal = ActorTM.InverseTransformVectorNoScale(Dir);
					const FVec3 StartLocal = ActorTM.InverseTransformPositionNoScale(StartPoint);
					bHit = Geom->Raycast(StartLocal, DirLocal, CurData->CurrentLength, /*Thickness=*/0.0, Distance, LocalPosition, LocalNormal, FaceIdx);
					if (bHit)
					{
						WorldPosition = ActorTM.TransformPositionNoScale(LocalPosition);
						WorldNormal = ActorTM.TransformVectorNoScale(LocalNormal);
					}
				}
				else if(SQ == ESQType::Sweep && CurData->CurrentLength > 0 && ensure(QueryGeom))
				{
					bHit = SweepQuery(*Geom, ActorTM, *QueryGeom, StartTM, CurData->Dir, CurData->CurrentLength, Distance, WorldPosition, WorldNormal, FaceIdx, FaceNormal, 0.f, bComputeMTD);
				}
				else if ((SQ == ESQType::Overlap || (SQ == ESQType::Sweep && CurData->CurrentLength == 0)) && ensure(QueryGeom))
				{
					if (bComputeMTD)
					{
						FMTDInfo MTDInfo;
						bHit = OverlapQuery(*Geom, ActorTM, *QueryGeom, StartTM, /*Thickness=*/0, &MTDInfo);
						if (bHit)
						{
							WorldNormal = MTDInfo.Normal * MTDInfo.Penetration;
						}
					}
					else
					{
						bHit = OverlapQuery(*Geom, ActorTM, *QueryGeom, StartTM, /*Thickness=*/0);
					}
				}

				if(bHit)
				{
					//QUICK_SCOPE_CYCLE_COUNTER(SQNarrowHit);
					
					bool bAcceptHit = true;
					if constexpr(SQ == ESQType::Sweep && std::is_same_v<THitType, FSweepHit>)
					{
						const THitType* CurrentHit = HitBuffer.GetCurrentHit();
						if(FaceIdx != INDEX_NONE)
						{
							if(CurrentHit)
							{
								constexpr static FReal CoLocationEpsilon = 1e-6;
								const FReal DistDelta = FMath::Abs(Distance - CurrentHit->Distance);
								const int32 OldFace = CurrentHit->FaceIndex;

								if(DistDelta < CoLocationEpsilon && OldFace != INDEX_NONE)
								{
									// We already have a face hit from another triangle mesh - see if this one is better (more opposing the sweep)
									const FReal OldDot = FVec3::DotProduct(CurData->Dir, HitFaceNormal);
									const FReal NewDot = FVec3::DotProduct(CurData->Dir, FaceNormal);

									if(NewDot < OldDot)
									{
										// More opposing
										HitFaceNormal = FaceNormal;
									}
									else
									{
										// This hit is co-located but has a worse normal
										bAcceptHit = false;
									}
								}
							}
							
							if(bAcceptHit)
							{
								// Record the new face normal
								HitFaceNormal = FaceNormal;
							}
						}
					}

					if(bAcceptHit)
					{
						FillHitHelper(Hit, Distance, WorldPosition, WorldNormal, FaceIdx, bComputeMTD);

						HitType = QueryFilterData.flags & FChaosQueryFlag::ePOSTFILTER ? QueryCallback.PostFilter(QueryFilterDataConcrete, Hit) : HitType;

						if(HitType != ECollisionQueryHitType::None)
						{

							//overlap never blocks
							const bool bBlocker = (HitType == ECollisionQueryHitType::Block || bAnyHit || HitBuffer.WantsSingleResult());
							HitBuffer.InsertHit(Hit, bBlocker);
#if CHAOS_DEBUG_DRAW
							bHitBufferIncreased = true;
#endif

							if(bBlocker && SQ != ESQType::Overlap)
							{
								CurData->SetLength(FMath::Max((FReal)0., Distance));	//Max is needed for MTD which returns negative distance
								if(CurData->CurrentLength == 0 && (SQ == ESQType::Raycast || HitBuffer.WantsSingleResult()))	//raycasts always fail with distance 0, sweeps only matter if we want multi overlaps
								{
									bContinue = false; //initial overlap so nothing will be better than this
									break;
								}
							}

							if(bAnyHit)
							{
								bContinue = false;
								break;
							}
						}
					}
				}
			}
		}

#if CHAOS_DEBUG_DRAW
		if (DebugParams.IsDebugQuery() && ChaosSQDrawDebugVisitorQueries)
		{
			DebugDraw<SQ>(Instance, CurData, bAllShapesIgnoredInPrefilter, bHitBufferIncreased);
		}
#endif

		return bContinue;
	}

#if CHAOS_DEBUG_DRAW

	void DebugDrawPayloadImpl(const TPayload& Payload, const bool bExternal, const bool bHit, decltype(&TPayload::DebugDraw)) { Payload.DebugDraw(bExternal, bHit); }
	void DebugDrawPayloadImpl(const TPayload& Payload, const bool bExternal, const bool bHit, ...) { }
	void DebugDrawPayload(const TPayload& Payload, const bool bExternal, const bool bHit) { DebugDrawPayloadImpl(Payload, bExternal, bHit, 0); }

	template <ESQType SQ>
	void DebugDraw(const Chaos::TSpatialVisitorData<TPayload>& Instance, const Chaos::FQueryFastData* CurData, const bool bPrefiltered, const bool bHit)
	{
		if (SQ == ESQType::Raycast)
		{
			const FVector EndPoint = StartPoint + (Dir * CurData->CurrentLength);
			Chaos::FDebugDrawQueue::GetInstance().DrawDebugDirectionalArrow(StartPoint, EndPoint, 5.f, bHit ? FColor::Red : FColor::Green);
		}
		else if (SQ == ESQType::Overlap)
		{
			Chaos::DebugDraw::DrawShape(StartTM, QueryGeom, Chaos::FShapeOrShapesArray(), bHit ? FColor::Red : FColor::Green);
		}

		if (Instance.bHasBounds)
		{
			Chaos::FDebugDrawQueue::GetInstance().DrawDebugBox(Instance.Bounds.Center(), Instance.Bounds.Extents(), FQuat::Identity, bHit ? FColor(100, 50, 50) : FColor(50, 100, 50), false, -1.f, 0, 0.f);
		}

		if (!bPrefiltered)
		{
			DebugDrawPayload(Instance.Payload, DebugParams.bExternalQuery, bHit);
		}
	}
#endif

	const FVector StartPoint;
	const FVector Dir;
	const FVector HalfExtents;
	FHitFlags OutputFlags;
	bool bAnyHit;
	const FQueryDebugParams DebugParams;
	Chaos::FVec3 HitFaceNormal;
	ChaosInterface::FSQHitBuffer<THitType>& HitBuffer;
	const FQueryFilterData& QueryFilterData;
	const FCollisionFilterData QueryFilterDataConcrete;
	const QueryGeometryType* QueryGeom = nullptr;
	ICollisionQueryFilterCallbackBase& QueryCallback;
	const FTransform StartTM;
};

template <typename QueryGeometryType, typename TPayload, typename THitType, bool bGTData = true>
struct TBPVisitor : public Chaos::ISpatialVisitor<TPayload, Chaos::FReal>
{
	using TGeometryType = typename TChooseClass<bGTData, Chaos::FGeometryParticle, Chaos::FGeometryParticleHandle>::Result;

	TBPVisitor(const FTransform& InWorldTM, ChaosInterface::FSQHitBuffer<THitType>& InHitBuffer,
		const FQueryFilterData& InQueryFilterData, ICollisionQueryFilterCallbackBase& InQueryCallback, const QueryGeometryType& InQueryGeom, const FQueryDebugParams& InDebugParams)
		: HalfExtents(InQueryGeom.BoundingBox().Extents() * 0.5)
		, bAnyHit(false)
		, DebugParams(InDebugParams)
		, HitBuffer(InHitBuffer)
		, QueryFilterData(InQueryFilterData)
		, QueryFilterDataConcrete(ToUnrealFilterData(InQueryFilterData.data))
		, QueryGeom(&InQueryGeom)
		, QueryCallback(InQueryCallback)
		, StartTM(InWorldTM)
	{
		bAnyHit = QueryFilterData.flags & FPhysicsQueryFlag::eANY_HIT;
	}
	virtual bool Overlap(const Chaos::TSpatialVisitorData<TPayload>& Instance) override
	{
		return Visit<ESQType::Overlap>(Instance, nullptr);
	}

	virtual bool Raycast(const Chaos::TSpatialVisitorData<TPayload>& Instance, Chaos::FQueryFastData& CurData) override
	{
		ensure(false);
		return false;
	}

	virtual bool Sweep(const Chaos::TSpatialVisitorData<TPayload>& Instance, Chaos::FQueryFastData& CurData) override
	{
		ensure(false);
		return false;
	}

	virtual const void* GetQueryData() const override
	{
		return &QueryFilterDataConcrete;
	}

	virtual const void* GetSimData() const override
	{
		return nullptr;
	}
	
	/** Return a pointer to the payload on which we are querying the acceleration structure */
	virtual const void* GetQueryPayload() const override
	{
		return nullptr;
	}

private:

	enum class ESQType
	{
		Raycast,
		Sweep,
		Overlap
	};

	auto GetPayloadForThread(const TPayload& Payload)
	{
		if constexpr (bGTData)
		{
			return Payload.GetExternalGeometryParticle_ExternalThread();
		}
		else
		{
			return Payload.GetGeometryParticleHandle_PhysicsThread();
		}
	}

	template <ESQType SQ>
	bool Visit(const Chaos::TSpatialVisitorData<TPayload>& Instance, Chaos::FQueryFastData* CurData)
	{
		bool bContinue = true;
		TPayload Payload = Instance.Payload;
		//todo: add a check to ensure hitbuffer matches SQ type
		using namespace Chaos;
		TGeometryType* GeometryParticle = GetPayloadForThread(Payload);
		if (!GeometryParticle)
		{
			// This case handles particles created by the physics simulation without the main thread
			// being made aware of their creation. We have a PT particle but no external particle
			ensure(bGTData);
			return true;
		}
		const FShapesArray& Shapes = GeometryParticle->ShapesArray();
		THitType Hit;
		Hit.Actor = GeometryParticle;
		for (const auto& Shape : Shapes)
		{
			ECollisionQueryHitType HitType = QueryFilterData.flags & FPhysicsQueryFlag::ePREFILTER ? QueryCallback.PreFilter(QueryFilterDataConcrete, *Shape, *GeometryParticle) : ECollisionQueryHitType::Block;
			if (HitType != ECollisionQueryHitType::None)
			{
				const bool bBlocker = (HitType == ECollisionQueryHitType::Block || bAnyHit || HitBuffer.WantsSingleResult());
				Hit.Shape = Shape.Get();
				HitBuffer.InsertHit(Hit, bBlocker);
				if (bAnyHit)
				{
					bContinue = false;
				}
				break;
			}
		}
		return bContinue;
	}

	const FVector StartPoint;
	const FVector Dir;
	const FVector HalfExtents;
	FHitFlags OutputFlags;
	bool bAnyHit;
	const FQueryDebugParams DebugParams;
	ChaosInterface::FSQHitBuffer<THitType>& HitBuffer;
	const FQueryFilterData& QueryFilterData;
	const FCollisionFilterData QueryFilterDataConcrete;
	const QueryGeometryType* QueryGeom;
	ICollisionQueryFilterCallbackBase& QueryCallback;
	const FTransform StartTM;
};

template <typename TRaycastHit>
void FChaosSQAccelerator::RaycastImp(const FVector& Start, const FVector& Dir, const float DeltaMagnitude, ChaosInterface::FSQHitBuffer<TRaycastHit>& HitBuffer, EHitFlags OutputFlags, const FQueryFilterData& QueryFilterData, ICollisionQueryFilterCallbackBase& QueryCallback, const FQueryDebugParams& DebugParams) const
{
	using namespace Chaos;
	using namespace ChaosInterface;

	TSQVisitor<TSphere<FReal, 3>, FAccelerationStructureHandle, TRaycastHit, std::is_same<TRaycastHit,FRaycastHit>::value> RaycastVisitor(Start, Dir, HitBuffer, OutputFlags, QueryFilterData, QueryCallback, DebugParams);
	HitBuffer.IncFlushCount();
	SpatialAcceleration.Raycast(Start, Dir, DeltaMagnitude, RaycastVisitor);
	HitBuffer.DecFlushCount();
}

void FChaosSQAccelerator::Raycast(const FVector& Start, const FVector& Dir, const float DeltaMagnitude, ChaosInterface::FSQHitBuffer<ChaosInterface::FRaycastHit>& HitBuffer, EHitFlags OutputFlags, const FQueryFilterData& QueryFilterData, ICollisionQueryFilterCallbackBase& QueryCallback, const FQueryDebugParams& DebugParams) const
{
	RaycastImp(Start, Dir, DeltaMagnitude, HitBuffer, OutputFlags, QueryFilterData, QueryCallback, DebugParams);
}

void FChaosSQAccelerator::Raycast(const FVector& Start, const FVector& Dir, const float DeltaMagnitude, ChaosInterface::FSQHitBuffer<ChaosInterface::FPTRaycastHit>& HitBuffer, EHitFlags OutputFlags, const FQueryFilterData& QueryFilterData, ICollisionQueryFilterCallbackBase& QueryCallback, const FQueryDebugParams& DebugParams) const
{
	RaycastImp(Start, Dir, DeltaMagnitude, HitBuffer, OutputFlags, QueryFilterData, QueryCallback, DebugParams);
}

template <typename QueryGeomType, typename TSweepHit>
void SweepHelper(const QueryGeomType& QueryGeom,const Chaos::ISpatialAcceleration<Chaos::FAccelerationStructureHandle, Chaos::FReal,3>& SpatialAcceleration,const FTransform& StartTM,const FVector& Dir,const float DeltaMagnitude,ChaosInterface::FSQHitBuffer<TSweepHit>& HitBuffer,EHitFlags OutputFlags,const FQueryFilterData& QueryFilterData,ICollisionQueryFilterCallbackBase& QueryCallback,const FQueryDebugParams& DebugParams)
{
	using namespace Chaos;
	using namespace ChaosInterface;

	const FAABB3 Bounds = QueryGeom.BoundingBox().TransformedAABB(StartTM);
	const bool bSweepAsOverlap = DeltaMagnitude == 0;	//question: do we care about tiny sweeps?
	TSQVisitor<QueryGeomType,FAccelerationStructureHandle,TSweepHit, std::is_same<TSweepHit, FSweepHit>::value> SweepVisitor(StartTM,Dir,HitBuffer,OutputFlags,QueryFilterData,QueryCallback,QueryGeom,DebugParams);

	HitBuffer.IncFlushCount();

	if(bSweepAsOverlap)
	{
		//fallback to overlap
		SpatialAcceleration.Overlap(Bounds, SweepVisitor);
	} else
	{
		const FVector HalfExtents = Bounds.Extents() * 0.5f;
		SpatialAcceleration.Sweep(Bounds.GetCenter(),Dir,DeltaMagnitude,HalfExtents,SweepVisitor);
	}

	HitBuffer.DecFlushCount();
}

void FChaosSQAccelerator::Sweep(const Chaos::FImplicitObject& QueryGeom, const FTransform& StartTM, const FVector& Dir, const float DeltaMagnitude, ChaosInterface::FSQHitBuffer<ChaosInterface::FSweepHit>& HitBuffer, EHitFlags OutputFlags, const FQueryFilterData& QueryFilterData, ICollisionQueryFilterCallbackBase& QueryCallback, const FQueryDebugParams& DebugParams) const
{
	return Chaos::Utilities::CastHelper(QueryGeom, StartTM, [&](const auto& Downcast, const FTransform& StartFullTM) { return SweepHelper(Downcast, SpatialAcceleration, StartFullTM, Dir, DeltaMagnitude, HitBuffer, OutputFlags, QueryFilterData, QueryCallback, DebugParams); });
}

void FChaosSQAccelerator::Sweep(const Chaos::FImplicitObject& QueryGeom, const FTransform& StartTM, const FVector& Dir, const float DeltaMagnitude, ChaosInterface::FSQHitBuffer<ChaosInterface::FPTSweepHit>& HitBuffer, EHitFlags OutputFlags, const FQueryFilterData& QueryFilterData, ICollisionQueryFilterCallbackBase& QueryCallback, const FQueryDebugParams& DebugParams) const
{
	return Chaos::Utilities::CastHelper(QueryGeom, StartTM, [&](const auto& Downcast, const FTransform& StartFullTM) { return SweepHelper(Downcast, SpatialAcceleration, StartFullTM, Dir, DeltaMagnitude, HitBuffer, OutputFlags, QueryFilterData, QueryCallback, DebugParams); });
}

template <typename QueryGeomType, typename TOverlapHit>
void OverlapHelper(const QueryGeomType& QueryGeom, const Chaos::ISpatialAcceleration<Chaos::FAccelerationStructureHandle, Chaos::FReal, 3>& SpatialAcceleration, const FTransform& GeomPose, ChaosInterface::FSQHitBuffer<TOverlapHit>& HitBuffer, const FQueryFilterData& QueryFilterData, ICollisionQueryFilterCallbackBase& QueryCallback, const FQueryDebugParams& DebugParams)
{
	using namespace Chaos;
	using namespace ChaosInterface;

	const FAABB3 Bounds = QueryGeom.CalculateTransformedBounds(GeomPose);

	HitBuffer.IncFlushCount();

	bool bSkipNarrowPhase = QueryFilterData.flags & FPhysicsQueryFlag::eSKIPNARROWPHASE;

	constexpr bool bGTData = std::is_same<TOverlapHit, FOverlapHit>::value;
	if (bSkipNarrowPhase)
	{
		TBPVisitor<QueryGeomType, FAccelerationStructureHandle, TOverlapHit, bGTData> OverlapVisitor(GeomPose, HitBuffer, QueryFilterData, QueryCallback, QueryGeom, DebugParams);
		SpatialAcceleration.Overlap(Bounds, OverlapVisitor);
	}
	else
	{
		TSQVisitor<QueryGeomType, FAccelerationStructureHandle, TOverlapHit, bGTData> OverlapVisitor(GeomPose, HitBuffer, QueryFilterData, QueryCallback, QueryGeom, DebugParams);
		SpatialAcceleration.Overlap(Bounds, OverlapVisitor);
	}
	HitBuffer.DecFlushCount();
}

void FChaosSQAccelerator::Overlap(const Chaos::FImplicitObject& QueryGeom, const FTransform& GeomPose, ChaosInterface::FSQHitBuffer<ChaosInterface::FOverlapHit>& HitBuffer, const FQueryFilterData& QueryFilterData, ICollisionQueryFilterCallbackBase& QueryCallback, const FQueryDebugParams& DebugParams) const
{
	return Chaos::Utilities::CastHelper(QueryGeom, GeomPose, [&](const auto& Downcast, const FTransform& GeomFullPose) { return OverlapHelper(Downcast, SpatialAcceleration, GeomFullPose, HitBuffer, QueryFilterData, QueryCallback, DebugParams); });
}

void FChaosSQAccelerator::Overlap(const Chaos::FImplicitObject& QueryGeom, const FTransform& GeomPose, ChaosInterface::FSQHitBuffer<ChaosInterface::FPTOverlapHit>& HitBuffer, const FQueryFilterData& QueryFilterData, ICollisionQueryFilterCallbackBase& QueryCallback, const FQueryDebugParams& DebugParams) const
{
	return Chaos::Utilities::CastHelper(QueryGeom, GeomPose, [&](const auto& Downcast, const FTransform& GeomFullPose) { return OverlapHelper(Downcast, SpatialAcceleration, GeomFullPose, HitBuffer, QueryFilterData, QueryCallback, DebugParams); });
}
