// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Core.h"
#include "Chaos/Defines.h"
#include "Chaos/Evolution/SolverBody.h"

CHAOS_API DECLARE_LOG_CATEGORY_EXTERN(LogChaosCollision, Log, All);

// Set to 0 to use a linearized error calculation, and set to 1 to use a non-linear error calculation in collision detection. 
// In principle nonlinear is more accurate when large rotation corrections occur, but this is not too important for collisions because 
// when the bodies settle the corrections are small. The linearized version is significantly faster than the non-linear version because 
// the non-linear version requires a quaternion multiply and renormalization whereas the linear version is just a cross product.
#define CHAOS_NONLINEAR_COLLISIONS_ENABLED 0

namespace Chaos
{
	class FManifoldPoint;
	class FPBDCollisionSolver;
	class FPBDCollisionConstraint;

	namespace CVars
	{
		extern float Chaos_PBDCollisionSolver_Position_StaticFrictionStiffness;
	}


	/**
	 * @brief A single contact point in a FPBDCollisionSolver
	*/
	class FPBDCollisionSolverManifoldPoint
	{
	public:
		/**
		 * @brief Initialize the geometric data for the contact
		*/
		void InitContact(
			const FSolverReal Dt,
			const FConstraintSolverBody& Body0,
			const FConstraintSolverBody& Body1,
			const FSolverVec3& InRelativeContactPosition0,
			const FSolverVec3& InRelativeContactPosition1,
			const FSolverVec3& InWorldContactNormal,
			const FSolverVec3& InWorldContactTangentU,
			const FSolverVec3& InWorldContactTangentV,
			const FSolverReal InWorldContactDeltaNormal,
			const FSolverReal InWorldContactDeltaTangentU,
			const FSolverReal InWorldContactDeltaTangentV);

		void InitContact(
			const FSolverReal Dt,
			const FConstraintSolverBody& Body0,
			const FConstraintSolverBody& Body1,
			const SolverVectorRegister& InRelativeContactPosition0,
			const SolverVectorRegister& InRelativeContactPosition1,
			const SolverVectorRegister& InWorldContactNormal,
			const SolverVectorRegister& InWorldContactTangentU,
			const SolverVectorRegister& InWorldContactTangentV,
			const FSolverReal InWorldContactDeltaNormal,
			const FSolverReal InWorldContactDeltaTangentU,
			const FSolverReal InWorldContactDeltaTangentV);

		/**
		 * @brief Initialize the material related properties of the contact
		*/
		void InitMaterial(
			const FConstraintSolverBody& Body0,
			const FConstraintSolverBody& Body1,
			const FSolverReal InRestitution,
			const FSolverReal InRestitutionVelocityThreshold);

		/**
		 * @brief Update the cached mass properties based on the current body transforms
		*/
		void UpdateMass(const FConstraintSolverBody& Body0, const FConstraintSolverBody& Body1);

		/**
		 * @brief Update the contact mass for the normal correction
		 * This is used by shock propagation.
		*/
		void UpdateMassNormal(const FConstraintSolverBody& Body0, const FConstraintSolverBody& Body1);

		void UpdateMass(
			const FConstraintSolverBody& Body0,
			const FConstraintSolverBody& Body1,
			const SolverVectorRegister& InRelativeContactPosition0,
			const SolverVectorRegister& InRelativeContactPosition1,
			const SolverVectorRegister& InWorldContactNormal,
			const SolverVectorRegister& InWorldContactTangentU,
			const SolverVectorRegister& InWorldContactTangentV);

		/**
		 * @brief Calculate the position error at the current transforms
		 * @param MaxPushOut a limit on the position error for this iteration to prevent initial-penetration explosion (a common PBD problem)
		*/
		void CalculateContactPositionErrorNormal(const FConstraintSolverBody& Body0, const FConstraintSolverBody& Body1, const FSolverReal MaxPushOut, FSolverReal& OutContactDeltaNormal) const;
		void CalculateContactPositionErrorTangential(const FConstraintSolverBody& Body0, const FConstraintSolverBody& Body1, FSolverReal& OutContactDeltaTanget0, FSolverReal& OutContactDeltaTangent1) const;
		void CalculateContactPositionError(const FConstraintSolverBody& Body0, const FConstraintSolverBody& Body1, const FSolverReal MaxPushOut, FSolverReal& OutContactDeltaNormal, FSolverReal& OutContactDeltaTanget0, FSolverReal& OutContactDeltaTangent1) const;

		/**
		 * @brief Calculate the velocity error at the current transforms
		*/
		void CalculateContactVelocityErrorNormal(const FConstraintSolverBody& Body0, const FConstraintSolverBody& Body1, FSolverReal& OutContactVelocityDeltaNormal) const;
		void CalculateContactVelocityError(const FConstraintSolverBody& Body0, const FConstraintSolverBody& Body1, const FSolverReal DynamicFriction, const FSolverReal Dt, FSolverReal& OutContactVelocityDeltaNormal, FSolverReal& OutContactVelocityDeltaTangent0, FSolverReal& OutContactVelocityDeltaTangent1) const;

		// @todo(chaos): make private
	public:
		friend class FPBDCollisionSolver;

		/**
		 * @brief Calculate the relative velocity at the contact point
		 * @note InitContact must be called before calling this function
		*/
		FSolverVec3 CalculateContactVelocity(const FConstraintSolverBody& Body0, const FConstraintSolverBody& Body1) const;

		/**
		 * @brief Whether we need to solve velocity for this manifold point (only if we were penetrating or applied a pushout)
		*/
		bool ShouldSolveVelocity() const;

		// World-space body-relative contact points
		FSolverVec3 RelativeContactPosition0;
		FSolverVec3 RelativeContactPosition1;

		// World-space contact axes (normal and 2 tangents)
		FSolverVec3 WorldContactNormal;
		FSolverVec3 WorldContactTangentU;
		FSolverVec3 WorldContactTangentV;

		// I^-1.(R x A) for each body where A is each axis (Normal, TangentU, TangentV)
		FSolverVec3 WorldContactNormalAngular0;
		FSolverVec3 WorldContactTangentUAngular0;
		FSolverVec3 WorldContactTangentVAngular0;
		FSolverVec3 WorldContactNormalAngular1;
		FSolverVec3 WorldContactTangentUAngular1;
		FSolverVec3 WorldContactTangentVAngular1;

		// Contact mass (for non-friction)
		FSolverReal ContactMassNormal;
		FSolverReal ContactMassTangentU;
		FSolverReal ContactMassTangentV;

		// Initial world-space contact separation that we are trying to correct
		FSolverReal WorldContactDeltaNormal;
		FSolverReal WorldContactDeltaTangentU;
		FSolverReal WorldContactDeltaTangentV;

		// Desired final normal velocity, taking Restitution into account
		FSolverReal WorldContactVelocityTargetNormal;

		// Solver outputs
		FSolverReal NetPushOutNormal;
		FSolverReal NetPushOutTangentU;
		FSolverReal NetPushOutTangentV;
		FSolverReal NetImpulseNormal;
		FSolverReal NetImpulseTangentU;
		FSolverReal NetImpulseTangentV;

		// A measure of how much we exceeded the static friction threshold.
		// Equal to (NormalPushOut / TangentialPushOut) before clamping to the friction cone.
		// Used to move the static friction anchors to the edge of the cone in Scatter.
		FSolverReal StaticFrictionRatio;
	};

