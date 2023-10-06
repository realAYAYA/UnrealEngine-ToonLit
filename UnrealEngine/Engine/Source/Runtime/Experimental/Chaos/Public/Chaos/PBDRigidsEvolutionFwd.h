// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

namespace Chaos
{
	class FPBDRigidsEvolutionBase;

	class FPBDRigidsEvolutionGBF;

	//The default evolution used by unreal
	using FPBDRigidsEvolution = FPBDRigidsEvolutionGBF;

	class FPBDRigidsSolver;

	using FPBDRigidsSolver = FPBDRigidsSolver;

	class FEventManager;

	using FEventManager = FEventManager;
}

class FGeometryCollectionPhysicsProxy;