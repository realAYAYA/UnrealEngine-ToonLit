// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhysicsProxy/StaticMeshPhysicsProxy.h"
#include "PhysicsSolver.h"
#include "GeometryCollection/GeometryCollectionCollisionStructureManager.h"
#include "Chaos/Box.h"
#include "Chaos/Capsule.h"
#include "Chaos/Sphere.h"
#include "Chaos/ErrorReporter.h"
#include "Chaos/Serializable.h"
#include "ChaosStats.h"

#ifndef ADD_TRAITS_TO_STATIC_MESH_PROXY
#define ADD_TRAITS_TO_STATIC_MESH_PROXY 0
#endif

FStaticMeshPhysicsProxy::FStaticMeshPhysicsProxy(UObject* InOwner, FCallbackInitFunc InInitFunc, FSyncDynamicFunc InSyncFunc)
	: Base(InOwner)
	, bInitializedState(false)
	, RigidBodyId(INDEX_NONE)
	, CenterOfMass(FVector::ZeroVector)
	, Scale(FVector::ZeroVector)
	, SimTransform(FTransform::Identity)
	, InitialiseCallbackParamsFunc(InInitFunc)
	, SyncDynamicTransformFunc(InSyncFunc)
{
	check(IsInGameThread());

	Results.Get(0) = FTransform::Identity;
	Results.Get(1) = FTransform::Identity;
}

void FStaticMeshPhysicsProxy::Initialize()
{
	check(IsInGameThread());

	// Safe here - we're not in the solver yet if we're creating callbacks
	Results.Get(0) = FTransform::Identity;
	Results.Get(1) = FTransform::Identity;

	InitialiseCallbackParamsFunc(Parameters);
	Parameters.TargetTransform = &SimTransform;

	Reset();
}

void FStaticMeshPhysicsProxy::Reset()
{
	bInitializedState = false;
}

void FStaticMeshPhysicsProxy::BufferKinematicUpdate(const FPhysicsProxyKinematicUpdate& InParamUpdate)
{
	BufferedKinematicUpdate = InParamUpdate;
	bPendingKinematicUpdate = true;
};

bool FStaticMeshPhysicsProxy::IsSimulating() const
{
	return Parameters.bSimulating;
}

void FStaticMeshPhysicsProxy::UpdateKinematicBodiesCallback(const FParticlesType& Particles, const float Dt, const float Time, FKinematicProxy& Proxy)
{
	bool IsControlled = Parameters.ObjectType == EObjectStateTypeEnum::Chaos_Object_Kinematic;
	if(IsControlled && Parameters.bSimulating)
	{
#if TODO_REIMPLEMENT_KINEMATIC_PROXY
		bool bFirst = !Proxy.Ids.Num();
		if(bFirst)
		{
			Proxy.Ids.Add(RigidBodyId);
			Proxy.Position.SetNum(1);
			Proxy.NextPosition.SetNum(1);
			Proxy.Rotation.SetNum(1);
			Proxy.NextRotation.SetNum(1);

			const FTransform& Transform = Parameters.InitialTransform;
			Proxy.Position[0] = Chaos::FVec3(Transform.GetTranslation());
			Proxy.NextPosition[0] = Proxy.Position[0] + Chaos::FVec3(Parameters.InitialLinearVelocity) * Dt;
			Proxy.Rotation[0] = Chaos::TRotation<float, 3>(Transform.GetRotation().GetNormalized());
			Proxy.NextRotation[0] = Proxy.Rotation[0];
		}

		if (bPendingKinematicUpdate)
		{
			Proxy.Position[0] = Particles.X(RigidBodyId);
			Proxy.NextPosition[0] = BufferedKinematicUpdate.NewTransform.GetTranslation();
			Proxy.Rotation[0] = Particles.R(RigidBodyId);
			Proxy.NextRotation[0] = BufferedKinematicUpdate.NewTransform.GetRotation().GetNormalized();

			bPendingKinematicUpdate = false;
		}
#endif
	}
}

