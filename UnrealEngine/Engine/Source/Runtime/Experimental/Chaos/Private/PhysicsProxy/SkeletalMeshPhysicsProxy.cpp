// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhysicsProxy/SkeletalMeshPhysicsProxy.h"
#include "GeometryCollection/GeometryCollectionCollisionStructureManager.h"
#include "ChaosStats.h"
#include "PhysicsSolver.h"

#ifndef ADD_TRAITS_TO_SKELTAL_MESH_PROXY
#define ADD_TRAITS_TO_SKELTAL_MESH_PROXY 0
#endif

FSkeletalMeshPhysicsProxy::FSkeletalMeshPhysicsProxy(UObject* InOwner, const FInitFunc& InInitFunc)
	: Base(InOwner)
	, JointConstraints()
	, NextInputProducerBuffer(nullptr)
	, CurrentOutputConsumerBuffer(nullptr)
	, bInitializedState(false)
	, InitFunc(InInitFunc)
{
	check(IsInGameThread())
}

FSkeletalMeshPhysicsProxy::~FSkeletalMeshPhysicsProxy()
{
}

void FSkeletalMeshPhysicsProxy::Initialize()
{
	check(IsInGameThread());

	// InitFunc builds a bone hierarchy from the UPhysicsAsset on a USkeletalMeshComponent.
	// Transforms will be set from the FReferenceSkeleton reference bone poses.
	InitFunc(Parameters);

	// Set InitializedState to false so that rigid bodies will now be created.
	Reset();
}

void FSkeletalMeshPhysicsProxy::Reset()
{
	bInitializedState = false;
}

bool FSkeletalMeshPhysicsProxy::IsSimulating() const
{
	return Parameters.bSimulating;
}

void FSkeletalMeshPhysicsProxy::UpdateKinematicBodiesCallback(const FParticlesType& Particles, const float Dt, const float Time, FKinematicProxy& Proxy)
{
	// @todo(ccaulfield): Handle changes in body state

	if (bInitializedState && Parameters.bSimulating)
	{
		// The triple buffer will return null if there's no new data for the consumer thread.
		const FSkeletalMeshPhysicsProxyInputs* InputBuffer = InputBuffers.ExchangeConsumerBuffer();
		if (InputBuffer == nullptr)
		{
			return;
		}

#if TODO_REIMPLEMENT_KINEMATIC_PROXY

		// @todo(ccaulfield): Optimize. Keep a list of kinematic particles?
		const int32 MaxRigidBodies = RigidBodyIds.Num();
		Proxy.Ids.Reset(MaxRigidBodies);
		Proxy.Position.Reset(MaxRigidBodies);
		Proxy.NextPosition.Reset(MaxRigidBodies);
		Proxy.Rotation.Reset(MaxRigidBodies);
		Proxy.NextRotation.Reset(MaxRigidBodies);

		int32 RigidBodyIndex = 0;
		TArray<TUniquePtr<FAnalyticImplicitGroup>>& Groups = Parameters.BoneHierarchy.GetAnalyticShapeGroups();
		for (TUniquePtr<FAnalyticImplicitGroup>& Group : Groups)
		{
			if (!Group->NumStructures())
			{
				continue;
			}

			const int32 RigidBodyId = Group->GetRigidBodyId();
			const int32 BoneIndex = Group->GetBoneIndex();
			const int32 TransformIndex = Parameters.BoneHierarchy.GetTransformIndex(BoneIndex);
			bool bIsKinematic = (Particles.ObjectState(RigidBodyId) == Chaos::EObjectStateType::Kinematic);
			if (bIsKinematic)
			{
				const FTransform& Transform = InputBuffer->Transforms[TransformIndex];
				const FVector& LinearVelocity = InputBuffer->LinearVelocities[TransformIndex];
				const FVector& AngularVelocity = InputBuffer->AngularVelocities[TransformIndex];

				Proxy.Ids.Add(RigidBodyId);
				Proxy.Position.Add(Chaos::FVec3(Transform.GetTranslation()));
				Proxy.NextPosition.Add(Proxy.Position[RigidBodyIndex] + Chaos::FVec3(LinearVelocity) * Dt);
				Proxy.Rotation.Add(Chaos::TRotation<float, 3>(Transform.GetRotation().GetNormalized()));
				Proxy.NextRotation.Add(Chaos::TRotation<float, 3>::IntegrateRotationWithAngularVelocity(Proxy.Rotation[RigidBodyIndex], AngularVelocity, Dt));
				++RigidBodyIndex;
			}
		}

#endif

	}
}