	/**
	 * @brief 
	 * @todo(chaos): Make this solver operate on a single contact point rather than all points in a manifold.
	 * This would be beneficial if we have many contacts with less than 4 points in the manifold. However this
	 * is dificult to do while we are still supporting non-manifold collisions.
	*/
	class FPBDCollisionSolver
	{
	public:
		static const int32 MaxConstrainedBodies = 2;
		static const int32 MaxPointsPerConstraint = 4;

		FPBDCollisionSolver();

		/** Reset the state of the collision solver */
		void Reset()
		{
			State.Reset();
		}

		FSolverReal StaticFriction() const { return State.StaticFriction; }
		FSolverReal DynamicFriction() const { return State.DynamicFriction; }
		FSolverReal VelocityFriction() const { return State.VelocityFriction; }

		void SetFriction(const FSolverReal InStaticFriction, const FSolverReal InDynamicFriction, const FSolverReal InVelocityFriction)
		{
			State.StaticFriction = InStaticFriction;
			State.DynamicFriction = InDynamicFriction;
			State.VelocityFriction = InVelocityFriction;
		}

		void SetStiffness(const FSolverReal InStiffness)
		{
			State.Stiffness = InStiffness;
		}

		void SetSolverBodies(FSolverBody& SolverBody0, FSolverBody& SolverBody1)
		{
			State.SolverBodies[0] = FConstraintSolverBody(SolverBody0);
			State.SolverBodies[1] = FConstraintSolverBody(SolverBody1);
		}

		void ResetSolverBodies()
		{
			State.SolverBodies[0].Reset();
			State.SolverBodies[1].Reset();
		}

		int32 NumManifoldPoints() const
		{
			return State.NumManifoldPoints;
		}

		int32 SetNumManifoldPoints(const int32 InNumManifoldPoints)
		{
			State.NumManifoldPoints = FMath::Min(InNumManifoldPoints, MaxPointsPerConstraint);
			return State.NumManifoldPoints;
		}

		const FPBDCollisionSolverManifoldPoint& GetManifoldPoint(const int32 ManifoldPointIndex) const
		{
			check(ManifoldPointIndex < NumManifoldPoints());
			return State.ManifoldPoints[ManifoldPointIndex];
		}

		void SetManifoldPoint(
			const int32 ManifoldPoiontIndex,
			const FSolverReal Dt,
			const FSolverReal InRestitution,
			const FSolverReal InRestitutionVelocityThreshold,
			const FSolverVec3& InRelativeContactPosition0,
			const FSolverVec3& InRelativeContactPosition1,
			const FSolverVec3& InWorldContactNormal,
			const FSolverVec3& InWorldContactTangentU,
			const FSolverVec3& InWorldContactTangentV,
			const FSolverReal InWorldContactDeltaNormal,
			const FSolverReal InWorldContactDeltaTangentU,
			const FSolverReal InWorldContactDeltaTangentV);

		void SetManifoldPoint(
			const int32 ManifoldPoiontIndex,
			const FSolverReal Dt,
			const FSolverReal InRestitution,
			const FSolverReal InRestitutionVelocityThreshold,
			const SolverVectorRegister& InRelativeContactPosition0,
			const SolverVectorRegister& InRelativeContactPosition1,
			const SolverVectorRegister& InWorldContactNormal,
			const SolverVectorRegister& InWorldContactTangentU,
			const SolverVectorRegister& InWorldContactTangentV,
			const FSolverReal InWorldContactDeltaNormal,
			const FSolverReal InWorldContactDeltaTangentU,
			const FSolverReal InWorldContactDeltaTangentV);

		/**
		 * @brief Get the first (decaorated) solver body
		 * The decorator add a possible mass scale
		*/
		FConstraintSolverBody& SolverBody0() { return State.SolverBodies[0]; }
		const FConstraintSolverBody& SolverBody0() const { return State.SolverBodies[0]; }

		/**
		 * @brief Get the second (decaorated) solver body
		 * The decorator add a possible mass scale
		*/
		FConstraintSolverBody& SolverBody1() { return State.SolverBodies[1]; }
		const FConstraintSolverBody& SolverBody1() const { return State.SolverBodies[1]; }

		/**
		 * @brief Set up the mass scaling for shock propagation, using the position-phase mass scale
		*/
		void EnablePositionShockPropagation();

		/**
		 * @brief Set up the mass scaling for shock propagation, using the velocity-phase mass scale
		*/
		void EnableVelocityShockPropagation();

		/**
		 * @brief Disable mass scaling
		*/
		void DisableShockPropagation();

		/**
		 * @brief Calculate and apply the position correction for this iteration
		 * @return true if we need to run more iterations, false if we did not apply any correction
		*/
		bool SolvePositionWithFriction(const FSolverReal Dt, const FSolverReal MaxPushOut);
		bool SolvePositionNoFriction(const FSolverReal Dt, const FSolverReal MaxPushOut);

		/**
		 * @brief Calculate and apply the velocity correction for this iteration
		 * @return true if we need to run more iterations, false if we did not apply any correction
		*/
		bool SolveVelocity(const FSolverReal Dt, const bool bApplyDynamicFriction);

	private:
		/**
		 * @brief Apply the inverse mass scale the body with the lower level
		 * @param InvMassScale 
		*/
		void SetShockPropagationInvMassScale(const FSolverReal InvMassScale);

		/**
		 * @brief Run a velocity solve on the average point from all the points that received a position impulse
		 * This is used to enforce Restitution constraints without introducing rotation artefacts without
		 * adding more velocity iterations.
		 * This will only perform work if there is more than one active contact.
		*/
		void SolveVelocityAverage(const FSolverReal Dt);

		struct FState
		{
			FState()
				: StaticFriction(0)
				, DynamicFriction(0)
				, VelocityFriction(0)
				, Stiffness(1)
				, NumManifoldPoints(0)
				, SolverBodies()
				, ManifoldPoints()
			{
			}

			/** Reset the state struct members to its default values */
			void Reset()
			{
				StaticFriction = 0;
				DynamicFriction = 0;
				VelocityFriction = 0;
				Stiffness = 1;
				NumManifoldPoints = 0;
			}

			// Static Friction in the position-solve phase
			FSolverReal StaticFriction;

			// Dynamic Friction in the position-solve phase
			FSolverReal DynamicFriction;
			
			// Dynamic Friction in the velocity-solve phase
			FSolverReal VelocityFriction;

			// Solver stiffness (scales all pushout and impulses)
			FSolverReal Stiffness;

			// Bodies and contacts
			int32 NumManifoldPoints;
			FConstraintSolverBody SolverBodies[MaxConstrainedBodies];
			FPBDCollisionSolverManifoldPoint ManifoldPoints[MaxPointsPerConstraint];
		};

		FState State;
	};


	//////////////////////////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////////////////////////


	FORCEINLINE_DEBUGGABLE void CalculatePositionCorrectionNormal(
		const FSolverReal Stiffness,
		const FSolverReal ContactDeltaNormal,
		const FSolverReal ContactMassNormal,
		const FSolverReal NetPushOutNormal,
		FSolverReal& OutPushOutNormal)
	{
		const FSolverReal PushOutNormal = -Stiffness * ContactDeltaNormal * ContactMassNormal;

		// The total pushout so far this sub-step
		// Unilateral constraint: Net-negative impulses not allowed (negative incremental impulses are allowed as long as the net is positive)
		if ((NetPushOutNormal + PushOutNormal) > FSolverReal(0))
		{
			OutPushOutNormal = PushOutNormal;
		}
		else
		{
			OutPushOutNormal = -NetPushOutNormal;
		}
	}


