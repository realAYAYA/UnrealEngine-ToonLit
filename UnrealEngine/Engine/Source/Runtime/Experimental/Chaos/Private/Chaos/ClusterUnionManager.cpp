// Copyright Epic Games, Inc. All Rights Reserved.

#include "Chaos/ClusterUnionManager.h"

#include "Chaos/CastingUtilities.h"
#include "Chaos/GeometryQueries.h"
#include "Chaos/ImplicitObjectUnion.h"
#include "Chaos/PBDRigidClustering.h"
#include "Chaos/PBDRigidClusteringAlgo.h"
#include "Chaos/PBDRigidsEvolution.h"
#include "Chaos/PBDRigidsEvolutionGBF.h"
#include "ChaosStats.h"

namespace Chaos
{
	namespace
	{
		FRigidTransform3 GetParticleRigidFrameInClusterUnion(FPBDRigidParticleHandle* Child, const FRigidTransform3& ClusterWorldTM)
		{
			FRigidTransform3 Frame = FRigidTransform3::Identity;

			if (FPBDRigidClusteredParticleHandle* ClusterChild = Child->CastToClustered(); ClusterChild && ClusterChild->IsChildToParentLocked())
			{
				Frame = ClusterChild->ChildToParent();
			}
			else
			{
				const FRigidTransform3 ChildWorldTM(Child->X(), Child->R());
				Frame = ChildWorldTM.GetRelativeTransform(ClusterWorldTM);
			}

			return Frame;
		}

		void AddParticleToConnectionGraph(FRigidClustering& Clustering, FClusterUnion& ClusterUnion, FPBDRigidParticleHandle* Particle)
		{
			if (!ClusterUnion.ChildParticles.IsEmpty() && ClusterUnion.SharedGeometry->GetType() == ImplicitObjectType::Union)
			{
				constexpr FReal kAddThickness = 5.0;
				const FRigidTransform3 ClusterWorldTM(ClusterUnion.InternalCluster->X(), ClusterUnion.InternalCluster->R());

				// Use the acceleration structure of the cluster union itself to make finding overlaps easy.
				const FImplicitObjectUnion& ShapeUnion = ClusterUnion.SharedGeometry->GetObjectChecked<FImplicitObjectUnion>();
				const FRigidTransform3 FromTransform = GetParticleRigidFrameInClusterUnion(Particle, ClusterWorldTM);
				FAABB3 ParticleLocalBounds = Particle->SharedGeometry()->BoundingBox().TransformedAABB(FromTransform);
				ParticleLocalBounds.Thicken(kAddThickness);

				ShapeUnion.VisitOverlappingLeafObjects(
					ParticleLocalBounds,
					[&Clustering, &ClusterUnion, &FromTransform, Particle, kAddThickness](const FImplicitObject* ToGeom, const FRigidTransform3& ToTransform, const int32 RootObjectIndex, const int32 ObjectIndex, const int32 LeafObjectIndex)
					{
						check(ToGeom != nullptr);
						// RootObjectIndex is the index in the cluster union.
						// NOTE: This will need to be re-thought if we ever decide to make the mapping between child shapes and child particles not 1-to-1 (e.g. if we ever attempt to simplify the cluster union shape).

						// Ignore intersections against the same particle since we just added the particle (potentially) into the geometry.
						if (Particle == ClusterUnion.ChildParticles[RootObjectIndex])
						{
							return;
						}

						FAABB3 ToAABB = ToGeom->CalculateTransformedBounds(ToTransform);
						ToAABB.Thicken(kAddThickness);


						bool bOverlap = false;
						if (ToGeom->GetType() == ImplicitObjectType::LevelSet)
						{
							const FShapesArray& AllFromShapes = Particle->ShapesArray();
							for (int32 FromIndex = 0; FromIndex < AllFromShapes.Num(); ++FromIndex)
							{
								if (AllFromShapes[FromIndex])
								{
									const FPerShapeData& FromShape = *AllFromShapes[FromIndex];
									if (const FImplicitObject* FromGeom = FromShape.GetGeometry().Get())
									{
										FAABB3 FromAABB = FromGeom->CalculateTransformedBounds(FromTransform);
										FromAABB.Thicken(kAddThickness);

										// First sanity check to see if the two shape AABB's intersect.
										if (FromAABB.Intersects(ToAABB))
										{
											// level set have missing implementation for some of the shapes at the moment 
											// for now we stop at the AABB test 
											bOverlap = true;
											break;
										}
									}
								}
							}
						}
						else
						{
							bOverlap = Utilities::CastHelper(*ToGeom, ToTransform,
								[Particle, &FromTransform, &ToAABB, kAddThickness](const auto& ToGeomDowncast, const FRigidTransform3& FinalToTransform)
								{
									const FShapesArray& AllFromShapes = Particle->ShapesArray();
									for (int32 FromIndex = 0; FromIndex < AllFromShapes.Num(); ++FromIndex)
									{
										if (!AllFromShapes[FromIndex])
										{
											continue;
										}

										const FPerShapeData& FromShape = *AllFromShapes[FromIndex];
										const FImplicitObject* FromGeom = FromShape.GetGeometry().Get();

										if (!FromGeom)
										{
											continue;
										}

										FAABB3 FromAABB = FromGeom->CalculateTransformedBounds(FromTransform);
										FromAABB.Thicken(kAddThickness);

										// First sanity check to see if the two shape AABB's intersect.
										if (!FromAABB.Intersects(ToAABB))
										{
											continue;
										}

										// Now do a more accurate overlap check between the two shapes.
										// Note that passing MTD here is critical as it gets us a more accurate check for some reason...
										FMTDInfo MTDInfo;
										if (OverlapQuery(*FromGeom, FromTransform, ToGeomDowncast, FinalToTransform, kAddThickness, &MTDInfo))
										{
											return true;
										}
										else
										{
											continue;
										}
									}

									return false;
								});
						}
						if (bOverlap)
						{
							Clustering.CreateNodeConnection(Particle, ClusterUnion.ChildParticles[RootObjectIndex]);
						}
					}
				);
			}
		}

