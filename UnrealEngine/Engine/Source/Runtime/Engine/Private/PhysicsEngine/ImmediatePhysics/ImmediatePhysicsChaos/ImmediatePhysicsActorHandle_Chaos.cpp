// Copyright Epic Games, Inc. All Rights Reserved.

#include "Physics/ImmediatePhysics/ImmediatePhysicsChaos/ImmediatePhysicsActorHandle_Chaos.h"

#include "Chaos/Box.h"
#include "Chaos/Capsule.h"
#include "Chaos/Evolution/PBDMinEvolution.h"
#include "Chaos/ImplicitObjectScaled.h"
#include "Chaos/MassProperties.h"
#include "Chaos/Particle/ParticleUtilities.h"
#include "Chaos/ParticleHandle.h"
#include "Chaos/PBDRigidParticles.h"
#include "Chaos/PBDRigidsEvolutionGBF.h"
#include "Chaos/Sphere.h"
#include "Chaos/TriangleMeshImplicitObject.h"
#include "Chaos/Utilities.h"

#include "Physics/Experimental/ChaosInterfaceUtils.h"

#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/BodyUtils.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"

//PRAGMA_DISABLE_OPTIMIZATION

namespace ImmediatePhysics_Chaos
{
	//
	// Utils
	//

	bool CreateDefaultGeometry(const FVector& Scale, FReal& OutMass, Chaos::FVec3& OutInertia, Chaos::FRigidTransform3& OutCoMTransform, TUniquePtr<Chaos::FImplicitObject>& OutGeom, TArray<TUniquePtr<Chaos::FPerShapeData>>& OutShapes)
	{
		using namespace Chaos;

		const FReal Mass = 1.0f;
		const FReal Radius = 1.0f * Scale.GetMax();

		auto ImplicitSphere = MakeUnique<Chaos::TSphere<FReal, 3>>(FVec3(0), Radius);
		auto NewShape = Chaos::FPerShapeData::CreatePerShapeData(OutShapes.Num(), MakeSerializable(ImplicitSphere));
		NewShape->UpdateShapeBounds(FTransform::Identity);
		NewShape->SetUserData(nullptr);
		NewShape->SetQueryEnabled(false);
		NewShape->SetSimEnabled(false);

		OutMass = Mass;
		OutInertia = TSphere<FReal, 3>::GetInertiaTensor(Mass, Radius).GetDiagonal();
		OutCoMTransform = FTransform::Identity;
		OutShapes.Emplace(MoveTemp(NewShape));
		OutGeom = MoveTemp(ImplicitSphere);

		return true;
	}

	TUniquePtr<Chaos::FImplicitObject> CloneGeometry(const Chaos::FImplicitObject* Geom, TArray<TUniquePtr<Chaos::FPerShapeData>>& OutShapes)
	{
		using namespace Chaos;

		EImplicitObjectType GeomType = GetInnerType(Geom->GetCollisionType());
		bool bIsInstanced = IsInstanced(Geom->GetCollisionType());
		bool bIsScaled = IsScaled(Geom->GetCollisionType());

		// Transformed HeightField
		if (GeomType == ImplicitObjectType::Transformed)
		{
			const TImplicitObjectTransformed<FReal, 3>* SrcTransformed = Geom->template GetObject<const TImplicitObjectTransformed<FReal, 3>>();
			if ((SrcTransformed != nullptr) && (SrcTransformed->GetTransformedObject()->GetType() == ImplicitObjectType::HeightField))
			{
				FImplicitObject* InnerGeom = const_cast<FImplicitObject*>(SrcTransformed->GetTransformedObject());
				TUniquePtr<TImplicitObjectTransformed<FReal, 3, false>> Cloned = MakeUnique<Chaos::TImplicitObjectTransformed<FReal, 3, false>>(InnerGeom, SrcTransformed->GetTransform());
				return Cloned;
			}
		}

		// Instanced Trimesh
		if (bIsInstanced && (GeomType == ImplicitObjectType::TriangleMesh))
		{
			const TImplicitObjectInstanced<FTriangleMeshImplicitObject>* SrcInstanced = Geom->template GetObject<const TImplicitObjectInstanced<FTriangleMeshImplicitObject>>();
			if (SrcInstanced != nullptr)
			{
				const TImplicitObjectInstanced<FTriangleMeshImplicitObject>::ObjectType InnerGeom = SrcInstanced->Object();
				TUniquePtr<TImplicitObjectInstanced<FTriangleMeshImplicitObject>> Cloned = MakeUnique<TImplicitObjectInstanced<FTriangleMeshImplicitObject>>(InnerGeom);
				return Cloned;
			}
		}

		return nullptr;
	}