	FORCEINLINE_DEBUGGABLE void CalculatePositionCorrectionTangent(
		const FSolverReal Stiffness,
		const FSolverReal ContactDeltaTangent,
		const FSolverReal ContactMassTangent,
		FSolverReal& OutPushOutTangent)
	{
		const FSolverReal FrictionStiffness = CVars::Chaos_PBDCollisionSolver_Position_StaticFrictionStiffness;

		// Bilateral constraint - negative values allowed (unlike the normal correction)
		OutPushOutTangent = -Stiffness * FrictionStiffness * ContactMassTangent * ContactDeltaTangent;
	}


	FORCEINLINE_DEBUGGABLE void ApplyFrictionCone(
		const FSolverReal StaticFriction,
		const FSolverReal DynamicFriction,
		const FSolverReal MaxFrictionPushOut,
		FSolverReal& InOutPushOutTangentU,
		FSolverReal& InOutPushOutTangentV,
		FSolverReal& InOutNetPushOutTangentU,
		FSolverReal& InOutNetPushOutTangentV,
		FSolverReal& OutStaticFrictionRatio)
	{
		// Assume we stay in the friction cone...
		OutStaticFrictionRatio = FSolverReal(1);

		if (MaxFrictionPushOut < FSolverReal(UE_KINDA_SMALL_NUMBER))
		{
			// Note: we have already added the current iteration's PushOut to the NetPushOut but it has not been applied to the body
			// so we must subtract it again to calculate the actual pushout we want to undo (i.e., the net pushout that has been applied 
			// to the body so far from previous iterations)
			InOutPushOutTangentU = -(InOutNetPushOutTangentU - InOutPushOutTangentU);
			InOutPushOutTangentV = -(InOutNetPushOutTangentV - InOutPushOutTangentV);
			InOutNetPushOutTangentU = FSolverReal(0);
			InOutNetPushOutTangentV = FSolverReal(0);
			OutStaticFrictionRatio = FSolverReal(0);
		}
		else
		{
			// If we exceed the static friction cone, clip to the dynamic friction cone
			const FSolverReal MaxStaticPushOutTangentSq = FMath::Square(StaticFriction * MaxFrictionPushOut);
			const FSolverReal NetPushOutTangentSq = FMath::Square(InOutNetPushOutTangentU) + FMath::Square(InOutNetPushOutTangentV);
			if (NetPushOutTangentSq > MaxStaticPushOutTangentSq)
			{
				const FSolverReal MaxDynamicPushOutTangent = DynamicFriction * MaxFrictionPushOut;
				const FSolverReal FrictionMultiplier = MaxDynamicPushOutTangent * FMath::InvSqrt(NetPushOutTangentSq);
				const FSolverReal NetPushOutTangentU = FrictionMultiplier * InOutNetPushOutTangentU;
				const FSolverReal NetPushOutTangentV = FrictionMultiplier * InOutNetPushOutTangentV;
				InOutPushOutTangentU = NetPushOutTangentU - (InOutNetPushOutTangentU - InOutPushOutTangentU);
				InOutPushOutTangentV = NetPushOutTangentV - (InOutNetPushOutTangentV - InOutPushOutTangentV);
				InOutNetPushOutTangentU = NetPushOutTangentU;
				InOutNetPushOutTangentV = NetPushOutTangentV;
				OutStaticFrictionRatio = FrictionMultiplier;
			}
		}
	}


	FORCEINLINE_DEBUGGABLE void ApplyPositionCorrectionTangential(
		const FSolverReal Stiffness,
		const FSolverReal StaticFriction,
		const FSolverReal DynamicFriction,
		const FSolverReal MaxFrictionPushOut,
		const FSolverReal ContactDeltaTangentU,
		const FSolverReal ContactDeltaTangentV,
		FPBDCollisionSolverManifoldPoint& ManifoldPoint,
		FConstraintSolverBody& Body0,
		FConstraintSolverBody& Body1)
	{
		FSolverReal PushOutTangentU = FSolverReal(0);
		FSolverReal PushOutTangentV = FSolverReal(0);

		CalculatePositionCorrectionTangent(
			Stiffness,
			ContactDeltaTangentU,
			ManifoldPoint.ContactMassTangentU,
			PushOutTangentU);					// Out

		CalculatePositionCorrectionTangent(
			Stiffness,
			ContactDeltaTangentV,
			ManifoldPoint.ContactMassTangentV,
			PushOutTangentV);					// Out

		ManifoldPoint.NetPushOutTangentU += PushOutTangentU;
		ManifoldPoint.NetPushOutTangentV += PushOutTangentV;

		ApplyFrictionCone(
			StaticFriction,
			DynamicFriction,
			MaxFrictionPushOut,
			PushOutTangentU,									// InOut
			PushOutTangentV,									// InOut
			ManifoldPoint.NetPushOutTangentU,					// InOut
			ManifoldPoint.NetPushOutTangentV,					// InOut
			ManifoldPoint.StaticFrictionRatio);					// Out

		// Update the particle state based on the pushout
		const FVec3 PushOut = PushOutTangentU * ManifoldPoint.WorldContactTangentU + PushOutTangentV * ManifoldPoint.WorldContactTangentV;
		if (Body0.IsDynamic())
		{
			const FSolverVec3 DX0 = Body0.InvM() * PushOut;
			const FSolverVec3 DR0 = ManifoldPoint.WorldContactTangentUAngular0 * PushOutTangentU + ManifoldPoint.WorldContactTangentVAngular0 * PushOutTangentV;
			Body0.ApplyPositionDelta(DX0);
			Body0.ApplyRotationDelta(DR0);
		}
		if (Body1.IsDynamic())
		{
			const FSolverVec3 DX1 = -Body1.InvM() * PushOut;
			const FSolverVec3 DR1 = ManifoldPoint.WorldContactTangentUAngular1 * -PushOutTangentU + ManifoldPoint.WorldContactTangentVAngular1 * -PushOutTangentV;
			Body1.ApplyPositionDelta(DX1);
			Body1.ApplyRotationDelta(DR1);
		}
	}

	FORCEINLINE_DEBUGGABLE void ApplyPositionCorrectionNormal(
		const FSolverReal Stiffness,
		const FSolverReal ContactDeltaNormal,
		FPBDCollisionSolverManifoldPoint& ManifoldPoint,
		FConstraintSolverBody& Body0,
		FConstraintSolverBody& Body1)
	{
		FSolverReal PushOutNormal = FSolverReal(0);

		CalculatePositionCorrectionNormal(
			Stiffness,
			ContactDeltaNormal,
			ManifoldPoint.ContactMassNormal,
			ManifoldPoint.NetPushOutNormal,
			PushOutNormal);						// Out

		ManifoldPoint.NetPushOutNormal += PushOutNormal;

		// Update the particle state based on the pushout
		if (Body0.IsDynamic())
		{
			const FSolverVec3 DX0 = (Body0.InvM() * PushOutNormal) * ManifoldPoint.WorldContactNormal;
			const FSolverVec3 DR0 = ManifoldPoint.WorldContactNormalAngular0 * PushOutNormal;
			Body0.ApplyPositionDelta(DX0);
			Body0.ApplyRotationDelta(DR0);
		}
		if (Body1.IsDynamic())
		{
			const FSolverVec3 DX1 = (Body1.InvM() * -PushOutNormal) * ManifoldPoint.WorldContactNormal;
			const FSolverVec3 DR1 = ManifoldPoint.WorldContactNormalAngular1 * -PushOutNormal;
			Body1.ApplyPositionDelta(DX1);
			Body1.ApplyRotationDelta(DR1);
		}
	}


