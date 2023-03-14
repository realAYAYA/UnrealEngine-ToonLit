// Copyright Epic Games, Inc. All Rights Reserved.

#include "Chaos/ParticleHandle.h"

#include "Chaos/ImplicitObjectTransformed.h"
#include "Chaos/ImplicitObjectScaled.h"
#include "Chaos/TriangleMeshImplicitObject.h"
#include "Chaos/Framework/PhysicsProxy.h"
#include "Chaos/Framework/PhysicsSolverBase.h"
#include "Chaos/Framework/PhysicsProxyBase.h"
#include "Chaos/CastingUtilities.h"
#include "PBDRigidsSolver.h"

namespace Chaos
{
	void SetObjectStateHelper(IPhysicsProxyBase& Proxy, FPBDRigidParticleHandle& Rigid, EObjectStateType InState, bool bAllowEvents, bool bInvalidate)
	{
		if (auto PhysicsSolver = Proxy.GetSolver<Chaos::FPBDRigidsSolver>())
		{
			PhysicsSolver->GetEvolution()->SetParticleObjectState(&Rigid, InState);
		}
		else
		{
			//not in solver so just set it directly (can this possibly happen?)
			Rigid.SetObjectStateLowLevel(InState);
		}
	}

	inline FImplicitObject* GetInstancedImplicitHelper(FImplicitObject* Implicit0)
	{
		EImplicitObjectType Implicit0OuterType = Implicit0->GetType();

		if (Implicit0OuterType == TImplicitObjectInstanced<FConvex>::StaticType())
		{
			return const_cast<FConvex*>(Implicit0->template GetObject<TImplicitObjectInstanced<FConvex>>()->GetInstancedObject());
		}
		else if (Implicit0OuterType == TImplicitObjectInstanced<TBox<FReal, 3>>::StaticType())
		{
			return const_cast<TBox<FReal, 3>*>(Implicit0->template GetObject<TImplicitObjectInstanced<TBox<FReal, 3>>>()->GetInstancedObject());
		}
		else if (Implicit0OuterType == TImplicitObjectInstanced<FCapsule>::StaticType())
		{
			return const_cast<FCapsule*>(Implicit0->template GetObject<TImplicitObjectInstanced<FCapsule>>()->GetInstancedObject());
		}
		else if (Implicit0OuterType == TImplicitObjectInstanced<TSphere<FReal, 3>>::StaticType())
		{
			return const_cast<TSphere<FReal, 3>*>(Implicit0->template GetObject<TImplicitObjectInstanced<TSphere<FReal, 3>>>()->GetInstancedObject());
		}
		else if (Implicit0OuterType == TImplicitObjectInstanced<FTriangleMeshImplicitObject>::StaticType())
		{
			return const_cast<FTriangleMeshImplicitObject*>(Implicit0->template GetObject<TImplicitObjectInstanced<FTriangleMeshImplicitObject>>()->GetInstancedObject());
		}

		return nullptr;
	}

	template <typename T, int d>
	void Chaos::TGeometryParticle<T, d>::MergeGeometry(TArray<TUniquePtr<FImplicitObject>>&& Objects)
	{
		ensure(MNonFrequentData.Read().Geometry());

		// we only support FImplicitObjectUnion
		ensure(MNonFrequentData.Read().Geometry()->GetType() == FImplicitObjectUnion::StaticType());

		if (MNonFrequentData.Read().Geometry()->GetType() == FImplicitObjectUnion::StaticType())
		{
			ModifyGeometry([&Objects, this](FImplicitObject& GeomToModify)
			{
				if (FImplicitObjectUnion* Union = GeomToModify.template GetObject<FImplicitObjectUnion>())
				{
					Union->Combine(Objects);
				}
			});
		}
	}