void FStaticMeshPhysicsProxy::StartFrameCallback(const float InDt, const float InTime)
{

}

void FStaticMeshPhysicsProxy::EndFrameCallback(const float InDt)
{
	bool IsControlled = Parameters.ObjectType == EObjectStateTypeEnum::Chaos_Object_Kinematic;
	if(Parameters.bSimulating && !IsControlled && Parameters.TargetTransform)
	{
#if TODO_REIMPLEMENT_GET_RIGID_PARTICLES
		const Chaos::TPBDRigidParticles<float, 3>& Particles = GetSolver()->GetRigidParticles();

		Parameters.TargetTransform->SetTranslation((FVector)Particles.X(RigidBodyId));
		Parameters.TargetTransform->SetRotation((FQuat)Particles.R(RigidBodyId));
#endif
	}
}

void FStaticMeshPhysicsProxy::BindParticleCallbackMapping(Chaos::TArrayCollectionArray<PhysicsProxyWrapper> & PhysicsProxyReverseMap, Chaos::TArrayCollectionArray<int32> & ParticleIDReverseMap)
{
	if (bInitializedState)
	{
		PhysicsProxyReverseMap[RigidBodyId] = { this, EPhysicsProxyType::StaticMeshType };
		ParticleIDReverseMap[RigidBodyId] = 0;
	}
}

