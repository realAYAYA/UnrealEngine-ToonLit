// Copyright Epic Games, Inc. All Rights Reserved.

#include "Chaos/Adapters/GeometryCollectionComponentCacheAdapter.h"
#include "Chaos/ChaosCache.h"
#include "Chaos/ParticleHandle.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "PBDRigidsSolver.h"
#include "PhysicsProxy/GeometryCollectionPhysicsProxy.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"
#include "Chaos/ChaosSolverActor.h"
#include "GeometryCollection/GeometryCollectionAlgo.h"

FName FEnableStateEvent::EventName("GC_Enable");
FName FBreakingEvent::EventName("GC_Breaking");
FName FCollisionEvent::EventName("GC_Collision");
FName FTrailingEvent::EventName("GC_Trailing");

namespace Chaos
{

	FComponentCacheAdapter::SupportType FGeometryCollectionCacheAdapter::SupportsComponentClass(UClass* InComponentClass) const
	{
		UClass* Desired = GetDesiredClass();
		if(InComponentClass == Desired)
		{
			return FComponentCacheAdapter::SupportType::Direct;
		}
		else if(InComponentClass->IsChildOf(Desired))
		{
			return FComponentCacheAdapter::SupportType::Derived;
		}

		return FComponentCacheAdapter::SupportType::None;
	}

	UClass* FGeometryCollectionCacheAdapter::GetDesiredClass() const
	{
		return UGeometryCollectionComponent::StaticClass();
	}

	uint8 FGeometryCollectionCacheAdapter::GetPriority() const
	{
		return EngineAdapterPriorityBegin;
	}
	