void FSkeletalMeshPhysicsProxy::StartFrameCallback(const float InDt, const float InTime)
{
}

void FSkeletalMeshPhysicsProxy::EndFrameCallback(const float InDt)
{
}

void FSkeletalMeshPhysicsProxy::CreateRigidBodyCallback(FParticlesType& Particles)
{
	if (!bInitializedState && Parameters.bSimulating)
	{
#if TODO_REIMPLEMENT_RIGID_CLUSTERING
		// @todo(ccaulfield): should not have to do this cluster thing here...
		Chaos::TArrayCollectionArray<int32>& ClusterGroupIndex = GetSolver()->GetRigidClustering().GetClusterGroupIndexArray();
#endif
#if 0
		Chaos::TArrayCollectionArray<float>& StrainArray = GetSolver()->GetRigidClustering().GetStrainArray();
#endif
		Parameters.BoneHierarchy.PrepareAnimWorldSpaceTransforms();
		RigidBodyIds.Reset();
		RigidBodyIds.Reserve(Parameters.BoneHierarchy.GetBoneIndices().Num());
		for (const int32 BoneIndex : Parameters.BoneHierarchy.GetBoneIndices())
		{
			FAnalyticImplicitGroup* Group = Parameters.BoneHierarchy.GetAnalyticShapeGroup(BoneIndex);
			if (!Group || !Group->NumStructures())
			{
				continue;
			}

			const FTransform* WorldTransform = Parameters.BoneHierarchy.GetAnimWorldSpaceTransformsForBone(BoneIndex);
			check(WorldTransform);

			Chaos::FReal TotalMass = 0.;
			const Chaos::FReal DensityKgCm3 = Parameters.Density / 1000.f;
			Chaos::FMassProperties MassProperties = Group->BuildMassProperties(DensityKgCm3, TotalMass);

			const bool DoCollisionGeom = Parameters.CollisionType == ECollisionTypeEnum::Chaos_Surface_Volumetric;
			TArray<Chaos::FVec3>* SamplePoints = nullptr;
			Chaos::FAABB3 SamplePointsBBox = Chaos::FAABB3::EmptyAABB();
			if (DoCollisionGeom)
			{
				SamplePoints =
					Group->BuildSamplePoints(
						Parameters.ParticlesPerUnitArea,
						Parameters.MinNumParticles,
						Parameters.MaxNumParticles);
				for (const Chaos::FVec3& Point : *SamplePoints)
					SamplePointsBBox.GrowToInclude(Point);
			}
			//Chaos::FTriangleMesh TriMesh = Chaos::FTriangleMesh::GetConvexHullFromParticles(
			//	TArrayView<const Chaos::TVector<T, 3>>(SamplePoints));

			TUniquePtr<Chaos::FImplicitObject> ImplicitObject(Group->BuildSimImplicitObject());
			check(ImplicitObject);

			if (SamplePoints)
			{
				auto PointVolumeRegistrationCheck = 
					[&](const TArray<Chaos::FVec3> &InSamplePoints, const Chaos::FImplicitObject &InImplicitObject, const float InTolerance) -> bool
					{
						for (int32 i = 0; i < InSamplePoints.Num(); i++)
						{
							const auto& Pt = InSamplePoints[i];
							const auto Phi = InImplicitObject.SignedDistance(Pt);
							if (FMath::Abs(Phi) > InTolerance)
							{
								return false;
							}
						}
						return true; 
					};
				// Precision gets worse in opt builds, so only do the phi test in debug.
				checkSlow(PointVolumeRegistrationCheck(*SamplePoints, *ImplicitObject, 0.01f));
				auto PointBBoxCheck =
					[&](const auto &InSamplePoints, const Chaos::FAABB3 &BBox) -> bool
					{
						for (int32 i = 0; i < InSamplePoints.Num(); i++)
						{
							const auto& Pt = InSamplePoints[i];
							if (!BBox.Contains(Pt))
							{
								return false;
							}
						}
						return true;
					};
				// BBox tests should still work though.
				check(PointBBoxCheck(*SamplePoints, ImplicitObject->BoundingBox()));
			}

			const Chaos::FAABB3& BBox = ImplicitObject->BoundingBox();

			const FVector Scale = WorldTransform->GetScale3D();
			const FVector &CenterOfMass = MassProperties.CenterOfMass;

			FBox Bounds(BBox.Min(), BBox.Max());
			Bounds = Bounds.InverseTransformBy(FTransform(CenterOfMass));
			Bounds.Min *= Scale;
			Bounds.Max *= Scale;
			checkSlow(Bounds.GetVolume() > FLT_EPSILON);

			const int32 RigidBodyId = Particles.Size();
			Particles.AddParticles(1);
			RigidBodyIds.Add(RigidBodyId);
			Group->SetRigidBodyId(RigidBodyId);
#if 0
			ClusterGroupIndex[RigidBodyId] = Parameters.ClusterGroupIndex;
			StrainArray[RigidBodyId] = Parameters.DamageThreshold;
#else
#if TODO_REIMPLEMENT_RIGID_CLUSTERING
			ClusterGroupIndex[RigidBodyId] = 0;
#endif
#endif

#if TODO_REIMPLEMENT_SET_PHYSICS_MATERIAL
			GetSolver()->SetPhysicsMaterial(RigidBodyId, Parameters.PhysicalMaterial);
#endif
			Particles.DisabledRef(RigidBodyId) = false;

			//Particles.X(RigidBodyId) = WorldTransform->TransformPosition(CenterOfMass);
			Particles.X(RigidBodyId) = WorldTransform->GetTranslation();
			check(!FMath::IsNaN(Particles.X(RigidBodyId)[0]) && !FMath::IsNaN(Particles.X(RigidBodyId)[1]) && !FMath::IsNaN(Particles.X(RigidBodyId)[2]));
			Particles.V(RigidBodyId) = Chaos::FVec3(0.f);
			//Particles.R(RigidBodyId) = (WorldTransform->TransformRotation(MassProperties.RotationOfMass)).GetNormalized();
			Particles.R(RigidBodyId) = WorldTransform->GetRotation().GetNormalized();
			Particles.W(RigidBodyId) = Chaos::FVec3(0.f);
			Particles.P(RigidBodyId) = Particles.X(RigidBodyId);
			Particles.Q(RigidBodyId) = Particles.R(RigidBodyId);

			Particles.M(RigidBodyId) = FMath::Clamp(TotalMass, (Chaos::FReal)Parameters.MinMass, (Chaos::FReal)Parameters.MaxMass);
			Particles.InvM(RigidBodyId) = Particles.M(RigidBodyId) < UE_KINDA_SMALL_NUMBER ? UE_BIG_NUMBER/2 : 1.f / Particles.M(RigidBodyId);
			Particles.I(RigidBodyId) = Chaos::TVec3<Chaos::FRealSingle>((Chaos::FRealSingle)MassProperties.InertiaTensor.M[0][0], (Chaos::FRealSingle)MassProperties.InertiaTensor.M[1][1], (Chaos::FRealSingle)MassProperties.InertiaTensor.M[2][2]);
			Particles.InvI(RigidBodyId) = Chaos::TVec3<Chaos::FRealSingle>(1.f / (Chaos::FRealSingle)MassProperties.InertiaTensor.M[0][0], 1.f / (Chaos::FRealSingle)MassProperties.InertiaTensor.M[1][1], 1.f / (Chaos::FRealSingle)MassProperties.InertiaTensor.M[2][2]);

			Particles.CollisionGroup(RigidBodyId) = Parameters.CollisionGroup;
			
			// Set implicit surface
			const Chaos::EImplicitObjectType ShapeType = ImplicitObject->GetType();
			if (ShapeType == Chaos::ImplicitObjectType::LevelSet)
			{
				FCollisionStructureManager::UpdateImplicitFlags(
					ImplicitObject.Get(), ECollisionTypeEnum::Chaos_Surface_Volumetric);
				Particles.SetDynamicGeometry(RigidBodyId, MoveTemp(ImplicitObject));
			}
			else
			{
				Particles.SetDynamicGeometry(RigidBodyId, MoveTemp(ImplicitObject));
			}

			if (Parameters.CollisionType == ECollisionTypeEnum::Chaos_Surface_Volumetric)
			{
				if (SamplePoints)
				{
					Particles.CollisionParticlesInitIfNeeded(RigidBodyId);

					Particles.CollisionParticles(RigidBodyId)->Resize(0);
					Particles.CollisionParticles(RigidBodyId)->AddParticles(SamplePoints->Num());
					for (int Vi = 0; Vi < SamplePoints->Num(); Vi++)
					{
						Particles.CollisionParticles(RigidBodyId)->X(Vi) = (*SamplePoints)[Vi];
					}
					if (Particles.CollisionParticles(RigidBodyId)->Size())
					{
						Particles.CollisionParticles(RigidBodyId)->UpdateAccelerationStructures();
					}
				}
			}

			// Set the simulation state of the body.
			// If set to "user defined" we inherit the owner's state
			// @todo(ccaulfield): formalize the state inheritence - currently hijacking UserDefined...
			Chaos::EObjectStateType RigidBodyState = Chaos::EObjectStateType::Kinematic;
			EObjectStateTypeEnum SourceState = Group->GetRigidBodyState();
			if (SourceState == EObjectStateTypeEnum::Chaos_Object_UserDefined)
			{
				SourceState = Parameters.ObjectType;
			}
			switch (SourceState)
			{
			case EObjectStateTypeEnum::Chaos_Object_Sleeping:
				RigidBodyState = Chaos::EObjectStateType::Sleeping;
				break;
			case EObjectStateTypeEnum::Chaos_Object_Kinematic:
				RigidBodyState = Chaos::EObjectStateType::Kinematic;
				break;
			case EObjectStateTypeEnum::Chaos_Object_Static:
				RigidBodyState = Chaos::EObjectStateType::Static;
				break;
			default:
				RigidBodyState = Chaos::EObjectStateType::Dynamic;
				break;
			}
			Particles.SetObjectState(RigidBodyId, RigidBodyState);

			// Create joints
			if (RigidBodyState == Chaos::EObjectStateType::Dynamic)
			{
				const int32 ParentBoneIndex = Group->GetParentBoneIndex();
				if (ParentBoneIndex != INDEX_NONE)
				{
					const FAnalyticImplicitGroup* ParentGroup = Parameters.BoneHierarchy.GetAnalyticShapeGroup(ParentBoneIndex);
					if (ParentGroup && (ParentGroup->GetRigidBodyId() != INDEX_NONE))
					{
						const int32 ParentRigidBodyId = ParentGroup->GetRigidBodyId();
						const FTransform& ParentWorldTransform = *Parameters.BoneHierarchy.GetAnimWorldSpaceTransformsForBone(ParentBoneIndex);

						const Chaos::TVector<int32, 2> ConstrainedParticleIndices = { RigidBodyId, ParentRigidBodyId };
						const Chaos::FVec3 JointPosition = ParentWorldTransform.GetTranslation();
#if TODO_REWRITE_ALL_CONSTRAINT_ADDS_TO_USE_HANDLES
						JointConstraints.AddConstraint(Particles, ConstrainedParticleIndices, JointPosition);
#endif
					}
				}
			}


			UE_LOG(USkeletalMeshSimulationComponentLogging, Log,
				TEXT("FSkeletalMeshPhysicsProxy::CreateRigidBodyCallback() - this: %p - "
					"Created rigid body %d for bone %d.\n"
					"    center of mass: (%g, %g, %g), Bone pos: (%g, %g, %g), RB position: (%g, %g, %g)\n"
					"    implicit shape domain: [(%g %g %g), (%g %g %g)] - volume: %g.\n"
					"    %d local space collision points, domain: [(%g %g %g), (%g %g %g)] - volume: %g."),

				this, 
				RigidBodyId, Group->GetBoneIndex(),

				CenterOfMass[0], CenterOfMass[1], CenterOfMass[2], 
				WorldTransform->GetTranslation()[0], WorldTransform->GetTranslation()[1], WorldTransform->GetTranslation()[2], 
				Particles.X(RigidBodyId)[0], Particles.X(RigidBodyId)[2], Particles.X(RigidBodyId)[2], 

				SamplePointsBBox.Min()[0], SamplePointsBBox.Min()[1], SamplePointsBBox.Min()[2],
				SamplePointsBBox.Max()[0], SamplePointsBBox.Max()[1], SamplePointsBBox.Max()[2],
				SamplePointsBBox.Extents().Product(),

				SamplePoints?SamplePoints->Num():0,
				BBox.Min()[0], BBox.Min()[1], BBox.Min()[2],
				BBox.Max()[0], BBox.Max()[1], BBox.Max()[2],
				BBox.Extents().Product());
		}

	ensure(false);	//GetSolver needs a trait, but we don't know it - this class changing anyway
#if ADD_TRAITS_TO_SKELTAL_MESH_PROXY
		// Add joints to the scene
		if (JointConstraints.NumConstraints() > 0)
		{
			GetSolver()->GetEvolution()->AddConstraintRule(&JointConstraintsRule);
		}

		bInitializedState = true;
#endif
	}
}