		void GenerateConnectionGraph(FRigidClustering& Clustering, FClusterUnion& ClusterUnion)
		{
			Clustering.ClearConnectionGraph(ClusterUnion.InternalCluster);
			for (FPBDRigidParticleHandle* ChildParticle : ClusterUnion.ChildParticles)
			{
				AddParticleToConnectionGraph(Clustering, ClusterUnion, ChildParticle);
			}
		}
	}

	FClusterUnionManager::FClusterUnionManager(FRigidClustering& InClustering, FPBDRigidsEvolutionGBF& InEvolution)
		: MClustering(InClustering)
		, MEvolution(InEvolution)
	{
	}

	DECLARE_CYCLE_STAT(TEXT("FClusterUnionManager::CreateNewClusterUnion"), STAT_CreateNewClusterUnion, STATGROUP_Chaos);
	FClusterUnionIndex FClusterUnionManager::CreateNewClusterUnion(const FClusterCreationParameters& Parameters, const FClusterUnionCreationParameters& ClusterUnionParameters)
	{
		SCOPE_CYCLE_COUNTER(STAT_CreateNewClusterUnion);
		FClusterUnionIndex NewIndex = ClaimNextUnionIndex();
		check(NewIndex > 0);

		FClusterUnion NewUnion;
		NewUnion.InternalIndex = NewIndex;
		NewUnion.ExplicitIndex = ClusterUnionParameters.ExplicitIndex;
		NewUnion.SharedGeometry = ForceRecreateClusterUnionSharedGeometry(NewUnion);
		NewUnion.InternalCluster = MClustering.CreateClusterParticle(-NewIndex, {}, Parameters, NewUnion.SharedGeometry, nullptr, ClusterUnionParameters.UniqueIndex);
		NewUnion.Parameters = Parameters;
		NewUnion.ClusterUnionParameters = ClusterUnionParameters;

		// Some parameters aren't relevant after creation.
		NewUnion.ClusterUnionParameters.UniqueIndex = nullptr;

		if (ensure(NewUnion.InternalCluster != nullptr))
		{
			NewUnion.InternalCluster->SetInternalCluster(true);
			if (AccelerationStructureSplitStaticAndDynamic == 1)
			{
				NewUnion.InternalCluster->SetSpatialIdx(FSpatialAccelerationIdx{ 0, 1 });
			}
			else
			{
				NewUnion.InternalCluster->SetSpatialIdx(FSpatialAccelerationIdx{ 0, 0 });
			}
			// No bounds for now since we don't have particles. When/if we do get particles later, updating the
			// geometry should switch this flag back on.
			NewUnion.InternalCluster->SetHasBounds(false);
		}
		MEvolution.GetIslandManager().RemoveParticle(NewUnion.InternalCluster);
		MEvolution.DisableParticle(NewUnion.InternalCluster);

		ClusterUnions.Add(NewIndex, NewUnion);
		return NewIndex;
	}

	DECLARE_CYCLE_STAT(TEXT("FClusterUnionManager::DestroyClusterUnion"), STAT_DestroyClusterUnion, STATGROUP_Chaos);
	void FClusterUnionManager::DestroyClusterUnion(FClusterUnionIndex Index)
	{
		SCOPE_CYCLE_COUNTER(STAT_DestroyClusterUnion);

		if (FClusterUnion* ClusterUnion = FindClusterUnion(Index))
		{
			// Need to actually remove the particles and set them back into a simulatable state.
			// We need a clean removal here just in case the cluster union is actually being destroyed 
			// on the game thread prior to its children (which would live on another actor).
			// 
			// Note that we need to make a copy of the array here since the children list will be modified by the HandleRemoveOperation.
			// However, the function does not expect that the input array will change.
			TArray<FPBDRigidParticleHandle*> ChildrenCopy = ClusterUnion->ChildParticles;
			HandleRemoveOperation(Index, ChildrenCopy, EClusterUnionOperationTiming::Never);
			ClusterUnion->ChildParticles.Empty();
			MClustering.DestroyClusterParticle(ClusterUnion->InternalCluster);

			if (ClusterUnion->ExplicitIndex != INDEX_NONE)
			{
				ExplicitIndexMap.Remove(ClusterUnion->ExplicitIndex);
				PendingExplicitIndexOperations.Remove(ClusterUnion->ExplicitIndex);
			}
			ReusableIndices.Add(Index);
			PendingClusterIndexOperations.Remove(Index);
			ClusterUnions.Remove(Index);
		}
	}

	DECLARE_CYCLE_STAT(TEXT("FClusterUnionManager::ForceRecreateClusterUnionSharedGeometry"), STAT_ForceRecreateClusterUnionSharedGeometry, STATGROUP_Chaos);
	TSharedPtr<FImplicitObject, ESPMode::ThreadSafe> FClusterUnionManager::ForceRecreateClusterUnionSharedGeometry(const FClusterUnion& Union)
	{
		SCOPE_CYCLE_COUNTER(STAT_ForceRecreateClusterUnionSharedGeometry);
		if (Union.ChildParticles.IsEmpty() || !Union.InternalCluster)
		{
			return MakeShared<FImplicitObjectUnionClustered>();
		}

		// TODO: Can we do something better than a union?
		const FRigidTransform3 ClusterWorldTM(Union.InternalCluster->X(), Union.InternalCluster->R());
		TArray<TUniquePtr<FImplicitObject>> Objects;
		Objects.Reserve(Union.ChildParticles.Num());

		for (FPBDRigidParticleHandle* Child : Union.ChildParticles)
		{
			const FRigidTransform3 Frame = GetParticleRigidFrameInClusterUnion(Child, ClusterWorldTM);
			if (Child->Geometry())
			{
				Objects.Add(TUniquePtr<FImplicitObject>(CreateTransformGeometryForClusterUnion<EThreadContext::Internal>(Child, Frame)));
			}
		}

		FImplicitObjectUnion* NewGeometry = new FImplicitObjectUnion(MoveTemp(Objects));
		NewGeometry->SetAllowBVH(true);

		return TSharedPtr<FImplicitObject, ESPMode::ThreadSafe>(NewGeometry);
	}