	void FGeometryCollectionCacheAdapter::Record_PostSolve(UPrimitiveComponent* InComp, const FTransform& InRootTransform, FPendingFrameWrite& OutFrame, Chaos::FReal InTime) const
	{

		EnsureIsInPhysicsThreadContext();


		using FClusterParticle = Chaos::FPBDRigidClusteredParticleHandle;
		using FRigidParticle = Chaos::FPBDRigidParticleHandle;

		UGeometryCollectionComponent*    Comp  = CastChecked<UGeometryCollectionComponent>(InComp);
		FGeometryCollectionPhysicsProxy* Proxy = Comp->GetPhysicsProxy();
		
		if(!Proxy)
		{
			return;
		}

		if (!CachedData.Contains(Proxy))
		{
			return;
		}

		const FTransform WorldToComponent = Proxy->GetSimParameters().WorldTransform.Inverse();

		const FCachedEventData& ProxyCachedEventData = CachedData[Proxy];

		const Chaos::FPhysicsSolver* Solver         = Proxy->GetSolver<Chaos::FPhysicsSolver>();
		const FGeometryCollection*   RestCollection = Proxy->GetSimParameters().RestCollection;

		if(!RestCollection || !Solver)
		{
			return;
		}

		const FPhysScene* Scene = static_cast<FPhysScene*>(Solver->PhysSceneHack);
		if (!Scene)
		{
			return;
		}

		FGeometryDynamicCollection&            Collection       = Proxy->GetPhysicsCollection();
		const TManagedArray<FTransform>&       MassToLocal		= Collection.MassToLocal;
		const TManagedArray<int32>&            Parents			= RestCollection->Parent;
		const TManagedArray<TSet<int32>>&	   Children         = RestCollection->Children;
		const TArray<FBreakingData>&		   Breaks           = Solver->GetEvolution()->GetRigidClustering().GetAllClusterBreakings();
		const FTransform					   WorldToActor		= InRootTransform.Inverse();

		// A transform index exists for each 'real' (i.e. leaf node in the rest collection)
		const int32 NumTransforms = Collection.NumElements(FGeometryCollection::TransformGroup);

		// Pre-alloc once for worst case.
		OutFrame.PendingParticleData.Reserve(NumTransforms);

		// Record the cluster indices that released this frame. Those particles become
		// inactive: to make random access reads convenient, we set their transforms to identity.
		TSet<int32> ReleasedClusters;
		ReleasedClusters.Reserve(Breaks.Num());
		
		TArray<int32> RelatedBreaks;
		RelatedBreaks.Reserve(Breaks.Num());
		
		for(const FBreakingData& Break : Breaks)
		{
			// Is the proxy pending destruction? If they are no longer tracked by the PhysScene, the proxy is deleted or pending deletion.
			if (Scene->GetOwningComponent<UPrimitiveComponent>(Break.Proxy) == nullptr)
			{
				continue;
			}
			
			if(Break.Proxy->GetType() == EPhysicsProxyType::GeometryCollectionType)
			{
				FGeometryCollectionPhysicsProxy* ConcreteProxy = static_cast<FGeometryCollectionPhysicsProxy*>(Break.Proxy);

				if(ConcreteProxy == Proxy)
				{
					// Record the cluster it belonged to.
					ReleasedClusters.Add(Parents[Break.TransformGroupIndex]);

					// Force a break on all children of the cluster
					for (int32 ChildToBreak : Children[Parents[Break.TransformGroupIndex]])
					{
						RelatedBreaks.AddUnique(ChildToBreak);
					}
				}
			}
		}

		for(int32 TransformIndex = 0; TransformIndex < NumTransforms; ++TransformIndex)
		{
			FClusterParticle* Handle = Proxy->GetParticles()[TransformIndex];
			
			if(Handle)
			{
				const FRigidParticle* Parent = Handle ? Handle->ClusterIds().Id : nullptr;
				const FClusterParticle* ParentAsCluster = Parent ? Parent->CastToClustered() : nullptr;
				const bool bParentIsInternalCluster = ParentAsCluster ? ParentAsCluster->InternalCluster() : false;
				const bool bParentIsActiveInternalCluster = bParentIsInternalCluster && !Parent->Disabled();

				const bool bParticleDisabled = Handle->Disabled();
				if(!bParticleDisabled || bParentIsActiveInternalCluster)
				{
					OutFrame.PendingParticleData.AddDefaulted();
					FPendingParticleWrite& Pending = OutFrame.PendingParticleData.Last();

					Pending.ParticleIndex = TransformIndex;
					
					// All recorded transforms are in actor space, ie relative to the Cache Manager making the recording.
					FTransform LocalTransform = MassToLocal[TransformIndex].Inverse() * FTransform(Handle->R(), Handle->X());
					FTransform ActorSpaceTransform = LocalTransform * WorldToActor;
					Pending.PendingTransform = ActorSpaceTransform;
				}

				if (RelatedBreaks.Contains(TransformIndex))
				{
					OutFrame.PushEvent(FEnableStateEvent::EventName, InTime, FEnableStateEvent(TransformIndex, true));
				}
			}
		}

		// If a cluster particle released, set its transform to identity.
		for (int32 TransformIndex : ReleasedClusters)
		{
			if (TransformIndex > INDEX_NONE)
			{
				FClusterParticle* Handle = Proxy->GetParticles()[TransformIndex];
				if (Handle)
				{
					OutFrame.PendingParticleData.AddDefaulted();
					FPendingParticleWrite& Pending = OutFrame.PendingParticleData.Last();
					Pending.ParticleIndex = TransformIndex;
					FTransform LocalTransform = MassToLocal[TransformIndex].Inverse() * FTransform(Handle->R(), Handle->X());
					FTransform ActorSpaceTransform = LocalTransform * WorldToActor;
					Pending.PendingTransform = ActorSpaceTransform;
					Pending.bPendingDeactivate = true;
				}
			}
		}

		if (Proxy->GetSimParameters().bGenerateBreakingData && BreakingDataArray && ProxyCachedEventData.ProxyBreakingDataIndices)
		{
			for (int32 Index : *ProxyCachedEventData.ProxyBreakingDataIndices)
			{
				if (BreakingDataArray->IsValidIndex(Index))
				{
					const FBreakingData& BreakingData = (*BreakingDataArray)[Index];
					if (BreakingData.TransformGroupIndex > INDEX_NONE)
					{
						OutFrame.PushEvent(FBreakingEvent::EventName, InTime, FBreakingEvent(BreakingData.TransformGroupIndex, BreakingData, WorldToComponent));
					}
				}
				
			}
		}
		
		if (Proxy->GetSimParameters().bGenerateCollisionData && CollisionDataArray && ProxyCachedEventData.ProxyCollisionDataIndices)
		{
			for (int32 Index : *ProxyCachedEventData.ProxyCollisionDataIndices)
			{
				if (CollisionDataArray->IsValidIndex(Index))
				{
					const FCollidingData& CollisionData = (*CollisionDataArray)[Index];
					OutFrame.PushEvent(FCollisionEvent::EventName, InTime, FCollisionEvent(CollisionData, WorldToComponent));
				}

			}
		}
		
		if (Proxy->GetSimParameters().bGenerateTrailingData && TrailingDataArray && ProxyCachedEventData.ProxyTrailingDataIndices)
		{
			for (int32 Index : *ProxyCachedEventData.ProxyTrailingDataIndices)
			{
				if (TrailingDataArray->IsValidIndex(Index))
				{
					const FTrailingData& TrailingData = (*TrailingDataArray)[Index];
					if (TrailingData.TransformGroupIndex > INDEX_NONE)
					{
						OutFrame.PushEvent(FTrailingEvent::EventName, InTime, FTrailingEvent(TrailingData.TransformGroupIndex, TrailingData, WorldToComponent));
					}
				}

			}
		}

		// Never going to change again till freed after writing to the cache so free up the extra space we reserved
		OutFrame.PendingParticleData.Shrink();
	}
	