	FORCEINLINE_DEBUGGABLE void ApplyVelocityCorrection(
		const FSolverReal Stiffness,
		const FSolverReal Dt,
		const FSolverReal DynamicFriction,
		const FSolverReal ContactVelocityDeltaNormal,
		const FSolverReal ContactVelocityDeltaTangent0,
		const FSolverReal ContactVelocityDeltaTangent1,
		const FSolverReal MinImpulseNormal,
		FPBDCollisionSolverManifoldPoint& ManifoldPoint,
		FConstraintSolverBody& Body0,
		FConstraintSolverBody& Body1)
	{
		FSolverReal ImpulseNormal = -Stiffness * ManifoldPoint.ContactMassNormal * ContactVelocityDeltaNormal;
		FSolverReal ImpulseTangentU = -Stiffness * ManifoldPoint.ContactMassTangentU * ContactVelocityDeltaTangent0;
		FSolverReal ImpulseTangentV = -Stiffness * ManifoldPoint.ContactMassTangentV * ContactVelocityDeltaTangent1;

		// Clamp the total impulse to be positive along the normal. We can apply a net negative impulse, 
		// but only to correct the velocity that was added by pushout (in which case MinImpulseNormal will be negative).
		const FSolverReal NetImpulseNormal = ManifoldPoint.NetImpulseNormal + ImpulseNormal;
		if ((ManifoldPoint.NetImpulseNormal + ImpulseNormal) < MinImpulseNormal)
		{
			// We are trying to apply a net negative impulse larger than one to counteract the effective pushout impulse
			// so clamp the net impulse to be equal to minus the pushout impulse along the normal.
			ImpulseNormal = MinImpulseNormal - ManifoldPoint.NetImpulseNormal;
		}

		// Clamp the tangential impulses to the friction cone
		if ((DynamicFriction > 0) && (Dt > 0))
		{
			const FSolverReal MaxImpulseTangent = FMath::Max(FSolverReal(0), DynamicFriction * (ManifoldPoint.NetImpulseNormal + ImpulseNormal + ManifoldPoint.NetPushOutNormal / Dt));
			const FSolverReal MaxImpulseTangentSq = FMath::Square(MaxImpulseTangent);
			const FSolverReal ImpulseTangentSq = FMath::Square(ImpulseTangentU) + FMath::Square(ImpulseTangentV);
			if (ImpulseTangentSq > (MaxImpulseTangentSq + UE_SMALL_NUMBER))
			{
				const FSolverReal ImpulseTangentScale = MaxImpulseTangent * FMath::InvSqrt(ImpulseTangentSq);
				ImpulseTangentU *= ImpulseTangentScale;
				ImpulseTangentV *= ImpulseTangentScale;
			}
		}

		ManifoldPoint.NetImpulseNormal += ImpulseNormal;
		ManifoldPoint.NetImpulseTangentU += ImpulseTangentU;
		ManifoldPoint.NetImpulseTangentV += ImpulseTangentV;

		// Apply the velocity deltas from the impulse
		const FSolverVec3 Impulse = ImpulseNormal * ManifoldPoint.WorldContactNormal + ImpulseTangentU * ManifoldPoint.WorldContactTangentU + ImpulseTangentV * ManifoldPoint.WorldContactTangentV;
		if (Body0.IsDynamic())
		{
			const FSolverVec3 DV0 = Body0.InvM() * Impulse;
			const FSolverVec3 DW0 = ManifoldPoint.WorldContactNormalAngular0 * ImpulseNormal + ManifoldPoint.WorldContactTangentUAngular0 * ImpulseTangentU + ManifoldPoint.WorldContactTangentVAngular0 * ImpulseTangentV;
			Body0.ApplyVelocityDelta(DV0, DW0);
		}
		if (Body1.IsDynamic())
		{
			const FSolverVec3 DV1 = -Body1.InvM() * Impulse;
			const FSolverVec3 DW1 = ManifoldPoint.WorldContactNormalAngular1 * -ImpulseNormal + ManifoldPoint.WorldContactTangentUAngular1 * -ImpulseTangentU + ManifoldPoint.WorldContactTangentVAngular1 * -ImpulseTangentV;
			Body1.ApplyVelocityDelta(DV1, DW1);
		}
	}

	FORCEINLINE_DEBUGGABLE void ApplyVelocityCorrectionNormal(
		const FSolverReal Stiffness,
		const FSolverReal ContactVelocityDeltaNormal,
		const FSolverReal MinImpulseNormal,
		FPBDCollisionSolverManifoldPoint& ManifoldPoint,
		FConstraintSolverBody& Body0,
		FConstraintSolverBody& Body1)
	{
		FSolverReal ImpulseNormal = -(Stiffness * ManifoldPoint.ContactMassNormal) * ContactVelocityDeltaNormal;

		// See comments in ApplyVelocityCorrection
		const FSolverReal NetImpulseNormal = ManifoldPoint.NetImpulseNormal + ImpulseNormal;
		if (NetImpulseNormal < MinImpulseNormal)
		{
			ImpulseNormal = ImpulseNormal - NetImpulseNormal + MinImpulseNormal;
		}

		ManifoldPoint.NetImpulseNormal += ImpulseNormal;

		// Calculate the velocity deltas from the impulse
		FSolverVec3 Impulse = ImpulseNormal * ManifoldPoint.WorldContactNormal;
		if (Body0.IsDynamic())
		{
			const FSolverVec3 DV0 = Body0.InvM() * Impulse;
			const FSolverVec3 DW0 = ManifoldPoint.WorldContactNormalAngular0 * ImpulseNormal;
			Body0.ApplyVelocityDelta(DV0, DW0);
		}
		if (Body1.IsDynamic())
		{
			const FSolverVec3 DV1 = -Body1.InvM() * Impulse;
			const FSolverVec3 DW1 = ManifoldPoint.WorldContactNormalAngular1 * -ImpulseNormal;
			Body1.ApplyVelocityDelta(DV1, DW1);
		}
	}


	//////////////////////////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////////////////////////

	FORCEINLINE_DEBUGGABLE void FPBDCollisionSolverManifoldPoint::InitContact(
		const FSolverReal Dt,
		const FConstraintSolverBody& Body0,
		const FConstraintSolverBody& Body1,
		const FSolverVec3& InRelativeContactPosition0,
		const FSolverVec3& InRelativeContactPosition1,
		const FSolverVec3& InWorldContactNormal,
		const FSolverVec3& InWorldContactTangentU,
		const FSolverVec3& InWorldContactTangentV,
		const FSolverReal InWorldContactDeltaNormal,
		const FSolverReal InWorldContactDeltaTangentU,
		const FSolverReal InWorldContactDeltaTangentV)
	{
		RelativeContactPosition0 = InRelativeContactPosition0;
		RelativeContactPosition1 = InRelativeContactPosition1;
		WorldContactNormal = InWorldContactNormal;
		WorldContactTangentU = InWorldContactTangentU;
		WorldContactTangentV = InWorldContactTangentV;
		WorldContactDeltaNormal = InWorldContactDeltaNormal;
		WorldContactDeltaTangentU = InWorldContactDeltaTangentU;
		WorldContactDeltaTangentV = InWorldContactDeltaTangentV;

		NetPushOutNormal = FSolverReal(0);
		NetPushOutTangentU = FSolverReal(0);
		NetPushOutTangentV = FSolverReal(0);
		NetImpulseNormal = FSolverReal(0);
		NetImpulseTangentU = FSolverReal(0);
		NetImpulseTangentV = FSolverReal(0);

		StaticFrictionRatio = FSolverReal(0);
		WorldContactVelocityTargetNormal = FSolverReal(0);

		UpdateMass(Body0, Body1);
	}