	DECLARE_CYCLE_STAT(TEXT("FClusterUnionManager::ClaimNextUnionIndex"), STAT_ClaimNextUnionIndex, STATGROUP_Chaos);
	FClusterUnionIndex FClusterUnionManager::ClaimNextUnionIndex()
	{
		SCOPE_CYCLE_COUNTER(STAT_ClaimNextUnionIndex);
		if (ReusableIndices.IsEmpty())
		{
			return NextAvailableUnionIndex++;
		}
		else
		{
			return ReusableIndices.Pop(false);
		}
	}

	DECLARE_CYCLE_STAT(TEXT("FClusterUnionManager::AddPendingExplicitIndexOperation"), STAT_AddPendingExplicitIndexOperation, STATGROUP_Chaos);
	void FClusterUnionManager::AddPendingExplicitIndexOperation(FClusterUnionExplicitIndex Index, EClusterUnionOperation Op, const TArray<FPBDRigidParticleHandle*>& Particles)
	{
		check(Op != EClusterUnionOperation::UpdateChildToParent);
		SCOPE_CYCLE_COUNTER(STAT_AddPendingExplicitIndexOperation);
		AddPendingOperation(PendingExplicitIndexOperations, Index, Op, Particles);
	}

	DECLARE_CYCLE_STAT(TEXT("FClusterUnionManager::AddPendingClusterIndexOperation"), STAT_AddPendingClusterIndexOperation, STATGROUP_Chaos);
	void FClusterUnionManager::AddPendingClusterIndexOperation(FClusterUnionIndex Index, EClusterUnionOperation Op, const TArray<FPBDRigidParticleHandle*>& Particles)
	{
		check(Op != EClusterUnionOperation::UpdateChildToParent);
		SCOPE_CYCLE_COUNTER(STAT_AddPendingClusterIndexOperation);
		AddPendingOperation(PendingClusterIndexOperations, Index, Op, Particles);
	}

	DECLARE_CYCLE_STAT(TEXT("FClusterUnionManager::FlushPendingOperations"), STAT_FlushPendingOperations, STATGROUP_Chaos);
	void FClusterUnionManager::FlushPendingOperations()
	{
		SCOPE_CYCLE_COUNTER(STAT_FlushPendingOperations);
		if (!PendingExplicitIndexOperations.IsEmpty())
		{
			// Go through every explicit index operation and convert them into a normal cluster index operation.
			// This could be made more efficient but shouldn't happen enough for it to really matter.
			for (const TPair<FClusterUnionExplicitIndex, FClusterOpMap>& OpMap : PendingExplicitIndexOperations)
			{
				const FClusterUnionIndex UnionIndex = GetOrCreateClusterUnionIndexFromExplicitIndex(OpMap.Key);
				for (const TPair<EClusterUnionOperation, TArray<FPBDRigidParticleHandle*>>& Op : OpMap.Value)
				{
					AddPendingClusterIndexOperation(UnionIndex, Op.Key, Op.Value);
				}
			}
			PendingExplicitIndexOperations.Empty();
		}

		if (!PendingClusterIndexOperations.IsEmpty())
		{
			for (TPair<FClusterUnionIndex, FClusterOpMap>& OpMap : PendingClusterIndexOperations)
			{
				// Is this sort necessary? Better to be safe than sorry. Since we need to guarantee that the UpdateChildToParent happens after add.
				OpMap.Value.KeyStableSort(
					[](EClusterUnionOperation A, EClusterUnionOperation B)
					{
						return static_cast<int32>(A) < static_cast<int32>(B);
					}
				);

				for (TPair<EClusterUnionOperation, TArray<FPBDRigidParticleHandle*>>& Op : OpMap.Value)
				{
					switch (Op.Key)
					{
					case EClusterUnionOperation::Add:
					case EClusterUnionOperation::AddReleased:
						HandleAddOperation(OpMap.Key, Op.Value, Op.Key == EClusterUnionOperation::AddReleased);
						break;
					case EClusterUnionOperation::Remove:
						HandleRemoveOperation(OpMap.Key, Op.Value, EClusterUnionOperationTiming::Defer);
						break;
					case EClusterUnionOperation::UpdateChildToParent:
						HandleUpdateChildToParentOperation(OpMap.Key, Op.Value);
						break;
					}
				}
			}
			PendingClusterIndexOperations.Empty();
		}

		HandleDeferredClusterUnionUpdateProperties();
	}

	FClusterUnion* FClusterUnionManager::FindClusterUnionFromExplicitIndex(FClusterUnionExplicitIndex Index)
	{
		FClusterUnionIndex* ClusterIndex = ExplicitIndexMap.Find(Index);
		if (ClusterIndex == nullptr)
		{
			return nullptr;
		}

		return FindClusterUnion(*ClusterIndex);
	}

	FClusterUnion* FClusterUnionManager::FindClusterUnion(FClusterUnionIndex Index)
	{
		return ClusterUnions.Find(Index);
	}

