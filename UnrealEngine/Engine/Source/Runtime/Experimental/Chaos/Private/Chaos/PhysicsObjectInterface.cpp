// Copyright Epic Games, Inc. All Rights Reserved.
#include "Chaos/PhysicsObjectInterface.h"

#include "Chaos/CastingUtilities.h"
#include "Chaos/CollisionFilterData.h"
#include "Chaos/PhysicsObjectCollisionInterface.h"
#include "Chaos/PhysicsObjectInternalInterface.h"
#include "Chaos/PhysicsObjectInternal.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"
#include "Math/Transform.h"
#include "Chaos/ChaosEngineInterface.h"

namespace PhysicsObjectInterfaceCVars
{
	static float StrainModifier = 200;
	static FAutoConsoleVariableRef CVarStrainModifier(
		TEXT("Chaos.Debug.StrainModifier"),
		StrainModifier,
		TEXT("Modify the strain by this factor"),
		ECVF_Default
	);
}

namespace
{
	template<Chaos::EThreadContext Id>
	void SetParticleStateHelper(const Chaos::FPhysicsObjectHandle PhysicsObject, Chaos::EObjectStateType State)
	{
		if (!PhysicsObject)
		{
			return;
		}

		IPhysicsProxyBase* Proxy = PhysicsObject->PhysicsProxy();
		Chaos::TThreadParticle<Id>* Particle = PhysicsObject->GetParticle<Id>();
		if (!Particle || !Proxy)
		{
			return;
		}

		if (Chaos::TThreadRigidParticle<Id>* Rigid = Particle->CastToRigidParticle())
		{
			if constexpr (Id == Chaos::EThreadContext::External)
			{
				if (Proxy->GetType() == EPhysicsProxyType::SingleParticleProxy)
				{
					// Easiest way to maintain the same behavior as what we currently have for the single particle case on the game thread.
					static_cast<Chaos::FSingleParticlePhysicsProxy*>(Proxy)->GetGameThreadAPI().SetObjectState(State);
				}
				else
				{
					Rigid->SetObjectState(State, false, false);

					// In the case of the geometry collection, it won't marshal the state from the game thread to the physics thread
					// so we need to do it for it manually. 
					if (Proxy->GetType() == EPhysicsProxyType::GeometryCollectionType)
					{
						if (Chaos::FPhysicsSolverBase* Solver = Proxy->GetSolverBase())
						{
							Solver->EnqueueCommandImmediate(
								[PhysicsObject, State]() {
									SetParticleStateHelper<Chaos::EThreadContext::Internal>(PhysicsObject, State);
								}
							);
						}
					}
				}
			}
			else
			{
				if (Chaos::FPBDRigidsSolver* Solver = Proxy->GetSolver<Chaos::FPBDRigidsSolver>())
				{
					if (Chaos::FPBDRigidsSolver::FPBDRigidsEvolution* Evolution = Solver->GetEvolution())
					{
						Evolution->SetParticleObjectState(Rigid, State);
					}
				}
			}
		}
	}

	template<typename T>
	float AddRadialImpulseHelper(T ParticleHandle, FVector Origin, float Radius, float Strength, enum ERadialImpulseFalloff Falloff, float VelocityRatio = 1.0f, bool bInvalidate = true)
	{
		using namespace Chaos;

		if (ParticleHandle == nullptr)
		{
			return 0.0f;
		}

		const FVec3 CurrentImpulseVelocity = ParticleHandle->LinearImpulseVelocity();

		float FalloffStrength = Strength;
		if (Falloff == ERadialImpulseFalloff::RIF_Linear)
		{
			//Radius should always be greater than distance - if we get here and it's not, something has gone
			//wrong with detection
			double Distance = FVec3::Distance(ParticleHandle->X(), Origin);
			FalloffStrength = static_cast<float>(FalloffStrength * (1.0f - FMath::Min(1.0f, Distance / Radius)));
		}

		FVec3 Impulse = ParticleHandle->X() - Origin;
		Impulse = Impulse.GetSafeNormal();
		Impulse = Impulse * FalloffStrength;
		FVec3 Velocity = Impulse * ParticleHandle->InvM() * VelocityRatio;

		FVec3 FinalVeloc = CurrentImpulseVelocity + Velocity;

		ParticleHandle->SetLinearImpulseVelocity(CurrentImpulseVelocity + Velocity, bInvalidate);

		return FalloffStrength;
	}
}

FName FClosestPhysicsObjectResult::HitName() const
{
	if (!PhysicsObject)
	{
		return NAME_None;
	}
	return Chaos::FPhysicsObjectInterface::GetName(PhysicsObject);
}

