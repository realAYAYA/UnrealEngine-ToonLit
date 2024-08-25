// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Transform.h"
#include "Chaos/TriangleMesh.h"
#include "Chaos/AABB.h"
#include "Containers/ContainersFwd.h"
#include "ChaosCloth/ChaosClothConstraints.h"

struct FManagedArrayCollection;
struct FClothingSimulationCacheData;

namespace Chaos
{
	class FClothingSimulationSolver;
	class FClothingSimulationMesh;
	class FClothingSimulationCollider;
	class FClothingSimulationConfig;

	// Cloth simulation node
	class FClothingSimulationCloth final
	{
	public:
		enum EMassMode
		{
			UniformMass,
			TotalMass,
			Density
		};

		UE_DEPRECATED(5.3, "ETetherMode has been replaced with bUseGeodesicTethers.")
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		typedef FClothConstraints::ETetherMode ETetherMode;
CHAOSCLOTH_API PRAGMA_ENABLE_DEPRECATION_WARNINGS

		FClothingSimulationCloth(
			FClothingSimulationConfig* InConfig,
PRAGMA_DISABLE_DEPRECATION_WARNINGS  // TODO: CHAOS_IS_CLOTHINGSIMULATIONMESH_ABSTRACT
			FClothingSimulationMesh* InMesh,
PRAGMA_ENABLE_DEPRECATION_WARNINGS
			TArray<FClothingSimulationCollider*>&& InColliders,
			uint32 InGroupId);

		UE_DEPRECATED(5.2, "Use config based constructor instead.")
		FClothingSimulationCloth(
PRAGMA_DISABLE_DEPRECATION_WARNINGS  // TODO: CHAOS_IS_CLOTHINGSIMULATIONMESH_ABSTRACT
			FClothingSimulationMesh* InMesh,
PRAGMA_ENABLE_DEPRECATION_WARNINGS
			TArray<FClothingSimulationCollider*>&& InColliders,
			uint32 InGroupId,
			EMassMode InMassMode,
			FRealSingle InMassValue,
			FRealSingle InMinPerParticleMass,
			const TVec2<FRealSingle>& InEdgeStiffness,
			const TVec2<FRealSingle>& InBendingStiffness,
			FRealSingle InBucklingRatio,
			const TVec2<FRealSingle>& InBucklingStiffness,
			bool bInUseBendingElements,
			const TVec2<FRealSingle>& InAreaStiffness,
			FRealSingle InVolumeStiffness,
			bool bInUseThinShellVolumeConstraints,
			const TVec2<FRealSingle>& InTetherStiffness,
			const TVec2<FRealSingle>& InTetherScale,
PRAGMA_DISABLE_DEPRECATION_WARNINGS
			ETetherMode InTetherMode,
PRAGMA_ENABLE_DEPRECATION_WARNINGS
			FRealSingle InMaxDistancesMultiplier,
			const TVec2<FRealSingle>& InAnimDriveStiffness,
			const TVec2<FRealSingle>& InAnimDriveDamping,
			FRealSingle InShapeTargetStiffness,
			bool bInUseXPBDEdgeConstraints,
			bool bInUseXPBDBendingConstraints,
			bool bInUseXPBDAreaConstraints,
			FRealSingle InGravityScale,
			bool bIsGravityOverridden,
			const TVec3<FRealSingle>& InGravityOverride,
			const TVec3<FRealSingle>& InLinearVelocityScale,
			FRealSingle InAngularVelocityScale,
			FRealSingle InFictitiousAngularScale,
			const TVec2<FRealSingle>& InDrag,
			const TVec2<FRealSingle>& InLift,
			bool bInUsePointBasedWindModel,
			const TVec2<FRealSingle>& InPressure,
			FRealSingle InDampingCoefficient,
			FRealSingle InLocalDampingCoefficient,
			FRealSingle InCollisionThickness,
			FRealSingle InFrictionCoefficient,
			bool bInUseCCD,
			bool bInUseSelfCollisions,
			FRealSingle InSelfCollisionThickness,
			FRealSingle InSelfCollisionFrictionCoefficient,
			bool bInUseSelfIntersections,
			bool bInUseLegacyBackstop,
			bool bInUseLODIndexOverride,
			int32 InLODIndexOverride,
			const TVec2<FRealSingle>& EdgeDampingRatio = TVec2<FRealSingle>(0.f),
			const TVec2<FRealSingle>& BendDampingRatio = TVec2<FRealSingle>(0.f));
		CHAOSCLOTH_API ~FClothingSimulationCloth();