	FClusterUnion* FClusterUnionManager::FindClusterUnionFromParticle(FPBDRigidParticleHandle* Particle)
	{
		FClusterUnionIndex ClusterUnionIndex = FindClusterUnionIndexFromParticle(Particle);
		return FindClusterUnion(ClusterUnionIndex);
	}

	DECLARE_CYCLE_STAT(TEXT("FClusterUnionManager::HandleAddOperation"), STAT_HandleAddOperation, STATGROUP_Chaos);
	void FClusterUnionManager::HandleAddOperation(FClusterUnionIndex ClusterIndex, const TArray<FPBDRigidParticleHandle*>& Particles, bool bReleaseClustersFirst)
	{
		SCOPE_CYCLE_COUNTER(STAT_HandleAddOperation);
		FClusterUnion* Cluster = ClusterUnions.Find(ClusterIndex);
		if (!Cluster)
		{
			return;
		}

		// If we're adding particles to a cluster we need to first make sure they're not part of any other cluster.
		// Book-keeping might get a bit odd if we try to add a particle to a new cluster and then only later remove the particle from its old cluster.
		HandleRemoveOperationWithClusterLookup(Particles, EClusterUnionOperationTiming::Defer);

		TGuardValue_Bitfield_Cleanup<TFunction<void()>> Cleanup(
			[this, OldGenerateClusterBreaking=MClustering.GetDoGenerateBreakingData()]() {
				MClustering.SetGenerateClusterBreaking(OldGenerateClusterBreaking);
			}
		);
		MClustering.SetGenerateClusterBreaking(false);

		// If a physics proxy was set already on the cluster we want to make sure that doesn't change.
		// This is needed to eventually be able to introduce a new physics proxy that gets attached to the
		// cluster union particle so that it can communicate with the game thread.
		IPhysicsProxyBase* OldProxy = Cluster->InternalCluster->PhysicsProxy();
		EPhysicsProxyType OldProxyType = OldProxy ? OldProxy->GetType() : EPhysicsProxyType::NoneType;

		// If we're a new cluster, we need to determine whether to start the cluster in a sleeping or dynamic state.
		// Only stay sleeping if all the particles we add are also sleeping.
		const bool bIsNewCluster = Cluster->ChildParticles.IsEmpty();
		bool bIsSleeping = true;
		bool bIsAnchored = false;

		// Use a set to guarantee that there's no duplicates. This is necessary since further removal operations assume that the particle is only in here once.
		const TSet<FPBDRigidParticleHandle*> ParticleSet{ Particles };

		TArray<FPBDRigidParticleHandle*> FinalParticlesToAdd;
		FinalParticlesToAdd.Reserve(ParticleSet.Num());

		// This is only relevant when bReleaseClustersFirst=true. This is used to be able to
		// properly notify the parent cluster about its child proxies.
		TMap<FPBDRigidParticleHandle*, FPBDRigidParticleHandle*> ChildToParentMap;

		
		for (FPBDRigidParticleHandle* Handle : ParticleSet)
		{
			if (!Handle)
			{
				continue;
			}

			bIsSleeping &= Handle->ObjectState() == EObjectStateType::Sleeping;

			if (FPBDRigidClusteredParticleHandle* ClusterHandle = Handle->CastToClustered())
			{
				if (bReleaseClustersFirst)
				{
					TSet<FPBDRigidParticleHandle*> Children = MClustering.ReleaseClusterParticles(ClusterHandle, true);
					FinalParticlesToAdd.Append(Children.Array());

					for (FPBDRigidParticleHandle* Child : Children)
					{
						ChildToParentMap.Add(Child, ClusterHandle);
					}
				}
				else
				{
					FinalParticlesToAdd.Add(Handle);
				}

				bIsAnchored |= ClusterHandle->IsAnchored();
			}
			else
			{
				FinalParticlesToAdd.Add(Handle);
			}
		}

		if (FinalParticlesToAdd.IsEmpty())
		{
			return;
		}

		Cluster->ChildParticles.Append(FinalParticlesToAdd);
		for (FPBDRigidParticleHandle* Particle : FinalParticlesToAdd)
		{
			if (!bIsNewCluster)
			{
				Cluster->PendingConnectivityOperations.Add({Particle, EClusterUnionConnectivityOperation ::Add});
			}

			if (!Cluster->ChildProperties.Contains(Particle))
			{
				FClusterUnionParticleProperties Properties;
				Properties.bIsAuxiliaryParticle = false;
				Cluster->ChildProperties.Add(Particle, Properties);
			}

			// Need to set a temporary ChildToParent on the particle. If this ChildToParent doesn't exist, and the particle goes through the removal process BEFORE
			// the deferred cluster union update properties is called, the position of the particle will get reset to the position of the cluster union.
			if (FPBDRigidClusteredParticleHandle* ClusterParticle = Particle->CastToClustered())
			{
				const FRigidTransform3 ClusterWorldTM(Cluster->InternalCluster->X(), Cluster->InternalCluster->R());
				const FRigidTransform3 Frame = GetParticleRigidFrameInClusterUnion(Particle, ClusterWorldTM);
				ClusterParticle->SetChildToParent(Frame);
			}
		}

		MClustering.AddParticlesToCluster(Cluster->InternalCluster, FinalParticlesToAdd, ChildToParentMap);

		// For all the particles that have been added to the cluster we need to set their parent proxy to the
		// cluster's proxy if it exists. We need the proxy type check because for non-cluster union proxy backed unions,
		// the cluster union's proxy will be the proxy of the most recently added particle.
		if (OldProxy && OldProxyType == EPhysicsProxyType::ClusterUnionProxy)
		{
			for (FPBDRigidParticleHandle* Particle : FinalParticlesToAdd)
			{
				if (Particle && Particle->PhysicsProxy())
				{
					Particle->PhysicsProxy()->SetParentProxy(OldProxy);
				}
			}
		}

		if (bIsNewCluster && bIsAnchored && !Cluster->bAnchorLock)
		{
			// The anchored flag is taken care of in UpdateKinematicProperties so it must be set before that is called.
			Cluster->InternalCluster->SetIsAnchored(true);
		}

		RequestDeferredClusterPropertiesUpdate(ClusterIndex, bIsNewCluster ? EUpdateClusterUnionPropertiesFlags::All : EUpdateClusterUnionPropertiesFlags::IncrementalGenerateConnectionGraph);

		if (OldProxy)
		{
			Cluster->InternalCluster->SetPhysicsProxy(OldProxy);
		}

		if (bIsNewCluster)
		{
			if (bIsSleeping)
			{
				MEvolution.SetParticleObjectState(Cluster->InternalCluster, Chaos::EObjectStateType::Sleeping);
			}

			MEvolution.SetPhysicsMaterial(Cluster->InternalCluster, MEvolution.GetPhysicsMaterial(FinalParticlesToAdd[0]));
		}
	}