void FStaticMeshPhysicsProxy::CreateRigidBodyCallback(FParticlesType& Particles)
{
	if(!bInitializedState && Parameters.bSimulating)
	{
		RigidBodyId = Particles.Size();
		Particles.AddParticles(1);

		FBox Bounds(ForceInitToZero);
		if (Parameters.ShapeType == EImplicitTypeEnum::Chaos_Implicit_LevelSet)
		{
			// Build implicit surface
			const ECollisionTypeEnum CollisionType = ECollisionTypeEnum::Chaos_Surface_Volumetric;
			check(sizeof(Chaos::FVec3) == sizeof(FVector));

			//TODO: this should all be done on the asset instead of in the callback. Duplicates all mesh vertices every time
			Chaos::FParticles MeshParticles(MoveTemp(Parameters.MeshVertexPositions));

			TUniquePtr<Chaos::FTriangleMesh> TriangleMesh(new Chaos::FTriangleMesh(MoveTemp(Parameters.TriIndices)));
			Chaos::FErrorReporter ErrorReporter(Parameters.Name + " | RigidBodyId: " + FString::FromInt(RigidBodyId));;
			Particles.SetDynamicGeometry(
				RigidBodyId,
				TUniquePtr<Chaos::FImplicitObject>(
					FCollisionStructureManager::NewImplicit(
						ErrorReporter,
						MeshParticles, *TriangleMesh,
						Bounds,
						FVector::Distance(FVector(0.f, 0.f, 0.f), 
						Bounds.GetExtent()) * 0.5f,
						Parameters.MinRes, 
						Parameters.MaxRes, 
						0.f,
						CollisionType, 
						Parameters.ShapeType)));

			if (!ensure(Parameters.MeshVertexPositions.Size()))
			{
				Parameters.MeshVertexPositions.AddParticles(1);
				Parameters.MeshVertexPositions.X(0) = Chaos::FVec3(0, 0, 0);
			}

			for(uint32 i=0; i < Parameters.MeshVertexPositions.Size(); ++i)
			{
				Bounds += Parameters.MeshVertexPositions.X(i);
			}
		}
		else if (Parameters.ShapeType == EImplicitTypeEnum::Chaos_Implicit_Sphere)
		{
			Chaos::TSphere<Chaos::FReal,3>* Sphere = new Chaos::TSphere<Chaos::FReal, 3>(Chaos::FVec3(0), Parameters.ShapeParams.SphereRadius);
			const Chaos::FAABB3 BBox = Sphere->BoundingBox();
			Bounds.Min = BBox.Min();
			Bounds.Max = BBox.Max();
			Particles.SetDynamicGeometry(RigidBodyId, TUniquePtr<Chaos::FImplicitObject>(Sphere));
			if (!Parameters.MeshVertexPositions.Size())
			{
				float Radius = Parameters.ShapeParams.SphereRadius;

				Parameters.MeshVertexPositions.AddParticles(6);
				Parameters.MeshVertexPositions.X(0) = Chaos::FVec3(Radius, 0, 0);
				Parameters.MeshVertexPositions.X(1) = Chaos::FVec3(-Radius, 0, 0);
				Parameters.MeshVertexPositions.X(2) = Chaos::FVec3(0, Radius, Radius);
				Parameters.MeshVertexPositions.X(3) = Chaos::FVec3(0, -Radius, Radius);
				Parameters.MeshVertexPositions.X(4) = Chaos::FVec3(0, -Radius, -Radius);
				Parameters.MeshVertexPositions.X(5) = Chaos::FVec3(0, Radius, -Radius);
			}
		}
		else if (Parameters.ShapeType == EImplicitTypeEnum::Chaos_Implicit_Box)
		{
			Chaos::FVec3 HalfExtents = Parameters.ShapeParams.BoxExtents / static_cast<Chaos::FReal>(2);
			Chaos::TBox<Chaos::FReal,3>* Box = new Chaos::TBox<Chaos::FReal, 3>(-HalfExtents, HalfExtents);
			Bounds.Min = Box->Min();
			Bounds.Max = Box->Max();
			Particles.SetDynamicGeometry(RigidBodyId, TUniquePtr<Chaos::FImplicitObject>(Box));
			if (!Parameters.MeshVertexPositions.Size())
			{
				Chaos::FVec3 x1(-HalfExtents);
				Chaos::FVec3 x2(HalfExtents);

				Parameters.MeshVertexPositions.AddParticles(8);
				Parameters.MeshVertexPositions.X(0) = Chaos::FVec3(x1.X, x1.Y, x1.Z);
				Parameters.MeshVertexPositions.X(1) = Chaos::FVec3(x1.X, x1.Y, x2.Z);
				Parameters.MeshVertexPositions.X(2) = Chaos::FVec3(x1.X, x2.Y, x1.Z);
				Parameters.MeshVertexPositions.X(3) = Chaos::FVec3(x2.X, x1.Y, x1.Z);
				Parameters.MeshVertexPositions.X(4) = Chaos::FVec3(x2.X, x2.Y, x2.Z);
				Parameters.MeshVertexPositions.X(5) = Chaos::FVec3(x2.X, x2.Y, x1.Z);
				Parameters.MeshVertexPositions.X(6) = Chaos::FVec3(x2.X, x1.Y, x2.Z);
				Parameters.MeshVertexPositions.X(7) = Chaos::FVec3(x1.X, x2.Y, x2.Z);
			}
		}
		else if (Parameters.ShapeType == EImplicitTypeEnum::Chaos_Implicit_Capsule)
		{
			Chaos::FVec3 x1(0, -Parameters.ShapeParams.CapsuleHalfHeightAndRadius.X, 0);
			Chaos::FVec3 x2(0, Parameters.ShapeParams.CapsuleHalfHeightAndRadius.X, 0);
			Chaos::FCapsule* Capsule = new Chaos::FCapsule(x1, x2, Parameters.ShapeParams.CapsuleHalfHeightAndRadius.Y);
			const Chaos::FAABB3 BBox = Capsule->BoundingBox();
			Bounds.Min = BBox.Min();
			Bounds.Max = BBox.Max();
			Particles.SetDynamicGeometry(RigidBodyId, TUniquePtr<Chaos::FImplicitObject>(Capsule));
			if (!Parameters.MeshVertexPositions.Size())
			{
				FVector2D::FReal HalfHeight = Parameters.ShapeParams.CapsuleHalfHeightAndRadius.X;
				FVector2D::FReal Radius = Parameters.ShapeParams.CapsuleHalfHeightAndRadius.Y;

				Parameters.MeshVertexPositions.AddParticles(14);
				Parameters.MeshVertexPositions.X(0) = Chaos::FVec3(HalfHeight + Radius, 0, 0);
				Parameters.MeshVertexPositions.X(1) = Chaos::FVec3(-HalfHeight - Radius, 0, 0);
				Parameters.MeshVertexPositions.X(2) = Chaos::FVec3(HalfHeight, Radius, Radius);
				Parameters.MeshVertexPositions.X(3) = Chaos::FVec3(HalfHeight, -Radius, Radius);
				Parameters.MeshVertexPositions.X(4) = Chaos::FVec3(HalfHeight, -Radius, -Radius);
				Parameters.MeshVertexPositions.X(5) = Chaos::FVec3(HalfHeight, Radius, -Radius);
				Parameters.MeshVertexPositions.X(6) = Chaos::FVec3(0, Radius, Radius);
				Parameters.MeshVertexPositions.X(7) = Chaos::FVec3(0, -Radius, Radius);
				Parameters.MeshVertexPositions.X(8) = Chaos::FVec3(0, -Radius, -Radius);
				Parameters.MeshVertexPositions.X(9) = Chaos::FVec3(0, Radius, -Radius);
				Parameters.MeshVertexPositions.X(10) = Chaos::FVec3(-HalfHeight, Radius, Radius);
				Parameters.MeshVertexPositions.X(11) = Chaos::FVec3(-HalfHeight, -Radius, Radius);
				Parameters.MeshVertexPositions.X(12) = Chaos::FVec3(-HalfHeight, -Radius, -Radius);
				Parameters.MeshVertexPositions.X(13) = Chaos::FVec3(-HalfHeight, Radius, -Radius);
			}
		}
		else
		{
			Bounds.Min = Chaos::FVec3(-UE_KINDA_SMALL_NUMBER);
			Bounds.Max = Chaos::FVec3(UE_KINDA_SMALL_NUMBER);
			Particles.SetSharedGeometry(RigidBodyId, TSharedPtr<Chaos::FImplicitObject, ESPMode::ThreadSafe>());
		}

		FTransform WorldTransform = Parameters.InitialTransform;

		Scale = WorldTransform.GetScale3D();
		CenterOfMass = Bounds.GetCenter();
		Bounds = Bounds.InverseTransformBy(FTransform(CenterOfMass));
		Bounds.Min *= Scale;
		Bounds.Max *= Scale;
		checkSlow((Bounds.Max + Bounds.Min).Size() < UE_KINDA_SMALL_NUMBER);

		Particles.InvM(RigidBodyId) = 0.f;
		ensure(Parameters.Mass >= 0.f);
		Particles.M(RigidBodyId) = Parameters.Mass;
		if(Parameters.Mass < FLT_EPSILON)
		{
			Particles.InvM(RigidBodyId) = 1.f;
		}
		else
		{
			Particles.InvM(RigidBodyId) = 1.f / Parameters.Mass;
		}

		Particles.X(RigidBodyId) = WorldTransform.TransformPosition(CenterOfMass);
		Particles.V(RigidBodyId) = Chaos::FVec3(Parameters.InitialLinearVelocity);
		Particles.R(RigidBodyId) = WorldTransform.GetRotation().GetNormalized();
		Particles.W(RigidBodyId) = Chaos::FVec3(Parameters.InitialAngularVelocity);
		Particles.P(RigidBodyId) = Particles.X(RigidBodyId);
		Particles.Q(RigidBodyId) = Particles.R(RigidBodyId);

		const FVector SideSquared(Bounds.GetSize()[0] * Bounds.GetSize()[0], Bounds.GetSize()[1] * Bounds.GetSize()[1], Bounds.GetSize()[2] * Bounds.GetSize()[2]);
		const FVector3f Inertia((Chaos::FRealSingle)(Parameters.Mass * (SideSquared.Y + SideSquared.Z) / 12.f), (Chaos::FRealSingle)(Parameters.Mass * (SideSquared.X + SideSquared.Z) / 12.f), (Chaos::FRealSingle)(Parameters.Mass * (SideSquared.X + SideSquared.Y) / 12.f));
		Particles.I(RigidBodyId) = Inertia;
		Particles.InvI(RigidBodyId) = Chaos::TVec3<Chaos::FRealSingle>(1.f / Inertia.X, 1.f / Inertia.Y, 1.f / Inertia.Z);

		if(Parameters.ObjectType == EObjectStateTypeEnum::Chaos_Object_Sleeping)
		{
			Particles.SetObjectState(RigidBodyId, Chaos::EObjectStateType::Sleeping);
			Particles.SetSleeping(RigidBodyId, true);
		}
		else if(Parameters.ObjectType != EObjectStateTypeEnum::Chaos_Object_Dynamic)
		{
			Particles.InvM(RigidBodyId) = 0.f;
			Particles.InvI(RigidBodyId) = Chaos::TVec3<Chaos::FRealSingle>(0);
			Particles.SetObjectState(RigidBodyId, Chaos::EObjectStateType::Kinematic);
		}
		else
		{
			Particles.SetObjectState(RigidBodyId, Chaos::EObjectStateType::Dynamic);
		}

		if (ensure(Parameters.MeshVertexPositions.Size()))
		{
			// Add collision vertices
			check(Particles.CollisionParticles(RigidBodyId) == nullptr);
			Particles.SetCollisionParticles(RigidBodyId, MoveTemp(Parameters.MeshVertexPositions));
			if (Particles.CollisionParticles(RigidBodyId)->Size())
			{
				Particles.CollisionParticles(RigidBodyId)->UpdateAccelerationStructures();
			}
		}

#if TODO_REIMPLEMENT_SET_PHYSICS_MATERIAL
		GetSolver()->SetPhysicsMaterial(RigidBodyId, Parameters.PhysicalMaterial);
#endif

		bInitializedState = true;
	}
}

