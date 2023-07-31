// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "ClothCollisionData.h"
#include "Containers/ContainersFwd.h"
#include "Chaos/PBDSoftsEvolutionFwd.h"

class USkeletalMeshComponent;
class UClothingAssetCommon;
class FClothingSimulationContextCommon;

namespace Chaos
{
	class FImplicitObject;

	class FClothingSimulationSolver;
	class FClothingSimulationCloth;
	class FLevelSet;

	// Collider simulation node
	class FClothingSimulationCollider final
	{
	public:
		enum class ECollisionDataType : int32
		{
			LODless = 0,  // Global LODless collision slot filled with physics collisions
			External,  // External collision slot added/removed at every frame
			LODs,  // LODIndex based start slot for LODs collisions
		};

		FClothingSimulationCollider(
			const UClothingAssetCommon* InAsset,  // Cloth asset for collision data, can be nullptr
			const USkeletalMeshComponent* InSkeletalMeshComponent,  // For asset LODs management, can be nullptr
			bool bInUseLODIndexOverride,
			int32 InLODIndexOverride);
		~FClothingSimulationCollider();

		int32 GetNumGeometries() const { int32 NumGeometries = 0; for (const FLODData& LODDatum : LODData) { NumGeometries += LODDatum.NumGeometries; } return NumGeometries; }

		// Return source (untransformed) collision data for LODless, external and active LODs.
		FClothCollisionData GetCollisionData(const FClothingSimulationSolver* Solver, const FClothingSimulationCloth* Cloth) const;

		// ---- Animatable property setters ----
		// Set external collision data, will only get updated when used as a Solver Collider TODO: Subclass collider?
		void SetCollisionData(const FClothCollisionData* InCollisionData) { CollisionData = InCollisionData; }
		// ---- End of the animatable property setters ----

		// ---- Cloth interface ----
		void Add(FClothingSimulationSolver* Solver, FClothingSimulationCloth* Cloth);
		void Remove(FClothingSimulationSolver* Solver, FClothingSimulationCloth* Cloth);

		void PreUpdate(FClothingSimulationSolver* Solver, FClothingSimulationCloth* Cloth);
		void Update(FClothingSimulationSolver* Solver, FClothingSimulationCloth* Cloth);
		void ResetStartPose(FClothingSimulationSolver* Solver, FClothingSimulationCloth* Cloth);
		// ---- End of the Cloth interface ----

		// ---- Debugging and visualization functions ----
		// Return current active LOD collision particles translations, not thread safe, to use after solver update.
		TConstArrayView<Softs::FSolverVec3> GetCollisionTranslations(const FClothingSimulationSolver* Solver, const FClothingSimulationCloth* Cloth, ECollisionDataType CollisionDataType) const;

		// Return current active LOD collision particles rotations, not thread safe, to use after solver update.
		TConstArrayView<Softs::FSolverRotation3> GetCollisionRotations(const FClothingSimulationSolver* Solver, const FClothingSimulationCloth* Cloth, ECollisionDataType CollisionDataType) const;

		// Return current active LOD previous frame collision particles transforms, not thread safe, to use after solver update.
		TConstArrayView<Softs::FSolverRigidTransform3> GetOldCollisionTransforms(const FClothingSimulationSolver* Solver, const FClothingSimulationCloth* Cloth, ECollisionDataType CollisionDataType) const;

		// Return current active LOD collision geometries, not thread safe, to use after solver update.
		TConstArrayView<TUniquePtr<FImplicitObject>> GetCollisionGeometries(const FClothingSimulationSolver* Solver, const FClothingSimulationCloth* Cloth, ECollisionDataType CollisionDataType) const;

		// Return whether the collision has been hit by a particle during CCD.
		TConstArrayView<bool> GetCollisionStatus(const FClothingSimulationSolver* Solver, const FClothingSimulationCloth* Cloth, ECollisionDataType CollisionDataType) const;
		// ---- End of the debugging and visualization functions ----

	private:

		struct FLevelSetCollisionData
		{
			const TSharedPtr<Chaos::FLevelSet, ESPMode::ThreadSafe> LevelSet;
			FTransform Transform;
			int32 BoneIndex;
		};

		void ExtractPhysicsAssetCollision(FClothCollisionData& ClothCollisionData, TArray<FLevelSetCollisionData>& LevelSetCollisions, TArray<int32>& UsedBoneIndices);

		int32 GetNumGeometries(int32 InSlotIndex) const;

		// Return the collision particle offset for the specified slot being LODLess, external, or any of the LODs collision.
		int32 GetOffset(const FClothingSimulationSolver* Solver, const FClothingSimulationCloth* Cloth, int32 InSlotIndex) const;

		// Return the collision particle offset and number of geometries for the specified type if valid. If ECollisionDataType::LODs is asked, then the offset returned is for the current LOD.
		bool GetOffsetAndNumGeometries(const FClothingSimulationSolver* Solver, const FClothingSimulationCloth* Cloth, ECollisionDataType CollisionDataType, int32& OutOffset, int32& OutNumGeometries) const;

	private:
		typedef TPair<const FClothingSimulationSolver*, const FClothingSimulationCloth*> FSolverClothPair;

		struct FLODData
		{
			FClothCollisionData ClothCollisionData;
			int32 NumGeometries;  // Number of collision bodies
			TMap<FSolverClothPair, int32> Offsets;  // Solver particle offset

			FLODData() : NumGeometries(0) {}

			void Add(
				FClothingSimulationSolver* Solver,
				FClothingSimulationCloth* Cloth,
				const FClothCollisionData& InClothCollisionData,
				const TArray<FLevelSetCollisionData>& InLevelSetCollisionData,
				const FReal InScale = 1.f,
				const TArray<int32>& UsedBoneIndices = TArray<int32>());
			void Remove(FClothingSimulationSolver* Solver, FClothingSimulationCloth* Cloth);

			void Update(FClothingSimulationSolver* Solver, FClothingSimulationCloth* Cloth, const FClothingSimulationContextCommon* Context);

			void Enable(FClothingSimulationSolver* Solver, FClothingSimulationCloth* Cloth, bool bEnable);

			void ResetStartPose(FClothingSimulationSolver* Solver, FClothingSimulationCloth* Cloth);

			FORCEINLINE static int32 GetMappedBoneIndex(const TArray<int32>& UsedBoneIndices, int32 BoneIndex)
			{
				return UsedBoneIndices.IsValidIndex(BoneIndex) ? UsedBoneIndices[BoneIndex] : INDEX_NONE;
			}
		};

		const UClothingAssetCommon* Asset;
		const USkeletalMeshComponent* SkeletalMeshComponent;
		const FClothCollisionData* CollisionData;
		bool bUseLODIndexOverride;
		int32 LODIndexOverride;
		bool bHasExternalCollisionChanged;

		// Collision primitives
		TArray<FLODData> LODData;  // Actual LODs start at LODStart
		TMap<FSolverClothPair, int32> LODIndices;

		// Initial scale
		FReal Scale;
	};
} // namespace Chaos