// Copyright Epic Games, Inc. All Rights Reserved.

#include "Chaos/PBDRigidClusteringAlgo.h"

#include "Chaos/ErrorReporter.h"
#include "Chaos/Levelset.h"
#include "Chaos/Utilities.h"
#include "Chaos/PBDRigidClusteringCollisionParticleAlgo.h"
#include "Chaos/PBDRigidsEvolutionGBF.h"
#include "Chaos/MassProperties.h"

namespace Chaos
{
	//
    // Update Geometry PVar
	//

	int32 UseLevelsetCollision = 0;
	FAutoConsoleVariableRef CVarUseLevelsetCollision2(TEXT("p.UseLevelsetCollision"), UseLevelsetCollision, TEXT("Whether unioned objects use levelsets"));

	int32 MinLevelsetDimension = 4;
	FAutoConsoleVariableRef CVarMinLevelsetDimension2(TEXT("p.MinLevelsetDimension"), MinLevelsetDimension, TEXT("The minimum number of cells on a single level set axis"));

	int32 MaxLevelsetDimension = 20;
	FAutoConsoleVariableRef CVarMaxLevelsetDimension2(TEXT("p.MaxLevelsetDimension"), MaxLevelsetDimension, TEXT("The maximum number of cells on a single level set axis"));

	FRealSingle MinLevelsetSize = 50.f;
	FAutoConsoleVariableRef CVarLevelSetResolution2(TEXT("p.MinLevelsetSize"), MinLevelsetSize, TEXT("The minimum size on the smallest axis to use a level set"));

	int32 LevelsetGhostCells = 1;
	FAutoConsoleVariableRef CVarLevelsetGhostCells2(TEXT("p.LevelsetGhostCells"), LevelsetGhostCells, TEXT("Increase the level set grid by this many ghost cells"));

	int32 MinCleanedPointsBeforeRemovingInternals = 10;
	FAutoConsoleVariableRef CVarMinCleanedPointsBeforeRemovingInternals2(TEXT("p.MinCleanedPointsBeforeRemovingInternals"), MinCleanedPointsBeforeRemovingInternals, TEXT("If we only have this many clean points, don't bother removing internal points as the object is likely very small"));

	FRealSingle ClusterSnapDistance = 1.f;
	FAutoConsoleVariableRef CVarClusterSnapDistance2(TEXT("p.ClusterSnapDistance"), ClusterSnapDistance, TEXT(""));

	DECLARE_CYCLE_STAT(TEXT("TPBDRigidClustering<>::UpdateClusterMassProperties()"), STAT_UpdateClusterMassProperties, STATGROUP_Chaos);
	void UpdateClusterMassProperties(
		FPBDRigidClusteredParticleHandle* Parent,
		const TSet<FPBDRigidParticleHandle*>& Children)
	{
		SCOPE_CYCLE_COUNTER(STAT_UpdateClusterMassProperties);

		// NOTE: Default initializer will have zero mass
		FMassProperties ParentMass;

		// Combine mass properties of all children in parent's space
		if (Children.Num() > 0)
		{
			const FRigidTransform3 ParentTM = Parent->GetTransformXR();
			TArray<FMassProperties> ChildMasses;
			ChildMasses.Reserve(Children.Num());
			for (const FPBDRigidParticleHandle* Child : Children)
			{
				// Get the child's transform relative to the parent
				const FRigidTransform3 ChildTM = Child->GetTransformXR();
				const FRigidTransform3 ChildToParentTM = ChildTM.GetRelativeTransform(ParentTM);
				const FRigidTransform3 ChildCoM(Child->CenterOfMass(), Child->RotationOfMass());
				const FRigidTransform3 ChildCoMInParentSpace = ChildCoM * ChildToParentTM;

				// Get the child's mass properties
				FMassProperties ChildMass;
				ChildMass.Mass = Child->M();
				ChildMass.InertiaTensor = FMatrix33(Child->I());
				ChildMass.CenterOfMass = ChildCoMInParentSpace.GetTranslation();
				ChildMass.RotationOfMass = ChildCoMInParentSpace.GetRotation();
				ChildMasses.Add(ChildMass);
			}

			ParentMass = Chaos::Combine(ChildMasses);
		}

		// Set properties of particle based on combined mass properties
		// NOTE: The combine method will have diagonalized the inertia.
		const FVec3 Inertia = ParentMass.InertiaTensor.GetDiagonal();
		Parent->SetCenterOfMass(ParentMass.CenterOfMass);
		Parent->SetRotationOfMass(ParentMass.RotationOfMass);
		Parent->M() = ParentMass.Mass;
		Parent->InvM()
			= FMath::IsNearlyZero(ParentMass.Mass)
			? 0.f
			: 1.f / ParentMass.Mass;
		Parent->I() = Inertia;
		Parent->InvI()
			= (FMath::IsNearlyZero(Inertia[0]) || FMath::IsNearlyZero(Inertia[1]) || FMath::IsNearlyZero(Inertia[2]))
			? FVec3::ZeroVector
			: FReal(1) / Inertia;
	}