	template <typename T, int d>
	void Chaos::TGeometryParticle<T, d>::RemoveShape(FPerShapeData* InShape, bool bWakeTouching)
	{
		// NOTE: only intended use is to remove objects from inside a FImplicitObjectUnion
		CHAOS_ENSURE(MNonFrequentData.Read().Geometry()->GetType() == FImplicitObjectUnion::StaticType());

		int32 FoundIndex = INDEX_NONE;
		for (int32 Index = 0; Index < MShapesArray.Num(); Index++)
		{
			if (InShape == MShapesArray[Index].Get())
			{
				MShapesArray.RemoveAt(Index);
				FoundIndex = Index;
				break;
			}
		}

		if (MNonFrequentData.Read().Geometry()->GetType() == FImplicitObjectUnion::StaticType())
		{
			// if we are currently a union then remove geometry from this union
			ModifyGeometry([FoundIndex](FImplicitObject& GeomToModify)
			{
				if (FImplicitObjectUnion* Union = GeomToModify.template GetObject<FImplicitObjectUnion>())
				{
					Union->RemoveAt(FoundIndex);
				}
			});
		}

	}


	template <typename T, int d>
	void Chaos::TGeometryParticle<T, d>::SetIgnoreAnalyticCollisionsImp(FImplicitObject* Implicit, bool bIgnoreAnalyticCollisions)
	{
		check(Implicit);
		if (Implicit->GetType() == FImplicitObjectUnion::StaticType())
		{
			FImplicitObjectUnion* Union = Implicit->template GetObject<FImplicitObjectUnion>();
			for (const auto& Child : Union->GetObjects())
			{
				SetIgnoreAnalyticCollisionsImp(Child.Get(), bIgnoreAnalyticCollisions);
			}
		}
		else if (Implicit->GetType() == TImplicitObjectTransformed<T, d>::StaticType())
		{
			TImplicitObjectTransformed<FReal, 3>* TransformedImplicit = Implicit->template GetObject<TImplicitObjectTransformed<FReal, 3>>();
			SetIgnoreAnalyticCollisionsImp(const_cast<FImplicitObject*>(TransformedImplicit->GetTransformedObject()), bIgnoreAnalyticCollisions);
		}
		else if ((uint32)Implicit->GetType() & ImplicitObjectType::IsInstanced)
		{
			SetIgnoreAnalyticCollisionsImp(GetInstancedImplicitHelper(Implicit), bIgnoreAnalyticCollisions);
		}
		else
		{

			// Find our shape and see if sim is enabled.
			for (const TUniquePtr<FPerShapeData>& Shape : ShapesArray())
			{
				if (Shape->GetGeometry().Get() == Implicit)
				{
					if (!Shape->GetSimEnabled())
					{
						return;
					}
					break;
				}
			}
			if (bIgnoreAnalyticCollisions)
			{
				Implicit->SetCollisionType(Chaos::ImplicitObjectType::LevelSet);
				//Implicit->SetConvex(false);
			}
			else
			{
				Implicit->SetCollisionType(Implicit->GetType());
				// @todo (mlentine): Need to in theory set convex properly here
			}
		}
	}

	template <typename T, int d>
	void TGeometryParticle<T,d>::SetIgnoreAnalyticCollisions(bool bIgnoreAnalyticCollisions)
	{
		ModifyGeometry([this, bIgnoreAnalyticCollisions](FImplicitObject& GeomToModify)
		{
			SetIgnoreAnalyticCollisionsImp(&GeomToModify, bIgnoreAnalyticCollisions);
		});
	}

	template <typename T, int d, bool bPersistent>
	void TPBDRigidParticleHandleImp<T, d, bPersistent>::AddTorque(const TVector<T, d>& InTorque, bool bInvalidate)
	{
		const FRotation3 RCoM = FParticleUtilitiesPQ::GetCoMWorldRotation(this);
		const FMatrix33 WorldInvI = Utilities::ComputeWorldSpaceInertia(RCoM, InvI());
		SetAngularAcceleration(AngularAcceleration() + WorldInvI * InTorque);
	}


	template <typename T, int d, bool bPersistent>
	void TPBDRigidParticleHandleImp<T, d, bPersistent>::SetTorque(const TVector<T, d>& InTorque, bool bInvalidate)
	{
		const FRotation3 RCoM = FParticleUtilitiesPQ::GetCoMWorldRotation(this);
		const FMatrix33 WorldInvI = Utilities::ComputeWorldSpaceInertia(RCoM, InvI());
		SetAngularAcceleration(WorldInvI * InTorque);
	}

