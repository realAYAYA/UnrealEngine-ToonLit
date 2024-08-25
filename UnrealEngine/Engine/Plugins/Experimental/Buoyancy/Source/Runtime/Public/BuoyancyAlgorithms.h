// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Chaos/Core.h"
#include "Chaos/ParticleHandleFwd.h"
#include "Chaos/AABB.h"

namespace Chaos
{
	class FPBDRigidsEvolutionGBF;
}

namespace BuoyancyAlgorithms
{
	// Minimal struct containing essential data about a particular submersion
	struct FSubmersion
	{
		// Indicates the submerged particle
		Chaos::FPBDRigidParticleHandle* SubmergedParticle;

		// Total submerged volume
		float SubmergedVolume;

		// Effective submerged center of mass
		Chaos::FVec3 SubmergedCoM;
	};

	// Compute the effective volume of an entire particle based on its material
	// density and mass.
	Chaos::FRealSingle ComputeParticleVolume(const Chaos::FPBDRigidsEvolutionGBF& Evolution, const Chaos::FGeometryParticleHandle* Particle);

	// Compute the effective volume of a shape. This method must reflect the
	// maximum possible output value of the non-scaled ComputeSubmergedVolume.
	Chaos::FRealSingle ComputeShapeVolume(const Chaos::FGeometryParticleHandle* Particle);

	//
	void ScaleSubmergedVolume(const Chaos::FPBDRigidsEvolutionGBF& Evolution, const Chaos::FGeometryParticleHandle* Particle, Chaos::FRealSingle& SubmergedVol, Chaos::FRealSingle& TotalVol);

	// Compute an approximate volume and center of mass of particle B submerged in particle A,
	// adjusting for the volume of the object based on the material density and mass of the object
	bool ComputeSubmergedVolume(const Chaos::FPBDRigidsEvolutionGBF& Evolution, const Chaos::FGeometryParticleHandle* ParticleA, const Chaos::FGeometryParticleHandle* ParticleB, int32 NumSubdivisions, float MinVolume, TSparseArray<TBitArray<>>& SubmergedShapes, float& SubmergedVol, Chaos::FVec3& SubmergedCoM, float& TotalVol);

	// Compute an approximate volume and center of mass of particle B submerged in particle A
	bool ComputeSubmergedVolume(const Chaos::FGeometryParticleHandle* ParticleA, const Chaos::FGeometryParticleHandle* ParticleB, int32 NumSubdivisions, float MinVolume, TSparseArray<TBitArray<>>& SubmergedShapes, float& SubmergedVol, Chaos::FVec3& SubmergedCoM);

	// Compute submerged volume given a single waterlevel
	bool ComputeSubmergedVolume(const Chaos::FPBDRigidsEvolutionGBF& Evolution, const Chaos::FGeometryParticleHandle* SubmergedParticle, const Chaos::FGeometryParticleHandle* WaterParticle, const FVector& WaterX, const FVector& WaterN, int32 NumSubdivisions, float MinVolume, TSparseArray<TBitArray<>>& SubmergedShapes, float& SubmergedVol, Chaos::FVec3& SubmergedCoM, float& TotalVol);

	bool ComputeSubmergedVolume(const Chaos::FGeometryParticleHandle* SubmergedParticle, const Chaos::FGeometryParticleHandle* WaterParticle, const FVector& WaterX, const FVector& WaterN, int32 NumSubdivisions, float MinVolume, TSparseArray<TBitArray<>>& SubmergedShapes, float& SubmergedVol, Chaos::FVec3& SubmergedCoM);

	// Given an OOBB and a water level, generate another OOBB which is 1. entirely contained
	// within the input OOBB and 2. entirely contains the portion of the OOBB which is submerged
	// below the water level.
	bool FORCEINLINE ComputeSubmergedBounds(const FVector& SurfacePointLocal, const FVector& SurfaceNormalLocal, const Chaos::FAABB3& RigidBox, Chaos::FAABB3& OutSubmergedBounds);

	// Given a bounds object, recursively subdivide it in eighths to a fixed maximum depth and
	// a fixed minimum smallest subdivision volume.
	bool SubdivideBounds(const Chaos::FAABB3& Bounds, int32 NumSubdivisions, float MinVolume, TArray<Chaos::FAABB3>& OutBounds);

	// Given a rigid particle and it's submerged CoM and Volume, compute delta velocities for
	// integrated buoyancy forces on an object
	bool ComputeBuoyantForce(const Chaos::FPBDRigidParticleHandle* RigidParticle, const float DeltaSeconds, const float WaterDensity, const float WaterDrag, const Chaos::FVec3& GravityAccelVec, const Chaos::FVec3& SubmergedCoM, const float SubmergedVol, const Chaos::FVec3& WaterVel, const Chaos::FVec3& WaterN, Chaos::FVec3& OutDeltaV, Chaos::FVec3& OutDeltaW);
}
