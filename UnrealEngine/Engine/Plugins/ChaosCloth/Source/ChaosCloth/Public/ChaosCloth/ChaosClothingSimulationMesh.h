// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Core.h"
#include "Chaos/PBDSoftsEvolutionFwd.h"
#include "Containers/ContainersFwd.h"
#include "ClothVertBoneData.h"

//#define CHAOS_IS_CLOTHINGSIMULATIONMESH_ABSTRACT 1  // For testing only. TODO: Once FClothingSimulationMesh is abstract from 5.4, remove all lines where this is referenced

struct FMeshToMeshVertData;
struct FClothVertBoneData;

#if !defined(CHAOS_IS_CLOTHINGSIMULATIONMESH_ABSTRACT) || !CHAOS_IS_CLOTHINGSIMULATIONMESH_ABSTRACT
class USkeletalMeshComponent;
class UClothingAssetCommon;
class FClothingSimulationContextCommon;
#endif

namespace Chaos
{
	class FClothingSimulationSolver;

	// Mesh simulation node
#if !defined(CHAOS_IS_CLOTHINGSIMULATIONMESH_ABSTRACT) || !CHAOS_IS_CLOTHINGSIMULATIONMESH_ABSTRACT
	class UE_DEPRECATED(5.2, "This class is becoming an abstract interface, do not use directly. Use FClothingSimulationSkeletalMesh instead.")
		CHAOSCLOTH_API FClothingSimulationMesh
#else
	class CHAOSCLOTH_API FClothingSimulationMesh
#endif
	{
	public:
		explicit FClothingSimulationMesh(const FString& InDebugName);

		virtual ~FClothingSimulationMesh();

		FClothingSimulationMesh(const FClothingSimulationMesh&) = delete;
		FClothingSimulationMesh(FClothingSimulationMesh&&) = delete;
		FClothingSimulationMesh& operator=(const FClothingSimulationMesh&) = delete;
		FClothingSimulationMesh& operator=(FClothingSimulationMesh&&) = delete;

#if !UE_BUILD_SHIPPING
		const FString& GetDebugName() const { return DebugName; }
#else
		const FString& GetDebugName() const { return FText::GetEmpty().ToString(); }
#endif

#if !defined(CHAOS_IS_CLOTHINGSIMULATIONMESH_ABSTRACT) || !CHAOS_IS_CLOTHINGSIMULATIONMESH_ABSTRACT
		UE_DEPRECATED(5.2, "Use the FClothingSimulationSkeletalMesh constructor instead.")
		FClothingSimulationMesh(const UClothingAssetCommon* InAsset, const USkeletalMeshComponent* InSkeletalMeshComponent);
		UE_DEPRECATED(5.2, "Use the FClothingSimulationSkeletalMesh::GetAsset() instead.")
		virtual const UClothingAssetCommon* GetAsset() const { return Asset; }
		UE_DEPRECATED(5.2, "Use the FClothingSimulationSkeletalMesh::GetSkeletalMeshComponent() instead.")
		virtual const USkeletalMeshComponent* GetSkeletalMeshComponent() const { return SkeletalMeshComponent; }

		virtual int32 GetNumLODs() const;
		virtual int32 GetLODIndex() const;
		virtual int32 GetOwnerLODIndex(int32 LODIndex) const;
		virtual bool IsValidLODIndex(int32 LODIndex) const;
		virtual int32 GetNumPoints(int32 LODIndex) const;
		virtual int32 GetNumPatternPoints(int32 LODIndex) const;
		virtual TConstArrayView<FVector3f> GetPositions(int32 LODIndex) const;
		virtual TConstArrayView<FVector2f> GetPatternPositions(int32 LODIndex) const;
		virtual TConstArrayView<FVector3f> GetNormals(int32 LODIndex) const;
		virtual TConstArrayView<uint32> GetIndices(int32 LODIndex) const;
		virtual TConstArrayView<uint32> GetPatternIndices(int32 LODIndex) const;
		virtual TConstArrayView<uint32> GetPatternToWeldedIndices(int32 LODIndex) const;
		virtual TArray<FName> GetWeightMapNames(int32 LODIndex) const;
		UE_DEPRECATED(5.3, "Use LODIndex version.")
		virtual TArray<FName> GetWeightMapNames() const { return GetWeightMapNames(0); }
		virtual TMap<FString, int32> GetWeightMapIndices(int32 LODIndex) const;
		UE_DEPRECATED(5.3, "Use LODIndex version.")
		virtual TMap<FString, int32> GetWeightMapIndices() const { return GetWeightMapIndices(0); }
		virtual TArray<TConstArrayView<FRealSingle>> GetWeightMaps(int32 LODIndex) const;
		virtual TMap<FString, const TSet<int32>*> GetVertexSets(int32 LODIndex) const;
		virtual TMap<FString, const TSet<int32>*> GetFaceSets(int32 LODIndex) const;
		virtual TMap<FString, TConstArrayView<int32>> GetFaceIntMaps(int32 LODIndex) const;
		virtual TArray<TConstArrayView<TTuple<int32, int32, float>>> GetTethers(int32 LODIndex, bool bUseGeodesicTethers) const;
		virtual int32 GetReferenceBoneIndex() const;
		virtual FTransform GetReferenceBoneTransform() const;
		virtual const TArray<FTransform>& GetBoneTransforms() const;
		virtual const FTransform& GetComponentToWorldTransform() const;
		virtual const TArray<FMatrix44f>& GetRefToLocalMatrices() const;
		virtual TConstArrayView<int32> GetBoneMap() const;
		virtual TConstArrayView<FClothVertBoneData> GetBoneData(int32 LODIndex) const;
		virtual TConstArrayView<FMeshToMeshVertData> GetTransitionUpSkinData(int32 LODIndex) const;
		virtual TConstArrayView<FMeshToMeshVertData> GetTransitionDownSkinData(int32 LODIndex) const;
#else
		/* Return the number of LODs on this mesh. */
		virtual int32 GetNumLODs() const = 0;

		/* Return the cloth mesh LOD Index. The returned value can then be used to switch LODs on the simulation object. */
		virtual int32 GetLODIndex() const = 0;

		/* Return the owner component LOD Index from the specified cloth mesh LOD index, or 0 if the owner LOD cannot be determined. The mapping between the cloth mesh LOD and the owner component LOD is not necessarily one to one. */
		virtual int32 GetOwnerLODIndex(int32 LODIndex) const = 0;

		/* Return whether the specified LOD index is valid. */
		virtual bool IsValidLODIndex(int32 LODIndex) const = 0;

		/* Return the number of points for the specified LOD, or 0 if the LOD is empty or invalid. */
		virtual int32 GetNumPoints(int32 LODIndex) const = 0;

		/* Return the number of pattern points (2d, unwelded) for the specified LOD, or 0 if patterns are not supported or the LOD is empty or invalid. */
		virtual int32 GetNumPatternPoints(int32 LODIndex) const = 0;

		/* Return the source mesh positions (pre-skinning). */
		virtual TConstArrayView<FVector3f> GetPositions(int32 LODIndex) const = 0;

		/* Return the source mesh 2d pattern positions. */
		virtual TConstArrayView<FVector2f> GetPatternPositions(int32 LODIndex) const = 0;

		/* Return the source mesh normals (pre-skinning). */
		virtual TConstArrayView<FVector3f> GetNormals(int32 LODIndex) const = 0;

		/* Return the specified LOD's triangle indices for this mesh. */
		virtual TConstArrayView<uint32> GetIndices(int32 LODIndex) const = 0;

		/* Return the specified LOD's pattern (unwelded) triangle indices for this mesh, or empty array if patterns are not supported. */
		virtual TConstArrayView<uint32> GetPatternIndices(int32 LODIndex) const = 0;

		/* Return the specified LOD's map from pattern (unwelded) vertices to (welded) vertices, or empty array if patterns are not supported. */
		virtual TConstArrayView<uint32> GetPatternToWeldedIndices(int32 LODIndex) const = 0;

		/* Return all weight maps associated with this mesh returned in the same order as GetWeightMaps. */
		virtual TArray<FName> GetWeightMapNames(int32 LODIndex) const = 0;
		UE_DEPRECATED(5.3, "Use LODIndex version.")
		virtual TArray<FName> GetWeightMapNames() const = 0;

		/* Return a map of all weight map names associated with this mesh to the index in the array returned by GetWeightMaps. */
		virtual TMap<FString, int32> GetWeightMapIndices(int32 LODIndex) const = 0;
		UE_DEPRECATED(5.3, "Use LODIndex version.")
		virtual TMap<FString, int32> GetWeightMapIndices() const = 0;

		/* Return the specified LOD's weight map. */
		virtual TArray<TConstArrayView<FRealSingle>> GetWeightMaps(int32 LODIndex) const = 0;

		/* Return the specified LOD's vertex sets. */
		virtual const TMap<FString, TSet<int32>*> GetVertexSets(int32 LODIndex) const = 0;

		/* Return the specified LOD's face sets. */
		virtual const TMap<FString, TSet<int32>*> GetFaceSets(int32 LODIndex) const = 0;

		/* Return the specified LOD's face int maps. */
		virtual TMap<FString, TConstArrayView<int32>> GetFaceIntMaps(int32 LODIndex) const = 0;

		/* Return the tethers connections for the long range attachment into convenient parallel friendly batches. */
		virtual TArray<TConstArrayView<TTuple<int32, int32, float>>> GetTethers(int32 LODIndex, bool bUseGeodesicTethers) const = 0;

		/* Return the bone to treat as the root of the simulation space. */
		virtual int32 GetReferenceBoneIndex() const = 0;

		/* Return the transform of the bone treated as the root of the simulation space. */
		virtual FTransform GetReferenceBoneTransform() const = 0;

		/* Return the bone transforms as required when updating the collider pose. */
		const TArray<FTransform>& GetBoneTransforms() const = 0;

		/* Return the transform of the bone treated as the root of the simulation space. */
		virtual const FTransform& GetComponentToWorldTransform() const = 0;

		/* Return the skinning matrices. */
		virtual const TArray<FMatrix44f>& GetRefToLocalMatrices() const = 0;

		/* Return the bone map used to remap the used bones index into the correct skinning matrix index. */
		virtual TConstArrayView<int32> GetBoneMap() const = 0;

		/* Return the bone data containing bone weights and influences. */
		virtual TConstArrayView<FClothVertBoneData> GetBoneData(int32 LODIndex) const = 0;

		/* Return the transition up data (PrevLODIndex < LODIndex), for matching shapes during LOD changes. */
		virtual TConstArrayView<FMeshToMeshVertData> GetTransitionUpSkinData(int32 LODIndex) const = 0;

		/* Return the transition down data (PrevLODIndex > LODIndex), for matching shapes during LOD changes. */
		virtual TConstArrayView<FMeshToMeshVertData> GetTransitionDownSkinData(int32 LODIndex) const = 0;
#endif
		/** Return this mesh uniform scale as the maximum of the three axis scale value. */
		virtual Softs::FSolverReal GetScale() const;

		/** Deform the specified positions to match the shape of the previous LOD. */
		bool WrapDeformLOD(
			int32 PrevLODIndex,
			int32 LODIndex,
			const Softs::FSolverVec3* Normals,
			const Softs::FSolverVec3* Positions,
			Softs::FSolverVec3* OutPositions) const;

		/** Deform the specified positions and transfer velocities to match the dynamics of the previous LOD. */
		bool WrapDeformLOD(
			int32 PrevLODIndex,
			int32 LODIndex,
			const Softs::FSolverVec3* Normals,
			const Softs::FPAndInvM* PositionAndInvMs,
			const Softs::FSolverVec3* Velocities,
			Softs::FPAndInvM* OutPositionAndInvMs0,
			Softs::FSolverVec3* OutPositions1,
			Softs::FSolverVec3* OutVelocities) const;

		/* Update the mesh for the next solver step, doing skinning and matching the shapes during LOD changes. */
		void Update(
			FClothingSimulationSolver* Solver,
			int32 PrevLODIndex,
			int32 LODIndex,
			int32 PrevOffset,
			int32 Offset);
		// ---- End of the Cloth interface ----

	private:
		void SkinPhysicsMesh(
			int32 LODIndex,
			const FVec3& LocalSpaceLocation,
			Softs::FSolverVec3* OutPositions,
			Softs::FSolverVec3* OutNormals) const;

#if !defined(CHAOS_IS_CLOTHINGSIMULATIONMESH_ABSTRACT) || !CHAOS_IS_CLOTHINGSIMULATIONMESH_ABSTRACT
		const FClothingSimulationContextCommon* GetContext() const;

		const UClothingAssetCommon* Asset;
		const USkeletalMeshComponent* SkeletalMeshComponent;
#endif

#if !UE_BUILD_SHIPPING
		/** Debug name of the source component. */
		FString DebugName;
#endif
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
