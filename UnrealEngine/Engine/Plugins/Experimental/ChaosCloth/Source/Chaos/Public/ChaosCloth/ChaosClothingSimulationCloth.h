// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Transform.h"
#include "Chaos/TriangleMesh.h"
#include "Chaos/AABB.h"
#include "Containers/ContainersFwd.h"
#include "ChaosCloth/ChaosClothConstraints.h"

namespace Chaos
{
	class FClothingSimulationSolver;
	class FClothingSimulationMesh;
	class FClothingSimulationCollider;

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

		typedef FClothConstraints::ETetherMode ETetherMode;

		FClothingSimulationCloth(
			FClothingSimulationMesh* InMesh,
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
			ETetherMode InTetherMode,
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
			bool bInUseLegacyWind,
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
			int32 InLODIndexOverride);
		~FClothingSimulationCloth();

		uint32 GetGroupId() const { return GroupId; }
		uint32 GetLODIndex(const FClothingSimulationSolver* Solver) const { return LODIndices.FindChecked(Solver); }

		int32 GetNumActiveKinematicParticles() const { return NumActiveKinematicParticles; }
		int32 GetNumActiveDynamicParticles() const { return NumActiveDynamicParticles; }

		// ---- Animatable property setters ----
		void SetMaxDistancesMultiplier(FRealSingle InMaxDistancesMultiplier) { MaxDistancesMultiplier = InMaxDistancesMultiplier; }

		void SetMaterialProperties(const TVec2<FRealSingle>& InEdgeStiffness, const TVec2<FRealSingle>& InBendingStiffness, const TVec2<FRealSingle>& InAreaStiffness) { EdgeStiffness = InEdgeStiffness; BendingStiffness = InBendingStiffness; AreaStiffness = InAreaStiffness; }
		void SetLongRangeAttachmentProperties(const TVec2<FRealSingle>& InTetherStiffness, const TVec2<FRealSingle>& InTetherScale) { TetherStiffness = InTetherStiffness; TetherScale = InTetherScale;  }
		void SetCollisionProperties(FRealSingle InCollisionThickness, FRealSingle InFrictionCoefficient, bool bInUseCCD, FRealSingle InSelfCollisionThickness) { CollisionThickness = InCollisionThickness; FrictionCoefficient = InFrictionCoefficient; bUseCCD = bInUseCCD; SelfCollisionThickness = InSelfCollisionThickness; }
		void SetBackstopProperties(bool bInEnableBackstop) { bEnableBackstop = bInEnableBackstop; }
		void SetDampingProperties(FRealSingle InDampingCoefficient, FRealSingle InLocalDampingCoefficient = 0.f) { DampingCoefficient = InDampingCoefficient; LocalDampingCoefficient = InLocalDampingCoefficient; }
		void SetAerodynamicsProperties(const TVec2<FRealSingle>& InDrag, const TVec2<FRealSingle>& InLift, FRealSingle InAirDensity, const FVec3& InWindVelocity) { Drag = InDrag; Lift = InLift; InAirDensity = AirDensity; WindVelocity = InWindVelocity; }
		void SetPressureProperties(const TVec2<FRealSingle>& InPressure) { Pressure = InPressure; }
		void SetGravityProperties(FRealSingle InGravityScale, bool bInIsGravityOverridden, const FVec3& InGravityOverride) { GravityScale = InGravityScale; bIsGravityOverridden = bInIsGravityOverridden; GravityOverride = InGravityOverride; }
		void SetAnimDriveProperties(const TVec2<FRealSingle>& InAnimDriveStiffness, const TVec2<FRealSingle>& InAnimDriveDamping) { AnimDriveStiffness = InAnimDriveStiffness; AnimDriveDamping = InAnimDriveDamping; }
		void GetAnimDriveProperties(TVec2<FRealSingle>& OutAnimDriveStiffness, TVec2<FRealSingle>& OutAnimDriveDamping) { OutAnimDriveStiffness = AnimDriveStiffness; OutAnimDriveDamping = AnimDriveDamping; }
		void SetVelocityScaleProperties(const FVec3& InLinearVelocityScale, FRealSingle InAngularVelocityScale, FRealSingle InFictitiousAngularScale) { LinearVelocityScale = InLinearVelocityScale; AngularVelocityScale = InAngularVelocityScale; FictitiousAngularScale = InFictitiousAngularScale;  }