	void FGeometryCollectionCacheAdapter::Playback_PreSolve(UPrimitiveComponent*							   InComponent,
															UChaosCache*									   InCache,
															Chaos::FReal                                       InTime,
															FPlaybackTickRecord&							   TickRecord,
															TArray<TPBDRigidParticleHandle<Chaos::FReal, 3>*>& OutUpdatedRigids) const
	{
		EnsureIsInPhysicsThreadContext();
		
		using FClusterParticle = Chaos::FPBDRigidClusteredParticleHandle;
		using FRigidParticle = Chaos::FPBDRigidParticleHandle;

		UGeometryCollectionComponent*    Comp  = CastChecked<UGeometryCollectionComponent>(InComponent);
		FGeometryCollectionPhysicsProxy* Proxy = Comp->GetPhysicsProxy();
		
		if(!Proxy)
		{
			return;
		}

		const FTransform ComponentToWorld = Proxy->GetSimParameters().WorldTransform;
		const FGeometryCollection* RestCollection = Proxy->GetSimParameters().RestCollection;
		Chaos::FPhysicsSolver*     Solver         = Proxy->GetSolver<Chaos::FPhysicsSolver>();

		if(!RestCollection || !Solver)
		{
			return;
		}

		FGeometryDynamicCollection&      Collection       = Proxy->GetPhysicsCollection();
		const TManagedArray<FTransform>& MassToLocal	  = Collection.MassToLocal;
		TArray<FClusterParticle*>        Particles        = Proxy->GetParticles();

		FCacheEvaluationContext Context(TickRecord);
		Context.bEvaluateTransform = true;
		Context.bEvaluateCurves    = false;
		Context.bEvaluateEvents    = true;

		FCacheEvaluationResult EvaluatedResult = InCache->Evaluate(Context, &MassToLocal.GetConstArray());

		const int32                      NumEventTracks = EvaluatedResult.Events.Num();
		const TArray<FCacheEventHandle>* EnableEvents   = EvaluatedResult.Events.Find(FEnableStateEvent::EventName);
		const TArray<FCacheEventHandle>* BreakingEvents = EvaluatedResult.Events.Find(FBreakingEvent::EventName);
		const TArray<FCacheEventHandle>* CollisionEvents = EvaluatedResult.Events.Find(FCollisionEvent::EventName);
		const TArray<FCacheEventHandle>* TrailingEvents = EvaluatedResult.Events.Find(FTrailingEvent::EventName);

		if(EnableEvents)
		{
			TMap<FClusterParticle*, TArray<FRigidParticle*>> NewClusters;
			for(const FCacheEventHandle& Handle : *EnableEvents)
			{
				if(FEnableStateEvent* Event = Handle.Get<FEnableStateEvent>())
				{
					if(Particles.IsValidIndex(Event->Index))
					{
						Chaos::FPBDRigidClusteredParticleHandle* ChildParticle = Particles[Event->Index];
						
						if (ChildParticle)
						{
							if (ChildParticle->ObjectState() != EObjectStateType::Kinematic)
							{
								// If a field or other external actor set the particle to static or dynamic we no longer apply the cache
								continue;
							}

							if (FRigidParticle* ClusterParent = ChildParticle->ClusterIds().Id)
							{
								if (FClusterParticle* Parent = ClusterParent->CastToClustered())
								{
									TArray<FRigidParticle*>& Cluster = NewClusters.FindOrAdd(Parent);
									Cluster.Add(ChildParticle);
								}
							}
							else
							{
								// This is a cluster parent
								ChildParticle->SetDisabled(!Event->bEnable);
							}
						}
					}
				}
			}

			for(TPair<FClusterParticle*, TArray<FRigidParticle*>> Cluster : NewClusters)
			{
				TArray<FRigidParticle*>& ChildrenParticles = Cluster.Value;
				if (ChildrenParticles.Num())
				{
					FRigidParticle* ClusterHandle = nullptr;
					
					for (FRigidParticle* ChildHandle : ChildrenParticles)
					{
						if (FClusterParticle* ClusteredChildHandle = ChildHandle->CastToClustered())
						{
							if (ClusteredChildHandle->Disabled() && ClusteredChildHandle->ClusterIds().Id != nullptr)
							{
								if (ensure(!ClusterHandle || ClusteredChildHandle->ClusterIds().Id == ClusterHandle))
								{
									ClusterHandle = ClusteredChildHandle->ClusterIds().Id;
								}
								else
								{
									break; //shouldn't be here
								}
							}
						}
					}
					if (ClusterHandle)
					{
						Solver->GetEvolution()->GetRigidClustering().ReleaseClusterParticlesNoInternalCluster(ClusterHandle->CastToClustered(), true);
					}
				}
			}
		}

		if (Proxy->GetSimParameters().bGenerateBreakingData && BreakingEvents)
		{
			const FSolverBreakingEventFilter* SolverBreakingEventFilter = Solver->GetEventFilters()->GetBreakingFilter();

			for (const FCacheEventHandle& Handle : *BreakingEvents)
			{
				if (FBreakingEvent* Event = Handle.Get<FBreakingEvent>())
				{
					if (Particles.IsValidIndex(Event->Index))
					{
						Chaos::FPBDRigidClusteredParticleHandle* Particle = Particles[Event->Index];

						if (Particle)
						{
							if (Particle->ObjectState() != EObjectStateType::Kinematic)
							{
								// If a field or other external actor set the particle to static or dynamic we no longer apply the cache
								continue;
							}

							FBreakingData CachedBreak;
							CachedBreak.Proxy = Proxy;
							CachedBreak.Location = ComponentToWorld.TransformPosition(Event->Location);
							CachedBreak.Velocity = ComponentToWorld.TransformVector(Event->Velocity);
							CachedBreak.AngularVelocity = Event->AngularVelocity;
							CachedBreak.Mass = Event->Mass;
							CachedBreak.BoundingBox = TAABB<FReal, 3>(Event->BoundingBoxMin, Event->BoundingBoxMax);
							CachedBreak.BoundingBox = CachedBreak.BoundingBox.TransformedAABB(ComponentToWorld);

							if (!SolverBreakingEventFilter->Enabled() || SolverBreakingEventFilter->Pass(CachedBreak))
							{
								float TimeStamp = Solver->GetSolverTime();
								Solver->GetEventManager()->AddEvent<FBreakingEventData>(EEventType::Breaking, [&CachedBreak, TimeStamp](FBreakingEventData& BreakingEventData)
									{
										if (BreakingEventData.BreakingData.TimeCreated != TimeStamp)
										{
											BreakingEventData.BreakingData.AllBreakingsArray.Reset();
											BreakingEventData.BreakingData.TimeCreated = TimeStamp;
										}
										BreakingEventData.BreakingData.AllBreakingsArray.Add(CachedBreak);
									});
							}
						}
					}
				}
			}
		}

		if (Proxy->GetSimParameters().bGenerateTrailingData && TrailingEvents)
		{
			const FSolverTrailingEventFilter* SolverTrailingEventFilter = Solver->GetEventFilters()->GetTrailingFilter();

			for (const FCacheEventHandle& Handle : *TrailingEvents)
			{
				if (FTrailingEvent* Event = Handle.Get<FTrailingEvent>())
				{
					if (Particles.IsValidIndex(Event->Index))
					{
						Chaos::FPBDRigidClusteredParticleHandle* Particle = Particles[Event->Index];

						if (Particle)
						{
							if (Particle->ObjectState() != EObjectStateType::Kinematic)
							{
								// If a field or other external actor set the particle to static or dynamic we no longer apply the cache
								continue;
							}

							FTrailingData CachedTrail;
							CachedTrail.Proxy = Proxy;
							CachedTrail.Location = ComponentToWorld.TransformPosition(Event->Location);
							CachedTrail.Velocity = ComponentToWorld.TransformVector(Event->Velocity);
							CachedTrail.AngularVelocity = Event->AngularVelocity;
							CachedTrail.BoundingBox = TAABB<FReal, 3>(Event->BoundingBoxMin, Event->BoundingBoxMax);
							CachedTrail.BoundingBox = CachedTrail.BoundingBox.TransformedAABB(ComponentToWorld);

							if (!SolverTrailingEventFilter->Enabled() || SolverTrailingEventFilter->Pass(CachedTrail))
							{
								float TimeStamp = Solver->GetSolverTime();
								Solver->GetEventManager()->AddEvent<FTrailingEventData>(EEventType::Trailing, [&CachedTrail , TimeStamp](FTrailingEventData& TrailingEventData)
									{
										if (TrailingEventData.TrailingData.TimeCreated != TimeStamp)
										{
											TrailingEventData.TrailingData.AllTrailingsArray.Reset();
											TrailingEventData.TrailingData.TimeCreated = TimeStamp;
										}
										TrailingEventData.TrailingData.AllTrailingsArray.Add(CachedTrail);
									});
							}
						}
					}
				}
			}
		}

		if (Proxy->GetSimParameters().bGenerateCollisionData && CollisionEvents)
		{

			const FSolverCollisionEventFilter* SolverCollisionEventFilter = Solver->GetEventFilters()->GetCollisionFilter();
			for (const FCacheEventHandle& Handle : *CollisionEvents)
			{
				if (FCollisionEvent* Event = Handle.Get<FCollisionEvent>())
				{
					
					FCollidingData CachedCollision;
					CachedCollision.Location = ComponentToWorld.TransformPosition(Event->Location);
					CachedCollision.AccumulatedImpulse = ComponentToWorld.TransformVector(Event->AccumulatedImpulse);
					CachedCollision.Normal = ComponentToWorld.TransformVector(Event->Normal);
					CachedCollision.Velocity1 = ComponentToWorld.TransformVector(Event->Velocity1);
					CachedCollision.Velocity2 = ComponentToWorld.TransformVector(Event->Velocity2);
					CachedCollision.DeltaVelocity1 = ComponentToWorld.TransformVector(Event->DeltaVelocity1);
					CachedCollision.DeltaVelocity2 = ComponentToWorld.TransformVector(Event->DeltaVelocity2);
					CachedCollision.AngularVelocity1 = Event->AngularVelocity1;
					CachedCollision.AngularVelocity2 = Event->AngularVelocity2;
					CachedCollision.Mass1 = Event->Mass1;
					CachedCollision.Mass2 = Event->Mass2;
					CachedCollision.PenetrationDepth = Event->PenetrationDepth;
					
					if (!SolverCollisionEventFilter->Enabled() || SolverCollisionEventFilter->Pass(CachedCollision))
					{
						float TimeStamp = Solver->GetSolverTime();
						Solver->GetEventManager()->AddEvent<FCollisionEventData>(EEventType::Collision, [&CachedCollision, TimeStamp, Proxy](FCollisionEventData& CollisionEventData)
							{
								if (CollisionEventData.CollisionData.TimeCreated != TimeStamp)
								{
									CollisionEventData.CollisionData.AllCollisionsArray.Reset();
									CollisionEventData.PhysicsProxyToCollisionIndices.Reset();
									CollisionEventData.CollisionData.TimeCreated = TimeStamp;
								}
								int32 NewIdx = CollisionEventData.CollisionData.AllCollisionsArray.Add(CachedCollision);
								CollisionEventData.PhysicsProxyToCollisionIndices.PhysicsProxyToIndicesMap.FindOrAdd(Proxy).Add(NewIdx);
							});
					}
						
					
				}
			}
		}

		const int32 NumTransforms = EvaluatedResult.Transform.Num();
		for(int32 Index = 0; Index < NumTransforms; ++Index)
		{
			const int32      ParticleIndex      = EvaluatedResult.ParticleIndices[Index];
			const FTransform EvaluatedTransform = EvaluatedResult.Transform[Index];

			if(Particles.IsValidIndex(ParticleIndex))
			{
				Chaos::FPBDRigidClusteredParticleHandle* Handle = Particles[ParticleIndex];

				if(!Handle || Handle->ObjectState() != EObjectStateType::Kinematic)
				{
					// If a field or other external actor set the particle to static or dynamic we no longer apply the cache
					continue;
				}

				// EvaluatedTransform is in world space already as we have passed MassToLocal transform to InCache->Evaluate()
				// to make sure the interpolation was done correctly in world space  
				const FTransform WorldTransform{EvaluatedTransform};

				Handle->SetP(WorldTransform.GetTranslation());
				Handle->SetQ(WorldTransform.GetRotation());
				Handle->SetX(Handle->P());
				Handle->SetR(Handle->Q());
				
				Handle->UpdateWorldSpaceState(WorldTransform, FVec3(0));

				if(!Handle->Disabled())
				{
					Solver->GetEvolution()->DirtyParticle(*Handle);
				}

				if(FRigidParticle* ClusterParent = Handle->ClusterIds().Id)
				{
					if(FClusterParticle* Parent = ClusterParent->CastToClustered())
					{
						if(Parent->InternalCluster())
						{
							// This is an unmanaged particle. Because its children are kinematic it will be also.
							// however we need to update its position at least once to place it correctly.
							// The child was placed with:
							//     ChildT = ChildHandle->ChildToParent() * FTransform(ParentHandle->R(), ParentHandle->X());
							// When it was simulated, so we can work backwards to place the parent.
							// This will result in multiple transform sets happening to the parent but allows us to mostly ignore
							// that it exists, if it doesn't the child still gets set to the correct position.
							FTransform ChildTransform = Handle->ChildToParent();
							FTransform Result = ChildTransform.Inverse() * WorldTransform;
							Parent->SetP(Result.GetTranslation());
							Parent->SetX(Result.GetTranslation());
							Parent->SetQ(Result.GetRotation());
							Parent->SetR(Result.GetRotation());

							Parent->UpdateWorldSpaceState(Result, FVec3(0));

							if(!Parent->Disabled())
							{
								Solver->GetEvolution()->DirtyParticle(*Parent);
							}
						}
					}
				}

				OutUpdatedRigids.Add(Handle);
			}
		}
	}

