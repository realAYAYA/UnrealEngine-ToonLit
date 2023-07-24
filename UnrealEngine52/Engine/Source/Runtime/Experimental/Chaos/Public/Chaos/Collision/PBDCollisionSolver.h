// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Core.h"
#include "Chaos/Collision/ContactPoint.h"
#include "Chaos/Defines.h"
#include "Chaos/Evolution/SolverBody.h"

// Set to 0 to use a linearized error calculation, and set to 1 to use a non-linear error calculation in collision detection. 
// In principle nonlinear is more accurate when large rotation corrections occur, but this is not too important for collisions because 
// when the bodies settle the corrections are small. The linearized version is significantly faster than the non-linear version because 
// the non-linear version requires a quaternion multiply and renormalization whereas the linear version is just a cross product.
#define CHAOS_NONLINEAR_COLLISIONS_ENABLED 0

namespace Chaos
{
	class FManifoldPoint;
	class FPBDCollisionConstraint;

	namespace Private
	{
		class FPBDCollisionSolver;
	}

	namespace CVars
	{
		extern bool bChaos_PBDCollisionSolver_Velocity_AveragePointEnabled;
		extern bool bChaos_PBDCollisionSolver_Velocity_FrictionEnabled;
		extern float Chaos_PBDCollisionSolver_Position_StaticFrictionStiffness;
	}

	namespace Private
	{
		/**
		 * @brief A single contact point in a FPBDCollisionSolver
		 * @note Internal Chaos class subject to API changes, renaming or removal
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
				const FConstraintSolverBody& Body1);

			/**
			 * @brief Initialize the material related properties of the contact
			*/
			//void InitMaterial(
			//	const FConstraintSolverBody& Body0,
			//	const FConstraintSolverBody& Body1,
			//	const FSolverReal InRestitution,
			//	const FSolverReal InRestitutionVelocityThreshold);

			/**
			 * @brief Update the cached mass properties based on the current body transforms
			*/
			void UpdateMass(const FConstraintSolverBody& Body0, const FConstraintSolverBody& Body1);

			/**
			 * @brief Update the contact mass for the normal correction
			 * This is used by shock propagation.
			*/
			void UpdateMassNormal(const FConstraintSolverBody& Body0, const FConstraintSolverBody& Body1);

			/**
			 * @brief Calculate the position error at the current transforms
			 * @param MaxPushOut a limit on the position error for this iteration to prevent initial-penetration explosion (a common PBD problem)
			*/
			void CalculateContactPositionErrorNormal(const FConstraintSolverBody& Body0, const FConstraintSolverBody& Body1, const FSolverReal MaxPushOut, FSolverReal& OutContactDeltaNormal) const;
			void CalculateContactPositionErrorTangential(const FConstraintSolverBody& Body0, const FConstraintSolverBody& Body1, FSolverReal& OutContactDeltaTanget0, FSolverReal& OutContactDeltaTangent1) const;

			/**
			 * @brief Calculate the velocity error at the current transforms
			*/
			void CalculateContactVelocityErrorNormal(const FConstraintSolverBody& Body0, const FConstraintSolverBody& Body1, FSolverReal& OutContactVelocityDeltaNormal) const;
			void CalculateContactVelocityError(const FConstraintSolverBody& Body0, const FConstraintSolverBody& Body1, const FSolverReal DynamicFriction, const FSolverReal Dt, FSolverReal& OutContactVelocityDeltaNormal, FSolverReal& OutContactVelocityDeltaTangent0, FSolverReal& OutContactVelocityDeltaTangent1) const;

			// @todo(chaos): make private
		public:
			friend class FPBDCollisionSolver;

			/**
			 * @brief Whether we need to solve velocity for this manifold point (only if we were penetrating or applied a pushout)
			*/
			bool ShouldSolveVelocity() const;

			// World-space contact data
			FWorldContactPoint WorldContact;

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

			// Create a solver that is initialized to safe defaults
			static FPBDCollisionSolver MakeInitialized()
			{
				FPBDCollisionSolver Solver;
				Solver.State.Init();
				return Solver;
			}

			// Create a solver with no initialization
			static FPBDCollisionSolver MakeUninitialized()
			{
				return FPBDCollisionSolver();
			}

			// NOTE: Does not initialize any properties. See MakeInitialized
			FPBDCollisionSolver() {}