	// Intended for use with Tri Mesh and Heightfields when cloning world simulation objects into the immediate scene
	bool CloneGeometry(FBodyInstance* BodyInstance, EActorType ActorType, const FVector& Scale, FReal& OutMass, Chaos::FVec3& OutInertia, Chaos::FRigidTransform3& OutCoMTransform, TUniquePtr<Chaos::FImplicitObject>& OutGeom, TArray<TUniquePtr<Chaos::FPerShapeData>>& OutShapes)
	{
		// We should only get non-simulated objects through this path, but you never know...
		if ((BodyInstance != nullptr) && !BodyInstance->bSimulatePhysics && BodyInstance->ActorHandle)
		{
			OutMass = 0.0f;
			OutInertia = FVector::ZeroVector;
			OutCoMTransform = FTransform::Identity;
			OutGeom = CloneGeometry(BodyInstance->ActorHandle->GetGameThreadAPI().Geometry().Get(), OutShapes);
			if (OutGeom != nullptr)
			{
				return true;
			}
		}

		return CreateDefaultGeometry(Scale, OutMass, OutInertia, OutCoMTransform, OutGeom, OutShapes);
	}

	bool CreateGeometry(FBodyInstance* BodyInstance, EActorType ActorType, const FVector& Scale, FReal& OutMass, Chaos::FVec3& OutInertia, Chaos::FRigidTransform3& OutCoMTransform, TUniquePtr<Chaos::FImplicitObject>& OutGeom, TArray<TUniquePtr<Chaos::FPerShapeData>>& OutShapes)
	{
		using namespace Chaos;

		OutMass = 0.0f;
		OutInertia = FVector::ZeroVector;
		OutCoMTransform = FTransform::Identity;

		// If there's no BodySetup, we may be cloning an in-world object and probably have a TriMesh or HeightField so try to just copy references
		// @todo(ccaulfield): make this cleaner - we should have a separate path for this
		if ((BodyInstance == nullptr) || (BodyInstance->BodySetup == nullptr) || (BodyInstance->BodySetup->CollisionTraceFlag == ECollisionTraceFlag::CTF_UseComplexAsSimple))
		{
			return CloneGeometry(BodyInstance, ActorType, Scale, OutMass, OutInertia, OutCoMTransform, OutGeom, OutShapes);
		}

		UBodySetup* BodySetup = BodyInstance->GetBodySetup();

		// Set the filter to collide with everything (we use a broad phase that only contains particle pairs that are explicitly set to collide)
		FBodyCollisionData BodyCollisionData;
		// @todo(chaos): we need an API for setting up filters
		BodyCollisionData.CollisionFilterData.SimFilter.Word1 = 0xFFFF;
		BodyCollisionData.CollisionFilterData.SimFilter.Word3 = 0xFFFF;
		BodyCollisionData.CollisionFlags.bEnableSimCollisionSimple = true;

		//BodyInstance->BuildBodyFilterData(BodyCollisionData.CollisionFilterData);
		BodyInstance->BuildBodyCollisionFlags(BodyCollisionData.CollisionFlags, BodyInstance->GetCollisionEnabled(), BodyInstance->BodySetup->GetCollisionTraceFlag() == CTF_UseComplexAsSimple);

		FGeometryAddParams AddParams;
		AddParams.bDoubleSided = BodySetup->bDoubleSidedGeometry;
		AddParams.CollisionData = BodyCollisionData;
		AddParams.CollisionTraceType = BodySetup->GetCollisionTraceFlag();
		AddParams.Scale = Scale;
		//AddParams.SimpleMaterial = SimpleMaterial;
		//AddParams.ComplexMaterials = TArrayView<UPhysicalMaterial*>(ComplexMaterials);
#if CHAOS_PARTICLE_ACTORTRANSFORM
		AddParams.LocalTransform = FTransform::Identity;
#else
		AddParams.LocalTransform = FRigidTransform3(OutCoMTransform.GetRotation().Inverse() * -OutCoMTransform.GetTranslation(), OutCoMTransform.GetRotation().Inverse());
#endif
		AddParams.WorldTransform = BodyInstance->GetUnrealWorldTransform();
		AddParams.Geometry = &BodySetup->AggGeom;
		AddParams.ChaosTriMeshes = MakeArrayView(BodySetup->ChaosTriMeshes);

		TArray<TUniquePtr<FImplicitObject>> Geoms;
		FShapesArray Shapes;
		ChaosInterface::CreateGeometry(AddParams, Geoms, Shapes);

		if (Geoms.Num() == 0)
		{
			return false;
		}

		if (ActorType == EActorType::DynamicActor)
		{
			// Whether each shape contributes to mass
			// @todo(chaos): it would be easier if ComputeMassProperties knew how to extract this info. Maybe it should be a flag in PerShapeData
			TArray<bool> bContributesToMass;
			bContributesToMass.Reserve(Shapes.Num());
			for (int32 ShapeIndex = 0; ShapeIndex < Shapes.Num(); ++ShapeIndex)
			{
				const TUniquePtr<FPerShapeData>& Shape = Shapes[ShapeIndex];
				const FKShapeElem* ShapeElem = FChaosUserData::Get<FKShapeElem>(Shape->GetUserData());
				bool bHasMass = ShapeElem && ShapeElem->GetContributeToMass();
				bContributesToMass.Add(bHasMass);
			}

			// bInertaScaleIncludeMass = true is to match legacy physics behaviour. This will scale the inertia by the change in mass (density x volumescale) 
			// as well as the dimension change even though we don't actually change the mass.
			const bool bInertaScaleIncludeMass = true;
			FMassProperties MassProperties = BodyUtils::ComputeMassProperties(BodyInstance, Shapes, bContributesToMass, FTransform::Identity, bInertaScaleIncludeMass);
			OutMass = MassProperties.Mass;
			OutInertia = MassProperties.InertiaTensor.GetDiagonal();
			OutCoMTransform = FTransform(MassProperties.RotationOfMass, MassProperties.CenterOfMass);
		}

		// If we have multiple root shapes, wrap them in a union
		if (Geoms.Num() == 1)
		{
			OutGeom = MoveTemp(Geoms[0]);
		}
		else
		{
			OutGeom = MakeUnique<FImplicitObjectUnion>(MoveTemp(Geoms));
		}

		for (auto& Shape : Shapes)
		{
			OutShapes.Emplace(MoveTemp(Shape));
		}

		return true;
	}

