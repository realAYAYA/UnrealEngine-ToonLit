// Copyright Epic Games, Inc. All Rights Reserved.

#include "CharacterGroundConstraintSolver.h"

namespace Chaos
{
	namespace Private
	{
		//////////////////////////////////////////////////////////////////////////
		/// FBodyData

		FCharacterGroundConstraintSolver::FBodyData::FBodyData()
		{
			// Initializes the additional state data
			// These modifiers are not currently being used, so initialize them once to their default values
			CharacterBody.Init();
			GroundBody.Init();
		}

		void FCharacterGroundConstraintSolver::FBodyData::Init(FSolverBody* InCharacterBody, FSolverBody* InGroundBody)
		{
			check(InCharacterBody != nullptr);
			CharacterBody.SetSolverBody(*InCharacterBody);

			if (InGroundBody != nullptr)
			{
				GroundBody.SetSolverBody(*InGroundBody);
			}
		}

		void FCharacterGroundConstraintSolver::FBodyData::Reset()
		{
			CharacterBody.Reset();
			GroundBody.Reset();
		}

		//////////////////////////////////////////////////////////////////////////
		/// FImpulseData

		FCharacterGroundConstraintSolver::FImpulseData::FImpulseData()
		{
			Reset();
		}

		void FCharacterGroundConstraintSolver::FImpulseData::Reset()
		{
			LinearPositionImpulse = FSolverVec3::ZeroVector;
			AngularImpulse = FSolverReal(0.0f);
			LinearCorrectionImpulse = FSolverReal(0.0f);
		}

		//////////////////////////////////////////////////////////////////////////
		/// FConstraintData

		FCharacterGroundConstraintSolver::FConstraintData::FConstraintData()
			: Normal(FSolverVec3(0.0, 0.0, 1.0)) // Need to initialize normal as it is used in GetLinearImpulse
			, VerticalAxis(FSolverVec3(0.0, 0.0, 1.0)) // Need to vertical axis as it is used in GetAngularImpulse
			, CharacterInvM(-1.0f)
			, CharacterInvI(-1.0f)
		{
		}

		bool FCharacterGroundConstraintSolver::FConstraintData::IsValid()
		{
			return (CharacterInvM > 0.0f) && (CharacterInvI > 0.0f);
		}

		//////////////////////////////////////////////////////////////////////////
		/// FCharacterGroundConstraintSolver

		void FCharacterGroundConstraintSolver::SetBodies(FSolverBody* CharacterSolverBody, FSolverBody* GroundSolverBody)
		{
			BodyData.Init(CharacterSolverBody, GroundSolverBody);
		}

		void FCharacterGroundConstraintSolver::Reset()
		{
			BodyData.Reset();
		}

