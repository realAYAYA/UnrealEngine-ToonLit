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
				FSolverVec3 AngularSwingImpulse;
				FSolverReal AngularImpulse;
				FSolverReal LinearCorrectionImpulse;
			} ImpulseData;

			/// Constraints
			struct FConstraintData
			{
				FConstraintData();
				bool IsValid();

				/// World space ground body inverse inertia
				FSolverMatrix33 CharacterInvI;

				/// World space ground body inverse inertia
				FSolverMatrix33 GroundInvI;

				/// Offset vector from ground body CoM to the constraint position
				FSolverVec3 GroundOffset;

				/// Constraint error pre-integration
				FSolverVec3 InitialError;

				/// Projected constraint error post-integration
				FSolverVec3 InitialProjectedError;

				///	Ground plane normal direction
				FSolverVec3 Normal;

				/// World space vertical axis
				FSolverVec3 VerticalAxis;

				/// Vertical axis rotated by the character initial rotation
				FSolverVec3 CharacterVerticalAxis;

				/// Projected constraint error for motion target constraint
				FSolverVec3 MotionTargetError;

				/// Projected angular facing error for motion target constraint
				FSolverReal MotionTargetAngularError;

				/// Character inverse mass
				FSolverReal CharacterInvM;

				/// Character inverse inertia about the vertical axis
				FSolverReal CharacterInvIZ;

				/// Ground body inverse mass
				FSolverReal GroundInvM;

				/// Effective mass for the two body system
				FSolverReal TwoBodyEffectiveMass;

				/// Effective mass for a single body constraint (the character mass)
				FSolverReal SingleBodyEffectiveMass;

				/// Effective angular mass to rotate about the vertical axis
				FSolverReal EffectiveInertia;

				/// Angular impulse from the solver is clamped to this limit
				FSolverReal AngularImpulseLimit;

				/// Angular swing impulse from the solver is clamped to this limit
				FSolverReal AngularSwingImpulseLimit;

				/// Radial linear impulse from the solver is clamped to this limit
				FSolverReal RadialImpulseLimit;

				/// Height below which the character is assumed to be grounded
				FSolverReal AssumedOnGroundHeight;
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
			const FSolverReal Error = ConstraintData.Normal.Dot(BodyData.CharacterBody.CP() + ConstraintData.InitialError);
			if (Error < FSolverReal(0.0f))
			{
				const FSolverReal Delta = -ConstraintData.SingleBodyEffectiveMass * Error;
				ImpulseData.LinearCorrectionImpulse += Delta;
				BodyData.CharacterBody.ApplyPositionCorrectionDelta(FSolverVec3(ConstraintData.CharacterInvM * Delta * ConstraintData.Normal));
			}
		}

		FORCEINLINE void FCharacterGroundConstraintSolver::SolvePositionSingleBody(const FConstraintData& ConstraintData, FBodyData& BodyData, FImpulseData& ImpulseData)
		{
			// Normal
			const FSolverReal Error = ConstraintData.Normal.Dot(BodyData.CharacterBody.DP() + ConstraintData.InitialProjectedError);
			if (Error < FSolverReal(0.0f))
			{
				const FSolverVec3 Delta = -ConstraintData.SingleBodyEffectiveMass * Error * ConstraintData.Normal;
				ImpulseData.LinearPositionImpulse += Delta;
				BodyData.CharacterBody.ApplyPositionDelta(FSolverVec3(ConstraintData.CharacterInvM * Delta));
			}

			// Angular constraint
			FSolverVec3 NewCharacterVerticalAxis = ConstraintData.CharacterVerticalAxis + BodyData.CharacterBody.DQ().Cross(ConstraintData.CharacterVerticalAxis);
			NewCharacterVerticalAxis.Normalize();
			const FSolverVec3 CrossProd = NewCharacterVerticalAxis.Cross(ConstraintData.VerticalAxis);
			const FSolverReal SizeSq = CrossProd.SizeSquared();
			if (SizeSq > UE_SMALL_NUMBER)
			{
				const FSolverVec3 AngAxis = CrossProd * FMath::InvSqrt(SizeSq);
				const FSolverReal AngResistance = FSolverReal(1.0f) / (ConstraintData.CharacterInvI * AngAxis).Dot(AngAxis);
				const FSolverVec3 NewSwingImpulse = ClampMagnitude(ImpulseData.AngularSwingImpulse + AngResistance * FMath::Asin(FMath::Sqrt(SizeSq)) * AngAxis, ConstraintData.AngularSwingImpulseLimit);
				const FSolverVec3 Delta = NewSwingImpulse - ImpulseData.AngularSwingImpulse;
				ImpulseData.AngularSwingImpulse = NewSwingImpulse;
				BodyData.CharacterBody.ApplyRotationDelta(FSolverVec3(ConstraintData.CharacterInvI * Delta));
			}

			const FSolverReal NormalImpulse = FSolverVec3::DotProduct(ImpulseData.LinearPositionImpulse, ConstraintData.Normal);
			const FSolverReal MinNormalImpulse(0.0f);
			if (((NormalImpulse + ImpulseData.LinearCorrectionImpulse) > MinNormalImpulse) || Error < ConstraintData.AssumedOnGroundHeight)
			{
				// Target Position
				const FSolverVec3 MotionTargetError = ProjectOntoPlane(ConstraintData.MotionTargetError + BodyData.CharacterBody.DP(), ConstraintData.Normal);
				const FSolverVec3 InitialMotionTargetImpulse = ProjectOntoPlane(ImpulseData.LinearPositionImpulse, ConstraintData.Normal);
				FSolverVec3 NewMotionTargetImpulse = ClampMagnitude(InitialMotionTargetImpulse - ConstraintData.SingleBodyEffectiveMass * MotionTargetError, ConstraintData.RadialImpulseLimit);
				const FSolverVec3 Delta = NewMotionTargetImpulse - InitialMotionTargetImpulse;
				ImpulseData.LinearPositionImpulse += Delta;
				BodyData.CharacterBody.ApplyPositionDelta(FSolverVec3(ConstraintData.CharacterInvM * Delta));

				// Target Rotation
				const FSolverReal MotionTargetAngularError = ConstraintData.MotionTargetAngularError + ConstraintData.VerticalAxis.Dot(BodyData.CharacterBody.DQ());
				const FSolverReal NewAngularImpulse = ClampAbs(ImpulseData.AngularImpulse - ConstraintData.EffectiveInertia * MotionTargetAngularError, ConstraintData.AngularImpulseLimit);
				const FSolverReal AngularDelta = NewAngularImpulse - ImpulseData.AngularImpulse;
				ImpulseData.AngularImpulse += AngularDelta;
				BodyData.CharacterBody.ApplyRotationDelta(FSolverVec3(ConstraintData.CharacterInvI * AngularDelta * ConstraintData.VerticalAxis));
			}
		}

		FORCEINLINE void FCharacterGroundConstraintSolver::SolvePositionTwoBody(const FConstraintData& ConstraintData, FBodyData& BodyData, FImpulseData& ImpulseData)
		{
			const FSolverReal Error = ConstraintData.Normal.Dot(ConstraintData.InitialProjectedError + BodyData.CharacterBody.DP()
				- BodyData.GroundBody.DP() - BodyData.GroundBody.DQ().Cross(ConstraintData.GroundOffset));
			if (Error < FSolverReal(0.0f))
			{
				const FSolverVec3 Delta = -ConstraintData.TwoBodyEffectiveMass * Error * ConstraintData.Normal;
				ImpulseData.LinearPositionImpulse += Delta;
				BodyData.CharacterBody.ApplyPositionDelta(FSolverVec3(ConstraintData.CharacterInvM * Delta));
				BodyData.GroundBody.ApplyPositionDelta(FSolverVec3(-ConstraintData.GroundInvM * Delta));
				BodyData.GroundBody.ApplyRotationDelta(FSolverVec3(-ConstraintData.GroundInvI * ConstraintData.GroundOffset.Cross(Delta)));
			}

			// Angular constraint
			FSolverVec3 NewCharacterVerticalAxis = ConstraintData.CharacterVerticalAxis + BodyData.CharacterBody.DQ().Cross(ConstraintData.CharacterVerticalAxis);
			NewCharacterVerticalAxis.Normalize();
			const FSolverVec3 CrossProd = NewCharacterVerticalAxis.Cross(ConstraintData.VerticalAxis);
			const FSolverReal SizeSq = CrossProd.SizeSquared();
			if (SizeSq > UE_SMALL_NUMBER)
			{
				const FSolverVec3 AngAxis = CrossProd * FMath::InvSqrt(SizeSq);
				const FSolverReal AngResistance = FSolverReal(1.0f) / (ConstraintData.CharacterInvI * AngAxis).Dot(AngAxis);
				const FSolverVec3 NewSwingImpulse = ClampMagnitude(ImpulseData.AngularSwingImpulse + AngResistance * FMath::Asin(FMath::Sqrt(SizeSq)) * AngAxis, ConstraintData.AngularSwingImpulseLimit);
				const FSolverVec3 Delta = NewSwingImpulse - ImpulseData.AngularSwingImpulse;
				ImpulseData.AngularSwingImpulse = NewSwingImpulse;
				BodyData.CharacterBody.ApplyRotationDelta(FSolverVec3(ConstraintData.CharacterInvI * Delta));
			}

			const FSolverReal NormalImpulse = FSolverVec3::DotProduct(ImpulseData.LinearPositionImpulse, ConstraintData.Normal);
			const FSolverReal MinNormalImpulse(0.0f);
			if (((NormalImpulse + ImpulseData.LinearCorrectionImpulse) > MinNormalImpulse) || Error < ConstraintData.AssumedOnGroundHeight)
			{
				// Target Position
				FSolverVec3 MotionTargetError = ConstraintData.MotionTargetError + BodyData.CharacterBody.DP() - BodyData.GroundBody.DP();
				MotionTargetError -= BodyData.GroundBody.DQ().Cross(MotionTargetError);
				MotionTargetError = ProjectOntoPlane(MotionTargetError, ConstraintData.Normal);
				const FSolverVec3 InitialMotionTargetImpulse = ProjectOntoPlane(ImpulseData.LinearPositionImpulse, ConstraintData.Normal);
				FSolverVec3 NewMotionTargetImpulse = ClampMagnitude(InitialMotionTargetImpulse - ConstraintData.SingleBodyEffectiveMass * MotionTargetError, ConstraintData.RadialImpulseLimit);
				const FSolverVec3 Delta = NewMotionTargetImpulse - InitialMotionTargetImpulse;
				ImpulseData.LinearPositionImpulse += Delta;
				BodyData.CharacterBody.ApplyPositionDelta(FSolverVec3(ConstraintData.CharacterInvM * Delta));

				// Target Rotation
				const FSolverReal MotionTargetAngularError = ConstraintData.MotionTargetAngularError + ConstraintData.VerticalAxis.Dot(BodyData.CharacterBody.DQ() - BodyData.GroundBody.DQ());
				const FSolverReal NewAngularImpulse = ClampAbs(ImpulseData.AngularImpulse - ConstraintData.EffectiveInertia * MotionTargetAngularError, ConstraintData.AngularImpulseLimit);
				const FSolverReal AngularDelta = NewAngularImpulse - ImpulseData.AngularImpulse;
				ImpulseData.AngularImpulse += AngularDelta;
				BodyData.CharacterBody.ApplyRotationDelta(FSolverVec3(ConstraintData.CharacterInvI * AngularDelta * ConstraintData.VerticalAxis));
			}
		}

	} // namespace Private
} // namespace Chaos