	//
	// Actor Handle
	//

	FActorHandle::FActorHandle(
		Chaos::FPBDRigidsSOAs& InParticles, 
		Chaos::TArrayCollectionArray<Chaos::FVec3>& InParticlePrevXs, 
		Chaos::TArrayCollectionArray<Chaos::FRotation3>& InParticlePrevRs, 
		EActorType ActorType, 
		FBodyInstance* BodyInstance, 
		const FTransform& InTransform)
		: Particles(InParticles)
		, ParticleHandle(nullptr)
		, ParticlePrevXs(InParticlePrevXs)
		, ParticlePrevRs(InParticlePrevRs)
	{
		using namespace Chaos;

		const FTransform Transform = FTransform(InTransform.GetRotation(), InTransform.GetTranslation());
		const FVector Scale = InTransform.GetScale3D();

		FReal Mass = 0;
		FVec3 Inertia = FVec3::OneVector;
		FRigidTransform3 CoMTransform = FRigidTransform3::Identity;
		if (CreateGeometry(BodyInstance, ActorType, Scale, Mass, Inertia, CoMTransform, Geometry, Shapes))
		{
			switch (ActorType)
			{
			case EActorType::StaticActor:
				ParticleHandle = Particles.CreateStaticParticles(1, nullptr, FGeometryParticleParameters())[0];
				break;
			case EActorType::KinematicActor:
				ParticleHandle = Particles.CreateKinematicParticles(1, nullptr, FKinematicGeometryParticleParameters())[0];
				break;
			case EActorType::DynamicActor:
				ParticleHandle = Particles.CreateDynamicParticles(1, nullptr, FPBDRigidParticleParameters())[0];
				break;
			}

			if (ParticleHandle != nullptr)
			{
				SetWorldTransform(Transform);

				ParticleHandle->SetGeometry(MakeSerializable(Geometry));

				// Set the collision filter data for the shapes to collide with everything.
				// Even though we already tried to do this when we created the original shapes array, 
				// that gets thrown away and we need to do it here. This is not a good API
				FCollisionData CollisionData;
				CollisionData.SimData.Word1 = 0xFFFFF;
				CollisionData.SimData.Word3 = 0xFFFFF;
				CollisionData.bSimCollision = 1;
				for (const auto& Shape : ParticleHandle->ShapesArray())
				{
					Shape->SetCollisionData(CollisionData);
				}

				if (Geometry && Geometry->HasBoundingBox())
				{
					ParticleHandle->SetHasBounds(true);
					ParticleHandle->SetLocalBounds(Geometry->BoundingBox());
					ParticleHandle->UpdateWorldSpaceState(FRigidTransform3(ParticleHandle->X(), ParticleHandle->R()), FVec3(0));
				}

				if (auto* Kinematic = ParticleHandle->CastToKinematicParticle())
				{
					Kinematic->SetV(FVector3f::ZeroVector);
					Kinematic->SetW(FVector3f::ZeroVector);
				}

				auto* Dynamic = ParticleHandle->CastToRigidParticle();
				if (Dynamic && Dynamic->ObjectState() == EObjectStateType::Dynamic)
				{
					FReal MassInv = (Mass > 0.0f) ? 1.0f / Mass : 0.0f;
					FVec3 InertiaInv = (Mass > 0.0f) ? Inertia.Reciprocal() : FVec3::ZeroVector;
					Dynamic->SetM(Mass);
					Dynamic->SetInvM(MassInv);
					Dynamic->SetCenterOfMass(CoMTransform.GetTranslation());
					Dynamic->SetRotationOfMass(CoMTransform.GetRotation());
					Dynamic->SetI(TVec3<FRealSingle>( Inertia.X, Inertia.Y, Inertia.Z ));
					Dynamic->SetInvI(TVec3<FRealSingle>(InertiaInv.X, InertiaInv.Y, InertiaInv.Z ));
					if (BodyInstance != nullptr)
					{
						Dynamic->SetInertiaConditioningEnabled(BodyInstance->IsInertiaConditioningEnabled());
						Dynamic->SetLinearEtherDrag(BodyInstance->LinearDamping);
						Dynamic->SetAngularEtherDrag(BodyInstance->AngularDamping);
						Dynamic->SetGravityEnabled(BodyInstance->bEnableGravity);
					}
					Dynamic->Disabled() = true;
				}
			}
		}
	}

