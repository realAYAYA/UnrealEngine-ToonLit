// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

#include "Chaos/Evolution/SolverBody.h"
#include "Chaos/ParticleHandleFwd.h"
#include "Chaos/Character/CharacterGroundConstraintSettings.h"
#include "Chaos/Utilities.h"

namespace Chaos
{
	namespace Private
	{
		//////////////////////////////////////////////////////////////////////////
		/// FCharacterGroundConstraintSolver class
		
		/// Computes the and applies linear and angular displacement for a character ground constraint
		class FCharacterGroundConstraintSolver
		{
		public:
			/// Must call SetBodies and GatherInput before solve
			void SetBodies(FSolverBody* CharacterSolverBody, FSolverBody* GroundSolverBody = nullptr);
			void GatherInput(FReal Dt, const FCharacterGroundConstraintSettings& Settings, const FCharacterGroundConstraintDynamicData& Data);

			/// Solve function performs one iteration of the solver
			void SolvePosition();

			/// Gets the solver output as a force and torque in units ML/T^2 and ML^2/T^2 respectively
			/// and resets the solver
			void ScatterOutput(const FReal Dt, FVec3& OutSolverAppliedForce, FVec3& OutSolverAppliedTorque);

			void Reset();

			/// Gets the solver linear displacement for this constraint and converts to an impulse in units of ML/T
			FVec3 GetLinearImpulse(FReal Dt) const;

			/// Gets the solver angular displacement for this constraint and converts to an impulse in units of ML/T
			FVec3 GetAngularImpulse(FReal Dt) const;


		private:
			/// Utility functions
			static FSolverVec3 ProjectOntoPlane(const FSolverVec3& Vector, const FSolverVec3& PlaneNormal);
			static FSolverVec3 ClampMagnitude(const FSolverVec3& Vector, const FSolverReal& Max);
			static FSolverReal ClampAbs(const FSolverReal& Value, const FSolverReal& Max);

			/// FBodyData
			struct FBodyData
			{
				FBodyData();
				void Init(FSolverBody* InCharacterBody, FSolverBody* InGroundBody);
				bool IsTwoBody();
				void Reset();

				FConstraintSolverBody CharacterBody;
				FConstraintSolverBody GroundBody;
			} BodyData;

			/// FImpulseData
			struct FImpulseData
			{
				FImpulseData();
				void Reset();

				FSolverVec3 LinearPositionImpulse;
				FSolverReal AngularImpulse;
				FSolverReal LinearCorrectionImpulse;
			} ImpulseData;

			/// Constraints
			struct FConstraintData
			{
				FConstraintData();
				bool IsValid();

				FSolverMatrix33 GroundInvI;				/// World space ground body inverse inertia
				FSolverVec3 GroundOffset;				/// Offset vector from ground body CoM to the constraint position
				FSolverVec3 InitialError;				/// Constraint error pre-integration
				FSolverVec3 InitialProjectedError;		/// Projected constraint error post-integration
				FSolverVec3 Normal;						///	Ground plane normal direction
				FSolverVec3 VerticalAxis;				/// World space vertical axis
				FSolverVec3 MotionTargetError;			/// Projected constraint error for motion target constraint
				FSolverReal MotionTargetAngularError;	/// Projected angular facing error for motion target constraint
				FSolverReal CharacterInvM;				/// Character inverse mass
				FSolverReal CharacterInvI;				/// Character inverse inertia about the vertical axis
				FSolverReal GroundInvM;					/// Ground body inverse mass
				FSolverReal TwoBodyEffectiveMass;		/// Effective mass for the two body system
				FSolverReal SingleBodyEffectiveMass;	/// Effective mass for a single body constraint (the character mass)
				FSolverReal EffectiveInertia;			/// Effective angular mass to rotate about the vertical axis
				FSolverReal AngularImpulseLimit;		/// Angular impulse from the solver is clamped to this limit
				FSolverReal RadialImpulseLimit;			/// Linear impulse from the solver is clamped to this limit
				FSolverReal AssumedOnGroundHeight;		/// Height below which the character is assumed to be grounded
			} ConstraintData;

			using FSolveFunctionType = void (*)(const FConstraintData& ConstraintData, FBodyData& BodyData, FImpulseData& ImpulseData);
			FSolveFunctionType PositionSolveFunction;
			FSolveFunctionType CorrectionSolveFunction;

			static void SolveCorrectionSingleBody(const FConstraintData& ConstraintData, FBodyData& BodyData, FImpulseData& ImpulseData);
			static void SolvePositionSingleBody(const FConstraintData& ConstraintData, FBodyData& BodyData, FImpulseData& ImpulseData);
			static void SolvePositionTwoBody(const FConstraintData& ConstraintData, FBodyData& BodyData, FImpulseData& ImpulseData);
			static void NoSolve(const FConstraintData&, FBodyData&, FImpulseData&) {}
		};

