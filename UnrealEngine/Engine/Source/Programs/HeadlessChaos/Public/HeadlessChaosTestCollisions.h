// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HeadlessChaosTestUtility.h"
#include "Modules/ModuleManager.h"
#include "Chaos/PBDCollisionConstraints.h"
#include "Chaos/PBDRigidsEvolution.h"
#include "Chaos/PBDRigidParticles.h"
#include "Chaos/Sphere.h"
#include "Chaos/Utilities.h"

namespace ChaosTest {

	void LevelsetConstraint();

	void LevelsetConstraintGJK();
		
	void CollisionBoxPlane();

	void CollisionConvexConvex();

	void CollisionBoxPlaneZeroResitution();

	void CollisionBoxPlaneRestitution();

	void CollisionCubeCubeRestitution();

	void CollisionBoxToStaticBox();
}