	FORCEINLINE_DEBUGGABLE void FPBDCollisionSolverManifoldPoint::InitContact(
		const FSolverReal Dt,
		const FConstraintSolverBody& Body0,
		const FConstraintSolverBody& Body1,
		const SolverVectorRegister& InRelativeContactPosition0,
		const SolverVectorRegister& InRelativeContactPosition1,
		const SolverVectorRegister& InWorldContactNormal,
		const SolverVectorRegister& InWorldContactTangentU,
		const SolverVectorRegister& InWorldContactTangentV,
		const FSolverReal InWorldContactDeltaNormal,
		const FSolverReal InWorldContactDeltaTangentU,
		const FSolverReal InWorldContactDeltaTangentV)
	{
		VectorStoreFloat3(InRelativeContactPosition0, &RelativeContactPosition0);
		VectorStoreFloat3(InRelativeContactPosition1, &RelativeContactPosition1);
		VectorStoreFloat3(InWorldContactNormal, &WorldContactNormal);
		VectorStoreFloat3(InWorldContactTangentU, &WorldContactTangentU);
		VectorStoreFloat3(InWorldContactTangentV, &WorldContactTangentV);
		WorldContactDeltaNormal = InWorldContactDeltaNormal;
		WorldContactDeltaTangentU = InWorldContactDeltaTangentU;
		WorldContactDeltaTangentV = InWorldContactDeltaTangentV;

		NetPushOutNormal = FSolverReal(0);
		NetPushOutTangentU = FSolverReal(0);
		NetPushOutTangentV = FSolverReal(0);
		NetImpulseNormal = FSolverReal(0);
		NetImpulseTangentU = FSolverReal(0);
		NetImpulseTangentV = FSolverReal(0);

		StaticFrictionRatio = FSolverReal(0);
		WorldContactVelocityTargetNormal = FSolverReal(0);

		UpdateMass(
			Body0, 
			Body1,
			InRelativeContactPosition0,
			InRelativeContactPosition1,
			InWorldContactNormal,
			InWorldContactTangentU,
			InWorldContactTangentV);
	}

	FORCEINLINE_DEBUGGABLE void FPBDCollisionSolverManifoldPoint::InitMaterial(
		const FConstraintSolverBody& Body0,
		const FConstraintSolverBody& Body1,
		const FSolverReal InRestitution,
		const FSolverReal InRestitutionVelocityThreshold)
	{
		if (InRestitution > FSolverReal(0))
		{
			const FSolverVec3 ContactVelocity = CalculateContactVelocity(Body0, Body1);
			const FSolverReal ContactVelocityNormal = FSolverVec3::DotProduct(ContactVelocity, WorldContactNormal);
			if (ContactVelocityNormal < -InRestitutionVelocityThreshold)
			{
				WorldContactVelocityTargetNormal = -InRestitution * ContactVelocityNormal;
			}
		}
	}

	FORCEINLINE_DEBUGGABLE void FPBDCollisionSolverManifoldPoint::UpdateMass(const FConstraintSolverBody& Body0, const FConstraintSolverBody& Body1)
	{
		FSolverReal ContactMassInvNormal = FSolverReal(0);
		FSolverReal ContactMassInvTangentU = FSolverReal(0);
		FSolverReal ContactMassInvTangentV = FSolverReal(0);

		// These are not used if not initialized below so no need to clear
		//WorldContactNormalAngular0 = FSolverVec3(0);
		//WorldContactTangentUAngular0 = FSolverVec3(0);
		//WorldContactTangentVAngular0 = FSolverVec3(0);
		//WorldContactNormalAngular1 = FSolverVec3(0);
		//WorldContactTangentUAngular1 = FSolverVec3(0);
		//WorldContactTangentVAngular1 = FSolverVec3(0);

		if (Body0.IsDynamic())
		{
			const FSolverVec3 R0xN = FSolverVec3::CrossProduct(RelativeContactPosition0, WorldContactNormal);
			const FSolverVec3 R0xU = FSolverVec3::CrossProduct(RelativeContactPosition0, WorldContactTangentU);
			const FSolverVec3 R0xV = FSolverVec3::CrossProduct(RelativeContactPosition0, WorldContactTangentV);

			const FSolverMatrix33 InvI0 = Body0.InvI();

			WorldContactNormalAngular0 = InvI0 * R0xN;
			WorldContactTangentUAngular0 = InvI0 * R0xU;
			WorldContactTangentVAngular0 = InvI0 * R0xV;

			ContactMassInvNormal += FSolverVec3::DotProduct(R0xN, WorldContactNormalAngular0) + Body0.InvM();
			ContactMassInvTangentU += FSolverVec3::DotProduct(R0xU, WorldContactTangentUAngular0) + Body0.InvM();
			ContactMassInvTangentV += FSolverVec3::DotProduct(R0xV, WorldContactTangentVAngular0) + Body0.InvM();
		}
		if (Body1.IsDynamic())
		{
			const FSolverVec3 R1xN = FSolverVec3::CrossProduct(RelativeContactPosition1, WorldContactNormal);
			const FSolverVec3 R1xU = FSolverVec3::CrossProduct(RelativeContactPosition1, WorldContactTangentU);
			const FSolverVec3 R1xV = FSolverVec3::CrossProduct(RelativeContactPosition1, WorldContactTangentV);

			const FSolverMatrix33 InvI1 = Body1.InvI();

			WorldContactNormalAngular1 = InvI1 * R1xN;
			WorldContactTangentUAngular1 = InvI1 * R1xU;
			WorldContactTangentVAngular1 = InvI1 * R1xV;

			ContactMassInvNormal += FSolverVec3::DotProduct(R1xN, WorldContactNormalAngular1) + Body1.InvM();
			ContactMassInvTangentU += FSolverVec3::DotProduct(R1xU, WorldContactTangentUAngular1) + Body1.InvM();
			ContactMassInvTangentV += FSolverVec3::DotProduct(R1xV, WorldContactTangentVAngular1) + Body1.InvM();
		}

		ContactMassNormal = (ContactMassInvNormal > FSolverReal(UE_SMALL_NUMBER)) ? FSolverReal(1) / ContactMassInvNormal : FSolverReal(0);
		ContactMassTangentU = (ContactMassInvTangentU > FSolverReal(UE_SMALL_NUMBER)) ? FSolverReal(1) / ContactMassInvTangentU : FSolverReal(0);
		ContactMassTangentV = (ContactMassInvTangentV > FSolverReal(UE_SMALL_NUMBER)) ? FSolverReal(1) / ContactMassInvTangentV : FSolverReal(0);
	}