	FActorHandle::~FActorHandle()
	{
		if (ParticleHandle != nullptr)
		{
			Particles.DestroyParticle(ParticleHandle);
			ParticleHandle = nullptr;
			Geometry = nullptr;
		}
	}

	Chaos::FGenericParticleHandle FActorHandle::Handle() const
	{
		return { ParticleHandle };
	}

	Chaos::FGeometryParticleHandle* FActorHandle::GetParticle()
	{
		return ParticleHandle;
	}

	const Chaos::FGeometryParticleHandle* FActorHandle::GetParticle() const
	{
		return ParticleHandle;
	}

	bool FActorHandle::GetEnabled() const
	{
		const Chaos::FConstGenericParticleHandle Particle = ParticleHandle;
		return !Particle->Disabled();
	}

	void FActorHandle::SetEnabled(bool bEnabled)
	{
		Chaos::FPBDRigidParticleHandle* Dynamic = ParticleHandle->CastToRigidParticle();
		if (Dynamic && Dynamic->ObjectState() == Chaos::EObjectStateType::Dynamic)
		{
			Dynamic->Disabled() = !bEnabled;
		}
	}


	void FActorHandle::InitWorldTransform(const FTransform& WorldTM)
	{
		using namespace Chaos;

		SetWorldTransform(WorldTM);

		if (auto* Kinematic = ParticleHandle->CastToKinematicParticle())
		{
			Kinematic->V() = FVec3(0);
			Kinematic->W() = FVec3(0);
			Kinematic->KinematicTarget().Clear();
		}
	}