void FStaticMeshPhysicsProxy::ParameterUpdateCallback(FParticlesType& InParticles, const float InTime)
{

}

void FStaticMeshPhysicsProxy::DisableCollisionsCallback(TSet<TTuple<int32, int32>>& InPairs)
{

}

void FStaticMeshPhysicsProxy::AddForceCallback(FParticlesType& InParticles, const float InDt, const int32 InIndex)
{

}

void FStaticMeshPhysicsProxy::OnRemoveFromScene()
{
	ensure(false);	//GetSolver needs a trait, but we don't know it - this class changing anyway
#if ADD_TRAITS_TO_STATIC_MESH_PROXY
	// Disable the particle we added
	Chaos::FPhysicsSolver* CurrSolver = GetSolver();

	if(CurrSolver && RigidBodyId != INDEX_NONE)
	{
#if TODO_REIMPLEMENT_GET_RIGID_PARTICLES
		// #BG TODO Special case here because right now we reset/realloc the evolution per geom component
		// in endplay which clears this out. That needs to not happen and be based on world shutdown
		if(CurrSolver->GetRigidParticles().Size() == 0)
		{
			return;
		}

		CurrSolver->GetEvolution()->DisableParticle(RigidBodyId);
		CurrSolver->GetRigidClustering().GetTopLevelClusterParents().Remove(RigidBodyId);
#endif
	}
#endif
}

void FStaticMeshPhysicsProxy::BufferPhysicsResults()
{
	SCOPE_CYCLE_COUNTER(STAT_CacheResultStaticMesh);
	Results.GetPhysicsDataForWrite() = SimTransform;
}

void FStaticMeshPhysicsProxy::FlipBuffer()
{
	Results.Flip();
}

bool FStaticMeshPhysicsProxy::PullFromPhysicsState(const int32 SolverSyncTimestamp)
{
	if(Parameters.ObjectType == EObjectStateTypeEnum::Chaos_Object_Dynamic && Parameters.bSimulating && SyncDynamicTransformFunc)
	{
		// Send transform to update callable
		SyncDynamicTransformFunc(Results.GetGameDataForRead());
	}

	return true;
}