		void Reset() { bNeedsReset = true; }
		void Teleport() { bNeedsTeleport = true; }
		// ---- End of the animatable property setters ----

		// ---- Node property getters/setters
		FClothingSimulationMesh* GetMesh() const { return Mesh; }
		void SetMesh(FClothingSimulationMesh* InMesh);

		const TArray<FClothingSimulationCollider*>& GetColliders() const { return Colliders; }
		void SetColliders(TArray<FClothingSimulationCollider*>&& InColliders);
		void AddCollider(FClothingSimulationCollider* InCollider);
		void RemoveCollider(FClothingSimulationCollider* InCollider);
		void RemoveColliders();
		// ---- End of the Node property getters/setters

		// ---- Debugging/visualization functions
		// Return the solver's input positions for this cloth source current LOD, not thread safe, call must be done right after the solver update.
		TConstArrayView<Softs::FSolverVec3> GetAnimationPositions(const FClothingSimulationSolver* Solver) const;
		// Return the solver's input normals for this cloth source current LOD, not thread safe, call must be done right after the solver update.
		TConstArrayView<Softs::FSolverVec3> GetAnimationNormals(const FClothingSimulationSolver* Solver) const;
		// Return the solver's positions for this cloth current LOD, not thread safe, call must be done right after the solver update.
		TConstArrayView<Softs::FSolverVec3> GetParticlePositions(const FClothingSimulationSolver* Solver) const;
		// Return the solver's velocities for this cloth current LOD, not thread safe, call must be done right after the solver update.
		TConstArrayView<Softs::FSolverVec3> GetParticleVelocities(const FClothingSimulationSolver* Solver) const;
		// Return the solver's normals for this cloth current LOD, not thread safe, call must be done right after the solver update.
		TConstArrayView<Softs::FSolverVec3> GetParticleNormals(const FClothingSimulationSolver* Solver) const;
		// Return the solver's inverse masses for this cloth current LOD, not thread safe, call must be done right after the solver update.
		TConstArrayView<Softs::FSolverReal> GetParticleInvMasses(const FClothingSimulationSolver* Solver) const;
		// Return the current gravity as applied by the solver using the various overrides, not thread safe, call must be done right after the solver update.
		TVec3<FRealSingle> GetGravity(const FClothingSimulationSolver* Solver) const;
		// Return the current bounding box based on a given solver, not thread safe, call must be done right after the solver update.
		FAABB3 CalculateBoundingBox(const FClothingSimulationSolver* Solver) const;
		// Return the current LOD offset in the solver's particle array, or INDEX_NONE if no LOD is currently selected.
		int32 GetOffset(const FClothingSimulationSolver* Solver) const;
		// Return the current LOD mesh.
		const FTriangleMesh& GetTriangleMesh(const FClothingSimulationSolver* Solver) const;
		// Return the current LOD weightmaps.
		const TArray<TConstArrayView<FRealSingle>>& GetWeightMaps(const FClothingSimulationSolver* Solver) const;
		// Return the current LOD tethers.
		const TArray<TConstArrayView<TTuple<int32, int32, float>>>& GetTethers(const FClothingSimulationSolver* Solver) const;
		// Return the reference bone index for this cloth.
		int32 GetReferenceBoneIndex() const;
		// Return the local reference space transform for this cloth.
		const FRigidTransform3& GetReferenceSpaceTransform() const { return ReferenceSpaceTransform;  }
		// ---- End of the debugging/visualization functions

		// ---- Solver interface ----
		void Add(FClothingSimulationSolver* Solver);
		void Remove(FClothingSimulationSolver* Solver);

		void PreUpdate(FClothingSimulationSolver* Solver);
		void Update(FClothingSimulationSolver* Solver);
		void PostUpdate(FClothingSimulationSolver* Solver);
		// ---- End of the Solver interface ----

	private:
		int32 GetNumParticles(int32 InLODIndex) const;
		int32 GetOffset(const FClothingSimulationSolver* Solver, int32 InLODIndex) const;