void FSkeletalMeshPhysicsProxy::ParameterUpdateCallback(FParticlesType& InParticles, const float InTime)
{}

void FSkeletalMeshPhysicsProxy::DisableCollisionsCallback(TSet<TTuple<int32, int32>>& InPairs)
{}

void FSkeletalMeshPhysicsProxy::AddForceCallback(FParticlesType& InParticles, const float InDt, const int32 InIndex)
{}

void FSkeletalMeshPhysicsProxy::BindParticleCallbackMapping(Chaos::TArrayCollectionArray<PhysicsProxyWrapper>& PhysicsProxyReverseMap, Chaos::TArrayCollectionArray<int32>& ParticleIDReverseMap)
{
	if (bInitializedState)
	{
		for (int32 Index = 0; Index < RigidBodyIds.Num(); Index++)
		{
			if (RigidBodyIds[Index] != INDEX_NONE)
			{
				PhysicsProxyReverseMap[RigidBodyIds[Index]] = { this, EPhysicsProxyType::SkeletalMeshType };
				ParticleIDReverseMap[RigidBodyIds[Index]] = Index;
			}
		}
	}
}

void FSkeletalMeshPhysicsProxy::SyncBeforeDestroy() 
{}

void FSkeletalMeshPhysicsProxy::OnRemoveFromScene()
{
	ensure(false);	//GetSolver needs a trait, but we don't know it - this class changing anyway
#if ADD_TRAITS_TO_SKELTAL_MESH_PROXY
	// Disable the particle we added
	Chaos::FPhysicsSolver* CurrSolver = GetSolver();
	if(CurrSolver && RigidBodyIds.Num())
	{
#if TODO_REIMPLEMENT_GET_RIGID_PARTICLES
		Chaos::FPhysicsSolver::FParticlesType &Particles = CurrSolver->GetRigidParticles();
		// #BG TODO Special case here because right now we reset/realloc the evolution per geom component
		// in endplay which clears this out. That needs to not happen and be based on world shutdown
		if (Particles.Size() == 0)
		{
			return;
		}

		for (const int32 Id : RigidBodyIds)
		{
			if(static_cast<uint32>(Id) < Particles.Size())
			{
				Particles.DisabledRef(Id) = true;
				CurrSolver->GetRigidClustering().GetTopLevelClusterParents().Remove(Id);
			}
		}
#endif
	}
#endif
}