	void MoveClusterToMassOffset(FPBDRigidClusteredParticleHandle* Cluster, const EMassOffsetType MassOffsetTypes)
	{
		if (MassOffsetTypes | EMassOffsetType::EPosition)
		{
			Cluster->SetX(Cluster->XCom());
			Cluster->SetP(Cluster->X());
			Cluster->SetCenterOfMass(FVec3::ZeroVector);
		}

		if (MassOffsetTypes | EMassOffsetType::ERotation)
		{
			Cluster->SetR(Cluster->RCom());
			Cluster->SetQ(Cluster->R());
			Cluster->SetRotationOfMass(FQuat::Identity);
		}
	}

	DECLARE_CYCLE_STAT(TEXT("TPBDRigidClustering<>::UpdateKinematicProperties()"), STAT_UpdateKinematicProperties, STATGROUP_Chaos);
	void 
		UpdateKinematicProperties(
			Chaos::FPBDRigidParticleHandle* InParent,
			const FRigidClustering::FClusterMap& MChildren,
			FRigidClustering::FRigidEvolution& MEvolution)
	{
		SCOPE_CYCLE_COUNTER(STAT_UpdateKinematicProperties);
		typedef FPBDRigidClusteredParticleHandle* FClusterHandle;

		EObjectStateType ObjectState = EObjectStateType::Dynamic;
		check(InParent != nullptr);
		if (FClusterHandle ClusteredCurrentNode = InParent->CastToClustered())
		{
			if (MChildren.Contains(ClusteredCurrentNode) && MChildren[ClusteredCurrentNode].Num())
			{
				if (ClusteredCurrentNode->IsAnchored())
				{
					ObjectState = EObjectStateType::Kinematic;
				}
				else
				{
					// TQueue is a linked list, which has no preallocator.
					TQueue<Chaos::FPBDRigidParticleHandle*> Queue;
					for (Chaos::FPBDRigidParticleHandle* Child : MChildren[ClusteredCurrentNode])
					{
						Queue.Enqueue(Child);
					}

					Chaos::FPBDRigidParticleHandle* CurrentHandle;
					while (Queue.Dequeue(CurrentHandle) && ObjectState == EObjectStateType::Dynamic)
					{
						bool bIsAnchored = false;
						if (FClusterHandle CurrentClusterHandle = CurrentHandle->CastToClustered())
						{
							// @question : Maybe we should just store the leaf node bodies in a
							// map, that will require Memory(n*log(n))
							if (MChildren.Contains(CurrentClusterHandle))
							{
								for (Chaos::FPBDRigidParticleHandle* Child : MChildren[CurrentClusterHandle])
								{
									Queue.Enqueue(Child);
								}
							}
							
							bIsAnchored = CurrentClusterHandle->IsAnchored();
						}

						if (bIsAnchored)
						{
							ObjectState = EObjectStateType::Kinematic;
						}
						else
						{
							const EObjectStateType CurrState = CurrentHandle->ObjectState();
							if (CurrState == EObjectStateType::Kinematic)
							{
								ObjectState = EObjectStateType::Kinematic;
							}
							else if (CurrState == EObjectStateType::Static)
							{
								ObjectState = EObjectStateType::Static;
							}
						}
					}
				}

				MEvolution.SetParticleObjectState(ClusteredCurrentNode, ObjectState);
				if (ObjectState == Chaos::EObjectStateType::Dynamic)
				{
					MEvolution.SetParticleKinematicTarget(ClusteredCurrentNode, FKinematicTarget());
				}
			}
		}
	}
	