	DECLARE_CYCLE_STAT(TEXT("FClusterUnionManager::DeferredClusterUnionUpdate"), STAT_DeferredClusterUnionUpdate, STATGROUP_Chaos);
	void FClusterUnionManager::DeferredClusterUnionUpdate(FClusterUnion& Union, EUpdateClusterUnionPropertiesFlags Flags)
	{
		SCOPE_CYCLE_COUNTER(STAT_DeferredClusterUnionUpdate);

		UpdateAllClusterUnionProperties(Union, Flags);

		if (!Union.ChildParticles.IsEmpty())
		{
			if (Union.InternalCluster->Disabled())
			{
				MEvolution.EnableParticle(Union.InternalCluster);
			}
			MEvolution.DirtyParticle(*Union.InternalCluster);
		}
		else
		{
			// Note that if we have 0 child particles, our implicit object union will have an invalid bounding box.
			// We must eject from the acceleration structure otherwise we risk cashes.
			MEvolution.GetIslandManager().RemoveParticle(Union.InternalCluster);
			MEvolution.DisableParticle(Union.InternalCluster);
		}
		MEvolution.GetParticles().MarkTransientDirtyParticle(Union.InternalCluster);
	}

	DECLARE_CYCLE_STAT(TEXT("FClusterUnionManager::HandleDeferredClusterUnionUpdateProperties"), STAT_HandleDeferredClusterUnionUpdateProperties, STATGROUP_Chaos);
	void FClusterUnionManager::HandleDeferredClusterUnionUpdateProperties()
	{
		SCOPE_CYCLE_COUNTER(STAT_HandleDeferredClusterUnionUpdateProperties);
		if (DeferredClusterUnionsForUpdateProperties.IsEmpty())
		{
			return;
		}

		for (const TPair<FClusterUnionIndex, EUpdateClusterUnionPropertiesFlags>& Kvp : DeferredClusterUnionsForUpdateProperties)
		{
			if (FClusterUnion* Union = FindClusterUnion(Kvp.Key))
			{
				DeferredClusterUnionUpdate(*Union, Kvp.Value);
			}
		}

		DeferredClusterUnionsForUpdateProperties.Reset();
	}

	DECLARE_CYCLE_STAT(TEXT("FClusterUnionManager::HandleRemoveOperation"), STAT_HandleRemoveOperation, STATGROUP_Chaos);
	void FClusterUnionManager::HandleRemoveOperation(FClusterUnionIndex ClusterIndex, const TArray<FPBDRigidParticleHandle*>& Particles, EClusterUnionOperationTiming UpdateClusterPropertiesTiming)
	{
		SCOPE_CYCLE_COUNTER(STAT_HandleRemoveOperation);
		FClusterUnion* Cluster = ClusterUnions.Find(ClusterIndex);
		if (!Cluster || Particles.IsEmpty())
		{
			return;
		}

		TGuardValue_Bitfield_Cleanup<TFunction<void()>> Cleanup(
			[this, OldGenerateClusterBreaking = MClustering.GetDoGenerateBreakingData()]() {
				MClustering.SetGenerateClusterBreaking(OldGenerateClusterBreaking);
			}
		);
		MClustering.SetGenerateClusterBreaking(false);

		IPhysicsProxyBase* OldProxy = Cluster->InternalCluster->PhysicsProxy();
		TArray<int32> ParticleIndicesToRemove;
		ParticleIndicesToRemove.Reserve(Particles.Num());

		for (FPBDRigidParticleHandle* Handle : Particles)
		{
			const int32 ParticleIndex = Cluster->ChildParticles.Find(Handle);
			if (ParticleIndex != INDEX_NONE)
			{
				ParticleIndicesToRemove.Add(ParticleIndex);

				// Remove the child to parent lock if it exists since it's no longer managed by the cluster union manager.
				if (FPBDRigidClusteredParticleHandle* ClusterHandle = Handle->CastToClustered())
				{
					ClusterHandle->SetChildToParentLocked(false);
				}

				// Remove the parent proxy only if it's a cluster union proxy.
				if (IPhysicsProxyBase* Proxy = Handle->PhysicsProxy(); Proxy && Proxy->GetParentProxy() && Proxy->GetParentProxy()->GetType() == EPhysicsProxyType::ClusterUnionProxy)
				{
					Proxy->SetParentProxy(nullptr);
				}

				Cluster->PendingConnectivityOperations.Add({ Handle, EClusterUnionConnectivityOperation::Remove });
			}
		}

		ParticleIndicesToRemove.Sort();
		for (int32 Index = ParticleIndicesToRemove.Num() - 1; Index >= 0; --Index)
		{
			const int32 ParticleIndex = ParticleIndicesToRemove[Index];
			Cluster->ChildProperties.Remove(Cluster->ChildParticles[ParticleIndex]);
			Cluster->ChildParticles.RemoveAtSwap(ParticleIndex);
		}

		MClustering.RemoveParticlesFromCluster(Cluster->InternalCluster, Particles);

		// Removing a particle should have no bearing on the proxy of the cluster.
		// This gets changed because we go through an internal initialization route when we update the cluster union particle's properties.
		Cluster->InternalCluster->SetPhysicsProxy(OldProxy);

		constexpr EUpdateClusterUnionPropertiesFlags RemoveUpdateFlags = EUpdateClusterUnionPropertiesFlags::IncrementalGenerateConnectionGraph | EUpdateClusterUnionPropertiesFlags::UpdateKinematicProperties;
		switch (UpdateClusterPropertiesTiming)
		{
		case EClusterUnionOperationTiming::Immediate:
			DeferredClusterUnionUpdate(*Cluster, RemoveUpdateFlags);
			break;
		case EClusterUnionOperationTiming::Defer:
			RequestDeferredClusterPropertiesUpdate(ClusterIndex, RemoveUpdateFlags);
			break;
		}		
	}