	bool FGeometryCollectionCacheAdapter::ValidForPlayback(UPrimitiveComponent* InComponent, UChaosCache* InCache) const
	{
		UGeometryCollectionComponent*  GeomComponent = Cast<UGeometryCollectionComponent>(InComponent);

		if(!GeomComponent)
		{
			return false;
		}

		const UGeometryCollection* Collection = GeomComponent->RestCollection;

		if(!Collection || !Collection->GetGeometryCollection().IsValid())
		{
			return false;
		}

		// Really permissive check - as long as we can map all tracks to a particle in the geometry collection we'll allow this to play.
		// allows geometry changes without invalidating an entire cache on reimport or modification.
		const int32 NumTransforms = Collection->GetGeometryCollection()->Transform.Num();
		for(const int32 ParticleIndex : InCache->TrackToParticle)
		{
			if(ParticleIndex < 0 || ParticleIndex >= NumTransforms)
			{
				return false;
			}
		}

		return true;
	}

	FGuid FGeometryCollectionCacheAdapter::GetGuid() const
	{
		FGuid NewGuid;
		checkSlow(FGuid::Parse(TEXT("A3147746B50C47C883B93DBF85CBB589"), NewGuid));
		return NewGuid;
	}

	Chaos::FPhysicsSolver* FGeometryCollectionCacheAdapter::GetComponentSolver(UPrimitiveComponent* InComponent) const
	{
		// If the observed component is a Geometry Collection using a non-default Chaos solver..
		if (UGeometryCollectionComponent* GeometryCollectionComponent = Cast<UGeometryCollectionComponent>(InComponent))
		{
			if (AChaosSolverActor* SolverActor = GeometryCollectionComponent->GetPhysicsSolverActor())
			{
				return SolverActor->GetSolver();
			}			
		}

		// ..otherwise use the default solver.
		if (InComponent && InComponent->GetWorld())
		{
			UWorld* ComponentWorld = InComponent->GetWorld();

			if (FPhysScene* WorldScene = ComponentWorld->GetPhysicsScene())
			{
				return WorldScene->GetSolver();
			}
		}


		return nullptr;
	}
	