			/** Reset the state of the collision solver */
			void Reset()
			{
				State.SolverBodies[0].Reset();
				State.SolverBodies[1].Reset();
				State.ManifoldPoints = nullptr;
				State.NumManifoldPoints = 0;
				State.MaxManifoldPoints = 0;
			}

			void ResetManifold()
			{
				State.NumManifoldPoints = 0;
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
				State.SolverBodies[0].SetSolverBody(SolverBody0);
				State.SolverBodies[1].SetSolverBody(SolverBody1);
			}

			int32 NumManifoldPoints() const
			{
				return State.NumManifoldPoints;
			}

			int32 AddManifoldPoint()
			{
				check(State.NumManifoldPoints < State.MaxManifoldPoints);
				return State.NumManifoldPoints++;
			}

			void SetManifoldPointsBuffer(FPBDCollisionSolverManifoldPoint* InManifoldPoints, const int32 InMaxManifoldPoints)
			{
				State.ManifoldPoints = InManifoldPoints;
				State.MaxManifoldPoints = InMaxManifoldPoints;
				State.NumManifoldPoints = 0;
			}

			const FPBDCollisionSolverManifoldPoint& GetManifoldPoint(const int32 ManifoldPointIndex) const
			{
				check(ManifoldPointIndex < NumManifoldPoints());
				return State.ManifoldPoints[ManifoldPointIndex];
			}

			FWorldContactPoint& GetWorldContactPoint(const int32 ManifoldPointIndex)
			{
				return State.ManifoldPoints[ManifoldPointIndex].WorldContact;
			}

			/**
			 * Set up a manifold point (also calls FinalizeManifoldPoint)
			*/
			void SetManifoldPoint(
				const int32 ManifoldPoiontIndex,
				const FSolverReal Dt,
				const FSolverVec3& InRelativeContactPosition0,
				const FSolverVec3& InRelativeContactPosition1,
				const FSolverVec3& InWorldContactNormal,
				const FSolverVec3& InWorldContactTangentU,
				const FSolverVec3& InWorldContactTangentV,
				const FSolverReal InWorldContactDeltaNormal,
				const FSolverReal InWorldContactDeltaTangentU,
				const FSolverReal InWorldContactDeltaTangentV,
				const FSolverReal InWorldContactVelocityTargetNormal);

			/** 
			 * Finish manifold point setup.
			 * NOTE: Can only be called after the WorldContact has been set up (e.g., see SetManifoldPoint)
			*/
			void FinalizeManifoldPoint(const int32 ManifoldPoiontIndex, const FSolverReal Dt);

			/**
			 * @brief Get the first (decorated) solver body
			 * The decorator add a possible mass scale
			*/
			FConstraintSolverBody& SolverBody0() { return State.SolverBodies[0]; }
			const FConstraintSolverBody& SolverBody0() const { return State.SolverBodies[0]; }

