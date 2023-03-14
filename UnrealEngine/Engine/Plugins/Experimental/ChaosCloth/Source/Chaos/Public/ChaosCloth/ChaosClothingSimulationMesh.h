// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Core.h"
#include "Chaos/PBDSoftsEvolutionFwd.h"
#include "Containers/ContainersFwd.h"

class USkeletalMeshComponent;
class UClothingAssetCommon;

namespace Chaos
{
	class FClothingSimulationSolver;

	// Mesh simulation node
	class FClothingSimulationMesh final
	{
	public:
		FClothingSimulationMesh(const UClothingAssetCommon* InAsset, const USkeletalMeshComponent* InSkeletalMeshComponent);
		~FClothingSimulationMesh();

		// ---- Node property getters/setters
		const UClothingAssetCommon* GetAsset() const { return Asset; }
		const USkeletalMeshComponent* GetSkeletalMeshComponent() const { return SkeletalMeshComponent; }
		// ---- End of node property getters/setters

		// ---- Cloth interface ----
		void Update(
			FClothingSimulationSolver* Solver,
			int32 PrevLODIndex,
			int32 LODIndex,
			int32 PrevOffset,
			int32 Offset);

		// Return the LOD Index specified by the input SkeletalMeshComponent. Note that this is not the same as the current LOD index, and that Mesh LOD changes are always driven by the Cloth.
		int32 GetLODIndex() const;
		int32 GetNumLODs() const;
		int32 GetNumPoints(int32 LODIndex) const;
		TConstArrayView<uint32> GetIndices(int32 LODIndex) const;
		TArray<TConstArrayView<FRealSingle>> GetWeightMaps(int32 LODIndex) const;
		TArray<TConstArrayView<TTuple<int32, int32, float>>> GetTethers(int32 LODIndex, bool bUseGeodesicTethers) const;
		int32 GetReferenceBoneIndex() const;
		FRigidTransform3 GetReferenceBoneTransform() const;

		// Return this mesh component's scale (the max of the three axis scale values)
		Softs::FSolverReal GetScale() const;

		bool WrapDeformLOD(
			int32 PrevLODIndex,
			int32 LODIndex,
			const Softs::FSolverVec3* Normals,
			const Softs::FSolverVec3* Positions,
			Softs::FSolverVec3* OutPositions) const;

		bool WrapDeformLOD(
			int32 PrevLODIndex,
			int32 LODIndex,
			const Softs::FSolverVec3* Normals,
			const Softs::FPAndInvM* PositionAndInvMs,
			const Softs::FSolverVec3* Velocities,
			Softs::FPAndInvM* OutPositionAndInvMs0,
			Softs::FSolverVec3* OutPositions1,
			Softs::FSolverVec3* OutVelocities) const;
		// ---- End of the Cloth interface ----

	private:
		void SkinPhysicsMesh(
			int32 LODIndex,
			const FVec3& LocalSpaceLocation,
			Softs::FSolverVec3* OutPositions,
			Softs::FSolverVec3* OutNormals) const;

	private:
		const UClothingAssetCommon* Asset;
		const USkeletalMeshComponent* SkeletalMeshComponent;
	};
} // namespace Chaos

#if !defined(CHAOS_SKIN_PHYSICS_MESH_ISPC_ENABLED_DEFAULT)
#define CHAOS_SKIN_PHYSICS_MESH_ISPC_ENABLED_DEFAULT 1
#endif

// Support run-time toggling on supported platforms in non-shipping configurations
#if !INTEL_ISPC || UE_BUILD_SHIPPING
static constexpr bool bChaos_SkinPhysicsMesh_ISPC_Enabled = INTEL_ISPC && CHAOS_SKIN_PHYSICS_MESH_ISPC_ENABLED_DEFAULT;
#else
extern bool bChaos_SkinPhysicsMesh_ISPC_Enabled;
#endif