	DECLARE_CYCLE_STAT(TEXT("FClusterUnionManager::HandleRemoveOperationWithClusterLookup"), STAT_HandleRemoveOperationWithClusterLookup, STATGROUP_Chaos);
	void FClusterUnionManager::HandleRemoveOperationWithClusterLookup(const TArray<FPBDRigidParticleHandle*>& InParticles, EClusterUnionOperationTiming UpdateClusterPropertiesTiming)
	{
		SCOPE_CYCLE_COUNTER(STAT_HandleRemoveOperationWithClusterLookup);
		TMap<FClusterUnionIndex, TSet<FPBDRigidParticleHandle*>> ParticlesPerCluster;
		for (FPBDRigidParticleHandle* Particle : InParticles)
		{
			if (const int32 Index = FindClusterUnionIndexFromParticle(Particle); Index != INDEX_NONE)
			{
				ParticlesPerCluster.FindOrAdd(Index).Add(Particle);
			}
		}

		for (const TPair<FClusterUnionIndex, TSet<FPBDRigidParticleHandle*>>& Kvp : ParticlesPerCluster)
		{
			HandleRemoveOperation(Kvp.Key, Kvp.Value.Array(), UpdateClusterPropertiesTiming);
		}
	}

	DECLARE_CYCLE_STAT(TEXT("FClusterUnionManager::UpdateClusterUnionProperties"), STAT_UpdateClusterUnionProperties, STATGROUP_Chaos);
	void FClusterUnionManager::UpdateAllClusterUnionProperties(FClusterUnion& ClusterUnion, EUpdateClusterUnionPropertiesFlags Flags)
	{
		SCOPE_CYCLE_COUNTER(STAT_UpdateClusterUnionProperties);
		// Update cluster properties.
		FMatrix33 ClusterInertia(0);

		const bool bRecomputeMassOrientation = EnumHasAnyFlags(Flags, EUpdateClusterUnionPropertiesFlags::RecomputeMassOrientation) && ClusterUnion.bNeedsXRInitialization;
		TSet<FPBDRigidParticleHandle*> FullChildrenSet(ClusterUnion.ChildParticles);

		UpdateClusterMassProperties(ClusterUnion.InternalCluster, FullChildrenSet);

		// Position the internal cluster at the CoM of the children
		if (bRecomputeMassOrientation)
		{
			MoveClusterToMassOffset(ClusterUnion.InternalCluster, EMassOffsetType::EPosition);
			ClusterUnion.bNeedsXRInitialization = false;
		}

		if (EnumHasAnyFlags(Flags, EUpdateClusterUnionPropertiesFlags::UpdateKinematicProperties))
		{
			if (ClusterUnion.bAnchorLock)
			{
				// Skip UpdateKinematicPropertiesso as we won't need to look at our children for this.
				if (ClusterUnion.InternalCluster->IsAnchored())
				{
					MEvolution.SetParticleObjectState(ClusterUnion.InternalCluster, EObjectStateType::Kinematic);
				}
				else
				{
					MEvolution.SetParticleObjectState(ClusterUnion.InternalCluster, EObjectStateType::Dynamic);
					MEvolution.SetParticleKinematicTarget(ClusterUnion.InternalCluster, FKinematicTarget());
				}
			}
			else
			{
				UpdateKinematicProperties(ClusterUnion.InternalCluster, MClustering.GetChildrenMap(), MEvolution);
			}
		}

		// We must reset collisions etc on this particle since geometry will be changed
		MEvolution.InvalidateParticle(ClusterUnion.InternalCluster);

		// The recreation of the geometry must happen after the call to UpdateClusterMassProperties.
		// Creating the geometry requires knowing the relative frame between the parent cluster and the child clusters. The
		// parent transform is not set properly for a new empty cluster until UpdateClusterMassProperties is called for the first time.
		ClusterUnion.SharedGeometry = ForceRecreateClusterUnionSharedGeometry(ClusterUnion);
		UpdateGeometry(ClusterUnion.InternalCluster, FullChildrenSet, MClustering.GetChildrenMap(), ClusterUnion.SharedGeometry, ClusterUnion.Parameters);

		// TODO: Need to figure out how to do the mapping back to the child shape if we ever do shape simplification...
		if (!ClusterUnion.ChildParticles.IsEmpty() && ClusterUnion.ChildParticles.Num() == ClusterUnion.InternalCluster->ShapesArray().Num())
		{
			for (int32 ChildIndex = 0; ChildIndex < ClusterUnion.ChildParticles.Num(); ++ChildIndex)
			{
				// TODO: Is there a better way to do this merge?
				const TUniquePtr<Chaos::FPerShapeData>& TemplateShape = ClusterUnion.ChildParticles[ChildIndex]->ShapesArray()[0];
				const TUniquePtr<Chaos::FPerShapeData>& ShapeData = ClusterUnion.InternalCluster->ShapesArray()[ChildIndex];
				if (ShapeData && TemplateShape)
				{
					{
						FCollisionData Data = TemplateShape->GetCollisionData();
						Data.UserData = nullptr;
						ShapeData->SetCollisionData(Data);
					}

					{
						FCollisionFilterData Data = TemplateShape->GetQueryData();
						Data.Word0 = ClusterUnion.ClusterUnionParameters.ActorId;
						ShapeData->SetQueryData(Data);
					}

					{
						FCollisionFilterData Data = TemplateShape->GetSimData();
						Data.Word0 = 0;
						Data.Word2 = ClusterUnion.ClusterUnionParameters.ComponentId;
						ShapeData->SetSimData(Data);
					}
				}
			}
		}

		if(EnumHasAnyFlags(Flags, EUpdateClusterUnionPropertiesFlags::ForceGenerateConnectionGraph))
		{
			GenerateConnectionGraph(MClustering, ClusterUnion);
			ClusterUnion.PendingConnectivityOperations.Empty();
		}
		else if (EnumHasAnyFlags(Flags, EUpdateClusterUnionPropertiesFlags::IncrementalGenerateConnectionGraph))
		{
			FlushIncrementalConnectivityGraphOperations(ClusterUnion);
		}

		for (FPBDRigidClusteredParticleHandle* ChildParticle : PendingParticlesToUndoChildToParentLock)
		{
			if (ChildParticle)
			{
				ChildParticle->SetChildToParentLocked(false);
			}
		}

		PendingParticlesToUndoChildToParentLock.Empty();
	}

