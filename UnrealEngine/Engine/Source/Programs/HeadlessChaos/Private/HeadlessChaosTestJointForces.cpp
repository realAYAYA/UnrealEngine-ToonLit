// Copyright Epic Games, Inc. All Rights Reserved.

#include "HeadlessChaos.h"
#include "HeadlessChaosTestJoint.h"
#include "Modules/ModuleManager.h"
#include "Chaos/ParticleHandle.h"

namespace ChaosTest {

	using namespace Chaos;

	// 1 Kinematic Body with 4 Dynamic bodies hanging from it by a breakable constraint.
	// Verify that the force in each joint settles to about (Sum of Child Mass)xG
	template <typename TEvolution>
	void JointForces_Linear()
	{
		const int32 NumSolverIterations = 10;
		const FReal Gravity = 980;
		const FReal Dt = 0.01f;
		const int32 NumSteps = 100;
		const int32 NumBodies = 5;

		FJointChainTest<TEvolution> Test(NumSolverIterations, Gravity);
		Test.InitChain(NumBodies, FVec3(0, 0, -1));

		Test.Create();

		// Run the sim
		for (int32 i = 0; i < NumSteps; ++i)
		{
			Test.Evolution.AdvanceOneTimeStep(Dt);
			Test.Evolution.EndFrame(Dt);
		}

		for (int32 JointIndex = 0; JointIndex < Test.Evolution.GetJointConstraints().NumConstraints(); ++JointIndex)
		{
			FReal ChildMass = 0.0f;
			for (int32 ChildBodyIndex = JointIndex + 1; ChildBodyIndex < Test.ParticleMasses.Num(); ++ChildBodyIndex)
			{
				ChildMass += Test.ParticleMasses[ChildBodyIndex];
			}

			FVec3 ExpectedLinearImpulse = FVec3(0.0f, 0.0f, -ChildMass * Gravity * Dt);
			FVec3 LinearImpulse = Test.Evolution.GetJointConstraints().GetConstraintLinearImpulse(JointIndex);
			EXPECT_NEAR(LinearImpulse.X, ExpectedLinearImpulse.X, ExpectedLinearImpulse.Size() / 100.0f);
			EXPECT_NEAR(LinearImpulse.Y, ExpectedLinearImpulse.Y, ExpectedLinearImpulse.Size() / 100.0f);
			EXPECT_NEAR(LinearImpulse.Z, ExpectedLinearImpulse.Z, ExpectedLinearImpulse.Size() / 100.0f);
		}
	}

	GTEST_TEST(AllEvolutions, JointForcesTests_TestLinear)
	{
		JointForces_Linear<FPBDRigidsEvolutionGBF>();
	}

	// 1 Kinematic Body with 1 Dynamic bodies arranged horizontally.
	// Verify that the force in the joint settles to about MxG
	template <typename TEvolution>
	void JointForces_Linear2()
	{
		const int32 NumSolverIterations = 40;
		const FReal Gravity = 980;
		const FReal Dt = 0.01f;
		const int32 NumSteps = 100;
		const int32 NumBodies = 3;

		FJointChainTest<TEvolution> Test(NumSolverIterations, Gravity);
		Test.InitChain(NumBodies, FVec3(1, 0, 0));

		// Set all joints to fixed angular
		for (int32 JointIndex = 0; JointIndex < Test.JointSettings.Num(); ++JointIndex)
		{
			Test.JointSettings[JointIndex].AngularMotionTypes = { EJointMotionType::Locked, EJointMotionType::Locked, EJointMotionType::Locked };
			Test.JointSettings[JointIndex].bProjectionEnabled = false;
		}

		Test.Create();

		// Run the sim
		for (int32 i = 0; i < NumSteps; ++i)
		{
			Test.Evolution.AdvanceOneTimeStep(Dt);
			Test.Evolution.EndFrame(Dt);
		}

		FReal L = Test.ParticlePositions[1].X - Test.ParticlePositions[0].X;

		for (int32 JointIndex = 0; JointIndex < Test.Evolution.GetJointConstraints().NumConstraints(); ++JointIndex)
		{
			FReal ChildMass = 0.0f;
			FReal ChildMoment = 0.0f;
			for (int32 ChildBodyIndex = JointIndex + 1; ChildBodyIndex < Test.ParticleMasses.Num(); ++ChildBodyIndex)
			{
				ChildMass += Test.ParticleMasses[ChildBodyIndex];
				FReal ChildL = (ChildBodyIndex - JointIndex) * L;
				ChildMoment += Test.ParticleMasses[ChildBodyIndex] * ChildL;
			}

			FVec3 ExpectedLinearImpulse = FVec3(0.0f, 0.0f, -ChildMass * Gravity * Dt);
			FVec3 LinearImpulse = Test.Evolution.GetJointConstraints().GetConstraintLinearImpulse(JointIndex);
			EXPECT_NEAR(LinearImpulse.X, ExpectedLinearImpulse.X, ExpectedLinearImpulse.Size() / 100.0f);
			EXPECT_NEAR(LinearImpulse.Y, ExpectedLinearImpulse.Y, ExpectedLinearImpulse.Size() / 100.0f);
			EXPECT_NEAR(LinearImpulse.Z, ExpectedLinearImpulse.Z, ExpectedLinearImpulse.Size() / 100.0f);

			FVec3 ExpectedAngularImpulse = FVec3(0.0f, ChildMoment * Gravity * Dt, 0.0f);
			FVec3 AngularImpulse = Test.Evolution.GetJointConstraints().GetConstraintAngularImpulse(JointIndex);
			EXPECT_NEAR(AngularImpulse.X, ExpectedAngularImpulse.X, ExpectedAngularImpulse.Size() / 100.0f);
			EXPECT_NEAR(AngularImpulse.Y, ExpectedAngularImpulse.Y, ExpectedAngularImpulse.Size() / 100.0f);
			EXPECT_NEAR(AngularImpulse.Z, ExpectedAngularImpulse.Z, ExpectedAngularImpulse.Size() / 100.0f);
		}
	}