	Chaos::FPhysicsSolverEvents* FGeometryCollectionCacheAdapter::BuildEventsSolver(UPrimitiveComponent* InComponent) const
	{
		return GetComponentSolver(InComponent);
	}

	void FGeometryCollectionCacheAdapter::SetRestState(UPrimitiveComponent* InComponent, UChaosCache* InCache, const FTransform& InRootTransform, Chaos::FReal InTime) const
	{
		// Caches recorded previous to Version 1 may not scrub correctly as the MassToLocal transform has been burned in.
		
		EnsureIsInGameThreadContext();

		
		if (!InCache || InCache->GetDuration() == 0.0f)
		{
			return;
		}

		if (UGeometryCollectionComponent* Comp = CastChecked<UGeometryCollectionComponent>(InComponent))
		{
			const FTransform ActorToWorld = InRootTransform;
			const FTransform WorldToComponent = Comp->GetComponentTransform().Inverse();
			FTransform ActorToComponent = ActorToWorld * WorldToComponent;
			
			if (const UGeometryCollection* RestCollection = Comp->GetRestCollection())
			{
				TSharedPtr<FGeometryCollection, ESPMode::ThreadSafe> GeometryCollection = RestCollection->GetGeometryCollection();
				const int32 NumTransforms = GeometryCollection->NumElements(FGeometryCollection::TransformGroup);

				FPlaybackTickRecord TickRecord;
				TickRecord.SetLastTime(InTime);
				FCacheEvaluationContext Context(TickRecord);
				Context.bEvaluateTransform = true;
				Context.bEvaluateCurves = false;
				Context.bEvaluateEvents = false;

				// we only need the rest transforms, so no need to pass MassToLocal transforms 
				FCacheEvaluationResult EvaluatedResult = InCache->Evaluate(Context, nullptr);
				const int32 NumCacheTransforms = EvaluatedResult.Transform.Num();
			
				// Any bone that is not explicitly set by the cache defaults to it's rest collection position.
				TArray<FTransform> RestTransforms;
				RestTransforms.SetNum(NumTransforms);
				for (int32 Idx = 0; Idx < NumTransforms; ++Idx)
				{
					RestTransforms[Idx] = GeometryCollection->Transform[Idx];
				}

				for (int32 CacheIdx = 0; CacheIdx < NumCacheTransforms; ++CacheIdx)
				{
					const int32 TransformIdx = EvaluatedResult.ParticleIndices[CacheIdx];
					RestTransforms[TransformIdx] = EvaluatedResult.Transform[CacheIdx] * ActorToComponent;
				}

				// Set any broken clusters to identity so that they no longer influence child transforms.
				TArray<int32> ReleaseIndices = GatherAllBreaksUpToTime(InCache, InTime);
				const TManagedArray<int32>& Parent = GeometryCollection->Parent;
				for (int32 ReleaseIdx : ReleaseIndices)
				{
					int32 ClusterIdx = Parent[ReleaseIdx];
					if (ClusterIdx > INDEX_NONE)
					{
						RestTransforms[ClusterIdx] = FTransform::Identity;
					}
				}

				Comp->SetRestState(MoveTemp(RestTransforms));
			}
		}
	}