void FSkeletalMeshPhysicsProxy::BufferPhysicsResults()
{
	using namespace Chaos;
	// PHYSICS THREAD

	ensure(false);	//GetSolver needs a trait, but we don't know it - this class changing anyway
#if ADD_TRAITS_TO_SKELTAL_MESH_PROXY
	// @todo(ccaulfield)
	if (GetSolver() != nullptr)
	{
		// Copy the world-space state of the simulated bones into the output buffer
		int NumBones = Parameters.BoneHierarchy.GetBoneIndices().Num();
		FSkeletalMeshPhysicsProxyOutputs& Outputs = OutputBuffers.GetPhysicsDataForWrite();
		Outputs.Transforms.SetNumZeroed(NumBones);
		Outputs.LinearVelocities.SetNumZeroed(NumBones);
		Outputs.AngularVelocities.SetNumZeroed(NumBones);

#if TODO_REIMPLEMENT_GET_RIGID_PARTICLES
		FPhysicsSolver::FParticlesType& Particles = GetSolver()->GetRigidParticles();
		for (const int32 BoneIndex : Parameters.BoneHierarchy.GetBoneIndices())
		{
			FAnalyticImplicitGroup* ShapeGroup = Parameters.BoneHierarchy.GetAnalyticShapeGroup(BoneIndex);
			if (ShapeGroup != nullptr)
			{
				int32 RigidBodyId = ShapeGroup->GetRigidBodyId();
				if (RigidBodyId >= 0)
				{
					const FTransform BodyTransform = FTransform(Particles.Q(RigidBodyId), Particles.P(RigidBodyId));
					int32 TransformIndex = Parameters.BoneHierarchy.GetTransformIndex(BoneIndex);
					Outputs.Transforms[TransformIndex] = BodyTransform;
					Outputs.LinearVelocities[TransformIndex] = Particles.V(RigidBodyId);
					Outputs.AngularVelocities[TransformIndex] = Particles.W(RigidBodyId);
				}
			}
		}
#endif
	}
#endif
}