		FClothingSimulationCloth(const FClothingSimulationCloth&) = delete;
		FClothingSimulationCloth(FClothingSimulationCloth&&) = delete;
		FClothingSimulationCloth& operator=(const FClothingSimulationCloth&) = delete;
		FClothingSimulationCloth& operator=(FClothingSimulationCloth&&) = delete;

		uint32 GetGroupId() const { return GroupId; }
		uint32 GetLODIndex(const FClothingSimulationSolver* Solver) const { return LODIndices.FindChecked(Solver); }

		int32 GetNumActiveKinematicParticles() const { return NumActiveKinematicParticles; }
		int32 GetNumActiveDynamicParticles() const { return NumActiveDynamicParticles; }

		// ---- Animatable property setters ----
		void SetMaxDistancesMultiplier(FRealSingle InMaxDistancesMultiplier) { MaxDistancesMultiplier = InMaxDistancesMultiplier; }
		UE_DEPRECATED(5.3, "Set properties directly through FClothingSimulationConfig")
		CHAOSCLOTH_API void SetMaterialProperties(const TVec2<FRealSingle>& InEdgeStiffness, const TVec2<FRealSingle>& InBendingStiffness, const TVec2<FRealSingle>& InAreaStiffness);
		UE_DEPRECATED(5.3, "Set properties directly through FClothingSimulationConfig")
		CHAOSCLOTH_API void SetLongRangeAttachmentProperties(const TVec2<FRealSingle>& InTetherStiffness, const TVec2<FRealSingle>& InTetherScale);
		UE_DEPRECATED(5.3, "Set properties directly through FClothingSimulationConfig")
		CHAOSCLOTH_API void SetCollisionProperties(FRealSingle InCollisionThickness, FRealSingle InFrictionCoefficient, bool bInUseCCD, FRealSingle InSelfCollisionThickness);
		UE_DEPRECATED(5.3, "Set properties directly through FClothingSimulationConfig")
		CHAOSCLOTH_API void SetBackstopProperties(bool bInEnableBackstop);
		UE_DEPRECATED(5.3, "Set properties directly through FClothingSimulationConfig")
		CHAOSCLOTH_API void SetDampingProperties(FRealSingle InDampingCoefficient, FRealSingle InLocalDampingCoefficient = 0.f);
		UE_DEPRECATED(5.3, "Set properties directly through FClothingSimulationConfig")
		CHAOSCLOTH_API void SetAerodynamicsProperties(const TVec2<FRealSingle>& InDrag, const TVec2<FRealSingle>& InLift, FRealSingle InAirDensity, const FVec3& InWindVelocity);  // AirDensity is here in kg/cm^3 for legacy reason (kg/m^3 in UI)
		UE_DEPRECATED(5.3, "Set properties directly through FClothingSimulationConfig")
		CHAOSCLOTH_API void SetPressureProperties(const TVec2<FRealSingle>& InPressure);
		UE_DEPRECATED(5.3, "Set properties directly through FClothingSimulationConfig")
		CHAOSCLOTH_API void SetGravityProperties(FRealSingle InGravityScale, bool bInUseGravityOverride, const FVec3& InGravityOverride);
		UE_DEPRECATED(5.3, "Set properties directly through FClothingSimulationConfig")
		CHAOSCLOTH_API void SetAnimDriveProperties(const TVec2<FRealSingle>& InAnimDriveStiffness, const TVec2<FRealSingle>& InAnimDriveDamping);
		UE_DEPRECATED(5.3, "Get properties directly through FClothingSimulationConfig")
		CHAOSCLOTH_API void GetAnimDriveProperties(TVec2<FRealSingle>& OutAnimDriveStiffness, TVec2<FRealSingle>& OutAnimDriveDamping);
		UE_DEPRECATED(5.3, "Set properties directly through FClothingSimulationConfig")
		CHAOSCLOTH_API void SetVelocityScaleProperties(const FVec3& InLinearVelocityScale, FRealSingle InAngularVelocityScale, FRealSingle InFictitiousAngularScale);