	bool FGeometryCollectionCacheAdapter::InitializeForRecord(UPrimitiveComponent* InComponent, UChaosCache* InCache)
	{
		EnsureIsInGameThreadContext();
		
		UGeometryCollectionComponent*    Comp     = CastChecked<UGeometryCollectionComponent>(InComponent);
		FGeometryCollectionPhysicsProxy* Proxy    = Comp->GetPhysicsProxy();

		if (!Proxy)
		{
			return false;
		}

		const FSimulationParameters& SimulationParameters = Proxy->GetSimParameters();

		Chaos::FPhysicsSolver* Solver = Proxy->GetSolver<Chaos::FPhysicsSolver>();

		if (!Solver)
		{
			return false;
		}

		// In case commands have been issued that conflict with our requirement to generate data, flush the queue
		Solver->AdvanceAndDispatch_External(0);
		Solver->WaitOnPendingTasks_External();

		// We need secondary event data to record event information into the cache
		Solver->EnqueueCommandImmediate([Solver]()
			{
				Solver->SetGenerateBreakingData(true);
				Solver->SetGenerateCollisionData(true);
				Solver->SetGenerateTrailingData(true);
			});

		
		// We only need to register event handlers once, the first time we initialize.
		if (CachedData.Num() == 0)
		{
			Chaos::FEventManager* EventManager = Solver->GetEventManager();
			if (EventManager)
			{
				if (SimulationParameters.bGenerateBreakingData)
				{ 
					EventManager->RegisterHandler<Chaos::FBreakingEventData>(Chaos::EEventType::Breaking, const_cast<FGeometryCollectionCacheAdapter*>(this), &FGeometryCollectionCacheAdapter::HandleBreakingEvents);
				}

				if (SimulationParameters.bGenerateCollisionData)
				{ 
					EventManager->RegisterHandler<Chaos::FCollisionEventData>(Chaos::EEventType::Collision, const_cast<FGeometryCollectionCacheAdapter*>(this), &FGeometryCollectionCacheAdapter::HandleCollisionEvents);
				}

				if (SimulationParameters.bGenerateTrailingData)
				{ 
					EventManager->RegisterHandler<Chaos::FTrailingEventData>(Chaos::EEventType::Trailing, const_cast<FGeometryCollectionCacheAdapter*>(this), &FGeometryCollectionCacheAdapter::HandleTrailingEvents);
				}
			}

			BreakingDataArray = nullptr;
			CollisionDataArray = nullptr;
			TrailingDataArray = nullptr;
		}

		FCachedEventData& CachedEventData = CachedData.FindOrAdd(Proxy);
		CachedEventData.ProxyBreakingDataIndices = nullptr;
		CachedEventData.ProxyCollisionDataIndices = nullptr;
		CachedEventData.ProxyTrailingDataIndices = nullptr;

		return true;
	}