	DECLARE_CYCLE_STAT(TEXT("TPBDRigidClustering<>::UpdateGeometry"), STAT_UpdateGeometry, STATGROUP_Chaos);
	DECLARE_CYCLE_STAT(TEXT("TPBDRigidClustering<>::UpdateGeometry_GatherObjects"), STAT_UpdateGeometry_GatherObjects, STATGROUP_Chaos);
	DECLARE_CYCLE_STAT(TEXT("TPBDRigidClustering<>::UpdateGeometry_GatherPoints"), STAT_UpdateGeometry_GatherPoints, STATGROUP_Chaos);
	DECLARE_CYCLE_STAT(TEXT("TPBDRigidClustering<>::UpdateGeometry_CopyPoints"), STAT_UpdateGeometry_CopyPoints, STATGROUP_Chaos);
	DECLARE_CYCLE_STAT(TEXT("TPBDRigidClustering<>::UpdateGeometry_PointsBVH"), STAT_UpdateGeometry_PointsBVH, STATGROUP_Chaos);

	void
	UpdateGeometry(
		Chaos::FPBDRigidClusteredParticleHandle* Parent,
		const TSet<FPBDRigidParticleHandle*>& Children,
		const FRigidClustering::FClusterMap& ChildrenMap,
		TSharedPtr<Chaos::FImplicitObject, ESPMode::ThreadSafe> ProxyGeometry,
		const FClusterCreationParameters& Parameters)
	{
		SCOPE_CYCLE_COUNTER(STAT_UpdateGeometry);

		TArray<TUniquePtr<FImplicitObject>> Objects;
		TArray<TUniquePtr<FImplicitObject>> Objects2; //todo: find a better way to reuse this
		Objects.Reserve(Children.Num());
		Objects2.Reserve(Children.Num());

		const FRigidTransform3 ClusterWorldTM(Parent->X(), Parent->R());

		TArray<FVec3> OriginalPoints;
		TArray<FPBDRigidParticleHandle*> ChildParticleHandles;
		ChildParticleHandles.Reserve(Children.Num());

		const bool bUseCollisionPoints = (ProxyGeometry || Parameters.bCopyCollisionParticles) && !Parameters.CollisionParticles;
		bool bUseParticleImplicit = false;

		{ // STAT_UpdateGeometry_GatherObjects
			SCOPE_CYCLE_COUNTER(STAT_UpdateGeometry_GatherObjects);

			if (bUseCollisionPoints)
			{
				uint32 NumPoints = 0;
				for (FPBDRigidParticleHandle* Child : Children)
				{
					NumPoints += Child->CollisionParticlesSize();
				}
				OriginalPoints.Reserve(NumPoints);
			}

			for (FPBDRigidParticleHandle* Child : Children)
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

				FPBDRigidParticleHandle* UsedGeomChild = Child;
				if (Child->Geometry())
				{
					Objects.Add(TUniquePtr<FImplicitObject>(new TImplicitObjectTransformed<FReal, 3>(Child->Geometry(), Frame)));
					Objects2.Add(TUniquePtr<FImplicitObject>(new TImplicitObjectTransformed<FReal, 3>(Child->Geometry(), Frame)));
					ChildParticleHandles.Add(Child);
				}

				ensure(Child->Disabled() == true);
				check(Child->CastToClustered()->ClusterIds().Id == Parent);

				Child->CastToClustered()->SetChildToParent(Frame);

				if (bUseCollisionPoints)
				{
					SCOPE_CYCLE_COUNTER(STAT_UpdateGeometry_GatherPoints);
					if (const TUniquePtr<FBVHParticles>& CollisionParticles = Child->CollisionParticles())
					{
						for (uint32 i = 0; i < CollisionParticles->Size(); ++i)
						{
							OriginalPoints.Add(Frame.TransformPosition(CollisionParticles->X(i)));
						}
					}
				}
				if (Child->Geometry() && Child->Geometry()->GetType() == ImplicitObjectType::Unknown)
				{
					bUseParticleImplicit = true;
				}
			} // end for
		} // STAT_UpdateGeometry_GatherObjects