		//////////////////////////////////////////////////////////////////////////
		/// FCharacterGroundConstraintSolver inline functions

		FORCEINLINE FVec3 FCharacterGroundConstraintSolver::GetLinearImpulse(FReal Dt) const
		{
			return Dt > UE_SMALL_NUMBER ? FVec3(ImpulseData.LinearPositionImpulse + ImpulseData.LinearCorrectionImpulse * ConstraintData.Normal) / Dt : FVec3::ZeroVector;
		}

		FORCEINLINE FVec3 FCharacterGroundConstraintSolver::GetAngularImpulse(FReal Dt) const
		{
			return Dt > UE_SMALL_NUMBER ? FVec3(ImpulseData.AngularImpulse * ConstraintData.VerticalAxis) / Dt : FVec3::ZeroVector;
		}

		//////////////////////////////////////////////////////////////////////////
		/// Utility functions

		FORCEINLINE FSolverVec3 FCharacterGroundConstraintSolver::ProjectOntoPlane(const FSolverVec3& Vector, const FSolverVec3& PlaneNormal)
		{
			return Vector - FSolverVec3::DotProduct(Vector, PlaneNormal) * PlaneNormal;
		}

		FORCEINLINE FSolverVec3 FCharacterGroundConstraintSolver::ClampMagnitude(const FSolverVec3& Vector, const FSolverReal& Max)
		{
			const FSolverReal MagSq = Vector.SizeSquared();
			const FSolverReal MaxSq = Max * Max;
			if (MagSq > MaxSq)
			{
				if (MaxSq > UE_SMALL_NUMBER)
				{
					return Vector * FMath::InvSqrt(MagSq) * Max;

				}
				else
				{
					return FSolverVec3::ZeroVector;
				}
			}
			else
			{
				return Vector;
			}
		}

		FORCEINLINE FSolverReal FCharacterGroundConstraintSolver::ClampAbs(const FSolverReal& Value, const FSolverReal& Max)
		{
			return Value > Max ? Max : (Value < -Max ? -Max : Value);
		}

		//////////////////////////////////////////////////////////////////////////
		/// FBodyData

		FORCEINLINE bool FCharacterGroundConstraintSolver::FBodyData::IsTwoBody()
		{
			return GroundBody.IsValid() && GroundBody.IsDynamic();
		}

		//////////////////////////////////////////////////////////////////////////
		/// Solve Functions

		FORCEINLINE void FCharacterGroundConstraintSolver::SolvePosition()
		{
			check(ConstraintData.IsValid());

			// Note: Solving these together as part of the same loop for now but
			// may be better to split and solve correction first for the whole
			// system before starting the displacement solver
			(*CorrectionSolveFunction)(ConstraintData, BodyData, ImpulseData);
			(*PositionSolveFunction)(ConstraintData, BodyData, ImpulseData);
		}

		FORCEINLINE void FCharacterGroundConstraintSolver::SolveCorrectionSingleBody(const FConstraintData& ConstraintData, FBodyData& BodyData, FImpulseData& ImpulseData)
		{
			FSolverReal Error = FSolverVec3::DotProduct(BodyData.CharacterBody.CP() + ConstraintData.InitialError, ConstraintData.Normal);
			if (Error < FSolverReal(0.0f))
			{
				FSolverReal Delta = -ConstraintData.SingleBodyEffectiveMass * Error;
				ImpulseData.LinearCorrectionImpulse += Delta;
				BodyData.CharacterBody.ApplyPositionCorrectionDelta(ConstraintData.CharacterInvM * Delta * ConstraintData.Normal);
			}
		}