	bool FGeometryCollectionCacheAdapter::InitializeForPlayback(UPrimitiveComponent* InComponent, UChaosCache* InCache, float InTime)
	{
		EnsureIsInGameThreadContext();

		if (UGeometryCollectionComponent* Comp = CastChecked<UGeometryCollectionComponent>(InComponent))
		{
			if (const UGeometryCollection* RestCollection = Comp->GetRestCollection())
			{
				// Set up initial conditions... 
				
				TArray<FTransform> InitialTransforms;
				{
					FPlaybackTickRecord TickRecord;
					TickRecord.SetLastTime(InTime);
					FCacheEvaluationContext Context(TickRecord);
					Context.bEvaluateTransform = true;
					Context.bEvaluateCurves = false;
					Context.bEvaluateEvents = false;

					// initial transforms do no need to be multiplied by MassToLocal transforms so we can pass nullptr
					FCacheEvaluationResult EvaluatedResult = InCache->Evaluate(Context, nullptr);
					const int32 NumCacheTransforms = EvaluatedResult.Transform.Num();

					// Any bone that is not explicitly set by the cache defaults to it's rest collection position.
					TSharedPtr<FGeometryCollection, ESPMode::ThreadSafe> GeometryCollection = RestCollection->GetGeometryCollection();
					const int32 NumTransforms = GeometryCollection->NumElements(FGeometryCollection::TransformGroup);

					InitialTransforms.SetNum(NumTransforms);
					for (int32 Idx = 0; Idx < NumTransforms; ++Idx)
					{
						InitialTransforms[Idx] = GeometryCollection->Transform[Idx];
					}

					for (int32 CacheIdx = 0; CacheIdx < NumCacheTransforms; ++CacheIdx)
					{
						const int32 TransformIdx = EvaluatedResult.ParticleIndices[CacheIdx];
						InitialTransforms[TransformIdx] = EvaluatedResult.Transform[CacheIdx];
					}
				}
				Comp->SetInitialTransforms(InitialTransforms);
					
				TArray<int32> ReleaseIndices = GatherAllBreaksUpToTime(InCache, InTime);
				Comp->SetInitialClusterBreaks(ReleaseIndices);

				Comp->SetDynamicState(Chaos::EObjectStateType::Kinematic);
		
				// ...then initialize the proxy and particles.
				Comp->SetSimulatePhysics(true);

				return true;
			}
		}

		return false;
	}