void FSkeletalMeshPhysicsProxy::FlipBuffer()
{
	// PHYSICS THREAD

	OutputBuffers.Flip();
}

bool FSkeletalMeshPhysicsProxy::PullFromPhysicsState(const int32 SolverSyncTimestamp)
{
	// GAME THREAD
	// @todo(ccaulfield): this needs to update the Component-Space pose in BoneHierarchy and update non-simulated children

	CurrentOutputConsumerBuffer = &OutputBuffers.GetGameDataForRead();
	return true;
}

void FSkeletalMeshPhysicsProxy::CaptureInputs(const float Dt, const FInputFunc& InputFunc)
{
	// GAME THREAD
	SCOPE_CYCLE_COUNTER(STAT_SkelMeshUpdateAnim);

	// Get a new buffer to write the input data to
	if (NextInputProducerBuffer == nullptr)
	{
		NextInputProducerBuffer = InputBuffers.ExchangeProducerBuffer();
	}

	// Fill in the input data and switch the buffers if we had new data
	if (InputFunc(Dt, Parameters))
	{
		const FBoneHierarchy& BoneHierarchy = Parameters.BoneHierarchy;
		FSkeletalMeshPhysicsProxyInputs& PhysicsInputs = *NextInputProducerBuffer;
		PhysicsInputs.Transforms.SetNumZeroed(BoneHierarchy.GetBoneIndices().Num());
		PhysicsInputs.LinearVelocities.SetNumZeroed(BoneHierarchy.GetBoneIndices().Num());
		PhysicsInputs.AngularVelocities.SetNumZeroed(BoneHierarchy.GetBoneIndices().Num());
		for (int32 BoneIndex : BoneHierarchy.GetBoneIndices())
		{
			const int32 TransformIndex = BoneHierarchy.GetTransformIndex(BoneIndex);
			if (TransformIndex != INDEX_NONE)
			{
				const FTransform& BoneWorldTransform = *BoneHierarchy.GetAnimWorldSpaceTransformsForBone(BoneIndex);
				const FTransform* PrevBoneWorldTransform = BoneHierarchy.GetPrevAnimWorldSpaceTransformForBone(BoneIndex);
				if ((Dt > UE_SMALL_NUMBER) && PrevBoneWorldTransform)
				{
					PhysicsInputs.Transforms[TransformIndex] = *PrevBoneWorldTransform;
					PhysicsInputs.LinearVelocities[TransformIndex] = (BoneWorldTransform.GetTranslation() - PrevBoneWorldTransform->GetTranslation()) / Dt;
					PhysicsInputs.AngularVelocities[TransformIndex] = Chaos::FRotation3::CalculateAngularVelocity(PrevBoneWorldTransform->GetRotation(), BoneWorldTransform.GetRotation(), Dt);
				}
				else
				{
					PhysicsInputs.Transforms[TransformIndex] = BoneWorldTransform;
					PhysicsInputs.LinearVelocities[TransformIndex] = FVector::ZeroVector;
					PhysicsInputs.AngularVelocities[TransformIndex] = FVector::ZeroVector;
				}
			}
		}

		NextInputProducerBuffer = InputBuffers.ExchangeProducerBuffer();
	}
}