namespace Chaos
{
	template<EThreadContext Id>
	FPhysicsObjectHandle FReadPhysicsObjectInterface<Id>::GetRootObject(const FConstPhysicsObjectHandle Object)
	{
		if (!Object)
		{
			return nullptr;
		}

		return Object->GetRootObject<Id>();
	}

	template<EThreadContext Id>
	bool FReadPhysicsObjectInterface<Id>::HasChildren(const FConstPhysicsObjectHandle Object)
	{
		return Object ? Object->HasChildren<Id>() : false;
	}

	template<EThreadContext Id>
	int32 FReadPhysicsObjectInterface<Id>::GetClusterHierarchyLevel(const FConstPhysicsObjectHandle Object)
	{
		if (!Object)
		{
			return INDEX_NONE;
		}

		// TODO: I think the GC rest collection caches this information. If we're on the GT it might be
		// better for perf to try and grab that information somehow instead of walking the hierarchy.

		int32 CurrentLevel = 0;

		FPhysicsObjectHandle Parent = Object->GetParentObject<Id>();
		while (Parent)
		{
			// Being part of a cluster union doesn't count in terms of its default cluster hierarchy level.
			if (Parent->PhysicsProxy() != Object->PhysicsProxy())
			{
				break;
			}

			Parent = Parent->GetParentObject<Id>();
			++CurrentLevel;
		}

		return CurrentLevel;
	}

	template<EThreadContext Id>
	FTransform FReadPhysicsObjectInterface<Id>::GetTransform(const FConstPhysicsObjectHandle Object)
	{
		return FTransform{ GetR(Object), GetX(Object) };
	}

	template<EThreadContext Id>
	FVector FReadPhysicsObjectInterface<Id>::GetX(const FConstPhysicsObjectHandle Object)
	{
		if (!Object)
		{
			return FVector::Zero();
		}

		if (TThreadParticle<Id>* Particle = Object->GetParticle<Id>())
		{
			return Particle->X();
		}

		return FVector::Zero();
	}

	template<EThreadContext Id>
	FVector FReadPhysicsObjectInterface<Id>::GetCoM(const FConstPhysicsObjectHandle Object)
	{
		if (!Object)
		{
			return FVector::Zero();
		}

		if (TThreadParticle<Id>* Particle = Object->GetParticle<Id>())
		{
			if (Chaos::TThreadRigidParticle<Id>* Rigid = Particle->CastToRigidParticle())
			{
				return Rigid->CenterOfMass();
			}
		}

		return FVector::Zero();
	}

	template<EThreadContext Id>
	FVector FReadPhysicsObjectInterface<Id>::GetWorldCoM(const FConstPhysicsObjectHandle Object)
	{
		return GetX(Object) + GetR(Object).RotateVector(GetCoM(Object));
	}

	template<EThreadContext Id>
	FQuat FReadPhysicsObjectInterface<Id>::GetR(const FConstPhysicsObjectHandle Object)
	{
		if (!Object)
		{
			return FQuat::Identity;
		}

		if (TThreadParticle<Id>* Particle = Object->GetParticle<Id>())
		{
			return Particle->R();
		}

		return FQuat::Identity;
	}

	template<EThreadContext Id>
	FVector FReadPhysicsObjectInterface<Id>::GetV(const FConstPhysicsObjectHandle Object)
	{
		if (!Object)
		{
			return FVector::Zero();
		}

		if (TThreadParticle<Id>* Particle = Object->GetParticle<Id>())
		{
			if (Chaos::TThreadRigidParticle<Id>* Rigid = Particle->CastToRigidParticle())
			{
				return Rigid->V();
			}
		}

		return FVector::Zero();
	}

	template<EThreadContext Id>
	FVector FReadPhysicsObjectInterface<Id>::GetW(const FConstPhysicsObjectHandle Object)
	{
		if (!Object)
		{
			return FVector::Zero();
		}

		if (TThreadParticle<Id>* Particle = Object->GetParticle<Id>())
		{
			if (Chaos::TThreadRigidParticle<Id>* Rigid = Particle->CastToRigidParticle())
			{
				return Rigid->W();
			}
		}

		return FVector::Zero();
	}

	template<EThreadContext Id>
	FSpatialAccelerationIdx FReadPhysicsObjectInterface<Id>::GetSpatialIndex(const FConstPhysicsObjectHandle Object)
	{
		if (!Object)
		{
			return {};
		}

		if (TThreadParticle<Id>* Particle = Object->GetParticle<Id>())
		{
			return Particle->SpatialIdx();
		}

		return {};
	}