	void FActorHandle::SetWorldTransform(const FTransform& WorldTM)
	{
		using namespace Chaos;

		FParticleUtilitiesXR::SetActorWorldTransform(FGenericParticleHandle(ParticleHandle), WorldTM);

		auto* Dynamic = ParticleHandle->CastToRigidParticle();
		if(Dynamic && Dynamic->ObjectState() == Chaos::EObjectStateType::Dynamic)
		{
			Dynamic->P() = Dynamic->X();
			Dynamic->Q() = Dynamic->R();
			Dynamic->AuxilaryValue(ParticlePrevXs) = Dynamic->P();
			Dynamic->AuxilaryValue(ParticlePrevRs) = Dynamic->Q();
		}
	}

	void FActorHandle::SetIsKinematic(bool bKinematic)
	{
#if IMMEDIATEPHYSICS_CHAOS_TODO
		// This needs to destroy and recreate the particle
#endif
	}

	bool FActorHandle::GetIsKinematic() const
	{
		return Handle()->IsKinematic();
	}

	const FKinematicTarget& FActorHandle::GetKinematicTarget() const
	{
		check(ParticleHandle->CastToKinematicParticle());
		return ParticleHandle->CastToKinematicParticle()->KinematicTarget();
	}

	FKinematicTarget& FActorHandle::GetKinematicTarget()
	{
		check(ParticleHandle->CastToKinematicParticle());
		return ParticleHandle->CastToKinematicParticle()->KinematicTarget();
	}

	void FActorHandle::SetKinematicTarget(const FTransform& WorldTM)
	{
		using namespace Chaos;

		if (ensure(GetIsKinematic()))
		{
			FGenericParticleHandle GenericHandle(ParticleHandle);
			FTransform ParticleTransform = FParticleUtilities::ActorWorldToParticleWorld(GenericHandle, WorldTM);

			GetKinematicTarget().SetTargetMode(ParticleTransform);
		}

	}

	bool FActorHandle::HasKinematicTarget() const
	{
		if (GetIsKinematic())
		{
			return GetKinematicTarget().GetMode() == Chaos::EKinematicTargetMode::Position;
		}
		return false;
	}

	bool FActorHandle::IsSimulated() const
	{
		return ParticleHandle->CastToRigidParticle() != nullptr && ParticleHandle->ObjectState() == Chaos::EObjectStateType::Dynamic;
	}

	FTransform FActorHandle::GetWorldTransform() const
	{
		using namespace Chaos;

		return FParticleUtilities::GetActorWorldTransform(FGenericParticleHandle(ParticleHandle));
	}

	void FActorHandle::SetLinearVelocity(const FVector& NewLinearVelocity)
	{
		using namespace Chaos;

		if (FKinematicGeometryParticleHandle* KinematicParticleHandle = ParticleHandle->CastToKinematicParticle())
		{
			KinematicParticleHandle->SetV(NewLinearVelocity);
		}
	}

	FVector FActorHandle::GetLinearVelocity() const
	{
		return Handle()->V();
	}