		void Reset() { bNeedsReset = true; }
		void Teleport() { bNeedsTeleport = true; }
		// ---- End of the animatable property setters ----

		// ---- Node property getters/setters
		PRAGMA_DISABLE_DEPRECATION_WARNINGS  // TODO: CHAOS_IS_CLOTHINGSIMULATIONMESH_ABSTRACT
		FClothingSimulationMesh* GetMesh() const { return Mesh; }
		CHAOSCLOTH_API void SetMesh(FClothingSimulationMesh* InMesh);
		PRAGMA_ENABLE_DEPRECATION_WARNINGS

		FClothingSimulationConfig* GetConfig() const { return Config; }
		CHAOSCLOTH_API void SetConfig(FClothingSimulationConfig* InConfig);

		const TArray<FClothingSimulationCollider*>& GetColliders() const { return Colliders; }
		CHAOSCLOTH_API void SetColliders(TArray<FClothingSimulationCollider*>&& InColliders);
		CHAOSCLOTH_API void AddCollider(FClothingSimulationCollider* InCollider);
		CHAOSCLOTH_API void RemoveCollider(FClothingSimulationCollider* InCollider);
		CHAOSCLOTH_API void RemoveColliders();
		// ---- End of the Node property getters/setters

		// ---- Debugging/visualization functions
		// Return the solver's input positions for this cloth source current LOD, not thread safe, call must be done right after the solver update.
		CHAOSCLOTH_API TConstArrayView<Softs::FSolverVec3> GetAnimationPositions(const FClothingSimulationSolver* Solver) const;
		// Return the solver's input normals for this cloth source current LOD, not thread safe, call must be done right after the solver update.
		CHAOSCLOTH_API TConstArrayView<Softs::FSolverVec3> GetAnimationNormals(const FClothingSimulationSolver* Solver) const;
		// Return the solver's positions for this cloth current LOD, not thread safe, call must be done right after the solver update.
		CHAOSCLOTH_API TConstArrayView<Softs::FSolverVec3> GetParticlePositions(const FClothingSimulationSolver* Solver) const;
		// Return the solver's velocities for this cloth current LOD, not thread safe, call must be done right after the solver update.
		CHAOSCLOTH_API TConstArrayView<Softs::FSolverVec3> GetParticleVelocities(const FClothingSimulationSolver* Solver) const;
		// Return the solver's normals for this cloth current LOD, not thread safe, call must be done right after the solver update.
		CHAOSCLOTH_API TConstArrayView<Softs::FSolverVec3> GetParticleNormals(const FClothingSimulationSolver* Solver) const;
		// Return the solver's inverse masses for this cloth current LOD, not thread safe, call must be done right after the solver update.
		CHAOSCLOTH_API TConstArrayView<Softs::FSolverReal> GetParticleInvMasses(const FClothingSimulationSolver* Solver) const;
		// Return the current gravity as applied by the solver using the various overrides, not thread safe, call must be done right after the solver update.
		// Does not have GravityScale applied when using Force-based solver (Get the per-particle value directly from cloth constraints' external forces).
		CHAOSCLOTH_API TVec3<FRealSingle> GetGravity(const FClothingSimulationSolver* Solver) const;
		// Return the current bounding box based on a given solver, not thread safe, call must be done right after the solver update.
		CHAOSCLOTH_API FAABB3 CalculateBoundingBox(const FClothingSimulationSolver* Solver) const;
		// Return the current LOD ParticleRangeId, or INDEX_NONE if no LOD is currently selected.
		CHAOSCLOTH_API int32 GetParticleRangeId(const FClothingSimulationSolver* Solver) const;
		UE_DEPRECATED(5.4, "Offset has been renamed ParticleRangeId to reflect that it is no longer an offset.")
		int32 GetOffset(const FClothingSimulationSolver* Solver) const { return GetParticleRangeId(Solver); }
		// Return the current LOD num particles, or 0 if no LOD is currently selected.
		CHAOSCLOTH_API int32 GetNumParticles(const FClothingSimulationSolver* Solver) const;
		// Return the current LOD mesh.
		CHAOSCLOTH_API const FTriangleMesh& GetTriangleMesh(const FClothingSimulationSolver* Solver) const;
		// Return the weight map of the specified name if available on the current LOD, or an empty array view otherwise.
		CHAOSCLOTH_API TConstArrayView<FRealSingle> GetWeightMapByName(const FClothingSimulationSolver* Solver, const FString& Name) const;
		// Return the weight map of the specified property name if it exists and is available on the current LOD, or an empty array view otherwise.
		CHAOSCLOTH_API TConstArrayView<FRealSingle> GetWeightMapByProperty(const FClothingSimulationSolver* Solver, const FString& Property) const;
		// Return the face int map of the specified name if available on the current LOD, or an empty array view otherwise.
		CHAOSCLOTH_API TConstArrayView<int32> GetFaceIntMapByName(const FClothingSimulationSolver* Solver, const FString& Name) const;
		// Return the face int map of the specified property name if it exists and is available on the current LOD, or an empty array view otherwise.
		CHAOSCLOTH_API TConstArrayView<int32> GetFaceIntMapByProperty(const FClothingSimulationSolver* Solver, const FString& Property) const;
		UE_DEPRECATED(5.3, "Returns an empty array from 5.3. Update your code with GetWeightMapByName and GetWeightMapByProperty to return the current LOD weight map instead.")
		CHAOSCLOTH_API const TArray<TConstArrayView<FRealSingle>>& GetWeightMaps(const FClothingSimulationSolver* Solver) const;
		// Return the current LOD tethers.
		CHAOSCLOTH_API const TArray<TConstArrayView<TTuple<int32, int32, float>>>& GetTethers(const FClothingSimulationSolver* Solver) const;
		// Return the reference bone index for this cloth.
		CHAOSCLOTH_API int32 GetReferenceBoneIndex() const;
		// Return the local reference space transform for this cloth.
		const FRigidTransform3& GetReferenceSpaceTransform() const { return ReferenceSpaceTransform;  }
		// ---- End of the debugging/visualization functions