	FORCEINLINE_DEBUGGABLE void FPBDCollisionSolverManifoldPoint::UpdateMass(
		const FConstraintSolverBody& Body0, 
		const FConstraintSolverBody& Body1,
		const SolverVectorRegister& InRelativeContactPosition0,
		const SolverVectorRegister& InRelativeContactPosition1,
		const SolverVectorRegister& InWorldContactNormal,
		const SolverVectorRegister& InWorldContactTangentU,
		const SolverVectorRegister& InWorldContactTangentV)
	{
		FSolverReal ContactMassInvNormal = FSolverReal(0);
		FSolverReal ContactMassInvTangentU = FSolverReal(0);
		FSolverReal ContactMassInvTangentV = FSolverReal(0);

		if (Body0.IsDynamic())
		{
			const SolverVectorRegister R0xN = VectorCross(InRelativeContactPosition0, InWorldContactNormal);
			const SolverVectorRegister R0xU = VectorCross(InRelativeContactPosition0, InWorldContactTangentU);
			const SolverVectorRegister R0xV = VectorCross(InRelativeContactPosition0, InWorldContactTangentV);

			const SolverVectorRegister InvMScale0 = VectorSetFloat1(Body0.InvMScale());
			const FSolverMatrix33& InvI0 = Body0.SolverBody().InvI();

			const SolverVectorRegister NormalAngular0 = VectorMultiply(VectorTransformVector(R0xN, &InvI0), InvMScale0);
			const SolverVectorRegister TangentUAngular0 = VectorMultiply(VectorTransformVector(R0xU, &InvI0), InvMScale0);
			const SolverVectorRegister TangentVAngular0 = VectorMultiply(VectorTransformVector(R0xV, &InvI0), InvMScale0);

			ContactMassInvNormal += VectorGetComponent(VectorDot3(R0xN, NormalAngular0), 0) + Body0.InvM();
			ContactMassInvTangentU += VectorGetComponent(VectorDot3(R0xU, TangentUAngular0), 0) + Body0.InvM();
			ContactMassInvTangentV += VectorGetComponent(VectorDot3(R0xV, TangentVAngular0), 0) + Body0.InvM();

			VectorStoreFloat3(NormalAngular0, &WorldContactNormalAngular0);
			VectorStoreFloat3(TangentUAngular0, &WorldContactTangentUAngular0);
			VectorStoreFloat3(TangentVAngular0, &WorldContactTangentVAngular0);
		}
		if (Body1.IsDynamic())
		{
			const SolverVectorRegister R1xN = VectorCross(InRelativeContactPosition1, InWorldContactNormal);
			const SolverVectorRegister R1xU = VectorCross(InRelativeContactPosition1, InWorldContactTangentU);
			const SolverVectorRegister R1xV = VectorCross(InRelativeContactPosition1, InWorldContactTangentV);

			const SolverVectorRegister InvMScale1 = VectorSetFloat1(Body1.InvMScale());
			const FSolverMatrix33& InvI1 = Body1.SolverBody().InvI();

			const SolverVectorRegister NormalAngular1 = VectorMultiply(VectorTransformVector(R1xN, &InvI1), InvMScale1);
			const SolverVectorRegister TangentUAngular1 = VectorMultiply(VectorTransformVector(R1xU, &InvI1), InvMScale1);
			const SolverVectorRegister TangentVAngular1 = VectorMultiply(VectorTransformVector(R1xV, &InvI1), InvMScale1);

			ContactMassInvNormal += VectorGetComponent(VectorDot3(R1xN, NormalAngular1), 0) + Body1.InvM();
			ContactMassInvTangentU += VectorGetComponent(VectorDot3(R1xU, TangentUAngular1), 0) + Body1.InvM();
			ContactMassInvTangentV += VectorGetComponent(VectorDot3(R1xV, TangentVAngular1), 0) + Body1.InvM();

			VectorStoreFloat3(NormalAngular1, &WorldContactNormalAngular1);
			VectorStoreFloat3(TangentUAngular1, &WorldContactTangentUAngular1);
			VectorStoreFloat3(TangentVAngular1, &WorldContactTangentVAngular1);
		}

		ContactMassNormal = (ContactMassInvNormal > FSolverReal(UE_SMALL_NUMBER)) ? FSolverReal(1) / ContactMassInvNormal : FSolverReal(0);
		ContactMassTangentU = (ContactMassInvTangentU > FSolverReal(UE_SMALL_NUMBER)) ? FSolverReal(1) / ContactMassInvTangentU : FSolverReal(0);
		ContactMassTangentV = (ContactMassInvTangentV > FSolverReal(UE_SMALL_NUMBER)) ? FSolverReal(1) / ContactMassInvTangentV : FSolverReal(0);
	}

	FORCEINLINE_DEBUGGABLE void FPBDCollisionSolverManifoldPoint::UpdateMassNormal(const FConstraintSolverBody& Body0, const FConstraintSolverBody& Body1)
	{
		FSolverReal ContactMassInvNormal = FSolverReal(0);
		if (Body0.IsDynamic())
		{
			const FSolverVec3 R0xN = FSolverVec3::CrossProduct(RelativeContactPosition0, WorldContactNormal);
			const FSolverMatrix33 InvI0 = Body0.InvI();
			WorldContactNormalAngular0 = InvI0 * R0xN;
			ContactMassInvNormal += FSolverVec3::DotProduct(R0xN, WorldContactNormalAngular0) + Body0.InvM();
		}
		if (Body1.IsDynamic())
		{
			const FSolverVec3 R1xN = FSolverVec3::CrossProduct(RelativeContactPosition1, WorldContactNormal);
			const FSolverMatrix33 InvI1 = Body1.InvI();
			WorldContactNormalAngular1 = InvI1 * R1xN;
			ContactMassInvNormal += FSolverVec3::DotProduct(R1xN, WorldContactNormalAngular1) + Body1.InvM();
		}
		ContactMassNormal = (ContactMassInvNormal > FSolverReal(UE_SMALL_NUMBER)) ? FSolverReal(1) / ContactMassInvNormal : FSolverReal(0);
	}

	FORCEINLINE_DEBUGGABLE void FPBDCollisionSolverManifoldPoint::CalculateContactPositionErrorNormal(const FConstraintSolverBody& Body0, const FConstraintSolverBody& Body1, const FSolverReal MaxPushOut, FSolverReal& OutContactDeltaNormal) const
	{
		// Linear version: calculate the contact delta assuming linear motion after applying a positional impulse at the contact point. There will be an error that depends on the size of the rotation.
		const FSolverVec3 ContactDelta0 = Body0.DP() + FSolverVec3::CrossProduct(Body0.DQ(), RelativeContactPosition0);
		const FSolverVec3 ContactDelta1 = Body1.DP() + FSolverVec3::CrossProduct(Body1.DQ(), RelativeContactPosition1);
		const FSolverVec3 ContactDelta = ContactDelta0 - ContactDelta1;
		OutContactDeltaNormal = WorldContactDeltaNormal + FSolverVec3::DotProduct(ContactDelta, WorldContactNormal);

		// NOTE: OutContactDeltaNormal is negative for penetration
		// NOTE: MaxPushOut == 0 disables the pushout limits
		if ((MaxPushOut > 0) && (OutContactDeltaNormal < -MaxPushOut))
		{
			OutContactDeltaNormal = -MaxPushOut;
		}
	}