		{
			QUICK_SCOPE_CYCLE_COUNTER(SpatialBVH);
			TUniquePtr<FImplicitObjectUnionClustered>& ChildrenSpatial = Parent->ChildrenSpatial();
			ChildrenSpatial.Reset(
				Objects2.Num() ?
				new Chaos::FImplicitObjectUnionClustered(MoveTemp(Objects2), ChildParticleHandles) :
				nullptr);
		}

		TArray<FVec3> CleanedPoints;
		if (!Parameters.CollisionParticles)
		{
			CleanedPoints =
				Parameters.bCleanCollisionParticles ?
				CleanCollisionParticles(OriginalPoints, ClusterSnapDistance) :
				OriginalPoints;
		}

		// ignore unions for now as we don't yet support deep copy of it
		// on the GT they are only used by clusters that aggregate their children shapes ( see GeometryCollectionPhysicsProxy.cpp )
		// by failing artificially this condition thmake sure we create a FImplicitObjectUnionClustered for this particle 
		if (ProxyGeometry)
		{
			const FVector Scale = Parameters.Scale;
			auto DeepCopyImplicit = [&Scale](const TSharedPtr<Chaos::FImplicitObject, ESPMode::ThreadSafe>& ImplicitToCopy) -> TUniquePtr<Chaos::FImplicitObject>
			{
				if (Scale.Equals(FVector::OneVector))
				{
					return ImplicitToCopy->DeepCopy();
				}
				else
				{
					return ImplicitToCopy->DeepCopyWithScale(Scale);
				}
			};
			//ensureMsgf(false, TEXT("Checking usage with proxy"));
			//@coverage {production}

			Chaos::EImplicitObjectType GeometryType = ProxyGeometry->GetType();
			// Don't copy if it is not a level set and scale is one
			if (GeometryType != Chaos::ImplicitObjectType::LevelSet && Scale.Equals(FVector::OneVector))
			{
				Parent->SetSharedGeometry(TSharedPtr<FImplicitObject, ESPMode::ThreadSafe>(ProxyGeometry));
			}
			else
			{
				Parent->SetSharedGeometry(TSharedPtr<FImplicitObject, ESPMode::ThreadSafe>(DeepCopyImplicit(ProxyGeometry).Release()));
			}
		}
		else if (Objects.Num() == 0)
		{
			//ensureMsgf(false, TEXT("Checking usage with no proxy and no objects"));
			//@coverage : {production}
			Parent->SetGeometry(Chaos::TSerializablePtr<Chaos::FImplicitObject>());
		}
		else
		{
			if (UseLevelsetCollision)
			{
				ensureMsgf(false, TEXT("Checking usage with no proxy and multiple ojects with levelsets"));

				FImplicitObjectUnionClustered UnionObject(MoveTemp(Objects));
				FAABB3 Bounds = UnionObject.BoundingBox();
				const FVec3 BoundsExtents = Bounds.Extents();
				if (BoundsExtents.Min() >= MinLevelsetSize) //make sure the object is not too small
				{
					TVec3<int32> NumCells = Bounds.Extents() / MinLevelsetSize;
					for (int i = 0; i < 3; ++i)
					{
						NumCells[i] = FMath::Clamp(NumCells[i], MinLevelsetDimension, MaxLevelsetDimension);
					}

					FErrorReporter ErrorReporter;
					TUniformGrid<FReal, 3> Grid(Bounds.Min(), Bounds.Max(), NumCells, LevelsetGhostCells);
					TUniquePtr<FLevelSet> LevelSet(new FLevelSet(ErrorReporter, Grid, UnionObject));

					if (!Parameters.CollisionParticles)
					{
						const FReal MinDepthToSurface = Grid.Dx().Max();
						for (int32 Idx = CleanedPoints.Num() - 1; Idx >= 0; --Idx)
						{
							if (CleanedPoints.Num() > MinCleanedPointsBeforeRemovingInternals) //todo(ocohen): this whole thing should really be refactored
							{
								const FVec3& CleanedCollision = CleanedPoints[Idx];
								if (LevelSet->SignedDistance(CleanedCollision) < -MinDepthToSurface)
								{
									CleanedPoints.RemoveAtSwap(Idx);
								}
							}
						}
					}
					Parent->SetDynamicGeometry(MoveTemp(LevelSet));
				}
				else
				{
					Parent->SetDynamicGeometry(
						MakeUnique<TSphere<FReal, 3>>(FVec3(0), BoundsExtents.Size() * 0.5f));
				}
			}
			else // !UseLevelsetCollision
			{
				if (Objects.Num() == 1)
				{
					Parent->SetDynamicGeometry(MoveTemp(Objects[0]));
				}
				else
				{
					Parent->SetDynamicGeometry(
						MakeUnique<FImplicitObjectUnionClustered>(
							MoveTemp(Objects), ChildParticleHandles));
				}
			}
		}

