// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Physics/ImmediatePhysics/ImmediatePhysicsChaos/ImmediatePhysicsCore_Chaos.h"
#include "Chaos/ChaosEngineInterface.h"

#include "Engine/EngineTypes.h"

namespace ImmediatePhysics_Chaos
{
	/** handle associated with a physics actor. This is the proper way to read/write to the physics simulation */
	struct ENGINE_API FActorHandle
	{
	public:
		~FActorHandle();

		void SetName(const FName& InName) { Name = InName; }
		const FName& GetName() const { return Name; }

		bool GetEnabled() const;

		void SetEnabled(bool bEnabled);

		/** Sets the world transform, zeroes velocity, etc.*/
		void InitWorldTransform(const FTransform& WorldTM);

		/** Sets the world transform, maintains velocity etc.*/
		void SetWorldTransform(const FTransform& WorldTM);

		/** Make a body kinematic, or non-kinematic */
		void SetIsKinematic(bool bKinematic);

		/** Is the actor kinematic */
		bool GetIsKinematic() const;

		/** Gets the kinematic target (next transform) for the actor if one is set (check HasKinematicTarget() to see if a target is available) */
		const FKinematicTarget& GetKinematicTarget() const;

		/** Sets the kinematic target. This will affect velocities as expected*/
		void SetKinematicTarget(const FTransform& WorldTM);

		/** Does this actor have a kinematic target (next kinematic transform to be applied) */
		bool HasKinematicTarget() const;

		/** Whether the body is static */
		bool IsStatic() const;

		/** Whether the body is simulating */
		bool IsSimulated() const;

		/** Get the world transform */
		FTransform GetWorldTransform() const;

		/** Set the linear velocity */
		void SetLinearVelocity(const FVector& NewLinearVelocity);

		/** Get the linear velocity */
		FVector GetLinearVelocity() const;

		/** Set the angular velocity */
		void SetAngularVelocity(const FVector& NewAngularVelocity);

		/** Get the angular velocity */
		FVector GetAngularVelocity() const;

		void AddForce(const FVector& Force);

		void AddTorque(const FVector& Torque);

		void AddRadialForce(const FVector& Origin, FReal Strength, FReal Radius, ERadialImpulseFalloff Falloff, EForceType ForceType);

		void AddImpulseAtLocation(FVector Impulse, FVector Location);

		/** Set the linear damping*/
		void SetLinearDamping(FReal NewLinearDamping);

		/** Get the linear damping*/
		FReal GetLinearDamping() const;

		/** Set the angular damping*/
		void SetAngularDamping(FReal NewAngularDamping);

		/** Get the angular damping*/
		FReal GetAngularDamping() const;

		/** Set the max linear velocity squared*/
		void SetMaxLinearVelocitySquared(FReal NewMaxLinearVelocitySquared);

		/** Get the max linear velocity squared*/
		FReal GetMaxLinearVelocitySquared() const;

		/** Set the max angular velocity squared*/
		void SetMaxAngularVelocitySquared(FReal NewMaxAngularVelocitySquared);

		/** Get the max angular velocity squared*/
		FReal GetMaxAngularVelocitySquared() const;

		/** Set the inverse mass. 0 indicates kinematic object */
		void SetInverseMass(FReal NewInverseMass);

		/** Get the inverse mass. */
		FReal GetInverseMass() const;
		FReal GetMass() const;

		/** Set the inverse inertia. Mass-space inverse inertia diagonal vector */
		void SetInverseInertia(const FVector& NewInverseInertia);

		/** Get the inverse inertia. Mass-space inverse inertia diagonal vector */
		FVector GetInverseInertia() const;
		FVector GetInertia() const;

		/** Set the max depenetration velocity*/
		void SetMaxDepenetrationVelocity(FReal NewMaxDepenetrationVelocity);

		/** Get the max depenetration velocity*/
		FReal GetMaxDepenetrationVelocity() const;

		/** Set the max contact impulse*/
		void SetMaxContactImpulse(FReal NewMaxContactImpulse);

		/** Get the max contact impulse*/
		FReal GetMaxContactImpulse() const;

		/** Get the actor-space centre of mass offset */
		FTransform GetLocalCoMTransform() const;

		Chaos::FGeometryParticleHandle* GetParticle();
		const Chaos::FGeometryParticleHandle* GetParticle() const;

		int32 GetLevel() const;
		void SetLevel(int32 InLevel);

	private:
		FKinematicTarget& GetKinematicTarget();

		friend struct FSimulation;
		friend struct FJointHandle;

		FActorHandle(
			Chaos::FPBDRigidsSOAs& InParticles,
			Chaos::TArrayCollectionArray<Chaos::FVec3>& InParticlePrevXs,
			Chaos::TArrayCollectionArray<Chaos::FRotation3>& InParticlePrevRs,
			EActorType ActorType,
			FBodyInstance* BodyInstance,
			const FTransform& Transform);


		Chaos::FGenericParticleHandle Handle() const;

		FName Name;
		Chaos::FPBDRigidsSOAs& Particles;
		Chaos::FGeometryParticleHandle* ParticleHandle;
		Chaos::TArrayCollectionArray<Chaos::FVec3>& ParticlePrevXs;
		Chaos::TArrayCollectionArray<Chaos::FRotation3>& ParticlePrevRs;
		TUniquePtr<Chaos::FImplicitObject> Geometry;
		TArray<TUniquePtr<Chaos::FPerShapeData>> Shapes;
		int32 Level;
	};

}