	void FActorHandle::SetAngularVelocity(const FVector& NewAngularVelocity)
	{
		using namespace Chaos;

		if (FKinematicGeometryParticleHandle* KinematicParticleHandle = ParticleHandle->CastToKinematicParticle())
		{
			KinematicParticleHandle->SetW(NewAngularVelocity);
		}
	}

	FVector FActorHandle::GetAngularVelocity() const
	{
		return Handle()->W();
	}

	void FActorHandle::AddForce(const FVector& Force)
	{
		using namespace Chaos;

		if (FPBDRigidParticleHandle* Rigid = Handle()->CastToRigidParticle())
		{
			Rigid->AddForce(Force);
		}
	}

	void FActorHandle::AddTorque(const FVector& Torque)
	{
		using namespace Chaos;

		if (FPBDRigidParticleHandle * Rigid = Handle()->CastToRigidParticle())
		{
			Rigid->AddTorque(Torque);
		}
	}

	void FActorHandle::AddRadialForce(const FVector& Origin, FReal Strength, FReal Radius, ERadialImpulseFalloff Falloff, EForceType ForceType)
	{
		using namespace Chaos;

		if (FPBDRigidParticleHandle* Rigid = Handle()->CastToRigidParticle())
		{
			const FRigidTransform3& PCOMTransform = FParticleUtilities::GetCoMWorldTransform(Rigid);
			FVec3 Delta = PCOMTransform.GetTranslation() - Origin;

			const FReal Mag = Delta.Size();
			if (Mag > Radius)
			{
				return;
			}
			Delta.Normalize();

			FReal ImpulseMag = Strength;
			if (Falloff == RIF_Linear)
			{
				ImpulseMag *= ((FReal)1. - (Mag / Radius));
			}

			const FVec3 PImpulse = Delta * ImpulseMag;
			const FVec3 ApplyDelta = (ForceType == EForceType::AddAcceleration || ForceType == EForceType::AddVelocity) ? PImpulse : PImpulse * Rigid->InvM();

			if (ForceType == EForceType::AddImpulse || ForceType == EForceType::AddVelocity)
			{
				Rigid->V() += ApplyDelta;
			}
			else
			{
				Rigid->Acceleration() += ApplyDelta * Rigid->InvM();
			}
		}
	}

	void FActorHandle::AddImpulseAtLocation(FVector Impulse, FVector Location)
	{
		using namespace Chaos;

		if (FPBDRigidParticleHandle* Rigid = Handle()->CastToRigidParticle())
		{
			FVector CoM = FParticleUtilities::GetCoMWorldPosition(Rigid);
			Chaos::FMatrix33 InvInertia = FParticleUtilities::GetWorldInvInertia(Rigid);
			Rigid->LinearImpulseVelocity() += Impulse * Rigid->InvM();
			Rigid->AngularImpulseVelocity() += InvInertia * FVector::CrossProduct(Location - CoM, Impulse);
		}
	}

	void FActorHandle::SetLinearDamping(FReal NewLinearDamping)
	{
		using namespace Chaos;

		if (FPBDRigidParticleHandle* Rigid = Handle()->CastToRigidParticle())
		{
			Rigid->LinearEtherDrag() = NewLinearDamping;
		}
	}

	FReal FActorHandle::GetLinearDamping() const
	{
		using namespace Chaos;

		if (FPBDRigidParticleHandle* Rigid = Handle()->CastToRigidParticle())
		{
			return Rigid->LinearEtherDrag();
		}
		return 0.0f;
	}

	void FActorHandle::SetAngularDamping(FReal NewAngularDamping)
	{
		using namespace Chaos;

		if (FPBDRigidParticleHandle* Rigid = Handle()->CastToRigidParticle())
		{
			Rigid->AngularEtherDrag() = NewAngularDamping;
		}
	}

	FReal FActorHandle::GetAngularDamping() const
	{
		using namespace Chaos;

		if (FPBDRigidParticleHandle* Rigid = Handle()->CastToRigidParticle())
		{
			return Rigid->AngularEtherDrag();
		}
		return 0.0f;
	}