	template<EThreadContext Id>
	TThreadParticle<Id>* FReadPhysicsObjectInterface<Id>::GetParticle(const FConstPhysicsObjectHandle Handle)
	{
		if (!Handle)
		{
			return nullptr;
		}
		return Handle->GetParticle<Id>();
	}

	template<EThreadContext Id>
	TThreadRigidParticle<Id>* FReadPhysicsObjectInterface<Id>::GetRigidParticle(const FConstPhysicsObjectHandle Handle)
	{
		if (Handle)
		{
			if (TThreadParticle<Id>* Particle = Handle->GetParticle<Id>())
			{
				if (TThreadRigidParticle<Id>* RigidParticle = Particle->CastToRigidParticle())
				{
					return RigidParticle;
				}
			}
		}

		return nullptr;
	}

	template<EThreadContext Id>
	TArray<TThreadParticle<Id>*> FReadPhysicsObjectInterface<Id>::GetAllParticles(TArrayView<const FConstPhysicsObjectHandle> InObjects)
	{
		TArray<TThreadParticle<Id>*> Particles;
		Particles.Reserve(InObjects.Num());

		for (const FConstPhysicsObjectHandle& Handle : InObjects)
		{
			if (!Handle)
			{
				continue;
			}

			if (TThreadParticle<Id>* Particle = Handle->GetParticle<Id>())
			{
				Particles.Add(Particle);
			}
		}

		return Particles;
	}

	template<EThreadContext Id>
	TArray<TThreadRigidParticle<Id>*> FReadPhysicsObjectInterface<Id>::GetAllRigidParticles(TArrayView<const FConstPhysicsObjectHandle> InObjects)
	{
		TArray<TThreadRigidParticle<Id>*> Particles;
		Particles.Reserve(InObjects.Num());

		for (const FConstPhysicsObjectHandle& Handle : InObjects)
		{
			if (!Handle)
			{
				continue;
			}

			if (TThreadParticle<Id>* Particle = Handle->GetParticle<Id>())
			{
				if (TThreadRigidParticle<Id>* RigidParticle = Particle->CastToRigidParticle())
				{
					Particles.Add(RigidParticle);
				}
			}
		}

		return Particles;
	}

	template<EThreadContext Id>
	TArray<FPerShapeData*> FReadPhysicsObjectInterface<Id>::GetAllShapes(TArrayView<const FConstPhysicsObjectHandle> InObjects)
	{
		TArray<FPerShapeData*> AllShapes;

		for (const FConstPhysicsObjectHandle Object : InObjects)
		{
			if (!Object)
			{
				continue;
			}

			if (TThreadParticle<Id>* Particle = Object->GetParticle<Id>())
			{
				const Chaos::FShapesArray& ShapesArray = Particle->ShapesArray();
				for (const TUniquePtr<Chaos::FPerShapeData>& Shape : ShapesArray)
				{
					AllShapes.Add(Shape.Get());
				}
			}
		}

		return AllShapes;
	}

	template<EThreadContext Id>
	TArray<TThreadShapeInstance<Id>*> FReadPhysicsObjectInterface<Id>::GetAllThreadShapes(TArrayView<const FConstPhysicsObjectHandle> InObjects)
	{
		TArray<TThreadShapeInstance<Id>*> AllShapes;

		for (const FConstPhysicsObjectHandle Object : InObjects)
		{
			if (!Object)
			{
				continue;
			}

			if (TThreadParticle<Id>* Particle = Object->GetParticle<Id>())
			{
				for (const TUniquePtr<TThreadShapeInstance<Id>>& Shape : Particle->ShapeInstances())
				{
					AllShapes.Add(Shape.Get());
				}
			}
		}

		return AllShapes;
	}

	template<EThreadContext Id>
	void FReadPhysicsObjectInterface<Id>::VisitEveryShape(TArrayView<const FConstPhysicsObjectHandle> InObjects, TFunctionRef<bool(const FConstPhysicsObjectHandle, TThreadShapeInstance<Id>*)> Lambda)
	{
		for (const FConstPhysicsObjectHandle Object : InObjects)
		{
			if (!Object)
			{
				continue;
			}

			if (TThreadParticle<Id>* Particle = Object->GetParticle<Id>())
			{
				for (const TUniquePtr<TThreadShapeInstance<Id>>& Shape : Particle->ShapeInstances())
				{
					if (Lambda(Object, Shape.Get()))
					{
						return;
					}
				}
			}
		}
	}