	TArray<int32> FGeometryCollectionCacheAdapter::GatherAllBreaksUpToTime(UChaosCache* InCache, float InTime) const
	{
		// Evaluate all breaking event that have occured from the beginning of the cache up to the specified time.
		TArray<int32> ReleaseIndices;

		FPlaybackTickRecord TickRecord;
		TickRecord.SetLastTime(0.0);
		TickRecord.SetDt(InTime);
		FCacheEvaluationContext Context(TickRecord);
		Context.bEvaluateTransform = false;
		Context.bEvaluateCurves = false;
		Context.bEvaluateEvents = true;

		// passing nullptr for the MassToLocal as we do not need to evaluate the transforms
		FCacheEvaluationResult EvaluatedResult = InCache->Evaluate(Context, nullptr);
		const TArray<FCacheEventHandle>* EnableEvents = EvaluatedResult.Events.Find(FEnableStateEvent::EventName);
		if (EnableEvents)
		{
			ReleaseIndices.Reserve(EnableEvents->Num());
			for (const FCacheEventHandle& Handle : *EnableEvents)
			{
				if (FEnableStateEvent* Event = Handle.Get<FEnableStateEvent>())
				{
					if (Event->bEnable)
					{
						ReleaseIndices.Add(Event->Index);
					}
				}
			}
		}

		return ReleaseIndices;
	}

	void FGeometryCollectionCacheAdapter::HandleBreakingEvents(const Chaos::FBreakingEventData& Event)
	{
		EnsureIsInGameThreadContext();
		
		BreakingDataArray = &Event.BreakingData.AllBreakingsArray;

		for (TPair<IPhysicsProxyBase*, FCachedEventData>& Data : CachedData)
		{
			if (Event.PhysicsProxyToBreakingIndices.PhysicsProxyToIndicesMap.Contains(Data.Key))
			{
				Data.Value.ProxyBreakingDataIndices = &Event.PhysicsProxyToBreakingIndices.PhysicsProxyToIndicesMap[Data.Key];
			}
			else
			{
				Data.Value.ProxyBreakingDataIndices = nullptr;
			}
		}
	}

	void FGeometryCollectionCacheAdapter::HandleCollisionEvents(const Chaos::FCollisionEventData& Event)
	{
		EnsureIsInGameThreadContext();
		
		CollisionDataArray = &Event.CollisionData.AllCollisionsArray;

		for (TPair<IPhysicsProxyBase*, FCachedEventData>& Data : CachedData)
		{
			if (Event.PhysicsProxyToCollisionIndices.PhysicsProxyToIndicesMap.Contains(Data.Key))
			{
				Data.Value.ProxyCollisionDataIndices = &Event.PhysicsProxyToCollisionIndices.PhysicsProxyToIndicesMap[Data.Key];
			}
			else
			{
				Data.Value.ProxyCollisionDataIndices = nullptr;
			}
		}
	}

	void FGeometryCollectionCacheAdapter::HandleTrailingEvents(const Chaos::FTrailingEventData& Event)
	{
		EnsureIsInGameThreadContext();
		
		TrailingDataArray = &Event.TrailingData.AllTrailingsArray;

		for (TPair<IPhysicsProxyBase*, FCachedEventData>& Data : CachedData)
		{
			if (Event.PhysicsProxyToTrailingIndices.PhysicsProxyToIndicesMap.Contains(Data.Key))
			{
				Data.Value.ProxyTrailingDataIndices = &Event.PhysicsProxyToTrailingIndices.PhysicsProxyToIndicesMap[Data.Key];
			}
			else
			{
				Data.Value.ProxyTrailingDataIndices = nullptr;
			}
		}
	}

}    // namespace Chaos