	FORCEINLINE_DEBUGGABLE void FPBDCollisionSolverManifoldPoint::CalculateContactPositionErrorTangential(const FConstraintSolverBody& Body0, const FConstraintSolverBody& Body1, FSolverReal& OutContactDeltaTangentU, FSolverReal& OutContactDeltaTangentV) const
	{
		// Linear version: calculate the contact delta assuming linear motion after applying a positional impulse at the contact point. There will be an error that depends on the size of the rotation.
		const FSolverVec3 ContactDelta0 = Body0.DP() + FSolverVec3::CrossProduct(Body0.DQ(), RelativeContactPosition0);
		const FSolverVec3 ContactDelta1 = Body1.DP() + FSolverVec3::CrossProduct(Body1.DQ(), RelativeContactPosition1);
		const FSolverVec3 ContactDelta = ContactDelta0 - ContactDelta1;
		OutContactDeltaTangentU = WorldContactDeltaTangentU + FSolverVec3::DotProduct(ContactDelta, WorldContactTangentU);
		OutContactDeltaTangentV = WorldContactDeltaTangentV + FSolverVec3::DotProduct(ContactDelta, WorldContactTangentV);
	}

	FORCEINLINE_DEBUGGABLE void FPBDCollisionSolverManifoldPoint::CalculateContactPositionError(const FConstraintSolverBody& Body0, const FConstraintSolverBody& Body1, const FSolverReal MaxPushOut, FSolverReal& OutContactDeltaNormal, FSolverReal& OutContactDeltaTangentU, FSolverReal& OutContactDeltaTangentV) const
	{
#if CHAOS_NONLINEAR_COLLISIONS_ENABLED
#error "Non-linear collision solver probably no longer functional - fix it!"
		// Non-linear version: calculate the contact delta after we have converted the current positional impulses into position and rotation corrections.
		// We could precalculate and store the LocalContactPositions if we really want to use this nonlinear version
		const FSolverVec3 LocalContactPosition0 = Body0.Q().Inverse() * RelativeContactPosition0;
		const FSolverVec3 LocalContactPosition1 = Body1.Q().Inverse() * RelativeContactPosition1;
		const FSolverVec3 ContactDelta = (Body0.CorrectedP() + Body0.CorrectedQ() * LocalContactPosition0) - (Body1.CorrectedP() + Body1.CorrectedQ() * LocalContactPosition1);
		OutContactDeltaNormal = FSolverVec3::DotProduct(ContactDelta, WorldContactNormal);
		OutContactDeltaTangentU = FSolverVec3::DotProduct(ContactDelta, WorldContactTangentU);
		OutContactDeltaTangentV = FSolverVec3::DotProduct(ContactDelta, WorldContactTangentV);
#else
		// Linear version: calculate the contact delta assuming linear motion after applying a positional impulse at the contact point. There will be an error that depends on the size of the rotation.
		const FSolverVec3 ContactDelta0 = Body0.DP() + FSolverVec3::CrossProduct(Body0.DQ(), RelativeContactPosition0);
		const FSolverVec3 ContactDelta1 = Body1.DP() + FSolverVec3::CrossProduct(Body1.DQ(), RelativeContactPosition1);
		const FSolverVec3 ContactDelta = ContactDelta0 - ContactDelta1;
		OutContactDeltaNormal = WorldContactDeltaNormal + FSolverVec3::DotProduct(ContactDelta, WorldContactNormal);
		OutContactDeltaTangentU = WorldContactDeltaTangentU + FSolverVec3::DotProduct(ContactDelta, WorldContactTangentU);
		OutContactDeltaTangentV = WorldContactDeltaTangentV + FSolverVec3::DotProduct(ContactDelta, WorldContactTangentV);
#endif

		// NOTE: OutContactDeltaNormal is negative for penetration
		// NOTE: MaxPushOut == 0 disables the pushout limits
		if ((MaxPushOut > 0) && (OutContactDeltaNormal < -MaxPushOut))
		{
			OutContactDeltaNormal = -MaxPushOut;
		}
	}

	FORCEINLINE_DEBUGGABLE void FPBDCollisionSolverManifoldPoint::CalculateContactVelocityError(const FConstraintSolverBody& Body0, const FConstraintSolverBody& Body1, const FSolverReal DynamicFriction, const FSolverReal Dt, FSolverReal& OutContactVelocityDeltaNormal, FSolverReal& OutContactVelocityDeltaTangent0, FSolverReal& OutContactVelocityDeltaTangent1) const
	{
		const FSolverVec3 ContactVelocity0 = Body0.V() + FSolverVec3::CrossProduct(Body0.W(), RelativeContactPosition0);
		const FSolverVec3 ContactVelocity1 = Body1.V() + FSolverVec3::CrossProduct(Body1.W(), RelativeContactPosition1);
		const FSolverVec3 ContactVelocity = ContactVelocity0 - ContactVelocity1;
		const FSolverReal ContactVelocityNormal = FSolverVec3::DotProduct(ContactVelocity, WorldContactNormal);
		const FSolverReal ContactVelocityTangent0 = FSolverVec3::DotProduct(ContactVelocity, WorldContactTangentU);
		const FSolverReal ContactVelocityTangent1 = FSolverVec3::DotProduct(ContactVelocity, WorldContactTangentV);

		OutContactVelocityDeltaNormal = (ContactVelocityNormal - WorldContactVelocityTargetNormal);
		OutContactVelocityDeltaTangent0 = ContactVelocityTangent0;
		OutContactVelocityDeltaTangent1 = ContactVelocityTangent1;
	}

	FORCEINLINE_DEBUGGABLE void FPBDCollisionSolverManifoldPoint::CalculateContactVelocityErrorNormal(const FConstraintSolverBody& Body0, const FConstraintSolverBody& Body1, FSolverReal& OutContactVelocityDeltaNormal) const
	{
		const FSolverVec3 ContactVelocity0 = Body0.V() + FSolverVec3::CrossProduct(Body0.W(), RelativeContactPosition0);
		const FSolverVec3 ContactVelocity1 = Body1.V() + FSolverVec3::CrossProduct(Body1.W(), RelativeContactPosition1);
		const FSolverVec3 ContactVelocity = ContactVelocity0 - ContactVelocity1;
		const FSolverReal ContactVelocityNormal = FSolverVec3::DotProduct(ContactVelocity, WorldContactNormal);

		// Add up the errors in the velocity (current velocity - desired velocity)
		OutContactVelocityDeltaNormal = (ContactVelocityNormal - WorldContactVelocityTargetNormal);
	}

	FORCEINLINE_DEBUGGABLE FSolverVec3 FPBDCollisionSolverManifoldPoint::CalculateContactVelocity(const FConstraintSolverBody& Body0, const FConstraintSolverBody& Body1) const
	{
		const FSolverVec3 ContactVelocity0 = Body0.V() + FVec3::CrossProduct(Body0.W(), RelativeContactPosition0);
		const FSolverVec3 ContactVelocity1 = Body1.V() + FVec3::CrossProduct(Body1.W(), RelativeContactPosition1);
		return ContactVelocity0 - ContactVelocity1;
	}

	FORCEINLINE_DEBUGGABLE bool FPBDCollisionSolverManifoldPoint::ShouldSolveVelocity() const
	{
		// We ensure positive separating velocity for close contacts even if they didn't receive a pushout
		return (NetPushOutNormal > FSolverReal(0)) || (WorldContactDeltaNormal < FSolverReal(0));
	}

	//////////////////////////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////////////////////////