	template<EThreadContext Id>
	bool FReadPhysicsObjectInterface<Id>::GetPhysicsObjectOverlap(const FConstPhysicsObjectHandle ObjectA, const FConstPhysicsObjectHandle ObjectB, bool bTraceComplex, Chaos::FOverlapInfo& OutOverlap)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FReadPhysicsObjectInterface<Id>::GetPhysicsObjectOverlap);
		FPhysicsObjectCollisionInterface Interface{ *this };
		// This is slow and inefficient and hence deprecated.
		bool bRetOverlap = false;
		if (OutOverlap.MTD)
		{
			bRetOverlap |= Interface.PhysicsObjectOverlapWithMTD(ObjectA, FTransform::Identity, ObjectB, FTransform::Identity, bTraceComplex, *OutOverlap.MTD);
		}

		if (OutOverlap.AxisOverlap)
		{
			bRetOverlap |= Interface.PhysicsObjectOverlapWithAABB(ObjectA, FTransform::Identity, ObjectB, FTransform::Identity, bTraceComplex, FVector::Zero(), *OutOverlap.AxisOverlap);
		}

		return bRetOverlap;
	}

	template<EThreadContext Id>
	bool FReadPhysicsObjectInterface<Id>::GetPhysicsObjectOverlapWithTransform(const FConstPhysicsObjectHandle ObjectA, const FTransform& InTransformA, const FConstPhysicsObjectHandle ObjectB, const FTransform& InTransformB, bool bTraceComplex, Chaos::FOverlapInfo& OutOverlap)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FReadPhysicsObjectInterface<Id>::GetPhysicsObjectOverlapWithTransform);
		FPhysicsObjectCollisionInterface Interface{ *this };
		// This is slow and inefficient and hence deprecated.
		bool bRetOverlap = false;
		if (OutOverlap.MTD)
		{
			bRetOverlap |= Interface.PhysicsObjectOverlapWithMTD(ObjectA, InTransformA, ObjectB, InTransformB, bTraceComplex, *OutOverlap.MTD);
		}

		if (OutOverlap.AxisOverlap)
		{
			bRetOverlap |= Interface.PhysicsObjectOverlapWithAABB(ObjectA, InTransformA, ObjectB, InTransformB, bTraceComplex, FVector::Zero(), *OutOverlap.AxisOverlap);
		}

		return bRetOverlap;
	}

	template<EThreadContext Id>
	bool FReadPhysicsObjectInterface<Id>::AreAllValid(TArrayView<const FConstPhysicsObjectHandle> InObjects)
	{
		bool bCheck = !InObjects.IsEmpty();
		for (const FConstPhysicsObjectHandle Object : InObjects)
		{
			bCheck &= (Object != nullptr && Object->IsValid());
		}
		return bCheck;
	}

	template<EThreadContext Id>
	bool FReadPhysicsObjectInterface<Id>::AreAllKinematic(TArrayView<const FConstPhysicsObjectHandle> InObjects)
	{
		bool bCheck = !InObjects.IsEmpty();
		for (const FConstPhysicsObjectHandle Object : InObjects)
		{
			bCheck &= (Object && Object->IsValid() && Object->ObjectState<Id>() == EObjectStateType::Kinematic);
		}
		return bCheck;
	}

	template<EThreadContext Id>
	bool FReadPhysicsObjectInterface<Id>::AreAllSleeping(TArrayView<const FConstPhysicsObjectHandle> InObjects)
	{
		bool bCheck = !InObjects.IsEmpty();
		for (const FConstPhysicsObjectHandle Object : InObjects)
		{
			bCheck &= (Object && Object->IsValid() && Object->ObjectState<Id>() == EObjectStateType::Sleeping);
		}
		return bCheck;
	}

	template<EThreadContext Id>
	bool FReadPhysicsObjectInterface<Id>::AreAllRigidBody(TArrayView<const FConstPhysicsObjectHandle> InObjects)
	{
		bool bCheck = !InObjects.IsEmpty();
		for (const FConstPhysicsObjectHandle Object : InObjects)
		{
			bCheck &= (Object && Object->IsValid() && Object->ObjectState<Id>() != EObjectStateType::Static);
		}
		return bCheck;
	}

	template<EThreadContext Id>
	bool FReadPhysicsObjectInterface<Id>::AreAllDynamic(TArrayView<const FConstPhysicsObjectHandle> InObjects)
	{
		bool bCheck = !InObjects.IsEmpty();
		for (const FConstPhysicsObjectHandle Object : InObjects)
		{
			bCheck &= (Object && Object->IsValid() && Object->ObjectState<Id>() == EObjectStateType::Dynamic);
		}
		return bCheck;
	}

	template<EThreadContext Id>
	bool FReadPhysicsObjectInterface<Id>::AreAllDisabled(TArrayView<const FConstPhysicsObjectHandle> InObjects)
	{
		bool bDisabled = !InObjects.IsEmpty();
		for (const FConstPhysicsObjectHandle Object : InObjects)
		{
			bool bParticleDisabled = true;
			if (Object)
			{
				if (TThreadParticle<Id>* Particle = Object->GetParticle<Id>())
				{
					bParticleDisabled = FPhysicsObject::IsParticleDisabled<Id>(Particle);
				}
			}
			bDisabled &= bParticleDisabled;
		}
		return bDisabled;
	}

	template<EThreadContext Id>
	bool FReadPhysicsObjectInterface<Id>::AreAllShapesQueryEnabled(TArrayView<const FConstPhysicsObjectHandle> InObjects)
	{
		if (InObjects.IsEmpty())
		{
			return false;
		}

		for (const FConstPhysicsObjectHandle Object : InObjects)
		{
			if (Object)
			{
				if (TThreadParticle<Id>* Particle = Object->GetParticle<Id>())
				{
					for (const TUniquePtr<FPerShapeData>& ShapeData : Particle->ShapesArray())
					{
						if (!ShapeData->GetCollisionData().bQueryCollision)
						{
							return false;
						}
					}
				}
			}
		}
		return true;
	}

	template<EThreadContext Id>
	float FReadPhysicsObjectInterface<Id>::GetMass(TArrayView<const FConstPhysicsObjectHandle> InObjects)
	{
		float Mass = 0.f;
		for (const FConstPhysicsObjectHandle Object : InObjects)
		{
			if (Object)
			{
				if (TThreadParticle<Id>* Particle = Object->GetParticle<Id>())
				{
					if (TThreadRigidParticle<Id>* Rigid = Particle->CastToRigidParticle())
					{
						Mass += static_cast<float>(Rigid->M());
					}
				}
			}
		}
		return Mass;
	}

	template<EThreadContext Id>
	FBox FReadPhysicsObjectInterface<Id>::GetBounds(TArrayView<const FConstPhysicsObjectHandle> InObjects)
	{
		FBox RetBox(ForceInit);
		for (const FConstPhysicsObjectHandle Object : InObjects)
		{
			if (!Object)
			{
				continue;
			}

			const TThreadParticle<Id>* Particle = Object->GetParticle<Id>();
			if (!Particle)
			{
				continue;
			}

			FBox ParticleBox(ForceInit);
			if (const FImplicitObject* Geometry = Particle->Geometry().Get(); Geometry && Geometry->HasBoundingBox())
			{
				const Chaos::FAABB3 Box = Geometry->BoundingBox();
				ParticleBox = FBox{ Box.Min(), Box.Max() };
			}

			if (ParticleBox.IsValid)
			{
				RetBox += ParticleBox;
			}
		}
		return RetBox;
	}

	template<EThreadContext Id>
	FBox FReadPhysicsObjectInterface<Id>::GetWorldBounds(TArrayView<const FConstPhysicsObjectHandle> InObjects)
	{
		FBox RetBox(ForceInit);
		for (const FConstPhysicsObjectHandle Object : InObjects)
		{
			if (!Object)
			{
				continue;
			}

			const TThreadParticle<Id>* Particle = Object->GetParticle<Id>();
			if (!Particle)
			{
				continue;
			}

			const FTransform WorldTransform = GetTransform(Object);

			FBox ParticleBox(ForceInit);
			if (const FImplicitObject* Geometry = Particle->Geometry().Get(); Geometry && Geometry->HasBoundingBox())
			{
				const Chaos::FAABB3 WorldBox = Geometry->CalculateTransformedBounds(WorldTransform);
				ParticleBox = FBox{ WorldBox.Min(), WorldBox.Max() };
			}

			if (ParticleBox.IsValid)
			{
				RetBox += ParticleBox;
			}
		}
		return RetBox;
	}

	template<EThreadContext Id>
	FClosestPhysicsObjectResult FReadPhysicsObjectInterface<Id>::GetClosestPhysicsBodyFromLocation(TArrayView<const FConstPhysicsObjectHandle> InObjects, const FVector& WorldLocation)
	{
		FClosestPhysicsObjectResult AggregateResult;
		for (const FConstPhysicsObjectHandle Object : InObjects)
		{
			if (!Object)
			{
				continue;
			}

			const TThreadParticle<Id>* Particle = Object->GetParticle<Id>();
			if (!Particle)
			{
				continue;
			}

			const FTransform WorldTransform = GetTransform(Object);
			const FVector LocalLocation = WorldTransform.InverseTransformPosition(WorldLocation);

			FClosestPhysicsObjectResult Result;

			if (const FImplicitObject* Geometry = Particle->Geometry().Get())
			{
				Result.PhysicsObject = const_cast<FPhysicsObjectHandle>(Object);

				Chaos::FVec3 Normal;
				Result.ClosestDistance = static_cast<double>(Geometry->PhiWithNormal(LocalLocation, Normal));
				Result.ClosestLocation = WorldTransform.TransformPosition(LocalLocation - Result.ClosestDistance * Normal);
			}

			if (!Result)
			{
				continue;
			}

			if (!AggregateResult || Result.ClosestDistance < AggregateResult.ClosestDistance)
			{
				AggregateResult = Result;
			}
		}
		return AggregateResult;
	}

	template<EThreadContext Id>
	FAccelerationStructureHandle FReadPhysicsObjectInterface<Id>::CreateAccelerationStructureHandle(const FConstPhysicsObjectHandle InObject)
	{
		return FAccelerationStructureHandle{InObject->GetParticle<Id>()};
	}

	template<EThreadContext Id>
	void FWritePhysicsObjectInterface<Id>::PutToSleep(TArrayView<const FPhysicsObjectHandle> InObjects)
	{
		for (const FPhysicsObjectHandle Object : InObjects)
		{
			if (!Object)
			{
				continue;
			}

			EObjectStateType State = Object->ObjectState<Id>();
			if (State == EObjectStateType::Dynamic || State == EObjectStateType::Sleeping)
			{
				SetParticleStateHelper<Id>(Object, EObjectStateType::Sleeping);
			}
		}
	}

	template<EThreadContext Id>
	void FWritePhysicsObjectInterface<Id>::WakeUp(TArrayView<const FPhysicsObjectHandle> InObjects)
	{
		for (const FPhysicsObjectHandle Object : InObjects)
		{
			if (!Object)
			{
				continue;
			}

			if (TThreadParticle<Id>* Particle = Object->GetParticle<Id>())
			{
				EObjectStateType State = Object->ObjectState<Id>();
				if (State == EObjectStateType::Dynamic || State == EObjectStateType::Sleeping)
				{
					SetParticleStateHelper<Id>(Object, EObjectStateType::Dynamic);
					if constexpr (Id == EThreadContext::External)
					{
						if (TThreadRigidParticle<Id>* Rigid = Particle->CastToRigidParticle())
						{
							Rigid->ClearEvents();
						}
					}
				}
			}
		}
	}

	template<EThreadContext Id>
	void FWritePhysicsObjectInterface<Id>::AddForce(TArrayView<const FPhysicsObjectHandle> InObjects, const FVector& Force, bool bInvalidate)
	{
		for (const FPhysicsObjectHandle Object : InObjects)
		{
			if (!Object)
			{
				continue;
			}

			if (TThreadParticle<Id>* Particle = Object->GetParticle<Id>())
			{
				if (Chaos::TThreadRigidParticle<Id>* Rigid = Particle->CastToRigidParticle())
				{
					if (Rigid->ObjectState() == EObjectStateType::Sleeping || Rigid->ObjectState() == EObjectStateType::Dynamic)
					{
						if (bInvalidate)
						{
							SetParticleStateHelper<Id>(Object, EObjectStateType::Dynamic);
						}

						Rigid->AddForce(Force, bInvalidate);
					}
				}
			}
		}
	}

	template<EThreadContext Id>
	void FWritePhysicsObjectInterface<Id>::AddTorque(TArrayView<const FPhysicsObjectHandle> InObjects, const FVector& Torque, bool bInvalidate)
	{
		for (const FPhysicsObjectHandle Object : InObjects)
		{
			if (!Object)
			{
				continue;
			}

			if (TThreadParticle<Id>* Particle = Object->GetParticle<Id>())
			{
				if (Chaos::TThreadRigidParticle<Id>* Rigid = Particle->CastToRigidParticle())
				{
					if (Rigid->ObjectState() == EObjectStateType::Sleeping || Rigid->ObjectState() == EObjectStateType::Dynamic)
					{
						if (bInvalidate)
						{
							SetParticleStateHelper<Id>(Object, EObjectStateType::Dynamic);
						}

						Rigid->AddTorque(Torque, bInvalidate);
					}
				}
			}
		}
	}

	template<EThreadContext Id>
	void FWritePhysicsObjectInterface<Id>::AddRadialImpulse(TArrayView<const FPhysicsObjectHandle> InObjects, FVector Origin, float Radius, float Strength, enum ERadialImpulseFalloff Falloff, bool bApplyStrain, bool bInvalidate)
	{
		if (Chaos::FPBDRigidsSolver* RigidSolver = Chaos::FPhysicsObjectInterface::GetSolver(InObjects))
		{
			//put onto physics thread
			RigidSolver->EnqueueCommandImmediate([InObjects = TArray<FPhysicsObjectHandle>{ InObjects },
				RigidSolver,
				Origin,
				Radius,
				Strength,
				Falloff,
				bApplyStrain,
				bInvalidate]()
				{
					using namespace Chaos;
					for (FPhysicsObjectHandle Object : InObjects)
					{
						if (Object == nullptr)
						{
							continue;
						}

						//can apply to clusters or rigid particles
						TThreadParticle<EThreadContext::Internal>* Particle = Object->GetParticle<EThreadContext::Internal>();
						if (FPBDRigidClusteredParticleHandle* Cluster = reinterpret_cast<FPBDRigidClusteredParticleHandle*>(Particle))
						{
							if (bApplyStrain)
							{
								FRigidClustering& Clustering = RigidSolver->GetEvolution()->GetRigidClustering();
								TArray<FPBDRigidParticleHandle*>* ChildrenHandles = Clustering.GetChildrenMap().Find(Cluster);

								if (ChildrenHandles == nullptr)
								{
									continue;
								}

								for (FPBDRigidParticleHandle* ChildHandle : *ChildrenHandles)
								{
									float VelocityRatio = 1.0f / static_cast<float>(ChildrenHandles->Num());

									float FalloffStrength = AddRadialImpulseHelper(ChildHandle, Origin, Radius, Strength, Falloff, VelocityRatio, bInvalidate);

									//to do: remove cvar when material system is in place and densities are updated
									FalloffStrength = PhysicsObjectInterfaceCVars::StrainModifier * FalloffStrength;

									Clustering.SetExternalStrain(ChildHandle->CastToClustered(), FalloffStrength);
								}
							}
							else
							{
								FPBDRigidParticleHandle* ParticleHandle = Cluster->CastToRigidParticle();

								if (ParticleHandle != nullptr && ParticleHandle->IsSleeping())
								{
									RigidSolver->GetEvolution()->SetParticleObjectState(ParticleHandle, Chaos::EObjectStateType::Dynamic);
									RigidSolver->GetEvolution()->GetParticles().MarkTransientDirtyParticle(ParticleHandle);
								}

								AddRadialImpulseHelper(ParticleHandle, Origin, Radius, Strength, Falloff, bInvalidate);
							}
						}
						else
						{
							FPBDRigidParticleHandle* ParticleHandle = reinterpret_cast<FPBDRigidParticleHandle*>(Particle);

							if (ParticleHandle != nullptr && ParticleHandle->IsSleeping())
							{
								RigidSolver->GetEvolution()->SetParticleObjectState(ParticleHandle, Chaos::EObjectStateType::Dynamic);
								RigidSolver->GetEvolution()->GetParticles().MarkTransientDirtyParticle(ParticleHandle);
							}

							AddRadialImpulseHelper(ParticleHandle, Origin, Radius, Strength, Falloff, bInvalidate);
						}
					}
				});
		}
	}

	template<EThreadContext Id>
	void FWritePhysicsObjectInterface<Id>::UpdateShapeCollisionFlags(TArrayView<const FPhysicsObjectHandle> InObjects, bool bSimCollision, bool bQueryCollision)
	{
		for (const FPhysicsObjectHandle Object : InObjects)
		{
			if (!Object)
			{
				continue;
			}

			if (TThreadParticle<Id>* Particle = Object->GetParticle<Id>())
			{
				for (const TUniquePtr<Chaos::FPerShapeData>& ShapeData : Particle->ShapesArray())
				{
					FCollisionData Data = ShapeData->GetCollisionData();
					Data.bSimCollision = bSimCollision;
					Data.bQueryCollision = bQueryCollision;
					ShapeData->SetCollisionData(Data);
				}
			}
		}
	}

	template<EThreadContext Id>
	void FWritePhysicsObjectInterface<Id>::UpdateShapeFilterData(TArrayView<const FPhysicsObjectHandle> InObjects, const FCollisionFilterData& QueryData, const FCollisionFilterData& SimData)
	{
		for (const FPhysicsObjectHandle Object : InObjects)
		{
			if (!Object)
			{
				continue;
			}

			if (TThreadParticle<Id>* Particle = Object->GetParticle<Id>())
			{
				for (const TUniquePtr<Chaos::FPerShapeData>& ShapeData : Particle->ShapesArray())
				{
					ShapeData->SetQueryData(QueryData);
					ShapeData->SetSimData(SimData);
				}
			}
		}
	}

	template<EThreadContext Id>
	void FWritePhysicsObjectInterface<Id>::AddConnectivityEdgesBetween(TArrayView<const FPhysicsObjectHandle> FromObjects, TArrayView<const FPhysicsObjectHandle> ToObjects, const FClusterCreationParameters& Parameters)
	{
		if (FromObjects.IsEmpty() || ToObjects.IsEmpty())
		{
			return;
		}

		// Assume that the solver for both sets of objects are the same.
		if (FPBDRigidsSolver* Solver = FPhysicsObjectInterface::GetSolver(FromObjects))
		{
			if constexpr (Id == EThreadContext::External)
			{
				Solver->EnqueueCommandImmediate(
					[FromObjects = TArray<FPhysicsObjectHandle>{ FromObjects }, ToObjects = TArray<FPhysicsObjectHandle>{ ToObjects }, Parameters]()
					{
						FWritePhysicsObjectInterface_Internal Internal = FPhysicsObjectInternalInterface::GetWrite();
						Internal.AddConnectivityEdgesBetween(FromObjects, ToObjects, Parameters);
					}
				);
			}
			else
			{
				FRigidClustering& Clustering = Solver->GetEvolution()->GetRigidClustering();

				TArray<FPBDRigidParticleHandle*> AllParticles;
				AllParticles.Reserve(FromObjects.Num() + ToObjects.Num());

				TArray<FPBDRigidParticleHandle*> FromParticles = this->GetAllRigidParticles(FromObjects);
				TArray<FPBDRigidParticleHandle*> ToParticles = this->GetAllRigidParticles(ToObjects);
				AllParticles.Append(FromParticles);
				AllParticles.Append(ToParticles);

				TSet<FPBDRigidParticleHandle*> FromSet{ FromParticles };
				TSet<FPBDRigidParticleHandle*> ToSet{ ToParticles };
				Clustering.GenerateConnectionGraph(AllParticles, Parameters, &FromSet, &ToSet);
			}
		}
		
	}

	void FPhysicsObjectInterface::SetName(const FPhysicsObjectHandle Object, const FName& InName)
	{
		if (!Object)
		{
			return;
		}

		Object->SetName(InName);
	}

	FName FPhysicsObjectInterface::GetName(const FConstPhysicsObjectHandle Object)
	{
		if (!Object)
		{
			return NAME_None;
		}

		return Object->GetBodyName();
	}

	void FPhysicsObjectInterface::SetId(const FPhysicsObjectHandle Object, int32 InId)
	{
		if (!Object)
		{
			return;
		}

		Object->SetBodyIndex(InId);
	}

	int32 FPhysicsObjectInterface::GetId(const FConstPhysicsObjectHandle Object)
	{
		if (!Object)
		{
			return INDEX_NONE;
		}

		return Object->GetBodyIndex();
	}

	FPBDRigidsSolver* FPhysicsObjectInterface::GetSolver(TArrayView<const FConstPhysicsObjectHandle> InObjects)
	{
		FPBDRigidsSolver* RetSolver = nullptr;
		for (const FConstPhysicsObjectHandle Object : InObjects)
		{
			if (!Object)
			{
				continue;
			}

			FPBDRigidsSolver* Solver = nullptr;
			if (const IPhysicsProxyBase* Proxy = Object->PhysicsProxy())
			{
				Solver = Proxy->GetSolver<FPBDRigidsSolver>();
			}

			if (!Solver)
			{
				return nullptr;
			}
			else if (!RetSolver)
			{
				RetSolver = Solver;
			}
			else if (Solver != RetSolver)
			{
				return nullptr;
			}
		}
		return RetSolver;
	}

	IPhysicsProxyBase* FPhysicsObjectInterface::GetProxy(TArrayView<const FConstPhysicsObjectHandle> InObjects)
	{
		IPhysicsProxyBase* RetProxy = nullptr;
		for (const FConstPhysicsObjectHandle Object : InObjects)
		{
			if (!Object)
			{
				continue;
			}

			IPhysicsProxyBase* Proxy = const_cast<IPhysicsProxyBase*>(Object->PhysicsProxy());
			if (!Proxy)
			{
				return nullptr;
			}
			else if (!RetProxy)
			{
				RetProxy = Proxy;
			}
			else if (Proxy != RetProxy)
			{
				return nullptr;
			}
		}
		return RetProxy;
	}

	template class FReadPhysicsObjectInterface<EThreadContext::External>;
	template class FReadPhysicsObjectInterface<EThreadContext::Internal>;

	template class FWritePhysicsObjectInterface<EThreadContext::External>;
	template class FWritePhysicsObjectInterface<EThreadContext::Internal>;
}