			/**
			 * @brief Get the second (decorated) solver body
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
				{
				}

				void Init()
				{
					SolverBodies[0].Init();
					SolverBodies[1].Init();
					StaticFriction = 0;
					DynamicFriction = 0;
					VelocityFriction = 0;
					Stiffness = 1;
					ManifoldPoints = nullptr;
					NumManifoldPoints = 0;
					MaxManifoldPoints = 0;
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
				FConstraintSolverBody SolverBodies[MaxConstrainedBodies];
				FPBDCollisionSolverManifoldPoint* ManifoldPoints;
				int32 NumManifoldPoints;
				int32 MaxManifoldPoints;
			};

			FState State;
		};


		//////////////////////////////////////////////////////////////////////////////////////////////////
		//////////////////////////////////////////////////////////////////////////////////////////////////
		//////////////////////////////////////////////////////////////////////////////////////////////////
		//////////////////////////////////////////////////////////////////////////////////////////////////

		// If the vector size squared is below the threshold, zero the vector and return false, otherwise leave vector as-is and return true.
		// This is useful for maintaining perfect restitution bounces - we remove any rounding errors in the impulses before applying to the rotation
		// @todo(chaos): consider using a test of absolute element values instead
		// @todo(chaos): disabled for now - too expensive :(
		FORCEINLINE_DEBUGGABLE bool AboveThresholdElseZero(FSolverVec3& InOutV, const FSolverReal ThresholdSq)
		{
			//if (InOutV.SizeSquared() < ThresholdSq)
			//{
			//	InOutV = FSolverVec3(0);
			//	return false;
			//}
			return true;
		}

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
			const FSolverReal NetPushOutTangent,
			FSolverReal& OutPushOutTangent)
		{
			// Bilateral constraint - negative values allowed (unlike the normal correction)
			OutPushOutTangent = -Stiffness * ContactMassTangent * ContactDeltaTangent;
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
				ManifoldPoint.NetPushOutTangentU,
				PushOutTangentU);					// Out

			CalculatePositionCorrectionTangent(
				Stiffness,
				ContactDeltaTangentV,
				ManifoldPoint.ContactMassTangentV,
				ManifoldPoint.NetPushOutTangentV,
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
			const FVec3 PushOut = PushOutTangentU * ManifoldPoint.WorldContact.ContactTangentU + PushOutTangentV * ManifoldPoint.WorldContact.ContactTangentV;
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
				const FSolverVec3 DX0 = (Body0.InvM() * PushOutNormal) * ManifoldPoint.WorldContact.ContactNormal;
				const FSolverVec3 DR0 = ManifoldPoint.WorldContactNormalAngular0 * PushOutNormal;
				Body0.ApplyPositionDelta(DX0);
				Body0.ApplyRotationDelta(DR0);
			}
			if (Body1.IsDynamic())
			{
				const FSolverVec3 DX1 = (Body1.InvM() * -PushOutNormal) * ManifoldPoint.WorldContact.ContactNormal;
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
			const FSolverVec3 Impulse = ImpulseNormal * ManifoldPoint.WorldContact.ContactNormal + ImpulseTangentU * ManifoldPoint.WorldContact.ContactTangentU + ImpulseTangentV * ManifoldPoint.WorldContact.ContactTangentV;
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
			if (ManifoldPoint.NetImpulseNormal + ImpulseNormal < MinImpulseNormal)
			{
				ImpulseNormal = MinImpulseNormal - ManifoldPoint.NetImpulseNormal;
			}

			ManifoldPoint.NetImpulseNormal += ImpulseNormal;

			// Calculate the velocity deltas from the impulse
			FSolverVec3 Impulse = ImpulseNormal * ManifoldPoint.WorldContact.ContactNormal;
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
			const FConstraintSolverBody& Body1)
		{
			NetPushOutNormal = FSolverReal(0);
			NetPushOutTangentU = FSolverReal(0);
			NetPushOutTangentV = FSolverReal(0);
			NetImpulseNormal = FSolverReal(0);
			NetImpulseTangentU = FSolverReal(0);
			NetImpulseTangentV = FSolverReal(0);
			StaticFrictionRatio = FSolverReal(0);

			UpdateMass(Body0, Body1);
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
				const FSolverVec3 R0xN = FSolverVec3::CrossProduct(WorldContact.RelativeContactPoints[0], WorldContact.ContactNormal);
				const FSolverVec3 R0xU = FSolverVec3::CrossProduct(WorldContact.RelativeContactPoints[0], WorldContact.ContactTangentU);
				const FSolverVec3 R0xV = FSolverVec3::CrossProduct(WorldContact.RelativeContactPoints[0], WorldContact.ContactTangentV);

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
				const FSolverVec3 R1xN = FSolverVec3::CrossProduct(WorldContact.RelativeContactPoints[1], WorldContact.ContactNormal);
				const FSolverVec3 R1xU = FSolverVec3::CrossProduct(WorldContact.RelativeContactPoints[1], WorldContact.ContactTangentU);
				const FSolverVec3 R1xV = FSolverVec3::CrossProduct(WorldContact.RelativeContactPoints[1], WorldContact.ContactTangentV);

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

		FORCEINLINE_DEBUGGABLE void FPBDCollisionSolverManifoldPoint::UpdateMassNormal(const FConstraintSolverBody& Body0, const FConstraintSolverBody& Body1)
		{
			FSolverReal ContactMassInvNormal = FSolverReal(0);
			if (Body0.IsDynamic())
			{
				const FSolverVec3 R0xN = FSolverVec3::CrossProduct(WorldContact.RelativeContactPoints[0], WorldContact.ContactNormal);
				const FSolverMatrix33 InvI0 = Body0.InvI();
				WorldContactNormalAngular0 = InvI0 * R0xN;
				ContactMassInvNormal += FSolverVec3::DotProduct(R0xN, WorldContactNormalAngular0) + Body0.InvM();
			}
			if (Body1.IsDynamic())
			{
				const FSolverVec3 R1xN = FSolverVec3::CrossProduct(WorldContact.RelativeContactPoints[1], WorldContact.ContactNormal);
				const FSolverMatrix33 InvI1 = Body1.InvI();
				WorldContactNormalAngular1 = InvI1 * R1xN;
				ContactMassInvNormal += FSolverVec3::DotProduct(R1xN, WorldContactNormalAngular1) + Body1.InvM();
			}
			ContactMassNormal = (ContactMassInvNormal > FSolverReal(UE_SMALL_NUMBER)) ? FSolverReal(1) / ContactMassInvNormal : FSolverReal(0);
		}

		FORCEINLINE_DEBUGGABLE void FPBDCollisionSolverManifoldPoint::CalculateContactPositionErrorNormal(const FConstraintSolverBody& Body0, const FConstraintSolverBody& Body1, const FSolverReal MaxPushOut, FSolverReal& OutContactDeltaNormal) const
		{
			// Linear version: calculate the contact delta assuming linear motion after applying a positional impulse at the contact point. There will be an error that depends on the size of the rotation.
			const FSolverVec3 ContactDelta0 = Body0.DP() + FSolverVec3::CrossProduct(Body0.DQ(), WorldContact.RelativeContactPoints[0]);
			const FSolverVec3 ContactDelta1 = Body1.DP() + FSolverVec3::CrossProduct(Body1.DQ(), WorldContact.RelativeContactPoints[1]);
			const FSolverVec3 ContactDelta = ContactDelta0 - ContactDelta1;
			OutContactDeltaNormal = WorldContact.ContactDeltaNormal + FSolverVec3::DotProduct(ContactDelta, WorldContact.ContactNormal);

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
			const FSolverVec3 ContactDelta0 = Body0.DP() + FSolverVec3::CrossProduct(Body0.DQ(), WorldContact.RelativeContactPoints[0]);
			const FSolverVec3 ContactDelta1 = Body1.DP() + FSolverVec3::CrossProduct(Body1.DQ(), WorldContact.RelativeContactPoints[1]);
			const FSolverVec3 ContactDelta = ContactDelta0 - ContactDelta1;
			OutContactDeltaTangentU = WorldContact.ContactDeltaTangentU + FSolverVec3::DotProduct(ContactDelta, WorldContact.ContactTangentU);
			OutContactDeltaTangentV = WorldContact.ContactDeltaTangentV + FSolverVec3::DotProduct(ContactDelta, WorldContact.ContactTangentV);
		}

		FORCEINLINE_DEBUGGABLE void FPBDCollisionSolverManifoldPoint::CalculateContactVelocityError(const FConstraintSolverBody& Body0, const FConstraintSolverBody& Body1, const FSolverReal DynamicFriction, const FSolverReal Dt, FSolverReal& OutContactVelocityDeltaNormal, FSolverReal& OutContactVelocityDeltaTangent0, FSolverReal& OutContactVelocityDeltaTangent1) const
		{
			const FSolverVec3 ContactVelocity0 = Body0.V() + FSolverVec3::CrossProduct(Body0.W(), WorldContact.RelativeContactPoints[0]);
			const FSolverVec3 ContactVelocity1 = Body1.V() + FSolverVec3::CrossProduct(Body1.W(), WorldContact.RelativeContactPoints[1]);
			const FSolverVec3 ContactVelocity = ContactVelocity0 - ContactVelocity1;
			const FSolverReal ContactVelocityNormal = FSolverVec3::DotProduct(ContactVelocity, WorldContact.ContactNormal);
			const FSolverReal ContactVelocityTangent0 = FSolverVec3::DotProduct(ContactVelocity, WorldContact.ContactTangentU);
			const FSolverReal ContactVelocityTangent1 = FSolverVec3::DotProduct(ContactVelocity, WorldContact.ContactTangentV);

			OutContactVelocityDeltaNormal = (ContactVelocityNormal - WorldContact.ContactTargetVelocityNormal);
			OutContactVelocityDeltaTangent0 = ContactVelocityTangent0;
			OutContactVelocityDeltaTangent1 = ContactVelocityTangent1;
		}

		FORCEINLINE_DEBUGGABLE void FPBDCollisionSolverManifoldPoint::CalculateContactVelocityErrorNormal(const FConstraintSolverBody& Body0, const FConstraintSolverBody& Body1, FSolverReal& OutContactVelocityDeltaNormal) const
		{
			const FSolverVec3 ContactVelocity0 = Body0.V() + FSolverVec3::CrossProduct(Body0.W(), WorldContact.RelativeContactPoints[0]);
			const FSolverVec3 ContactVelocity1 = Body1.V() + FSolverVec3::CrossProduct(Body1.W(), WorldContact.RelativeContactPoints[1]);
			const FSolverVec3 ContactVelocity = ContactVelocity0 - ContactVelocity1;
			const FSolverReal ContactVelocityNormal = FSolverVec3::DotProduct(ContactVelocity, WorldContact.ContactNormal);

			// Add up the errors in the velocity (current velocity - desired velocity)
			OutContactVelocityDeltaNormal = (ContactVelocityNormal - WorldContact.ContactTargetVelocityNormal);
		}

		FORCEINLINE_DEBUGGABLE bool FPBDCollisionSolverManifoldPoint::ShouldSolveVelocity() const
		{
			// We ensure positive separating velocity for close contacts even if they didn't receive a pushout
			return (NetPushOutNormal > FSolverReal(0)) || (WorldContact.ContactDeltaNormal < FSolverReal(0));
		}

		//////////////////////////////////////////////////////////////////////////////////////////////////
		//////////////////////////////////////////////////////////////////////////////////////////////////
		//////////////////////////////////////////////////////////////////////////////////////////////////
		//////////////////////////////////////////////////////////////////////////////////////////////////

		FORCEINLINE_DEBUGGABLE  void FPBDCollisionSolver::FinalizeManifoldPoint(const int32 ManifoldPoiontIndex, const FSolverReal Dt)
		{
			State.ManifoldPoints[ManifoldPoiontIndex].InitContact(
				FSolverReal(Dt),
				State.SolverBodies[0],
				State.SolverBodies[1]);
		}

		FORCEINLINE_DEBUGGABLE void FPBDCollisionSolver::SetManifoldPoint(
			const int32 ManifoldPoiontIndex,
			const FSolverReal Dt,
			const FSolverVec3& InRelativeContactPosition0,
			const FSolverVec3& InRelativeContactPosition1,
			const FSolverVec3& InWorldContactNormal,
			const FSolverVec3& InWorldContactTangentU,
			const FSolverVec3& InWorldContactTangentV,
			const FSolverReal InWorldContactDeltaNormal,
			const FSolverReal InWorldContactDeltaTangentU,
			const FSolverReal InWorldContactDeltaTangentV,
			const FSolverReal InWorldContactVelocityTargetNormal)
		{
			FWorldContactPoint& WorldContactPoint = State.ManifoldPoints[ManifoldPoiontIndex].WorldContact;
			WorldContactPoint.RelativeContactPoints[0] = InRelativeContactPosition0;
			WorldContactPoint.RelativeContactPoints[1] = InRelativeContactPosition1;
			WorldContactPoint.ContactNormal = InWorldContactNormal;
			WorldContactPoint.ContactTangentU = InWorldContactTangentU;
			WorldContactPoint.ContactTangentV = InWorldContactTangentV;
			WorldContactPoint.ContactDeltaNormal = InWorldContactDeltaNormal;
			WorldContactPoint.ContactDeltaTangentU = InWorldContactDeltaTangentU;
			WorldContactPoint.ContactDeltaTangentV = InWorldContactDeltaTangentV;
			WorldContactPoint.ContactTargetVelocityNormal = InWorldContactVelocityTargetNormal;

			State.ManifoldPoints[ManifoldPoiontIndex].InitContact(
				FSolverReal(Dt),
				State.SolverBodies[0],
				State.SolverBodies[1]);
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
			for (int32 PointIndex = 0; PointIndex < NumManifoldPoints(); ++PointIndex)
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
				const FSolverReal FrictionStiffness = State.Stiffness * CVars::Chaos_PBDCollisionSolver_Position_StaticFrictionStiffness;

				for (int32 PointIndex = 0; PointIndex < NumManifoldPoints(); ++PointIndex)
				{
					if (bApplyFriction[PointIndex])
					{
						FPBDCollisionSolverManifoldPoint& SolverManifoldPoint = State.ManifoldPoints[PointIndex];

						FSolverReal ContactDeltaTangentU, ContactDeltaTangentV;
						SolverManifoldPoint.CalculateContactPositionErrorTangential(Body0.SolverBody(), Body1.SolverBody(), ContactDeltaTangentU, ContactDeltaTangentV);

						ApplyPositionCorrectionTangential(
							FrictionStiffness,
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
			for (int32 PointIndex = 0; PointIndex < NumManifoldPoints(); ++PointIndex)
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

		FORCEINLINE_DEBUGGABLE bool FPBDCollisionSolver::SolveVelocity(const FSolverReal Dt, const bool bApplyDynamicFriction)
		{
			// Apply restitution at the average contact point
			// This means we don't need to run as many iterations to get stable bouncing
			// It also helps with zero restitution to counter any velocioty added by the PBD solve
			const bool bSolveAverageContact = (NumManifoldPoints() > 1) && CVars::bChaos_PBDCollisionSolver_Velocity_AveragePointEnabled;
			if (bSolveAverageContact)
			{
				SolveVelocityAverage(Dt);
			}

			FConstraintSolverBody& Body0 = SolverBody0();
			FConstraintSolverBody& Body1 = SolverBody1();

			// NOTE: this dynamic friction implementation is iteration-count sensitive
			// @todo(chaos): fix iteration count dependence of dynamic friction
			const FSolverReal DynamicFriction = (bApplyDynamicFriction && (Dt > 0) && CVars::bChaos_PBDCollisionSolver_Velocity_FrictionEnabled) ? State.VelocityFriction : FSolverReal(0);

			for (int32 PointIndex = 0; PointIndex < NumManifoldPoints(); ++PointIndex)
			{
				FPBDCollisionSolverManifoldPoint& SolverManifoldPoint = State.ManifoldPoints[PointIndex];

				if (SolverManifoldPoint.ShouldSolveVelocity())
				{
					const FSolverReal MinImpulseNormal = FMath::Min(FSolverReal(0), -SolverManifoldPoint.NetPushOutNormal / Dt);

					if (DynamicFriction > 0)
					{
						FSolverReal ContactVelocityDeltaNormal, ContactVelocityDeltaTangentU, ContactVelocityDeltaTangentV;
						SolverManifoldPoint.CalculateContactVelocityError(Body0, Body1, DynamicFriction, Dt, ContactVelocityDeltaNormal, ContactVelocityDeltaTangentU, ContactVelocityDeltaTangentV);

						ApplyVelocityCorrection(
							State.Stiffness,
							Dt,
							DynamicFriction,
							ContactVelocityDeltaNormal,
							ContactVelocityDeltaTangentU,
							ContactVelocityDeltaTangentV,
							MinImpulseNormal,
							SolverManifoldPoint,
							Body0,
							Body1);
					}
					else
					{
						FSolverReal ContactVelocityDeltaNormal;
						SolverManifoldPoint.CalculateContactVelocityErrorNormal(Body0, Body1, ContactVelocityDeltaNormal);

						ApplyVelocityCorrectionNormal(
							State.Stiffness,
							ContactVelocityDeltaNormal,
							MinImpulseNormal,
							SolverManifoldPoint,
							Body0,
							Body1);
					}
				}
			}

			// Early-out support for the velocity solve is not currently very important because we
			// only run one iteration in the velocity solve phase.
			// @todo(chaos): support early-out in velocity solve if necessary
			return true;
		}

		/**
		 * A helper for solving arrays of constraints
		 */
		class FPBDCollisionSolverHelper
		{
		public:
			static void SolvePositionNoFriction(const TArrayView<FPBDCollisionSolver>& CollisionSolvers, const FSolverReal Dt, const FSolverReal MaxPushOut);
			static void SolvePositionWithFriction(const TArrayView<FPBDCollisionSolver>& CollisionSolvers, const FSolverReal Dt, const FSolverReal MaxPushOut);
			static void SolveVelocity(const TArrayView<FPBDCollisionSolver>& CollisionSolvers, const FSolverReal Dt, const bool bApplyDynamicFriction);

			static void CheckISPC();
		};

	}	// namespace Private
}	// namespace Chaos