	FORCEINLINE_DEBUGGABLE void FPBDCollisionSolver::SetManifoldPoint(
		const int32 ManifoldPoiontIndex,
		const FSolverReal Dt,
		const FSolverReal InRestitution,
		const FSolverReal InRestitutionVelocityThreshold,
		const FSolverVec3& InRelativeContactPosition0,
		const FSolverVec3& InRelativeContactPosition1,
		const FSolverVec3& InWorldContactNormal,
		const FSolverVec3& InWorldContactTangentU,
		const FSolverVec3& InWorldContactTangentV,
		const FSolverReal InWorldContactDeltaNormal,
		const FSolverReal InWorldContactDeltaTangentU,
		const FSolverReal InWorldContactDeltaTangentV)
	{
		State.ManifoldPoints[ManifoldPoiontIndex].InitContact(
			FSolverReal(Dt),
			State.SolverBodies[0],
			State.SolverBodies[1],
			InRelativeContactPosition0,
			InRelativeContactPosition1,
			InWorldContactNormal,
			InWorldContactTangentU,
			InWorldContactTangentV,
			InWorldContactDeltaNormal,
			InWorldContactDeltaTangentU,
			InWorldContactDeltaTangentV);

		State.ManifoldPoints[ManifoldPoiontIndex].InitMaterial(
			State.SolverBodies[0],
			State.SolverBodies[1],
			InRestitution,
			InRestitutionVelocityThreshold);
	}

	FORCEINLINE_DEBUGGABLE void FPBDCollisionSolver::SetManifoldPoint(
		const int32 ManifoldPoiontIndex,
		const FSolverReal Dt,
		const FSolverReal InRestitution,
		const FSolverReal InRestitutionVelocityThreshold,
		const SolverVectorRegister& InRelativeContactPosition0,
		const SolverVectorRegister& InRelativeContactPosition1,
		const SolverVectorRegister& InWorldContactNormal,
		const SolverVectorRegister& InWorldContactTangentU,
		const SolverVectorRegister& InWorldContactTangentV,
		const FSolverReal InWorldContactDeltaNormal,
		const FSolverReal InWorldContactDeltaTangentU,
		const FSolverReal InWorldContactDeltaTangentV)
	{
		State.ManifoldPoints[ManifoldPoiontIndex].InitContact(
			FSolverReal(Dt),
			State.SolverBodies[0],
			State.SolverBodies[1],
			InRelativeContactPosition0,
			InRelativeContactPosition1,
			InWorldContactNormal,
			InWorldContactTangentU,
			InWorldContactTangentV,
			InWorldContactDeltaNormal,
			InWorldContactDeltaTangentU,
			InWorldContactDeltaTangentV);

		State.ManifoldPoints[ManifoldPoiontIndex].InitMaterial(
			State.SolverBodies[0],
			State.SolverBodies[1],
			InRestitution,
			InRestitutionVelocityThreshold);
	}

	FORCEINLINE_DEBUGGABLE bool FPBDCollisionSolver::SolvePositionWithFriction(const FSolverReal Dt, const FSolverReal MaxPushOut)
	{
		// SolverBody decorator used to add mass scaling
		FConstraintSolverBody& Body0 = SolverBody0();
		FConstraintSolverBody& Body1 = SolverBody1();

		// Accumulate net pushout for friction limits below
		bool bApplyFriction[MaxPointsPerConstraint] = { false, };
		int32 NumFrictionContacts = 0;
		FSolverReal TotalPushOutNormal = FSolverReal(0);

		// Apply the position correction along the normal and determine if we want to run friction on each point
		for (int32 PointIndex = 0; PointIndex < State.NumManifoldPoints; ++PointIndex)
		{
			FPBDCollisionSolverManifoldPoint& SolverManifoldPoint = State.ManifoldPoints[PointIndex];

			FSolverReal ContactDeltaNormal;
			SolverManifoldPoint.CalculateContactPositionErrorNormal(Body0.SolverBody(), Body1.SolverBody(), MaxPushOut, ContactDeltaNormal);

			// Apply a normal correction if we still have penetration or if we are now separated but have previously applied a correction that we may want to undo
			const bool bProcessManifoldPoint = (ContactDeltaNormal < FSolverReal(0)) || (SolverManifoldPoint.NetPushOutNormal > FSolverReal(UE_SMALL_NUMBER));
			if (bProcessManifoldPoint)
			{
				ApplyPositionCorrectionNormal(
					State.Stiffness,
					ContactDeltaNormal,
					SolverManifoldPoint,
					Body0,
					Body1);

				TotalPushOutNormal += SolverManifoldPoint.NetPushOutNormal;
			}

			// Friction gets updated for any point with a net normal correction or where we have previously had a normal correction and 
			// already applied friction (in which case we may need to zero it)
			if ((SolverManifoldPoint.NetPushOutNormal != 0) || (SolverManifoldPoint.NetPushOutTangentU != 0) || (SolverManifoldPoint.NetPushOutTangentV != 0))
			{
				bApplyFriction[PointIndex] = true;
				++NumFrictionContacts;
			}
		}

		// Apply the tangential position correction if required
		if (NumFrictionContacts > 0)
		{
			// We clip the tangential correction at each contact to the friction cone, but we use to average impulse
			// among all contacts as the clipping limit. This is not really correct but it is much more stable to 
			// differences in contacts from tick to tick
			// @todo(chaos): try a decaying maximum per contact point rather than an average (again - we had that once!)
			const FSolverReal FrictionMaxPushOut = TotalPushOutNormal / FSolverReal(NumFrictionContacts);

			for (int32 PointIndex = 0; PointIndex < State.NumManifoldPoints; ++PointIndex)
			{
				if (bApplyFriction[PointIndex])
				{
					FPBDCollisionSolverManifoldPoint& SolverManifoldPoint = State.ManifoldPoints[PointIndex];

					FSolverReal ContactDeltaTangentU, ContactDeltaTangentV;
					SolverManifoldPoint.CalculateContactPositionErrorTangential(Body0.SolverBody(), Body1.SolverBody(), ContactDeltaTangentU, ContactDeltaTangentV);

					ApplyPositionCorrectionTangential(
						State.Stiffness,
						State.StaticFriction,
						State.DynamicFriction,
						FrictionMaxPushOut,
						ContactDeltaTangentU,
						ContactDeltaTangentV,
						SolverManifoldPoint,
						Body0,
						Body1);
				}
			}
		}

		return false;
	}

	FORCEINLINE_DEBUGGABLE bool FPBDCollisionSolver::SolvePositionNoFriction(const FSolverReal Dt, const FSolverReal MaxPushOut)
	{
		// SolverBody decorator used to add mass scaling
		FConstraintSolverBody& Body0 = SolverBody0();
		FConstraintSolverBody& Body1 = SolverBody1();

		// Apply the position correction so that all contacts have zero separation
		for (int32 PointIndex = 0; PointIndex < State.NumManifoldPoints; ++PointIndex)
		{
			FPBDCollisionSolverManifoldPoint& SolverManifoldPoint = State.ManifoldPoints[PointIndex];

			FSolverReal ContactDeltaNormal;
			SolverManifoldPoint.CalculateContactPositionErrorNormal(Body0.SolverBody(), Body1.SolverBody(), MaxPushOut, ContactDeltaNormal);

			const bool bProcessManifoldPoint = (ContactDeltaNormal < FSolverReal(0)) || (SolverManifoldPoint.NetPushOutNormal > FSolverReal(UE_SMALL_NUMBER));
			if (bProcessManifoldPoint)
			{
				ApplyPositionCorrectionNormal(
					State.Stiffness,
					ContactDeltaNormal,
					SolverManifoldPoint,
					Body0,
					Body1);
			}
		}

		return false;
	}

}