	GTEST_TEST(AllEvolutions, JointForcesTests_TestLinear2)
	{
		JointForces_Linear2<FPBDRigidsEvolutionGBF>();
	}


	// 1 Kinematic Body with 1 Dynamic bodies arranged vertically.
	// Apply a torque to the body and verify that the joint torque is the same.
	template <typename TEvolution>
	void JointForces_Angular()
	{
		const int32 NumSolverIterations = 20;
		const FReal Gravity = 0;
		const FReal Dt = 0.01f;
		const int32 NumSteps = 100;
		const int32 NumBodies = 2;
		const FVec3 Torque = FVec3(10000, 0, 0);

		FJointChainTest<TEvolution> Test(NumSolverIterations, Gravity);
		Test.InitChain(NumBodies, FVec3(1, 0, 0));

		// Set all joints to fixed angular and disable projection
		for (int32 JointIndex = 0; JointIndex < Test.JointSettings.Num(); ++JointIndex)
		{
			Test.JointSettings[JointIndex].AngularMotionTypes = { EJointMotionType::Locked, EJointMotionType::Locked, EJointMotionType::Locked };
			Test.JointSettings[JointIndex].bProjectionEnabled = false;
		}

		Test.Create();

		// Run the sim
		for (int32 i = 0; i < NumSteps; ++i)
		{
			Test.GetParticle(1)->CastToRigidParticle()->SetTorque(Torque);
			
			Test.Evolution.AdvanceOneTimeStep(Dt);
			Test.Evolution.EndFrame(Dt);
		}

		for (int32 JointIndex = 0; JointIndex < Test.Evolution.GetJointConstraints().NumConstraints(); ++JointIndex)
		{
			FReal ChildMass = 0.0f;
			for (int32 ChildBodyIndex = JointIndex + 1; ChildBodyIndex < Test.ParticleMasses.Num(); ++ChildBodyIndex)
			{
				ChildMass += Test.ParticleMasses[ChildBodyIndex];
			}

			FVec3 ExpectedAngularImpulse = Torque * Dt;
			FVec3 AngularImpulse = Test.Evolution.GetJointConstraints().GetConstraintAngularImpulse(JointIndex);
			EXPECT_NEAR(AngularImpulse.X, ExpectedAngularImpulse.X, ExpectedAngularImpulse.Size() / 100.0f);
			EXPECT_NEAR(AngularImpulse.Y, ExpectedAngularImpulse.Y, ExpectedAngularImpulse.Size() / 100.0f);
			EXPECT_NEAR(AngularImpulse.Z, ExpectedAngularImpulse.Z, ExpectedAngularImpulse.Size() / 100.0f);
		}
	}

	GTEST_TEST(AllEvolutions, JointForcesTests_Angular)
	{
		JointForces_Angular<FPBDRigidsEvolutionGBF>();
	}
}