		// ---- Solver interface ----
		CHAOSCLOTH_API void Add(FClothingSimulationSolver* Solver);
		CHAOSCLOTH_API void Remove(FClothingSimulationSolver* Solver);

		CHAOSCLOTH_API void PreUpdate(FClothingSimulationSolver* Solver);
		CHAOSCLOTH_API void Update(FClothingSimulationSolver* Solver);
		CHAOSCLOTH_API void PostUpdate(FClothingSimulationSolver* Solver);

		CHAOSCLOTH_API void UpdateFromCache(const FClothingSimulationCacheData& CacheData);
		// ---- End of the Solver interface ----

	private:
		CHAOSCLOTH_API int32 GetNumParticles(int32 InLODIndex) const;
		CHAOSCLOTH_API int32 GetParticleRangeId(const FClothingSimulationSolver* Solver, int32 InLODIndex) const;

	private:
		struct FLODData;

		// Cloth parameters
PRAGMA_DISABLE_DEPRECATION_WARNINGS  // TODO: CHAOS_IS_CLOTHINGSIMULATIONMESH_ABSTRACT
		FClothingSimulationMesh* Mesh = nullptr;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
		FClothingSimulationConfig* Config = nullptr;
		TArray<FClothingSimulationCollider*> Colliders;
		uint32 GroupId = 0;

		TSharedPtr<FManagedArrayCollection> PropertyCollection;  // Used for backward compatibility only, otherwise the properties are owned by the Config

		FRealSingle MaxDistancesMultiplier = 1.f;  // Legacy multiplier

		bool bUseLODIndexOverride = false;
		int32 LODIndexOverride = 0;
		bool bNeedsReset = false;
		bool bNeedsTeleport = false;

		// Reference space transform
		FRigidTransform3 ReferenceSpaceTransform;  // TODO: Add override in the style of LODIndexOverride

		// LOD data
		TArray<TUniquePtr<FLODData>> LODData;
		TMap<FClothingSimulationSolver*, int32> LODIndices;

		// Stats
		int32 NumActiveKinematicParticles = 0;
		int32 NumActiveDynamicParticles = 0;
	};
} // namespace Chaos