		//if children are ignore analytic and this is a dynamic geom, mark it too. todo(ocohen): clean this up
		if (bUseParticleImplicit && Parent->DynamicGeometry())
		{
			Parent->DynamicGeometry()->SetDoCollide(false);
		}

		if (Parameters.CollisionParticles)
		{
			SCOPE_CYCLE_COUNTER(STAT_UpdateGeometry_CopyPoints);
			Parent->CollisionParticles().Reset(Parameters.CollisionParticles);
		}
		else
		{
			{
				SCOPE_CYCLE_COUNTER(STAT_UpdateGeometry_GatherPoints);
				Parent->CollisionParticlesInitIfNeeded();
				TUniquePtr<FBVHParticles>& CollisionParticles = Parent->CollisionParticles();
				CollisionParticles->AddParticles(CleanedPoints.Num());
				for (int32 i = 0; i < CleanedPoints.Num(); ++i)
				{
					CollisionParticles->X(i) = CleanedPoints[i];
				}
			}

			if (bUseCollisionPoints)
			{
				SCOPE_CYCLE_COUNTER(STAT_UpdateGeometry_PointsBVH);
				Parent->CollisionParticles()->UpdateAccelerationStructures();
			}
		}

		if (TSerializablePtr<FImplicitObject> Implicit = Parent->Geometry())
		{
			// strange hacked initilization that seems misplaced and ill thought
			Parent->SetHasBounds(true);
			Parent->SetLocalBounds(Implicit->BoundingBox());
			const Chaos::FRigidTransform3 Xf(Parent->X(), Parent->R());
			Parent->UpdateWorldSpaceState(Xf, FVec3(0));

			if (const FImplicitObjectUnion* ImplicitUnion = Implicit->GetObject<FImplicitObjectUnion>())
			{
				const_cast<FImplicitObjectUnion*>(ImplicitUnion)->SetAllowBVH(true);
			}
			else if (const FImplicitObjectUnion* ImplicitUnionClustered = Implicit->GetObject<FImplicitObjectUnionClustered>())
			{
				const_cast<FImplicitObjectUnion*>(ImplicitUnionClustered)->SetAllowBVH(true);
			}
		}
	
		// Update filter data on new shapes
		const FRigidClustering::FRigidHandleArray& ChildrenArray = ChildrenMap[Parent];
		UpdateClusterFilterDataFromChildren(Parent, ChildrenArray);
	}
	
	void UpdateClusterFilterDataFromChildren(FPBDRigidClusteredParticleHandle* ClusterParent, const TArray<FPBDRigidParticleHandle*>& Children)
	{
		SCOPE_CYCLE_COUNTER(STAT_GCUpdateFilterData);

		FCollisionFilterData SelectedSimFilter, SelectedQueryFilter;
		bool bFilterValid = false;
		for (FPBDRigidParticleHandle* ChildHandle : Children)
		{
			for (const TUniquePtr<FPerShapeData>& Shape : ChildHandle->ShapesArray())
			{
				if (Shape)
				{
					SelectedSimFilter = Shape->GetSimData();
					bFilterValid = SelectedSimFilter.Word0 != 0 || SelectedSimFilter.Word1 != 0 || SelectedSimFilter.Word2 != 0 || SelectedSimFilter.Word3 != 0;
					SelectedQueryFilter = Shape->GetQueryData();

					if (bFilterValid)
					{
						break;
					}
				}
			}

			if (bFilterValid)
			{
				break;
			}
		}

		// Apply selected filters to shapes
		if (bFilterValid)
		{
			const FShapesArray& ShapesArray = ClusterParent->ShapesArray();
			for (const TUniquePtr<FPerShapeData>& Shape : ShapesArray)
			{
				Shape->SetQueryData(SelectedQueryFilter);
				Shape->SetSimData(SelectedSimFilter);
			}
		}
	}


} // namespace Chaos