		void FCharacterGroundConstraintSolver::GatherInput(FReal Dt, const FCharacterGroundConstraintSettings& Settings, const FCharacterGroundConstraintDynamicData& Data)
		{
			ImpulseData.Reset();

			ConstraintData.Normal = FSolverVec3(Data.GroundNormal);
			ConstraintData.VerticalAxis = FSolverVec3(Settings.VerticalAxis);
			ConstraintData.AssumedOnGroundHeight = FSolverReal(Settings.AssumedOnGroundHeight);

			// Convert force/torque limits into solver impulse limits
			ConstraintData.AngularImpulseLimit = FSolverReal(Settings.TorqueLimit * Dt * Dt);
			ConstraintData.RadialImpulseLimit = FSolverReal(Settings.RadialForceLimit * Dt * Dt);

			FRotation3 RelQuatCharacter = BodyData.CharacterBody.R().Inverse() * BodyData.CharacterBody.Q();
			FReal CharacterAngularDisplacement = FVec3::DotProduct(RelQuatCharacter.ToRotationVector(), Settings.VerticalAxis);
			FVec3 CharacterLinearDisplacement = BodyData.CharacterBody.P() - BodyData.CharacterBody.X();

			// Vertical ground constraint
			FVec3 PC_Init = BodyData.CharacterBody.X() - Settings.TargetHeight * Settings.VerticalAxis;
			FVec3 PG_Init = BodyData.CharacterBody.X() - Data.GroundDistance * Settings.VerticalAxis;
			FVec3 InitialError = PC_Init - PG_Init;
			FVec3 ProjectedError = InitialError + CharacterLinearDisplacement;

			// Motion Target
			FVec3 MotionTarget = BodyData.CharacterBody.X() + Data.TargetDeltaPosition;

			// If the slope is too steep adjust the target to not point up the slope
			FReal DP = FVec3::DotProduct(Settings.VerticalAxis, Data.GroundNormal);
			if (DP <= Settings.CosMaxWalkableSlopeAngle)
			{
				FVec3 UpSlope = Settings.VerticalAxis - DP * Data.GroundNormal;
				UpSlope.Normalize();
				FReal UpMotion = FVec3::DotProduct(MotionTarget - BodyData.CharacterBody.X(), UpSlope);
				if (UpMotion > FReal(0.0f))
				{
					MotionTarget -= UpMotion * UpSlope;
				}
			}

			FVec3 MotionTargetError = BodyData.CharacterBody.P() - MotionTarget;
			FReal MotionTargetAngularError = CharacterAngularDisplacement - Data.TargetDeltaFacing;

			if (BodyData.GroundBody.IsValid())
			{
				FVec3 RC_Init = PC_Init - BodyData.GroundBody.X();
				FVec3 RG_Init = PG_Init - BodyData.GroundBody.X();

				FRotation3 RelQuatGround = BodyData.GroundBody.R().Inverse() * BodyData.GroundBody.Q();
				FReal GroundAngularDisplacement = FVec3::DotProduct(RelQuatGround.ToRotationVector(), Settings.VerticalAxis);
				FVec3 GroundLinearDisplacement = BodyData.GroundBody.P() - BodyData.GroundBody.X() + RelQuatGround * RG_Init - RG_Init;

				ConstraintData.GroundOffset = RelQuatGround * RC_Init;
				ProjectedError -= GroundLinearDisplacement;

				MotionTargetError -= GroundLinearDisplacement;
				MotionTargetAngularError -= GroundAngularDisplacement;
			}

			bool CurrentlyOverlapping = FVec3::DotProduct(InitialError, Data.GroundNormal) < FReal(0.0);
			FReal ProjectedOverlap = FVec3::DotProduct(ProjectedError, -Data.GroundNormal);
			bool WillOverlap = ProjectedOverlap > FReal(0.0);
			bool NeedsCorrection = CurrentlyOverlapping && WillOverlap;

			if (WillOverlap)
			{
				FReal DampingFactor = FMath::Clamp(Settings.DampingFactor * Dt, 0.0, 1.0);
				FVec3 Diff = DampingFactor * ProjectedOverlap * Data.GroundNormal;
				InitialError += Diff;
				ProjectedError += Diff;
			}

			ConstraintData.InitialError = FSolverVec3(InitialError);
			ConstraintData.InitialProjectedError = FSolverVec3(ProjectedError);
			ConstraintData.MotionTargetError = FSolverVec3(MotionTargetError);
			ConstraintData.MotionTargetAngularError = FSolverReal(MotionTargetAngularError);

			FSolverMatrix33 WorldSpaceCharacterInvI = FSolverMatrix33(Utilities::ComputeWorldSpaceInertia(BodyData.CharacterBody.Q(), BodyData.CharacterBody.InvILocal()));
			ConstraintData.CharacterInvI = FSolverVec3::DotProduct(ConstraintData.VerticalAxis, WorldSpaceCharacterInvI * ConstraintData.VerticalAxis);
			ConstraintData.CharacterInvM = BodyData.CharacterBody.InvM();
			ConstraintData.SingleBodyEffectiveMass = FSolverReal(1.0f / BodyData.CharacterBody.InvM());
			ConstraintData.EffectiveInertia = FSolverReal(1.0f / ConstraintData.CharacterInvI);

			if (BodyData.IsTwoBody())
			{
				ConstraintData.GroundInvM = BodyData.GroundBody.InvM();
				ConstraintData.GroundInvI = FSolverMatrix33(Utilities::ComputeWorldSpaceInertia(BodyData.GroundBody.Q(), BodyData.GroundBody.InvILocal()));
				ConstraintData.TwoBodyEffectiveMass = FSolverReal(1.0f) / FSolverVec3::DotProduct(Data.GroundNormal, Utilities::ComputeJointFactorMatrix(ConstraintData.GroundOffset, ConstraintData.GroundInvI, ConstraintData.GroundInvM + ConstraintData.CharacterInvM) * Data.GroundNormal);
				PositionSolveFunction = &SolvePositionTwoBody;
			}
			else
			{
				PositionSolveFunction = &SolvePositionSingleBody;
			}
			// Note: It would be good to have a two body correction function, but that doesn't work so well
			// if contacts and other constraints don't support correction
			CorrectionSolveFunction = NeedsCorrection ? &SolveCorrectionSingleBody : &NoSolve;
		}

		void FCharacterGroundConstraintSolver::ScatterOutput(const FReal Dt, FVec3& OutSolverAppliedForce, FVec3& OutSolverAppliedTorque)
		{
			if (Dt > UE_SMALL_NUMBER)
			{
				FReal FreqSq = 1.0 / (Dt * Dt);
				OutSolverAppliedForce = FVec3(ImpulseData.LinearPositionImpulse + ImpulseData.LinearCorrectionImpulse * ConstraintData.Normal) * FreqSq;
				OutSolverAppliedTorque = FVec3(ImpulseData.AngularImpulse * ConstraintData.VerticalAxis) * FreqSq;
			}
			else
			{
				OutSolverAppliedForce = FVec3::ZeroVector;
				OutSolverAppliedTorque = FVec3::ZeroVector;
			}
			Reset();

		}

	} // namespace Private
} // namespace Chaos