	void FActorHandle::SetMaxLinearVelocitySquared(FReal NewMaxLinearVelocitySquared)
	{
#if IMMEDIATEPHYSICS_CHAOS_TODO
#endif
	}

	FReal FActorHandle::GetMaxLinearVelocitySquared() const
	{
#if IMMEDIATEPHYSICS_CHAOS_TODO
#endif
		return FLT_MAX;
	}

	void FActorHandle::SetMaxAngularVelocitySquared(FReal NewMaxAngularVelocitySquared)
	{
#if IMMEDIATEPHYSICS_CHAOS_TODO
#endif
	}

	FReal FActorHandle::GetMaxAngularVelocitySquared() const
	{
#if IMMEDIATEPHYSICS_CHAOS_TODO
#endif
		return FLT_MAX;
	}

	void FActorHandle::SetInverseMass(FReal NewInverseMass)
	{
		using namespace Chaos;

		FPBDRigidParticleHandle* Dynamic = ParticleHandle->CastToRigidParticle();
		if(Dynamic && Dynamic->ObjectState() == EObjectStateType::Dynamic)
		{
			FReal NewMass = (NewInverseMass > UE_SMALL_NUMBER) ? (FReal)1. / NewInverseMass : (FReal)0.;
			Dynamic->SetM(NewMass);
			Dynamic->SetInvM(NewInverseMass);
		}
	}

	FReal FActorHandle::GetInverseMass() const
	{
		return Handle()->InvM();
	}

	FReal FActorHandle::GetMass() const
	{
		return Handle()->M();
	}

	void FActorHandle::SetInverseInertia(const FVector& NewInverseInertia)
	{
		using namespace Chaos;

		FPBDRigidParticleHandle* Dynamic = ParticleHandle->CastToRigidParticle();
		if(Dynamic && Dynamic->ObjectState() == EObjectStateType::Dynamic)
		{
			Chaos::FVec3 NewInertia = FVector3f::ZeroVector;
			if ((NewInverseInertia.X > UE_SMALL_NUMBER) && (NewInverseInertia.Y > UE_SMALL_NUMBER) && (NewInverseInertia.Z > UE_SMALL_NUMBER))
			{
				NewInertia = FVector3f( 1.0f / NewInverseInertia.X , 1.0f / NewInverseInertia.Y, 1.0f / NewInverseInertia.Z );
			}
			Dynamic->SetI(TVec3<FRealSingle>(NewInertia.X, NewInertia.Y, NewInertia.Z ));
			Dynamic->SetInvI(TVec3<FRealSingle>(NewInverseInertia.X, NewInverseInertia.Y, NewInverseInertia.Z ));
			
			if (Dynamic->InertiaConditioningEnabled())
			{
				Dynamic->SetInertiaConditioningDirty();
			}
		}
	}

	FVector FActorHandle::GetInverseInertia() const
	{
		return FVector(Handle()->InvI());
	}

	FVector FActorHandle::GetInertia() const
	{
		return FVector(Handle()->I());
	}

	void FActorHandle::SetMaxDepenetrationVelocity(FReal NewMaxDepenetrationVelocity)
	{
#if IMMEDIATEPHYSICS_CHAOS_TODO
#endif
	}

	FReal FActorHandle::GetMaxDepenetrationVelocity() const
	{
#if IMMEDIATEPHYSICS_CHAOS_TODO
#endif
		return FLT_MAX;
	}

	void FActorHandle::SetMaxContactImpulse(FReal NewMaxContactImpulse)
	{
#if IMMEDIATEPHYSICS_CHAOS_TODO
#endif
	}

	FReal FActorHandle::GetMaxContactImpulse() const
	{
#if IMMEDIATEPHYSICS_CHAOS_TODO
#endif
		return FLT_MAX;
	}

	FTransform FActorHandle::GetLocalCoMTransform() const
	{
		return FTransform(Handle()->RotationOfMass(), Handle()->CenterOfMass());
	}

	int32 FActorHandle::GetLevel() const
	{
		return Level;
	}

	void FActorHandle::SetLevel(int32 InLevel)
	{
		Level = InLevel;
	}
}