	DECLARE_CYCLE_STAT(TEXT("FClusterUnionManager::FlushIncrementalConnectivityGraphOperations"), STAT_FlushIncrementalConnectivityGraphOperations, STATGROUP_Chaos);
	void FClusterUnionManager::FlushIncrementalConnectivityGraphOperations(FClusterUnion& ClusterUnion)
	{
		SCOPE_CYCLE_COUNTER(STAT_FlushIncrementalConnectivityGraphOperations);
		for (const TPair<FPBDRigidParticleHandle*, EClusterUnionConnectivityOperation>& Op : ClusterUnion.PendingConnectivityOperations)
		{
			if (Op.Value == EClusterUnionConnectivityOperation::Add)
			{
				AddParticleToConnectionGraph(MClustering, ClusterUnion, Op.Key);
			}
			else if (Op.Value == EClusterUnionConnectivityOperation::Remove)
			{
				MClustering.RemoveNodeConnections(Op.Key);
			}
		}

		ClusterUnion.PendingConnectivityOperations.Empty();
	}

	DECLARE_CYCLE_STAT(TEXT("FClusterUnionManager::GetOrCreateClusterUnionIndexFromExplicitIndex"), STAT_GetOrCreateClusterUnionIndexFromExplicitIndex, STATGROUP_Chaos);
	FClusterUnionIndex FClusterUnionManager::GetOrCreateClusterUnionIndexFromExplicitIndex(FClusterUnionExplicitIndex InIndex)
	{
		SCOPE_CYCLE_COUNTER(STAT_GetOrCreateClusterUnionIndexFromExplicitIndex);
		FClusterUnionIndex* OutIndex = ExplicitIndexMap.Find(InIndex);
		if (OutIndex != nullptr)
		{
			return *OutIndex;
		}

		FClusterUnionCreationParameters OtherParams;
		OtherParams.ExplicitIndex = InIndex;

		FClusterUnionIndex NewIndex = CreateNewClusterUnion(DefaultClusterCreationParameters(), OtherParams);
		ExplicitIndexMap.Add(InIndex, NewIndex);
		return NewIndex;
	}

	FClusterCreationParameters FClusterUnionManager::DefaultClusterCreationParameters() const
	{
		 FClusterCreationParameters Parameters{ 0.3f, 100, false, FRigidClustering::ShouldUnionsHaveCollisionParticles() };
		 Parameters.ConnectionMethod = MClustering.GetClusterUnionConnectionType();
		 return Parameters;
	}

	DECLARE_CYCLE_STAT(TEXT("FClusterUnionManager::FindClusterUnionIndexFromParticle"), STAT_FindClusterUnionIndexFromParticle, STATGROUP_Chaos);
	FClusterUnionIndex FClusterUnionManager::FindClusterUnionIndexFromParticle(FPBDRigidParticleHandle* ChildParticle)
	{
		SCOPE_CYCLE_COUNTER(STAT_FindClusterUnionIndexFromParticle);
		if (!ChildParticle)
		{
			return INDEX_NONE;
		}

		if (FPBDRigidClusteredParticleHandle* ChildClustered = ChildParticle->CastToClustered())
		{
			if (FPBDRigidClusteredParticleHandle* Parent = ChildClustered->Parent())
			{
				// Recursion should be fine here since the hierarchy should be fairly shallow.
				return FindClusterUnionIndexFromParticle(Parent);
			}

			// The only other check to do here is to see if this is the cluster union particle itself.
			const int32 ClusterGroupIndex = ChildClustered->ClusterGroupIndex();
			if (FClusterUnion* ClusterUnion = FindClusterUnion(FMath::Abs(ClusterGroupIndex)))
			{
				// This is a sanity check which may or may not be necessary.
				// TODO: Can probably be safely removed once we deprecate cluster group indices on GCs.
				if (ClusterUnion->InternalCluster == ChildClustered)
				{
					return ClusterUnion->InternalIndex;
				}
			}
		}
		
		return INDEX_NONE;
	}