	template <typename T, int d>
	void TPBDRigidParticle<T, d>::AddTorque(const TVector<T, d>& InTorque, bool bInvalidate)
	{
		const FRotation3 RCoM = FParticleUtilitiesGT::GetCoMWorldRotation(this);
		const FMatrix33 WorldInvI = Utilities::ComputeWorldSpaceInertia(RCoM, InvI());
		SetAngularAcceleration(AngularAcceleration() + WorldInvI * InTorque);
	}

	template class TGeometryParticle<FReal, 3>;

	template class TKinematicGeometryParticle<FReal, 3>;

	template class TPBDRigidParticle<FReal, 3>;

	template class TParticleHandleBase<FReal, 3>;
	template class TGeometryParticleHandleImp<FReal, 3, true>;
	template class TKinematicGeometryParticleHandleImp<FReal, 3, true>;
	template class TPBDRigidParticleHandleImp<FReal, 3, true>;

	template <>
	void Chaos::TGeometryParticle<FReal, 3>::MarkDirty(const EChaosPropertyFlags DirtyBits, bool bInvalidate )
	{
		if (bInvalidate)
		{
			this->MDirtyFlags.MarkDirty(DirtyBits);

			if (Proxy)
			{
				if (FPhysicsSolverBase* PhysicsSolverBase = Proxy->GetSolver<FPhysicsSolverBase>())
				{
					PhysicsSolverBase->AddDirtyProxy(Proxy);
				}
			}
		}
	}

	const FVec3 FGenericParticleHandleImp::ZeroVector = FVec3(0);
	const FRotation3 FGenericParticleHandleImp::IdentityRotation = FRotation3(FQuat::Identity);
	const FMatrix33 FGenericParticleHandleImp::ZeroMatrix = FMatrix33(0);
	const TUniquePtr<FBVHParticles> FGenericParticleHandleImp::NullBVHParticles = TUniquePtr<FBVHParticles>();
	const FKinematicTarget FGenericParticleHandleImp::EmptyKinematicTarget;

	template <>
	template <>
	int32 TGeometryParticleHandleImp<FReal, 3, true>::GetPayload<int32>(int32 Idx)
	{
		return Idx;
	}

	template <>
	template <>
	int32 TGeometryParticleHandleImp<FReal, 3, false>::GetPayload<int32>(int32 Idx)
	{
		return Idx;
	}

	template <>
	void TPBDRigidParticleHandleImp<FReal, 3, true>::SetDynamicMisc(const FParticleDynamicMisc& DynamicMisc, FPBDRigidsEvolutionBase& Evolution)
	{
	
		if (Disabled() != DynamicMisc.Disabled())
		{
			if (DynamicMisc.Disabled())
			{
				Evolution.DisableParticle(this);
			}
			else
			{
				Evolution.EnableParticle(this);
			}
		}		

		const bool bDirtyInertiaConditioning = (ObjectState() != DynamicMisc.ObjectState());
		if (bDirtyInertiaConditioning)
		{
			SetInertiaConditioningDirty();
		}

		SetLinearEtherDrag(DynamicMisc.LinearEtherDrag());
		SetAngularEtherDrag(DynamicMisc.AngularEtherDrag());
		SetMaxLinearSpeedSq(DynamicMisc.MaxLinearSpeedSq());
		SetMaxAngularSpeedSq(DynamicMisc.MaxAngularSpeedSq());
		SetCollisionGroup(DynamicMisc.CollisionGroup());
		SetDisabled(DynamicMisc.Disabled());
		SetCollisionConstraintFlags(DynamicMisc.CollisionConstraintFlags());
		SetControlFlags(DynamicMisc.ControlFlags());

		Evolution.SetParticleObjectState(this, DynamicMisc.ObjectState());
		Evolution.SetParticleSleepType(this, DynamicMisc.SleepType());
	}

}