		FORCEINLINE void FCharacterGroundConstraintSolver::SolvePositionSingleBody(const FConstraintData& ConstraintData, FBodyData& BodyData, FImpulseData& ImpulseData)
		{
			// Normal
			FSolverReal Error = FSolverVec3::DotProduct(BodyData.CharacterBody.DP() + ConstraintData.InitialProjectedError, ConstraintData.Normal);
			if (Error < FSolverReal(0.0f))
			{
				FSolverVec3 Delta = -ConstraintData.SingleBodyEffectiveMass * Error * ConstraintData.Normal;
				ImpulseData.LinearPositionImpulse += Delta;
				BodyData.CharacterBody.ApplyPositionDelta(ConstraintData.CharacterInvM * Delta);
			}

			const FSolverReal NormalImpulse = FSolverVec3::DotProduct(ImpulseData.LinearPositionImpulse, ConstraintData.Normal);
			if (((NormalImpulse + ImpulseData.LinearCorrectionImpulse) > FSolverReal(0.0f)) || Error < ConstraintData.AssumedOnGroundHeight)
			{
				// Target Position
				FSolverVec3 MotionTargetError = ProjectOntoPlane(ConstraintData.MotionTargetError + BodyData.CharacterBody.DP(), ConstraintData.Normal);
				FSolverVec3 InitialMotionTargetImpulse = ProjectOntoPlane(ImpulseData.LinearPositionImpulse, ConstraintData.Normal);
				FSolverVec3 NewMotionTargetImpulse = ClampMagnitude(InitialMotionTargetImpulse - ConstraintData.SingleBodyEffectiveMass * MotionTargetError, ConstraintData.RadialImpulseLimit);
				FSolverVec3 Delta = NewMotionTargetImpulse - InitialMotionTargetImpulse;
				ImpulseData.LinearPositionImpulse += Delta;
				BodyData.CharacterBody.ApplyPositionDelta(ConstraintData.CharacterInvM * Delta);

				// Target Rotation
				FSolverReal MotionTargetAngularError = ConstraintData.MotionTargetAngularError + FSolverVec3::DotProduct(BodyData.CharacterBody.DQ(), ConstraintData.VerticalAxis);
				FSolverReal NewAngularImpulse = ClampAbs(ImpulseData.AngularImpulse - ConstraintData.EffectiveInertia * MotionTargetAngularError, ConstraintData.AngularImpulseLimit);
				FSolverReal AngularDelta = NewAngularImpulse - ImpulseData.AngularImpulse;
				ImpulseData.AngularImpulse += AngularDelta;
				BodyData.CharacterBody.ApplyRotationDelta(ConstraintData.CharacterInvI * AngularDelta * ConstraintData.VerticalAxis);
			}
		}

		FORCEINLINE void FCharacterGroundConstraintSolver::SolvePositionTwoBody(const FConstraintData& ConstraintData, FBodyData& BodyData, FImpulseData& ImpulseData)
		{
			FSolverReal Error = FSolverVec3::DotProduct(ConstraintData.InitialProjectedError + BodyData.CharacterBody.DP() - BodyData.GroundBody.DP()
				- FSolverVec3::CrossProduct(BodyData.GroundBody.DQ(), ConstraintData.GroundOffset), ConstraintData.Normal);
			if (Error < FSolverReal(0.0f))
			{
				FSolverVec3 Delta = -ConstraintData.TwoBodyEffectiveMass * Error * ConstraintData.Normal;
				ImpulseData.LinearPositionImpulse += Delta;
				BodyData.CharacterBody.ApplyPositionDelta(ConstraintData.CharacterInvM * Delta);
				BodyData.GroundBody.ApplyPositionDelta(-ConstraintData.GroundInvM * Delta);
				BodyData.GroundBody.ApplyRotationDelta(-ConstraintData.GroundInvI * FSolverVec3::CrossProduct(ConstraintData.GroundOffset, Delta));
			}

			const FSolverReal NormalImpulse = FSolverVec3::DotProduct(ImpulseData.LinearPositionImpulse, ConstraintData.Normal);
			const FSolverReal MinNormalImpulse(0.0f);
			if (((NormalImpulse + ImpulseData.LinearCorrectionImpulse) > MinNormalImpulse) || Error < FSolverReal(5.0f))
			{
				// Target Position
				FSolverVec3 MotionTargetError = ConstraintData.MotionTargetError + BodyData.CharacterBody.DP() - BodyData.GroundBody.DP();
				MotionTargetError -= FSolverVec3::CrossProduct(BodyData.GroundBody.DQ(), MotionTargetError);
				MotionTargetError = ProjectOntoPlane(MotionTargetError, ConstraintData.Normal);
				FSolverVec3 InitialMotionTargetImpulse = ProjectOntoPlane(ImpulseData.LinearPositionImpulse, ConstraintData.Normal);
				FSolverVec3 NewMotionTargetImpulse = ClampMagnitude(InitialMotionTargetImpulse - ConstraintData.SingleBodyEffectiveMass* MotionTargetError, ConstraintData.RadialImpulseLimit);
				FSolverVec3 Delta = NewMotionTargetImpulse - InitialMotionTargetImpulse;
				ImpulseData.LinearPositionImpulse += Delta;
				BodyData.CharacterBody.ApplyPositionDelta(ConstraintData.CharacterInvM * Delta);

				// Target Rotation
				FSolverReal MotionTargetAngularError = ConstraintData.MotionTargetAngularError + FSolverVec3::DotProduct(BodyData.CharacterBody.DQ() - BodyData.GroundBody.DQ(), ConstraintData.VerticalAxis);
				FSolverReal NewAngularImpulse = ClampAbs(ImpulseData.AngularImpulse - ConstraintData.EffectiveInertia * MotionTargetAngularError, ConstraintData.AngularImpulseLimit);
				FSolverReal AngularDelta = NewAngularImpulse - ImpulseData.AngularImpulse;
				ImpulseData.AngularImpulse += AngularDelta;
				BodyData.CharacterBody.ApplyRotationDelta(ConstraintData.CharacterInvI * AngularDelta * ConstraintData.VerticalAxis);
			}
		}

	} // namespace Private
} // namespace Chaos