	bool FClusterUnionManager::IsClusterUnionParticle(FPBDRigidClusteredParticleHandle* Particle)
	{
		FClusterUnionIndex UnionIndex = FindClusterUnionIndexFromParticle(Particle);
		if (FClusterUnion* Union = FindClusterUnion(UnionIndex))
		{
			return Union->InternalCluster == Particle;
		}
		return false;
	}

	DECLARE_CYCLE_STAT(TEXT("FClusterUnionManager::UpdateClusterUnionParticlesChildToParent"), STAT_UpdateClusterUnionParticlesChildToParent, STATGROUP_Chaos);
	void FClusterUnionManager::UpdateClusterUnionParticlesChildToParent(FClusterUnionIndex Index, const TArray<FPBDRigidParticleHandle*>& Particles, const TArray<FTransform>& ChildToParent, bool bLock)
	{
		SCOPE_CYCLE_COUNTER(STAT_UpdateClusterUnionParticlesChildToParent);
		AddPendingOperation(PendingClusterIndexOperations, Index, EClusterUnionOperation::UpdateChildToParent, Particles);

		for (int32 InputIndex = 0; InputIndex < Particles.Num() && InputIndex < ChildToParent.Num(); ++InputIndex)
		{
			if (!Particles[InputIndex])
			{
				continue;
			}

			if (FPBDRigidClusteredParticleHandle* ClusterParticle = Particles[InputIndex]->CastToClustered())
			{
				FClusterUnionChildToParentUpdate& Update = PendingChildToParentUpdates.FindOrAdd(ClusterParticle);
				// If the current existing update wants to lock and we're not also locking, we can discard this new update.
				if (Update.bLock && !bLock)
				{
					continue;
				}
				Update.ChildToParent = ChildToParent[InputIndex];
				Update.bLock = bLock;
			}
		}
	}

	DECLARE_CYCLE_STAT(TEXT("FClusterUnionManager::HandleUpdateChildToParentOperation"), STAT_HandleUpdateChildToParentOperation, STATGROUP_Chaos);
	void FClusterUnionManager::HandleUpdateChildToParentOperation(FClusterUnionIndex ClusterIndex, const TArray<FPBDRigidParticleHandle*>& Particles)
	{
		SCOPE_CYCLE_COUNTER(STAT_UpdateClusterUnionParticlesChildToParent);
		FClusterUnion* ClusterUnion = ClusterUnions.Find(ClusterIndex);
		if (!ClusterUnion || Particles.IsEmpty())
		{
			return;
		}

		// We need to keep track of all the particles we touched here. We need this because UpdateClusterUnionParticlesChildToParent
		// is an *authoritative* update on the ChildToParent of the particle. However, UpdateAllClusterUnionProperties in certain cases
		// may try to recompute the ChildToParent using the position of the particle. To counteract this, we will force the ChildToParent
		// to be *temporarily* locked for the duration of the *next* UpdateAllClusterUnionProperties. However, unless bLock is true, we will restore the
		// lock state of the particle to what it was previously.
		for (FPBDRigidParticleHandle* Particle : Particles)
		{
			if (!ensure(Particle))
			{
				return;
			}

			const int32 ChildIndex = ClusterUnion->ChildParticles.Find(Particle->CastToRigidParticle());
			if (ChildIndex != INDEX_NONE && ClusterUnion->InternalCluster)
			{
				if (FPBDRigidClusteredParticleHandle* ChildHandle = ClusterUnion->ChildParticles[ChildIndex]->CastToClustered())
				{
					if (const FClusterUnionChildToParentUpdate* Update = PendingChildToParentUpdates.Find(ChildHandle))
					{
						const FRigidTransform3 ChildToParent = Update->ChildToParent;
						ChildHandle->SetChildToParent(ChildToParent);

						if (!Update->bLock && !ChildHandle->IsChildToParentLocked())
						{
							PendingParticlesToUndoChildToParentLock.Add(ChildHandle);
						}
						ChildHandle->SetChildToParentLocked(true);
						PendingChildToParentUpdates.Remove(ChildHandle);

						// Update the child's world transform to be consistent with its ChildToParent transform
						const FPBDRigidClusteredParticleHandle* ParentHandle = ClusterUnion->InternalCluster;
						const FRigidTransform3 ParticleToWorld = ChildToParent * FRigidTransform3(ParentHandle->X(), ParentHandle->R());
						ChildHandle->SetX(ParticleToWorld.GetTranslation());
						ChildHandle->SetP(ParticleToWorld.GetTranslation());
						ChildHandle->SetR(ParticleToWorld.GetRotation());
						ChildHandle->SetQ(ParticleToWorld.GetRotation());

						// A child to parent update needs to remove *and* add to the connectivity graph (in that order) since
						// the child to parent update might move the node so far away as to make the old connectivity edges incorrect.
						ClusterUnion->PendingConnectivityOperations.Add({ Particle, EClusterUnionConnectivityOperation::Remove });
						ClusterUnion->PendingConnectivityOperations.Add({ Particle, EClusterUnionConnectivityOperation::Add });
					}
				}
			}
		}

		RequestDeferredClusterPropertiesUpdate(ClusterIndex, EUpdateClusterUnionPropertiesFlags::IncrementalGenerateConnectionGraph);
	}

	void FClusterUnionManager::RequestDeferredClusterPropertiesUpdate(FClusterUnionIndex ClusterIndex, EUpdateClusterUnionPropertiesFlags Flags)
	{
		EUpdateClusterUnionPropertiesFlags& ExistingFlags = DeferredClusterUnionsForUpdateProperties.FindOrAdd(ClusterIndex);
		EnumAddFlags(ExistingFlags, Flags);
	}
}