	private:
		struct FLODData
		{
			// Input mesh
			const int32 NumParticles;
			const TConstArrayView<uint32> Indices;
			const TArray<TConstArrayView<FRealSingle>> WeightMaps;
			const TArray<TConstArrayView<TTuple<int32, int32, FRealSingle>>> Tethers;

			// Per Solver data
			struct FSolverData
			{
				int32 Offset;
				FTriangleMesh TriangleMesh;  // TODO: Triangle Mesh shouldn't really be solver dependent (ie not use an offset)
			};
			TMap<FClothingSimulationSolver*, FSolverData> SolverData;

			// Stats
			int32 NumKinenamicParticles;
			int32 NumDynammicParticles;

			FLODData(
				int32 InNumParticles,
				const TConstArrayView<uint32>& InIndices,
				const TArray<TConstArrayView<FRealSingle>>& InWeightMaps,
				const TArray<TConstArrayView<TTuple<int32, int32, FRealSingle>>>& InTethers);

			void Add(FClothingSimulationSolver* Solver, FClothingSimulationCloth* Cloth, int32 LODIndex);
			void Remove(FClothingSimulationSolver* Solver);

			void Update(FClothingSimulationSolver* Solver, FClothingSimulationCloth* Cloth);

			void Enable(FClothingSimulationSolver* Solver, bool bEnable) const;

			void ResetStartPose(FClothingSimulationSolver* Solver) const;

			void UpdateNormals(FClothingSimulationSolver* Solver) const;
		};

		// Cloth parameters
		FClothingSimulationMesh* Mesh;
		TArray<FClothingSimulationCollider*> Colliders;
		uint32 GroupId;
		EMassMode MassMode;
		FRealSingle MassValue;
		FRealSingle MinPerParticleMass;
		TVec2<FRealSingle> EdgeStiffness;
		TVec2<FRealSingle> BendingStiffness;
		FRealSingle BucklingRatio;
		TVec2<FRealSingle> BucklingStiffness;
		bool bUseBendingElements;
		TVec2<FRealSingle> AreaStiffness;
		FRealSingle VolumeStiffness;
		bool bUseThinShellVolumeConstraints;
		TVec2<FRealSingle> TetherStiffness;
		TVec2<FRealSingle> TetherScale;
		ETetherMode TetherMode;
		FRealSingle MaxDistancesMultiplier;  // Animatable
		TVec2<FRealSingle> AnimDriveStiffness;  // Animatable
		TVec2<FRealSingle> AnimDriveDamping;  // Animatable
		FRealSingle ShapeTargetStiffness;
		bool bUseXPBDEdgeConstraints;
		bool bUseXPBDBendingConstraints;
		bool bUseXPBDAreaConstraints;
		FRealSingle GravityScale;
		bool bIsGravityOverridden;
		TVec3<FRealSingle> GravityOverride;
		TVec3<FRealSingle> LinearVelocityScale;  // Linear ratio applied to the reference bone transforms
		FRealSingle AngularVelocityScale;  // Angular ratio factor applied to the reference bone transforms
		FRealSingle FictitiousAngularScale;
		TVec2<FRealSingle> Drag;
		TVec2<FRealSingle> Lift;
		FVec3 WindVelocity;
		FRealSingle AirDensity;
		bool bUseLegacyWind;
		TVec2<FRealSingle> Pressure;
		FRealSingle DampingCoefficient;
		FRealSingle LocalDampingCoefficient;
		FRealSingle CollisionThickness;
		FRealSingle FrictionCoefficient;
		bool bUseCCD;
		bool bUseSelfCollisions;
		FRealSingle SelfCollisionThickness;
		FRealSingle SelfCollisionFrictionCoefficient;
		bool bUseSelfIntersections;
		bool bEnableBackstop;
		bool bUseLegacyBackstop;
		bool bUseLODIndexOverride;
		int32 LODIndexOverride;
		bool bNeedsReset;
		bool bNeedsTeleport;

		// Reference space transform
		FRigidTransform3 ReferenceSpaceTransform;  // TODO: Add override in the style of LODIndexOverride

		// LOD data
		TArray<FLODData> LODData;
		TMap<FClothingSimulationSolver*, int32> LODIndices;

		// Stats
		int32 NumActiveKinematicParticles;
		int32 NumActiveDynamicParticles;
	};
} // namespace Chaos
