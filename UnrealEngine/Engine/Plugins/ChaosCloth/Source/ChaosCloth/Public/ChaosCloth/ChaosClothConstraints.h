// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Core.h"
#include "Containers/ContainersFwd.h"
#include "Templates/SharedPointer.h"
#include "Chaos/PBDSoftsEvolutionFwd.h"
#include "Chaos/PBDLongRangeConstraints.h"
#include "Chaos/PBDCollisionSpringConstraintsBase.h"
#include "Chaos/Deformable/GaussSeidelMainConstraint.h"
#include "Chaos/Deformable/GaussSeidelCorotatedCodimensionalConstraints.h"

namespace Chaos
{

	struct FClothingPatternData;

	class FClothConstraints final
	{
	public:
		UE_DEPRECATED(5.3, "ETetherMode has been replaced with bUseGeodesicTethers.")
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		typedef Softs::FPBDLongRangeConstraints::EMode ETetherMode;
PRAGMA_ENABLE_DEPRECATION_WARNINGS

		FClothConstraints();
		~FClothConstraints();

		// ---- Solver interface ----
		// Force-based solver
		void Initialize(
			Softs::FEvolution* InEvolution,
			FPerSolverFieldSystem* InPerSolverField,
			const TArray<Softs::FSolverVec3>& InInterpolatedAnimationPositions,
			const TArray<Softs::FSolverVec3>& InInterpolatedAnimationNormals,
			const TArray<Softs::FSolverVec3>& InAnimationVelocities,
			const TArray<Softs::FSolverVec3>& InNormals,
			const TArray<Softs::FSolverRigidTransform3>& InLastSubframeCollisionTransformsCCD,
			TArray<bool>& InCollisionParticleCollided,
			TArray<Softs::FSolverVec3>& InCollisionContacts,
			TArray<Softs::FSolverVec3>& InCollisionNormals,
			TArray<Softs::FSolverReal>& InCollisionPhis,
			int32 InParticleRangeId);

		// Force-based solver
		void UpdateFromSolver(const Softs::FSolverVec3& SolverGravity, bool bPerClothGravityOverrideEnabled,
			const Softs::FSolverVec3& FictitiousAngularVelocity, const Softs::FSolverVec3& ReferenceSpaceLocation,
			const Softs::FSolverVec3& SolverWindVelocity, const Softs::FSolverReal LegacyWindAdaptation);

		// PBD solver
		void Initialize(
			Softs::FPBDEvolution* InEvolution,
			const TArray<Softs::FSolverVec3>& InInterpolatedAnimationPositions,
			const TArray<Softs::FSolverVec3>& /*InOldAnimationPositions*/, // deprecated
			const TArray<Softs::FSolverVec3>& InInterpolatedAnimationNormals,
			const TArray<Softs::FSolverVec3>& InAnimationVelocities,
			int32 InParticleOffset,
			int32 InNumParticles);

		void SetSkipSelfCollisionInit(bool bValue) { bSkipSelfCollisionInit = bValue; }
		// ---- End of Solver interface ----

		// ---- Cloth interface ----
		void AddRules(
			const Softs::FCollectionPropertyConstFacade& ConfigProperties,
			const FTriangleMesh& TriangleMesh,
			const FClothingPatternData* PatternData,
			const TMap<FString, TConstArrayView<FRealSingle>>& WeightMaps,
			const TMap<FString, const TSet<int32>*>& VertexSets,
			const TMap<FString, const TSet<int32>*>& FaceSets,
			const TMap<FString, TConstArrayView<int32>>& FaceIntMaps,
			const TArray<TConstArrayView<TTuple<int32, int32, FRealSingle>>>& Tethers,
			Softs::FSolverReal MeshScale,
			bool bEnabled,
			const FTriangleMesh* MultiResCoarseLODMesh = nullptr,
			const int32 MultiResCoarseLODParticleRangeId = INDEX_NONE,
			const TSharedPtr<Softs::FMultiResConstraints>& FineLODMultiResConstraint = TSharedPtr<Softs::FMultiResConstraints>(nullptr));

		UE_DEPRECATED(5.4, "Use AddRules() with WeightMaps, VertexSets, FaceSets, FaceIntMaps, and optional PatternData instead.")
		void AddRules(
			const Softs::FCollectionPropertyConstFacade& ConfigProperties,
			const FTriangleMesh& TriangleMesh,
			const FClothingPatternData* PatternData,
			const TMap<FString, TConstArrayView<FRealSingle>>& WeightMaps,
			const TArray<TConstArrayView<TTuple<int32, int32, FRealSingle>>>& Tethers,
			Softs::FSolverReal MeshScale,
			bool bEnabled);

		UE_DEPRECATED(5.3, "Use AddRules() with WeightMaps, VertexSets, FaceSets, FaceIntMap, and optional PatternData instead.")
		void AddRules(
			const Softs::FCollectionPropertyConstFacade& ConfigProperties,
			const FTriangleMesh& TriangleMesh,
			const TArray<TConstArrayView<FRealSingle>>& WeightMapArray,
			const TArray<TConstArrayView<TTuple<int32, int32, FRealSingle>>>& Tethers,
			Softs::FSolverReal MeshScale,
			bool bEnabled);

		void Update(
			const Softs::FCollectionPropertyConstFacade& ConfigProperties,
			const TMap<FString, TConstArrayView<FRealSingle>>& WeightMaps,
			const TMap<FString, const TSet<int32>*>& VertexSets,
			const TMap<FString, const TSet<int32>*>& FaceSets,
			const TMap<FString, TConstArrayView<int32>>& FaceIntMaps,
			Softs::FSolverReal MeshScale,
			Softs::FSolverReal MaxDistancesScale = (Softs::FSolverReal)1.);

		UE_DEPRECATED(5.4, "Use Update() with WeightMaps, VertexSets, FaceSets, and FaceIntMaps instead.")
		void Update(
			const Softs::FCollectionPropertyConstFacade& ConfigProperties,
			const TMap<FString, TConstArrayView<FRealSingle>>& WeightMaps,
			Softs::FSolverReal MeshScale,
			Softs::FSolverReal MaxDistancesScale = (Softs::FSolverReal)1.);

		UE_DEPRECATED(5.3, "Use Update() with WeightMaps, VertexSets, FaceSets, and FaceIntMaps instead.")
		void Update(
			const Softs::FCollectionPropertyConstFacade& ConfigProperties,
			Softs::FSolverReal MeshScale,
			Softs::FSolverReal MaxDistancesScale = (Softs::FSolverReal)1.);

		// NOTE: this only does something if using the PBDSolver. Force-based solver constraints are activated automatically
		// when activating a particle range.
		void Enable(bool bEnable);
		// ---- End of Cloth interface ----

		// ---- Debug functions ----
		UE_DEPRECATED(5.4, "Shape constraints are no longer supported.")
		const TSharedPtr<Softs::FPBDShapeConstraints>& GetShapeConstraints() const { return ShapeConstraints_Deprecated; }

		const TSharedPtr<Softs::FPBDEdgeSpringConstraints>& GetEdgeSpringConstraints() const { return EdgeConstraints; }
		const TSharedPtr<Softs::FXPBDEdgeSpringConstraints>& GetXEdgeSpringConstraints() const { return XEdgeConstraints; }
		const TSharedPtr<Softs::FXPBDStretchBiasElementConstraints>& GetXStretchBiasConstraints() const { return XStretchBiasConstraints; }
		const TSharedPtr<Softs::FXPBDAnisotropicSpringConstraints>& GetXAnisoSpringConstraints() const { return XAnisoSpringConstraints; }
		const TSharedPtr<Softs::FPBDBendingSpringConstraints>& GetBendingSpringConstraints() const { return BendingConstraints; }
		const TSharedPtr<Softs::FXPBDBendingSpringConstraints>& GetXBendingSpringConstraints() const { return XBendingConstraints; }
		const TSharedPtr<Softs::FPBDBendingConstraints>& GetBendingElementConstraints() const { return BendingElementConstraints; }
		const TSharedPtr<Softs::FXPBDBendingConstraints>& GetXBendingElementConstraints() const { return XBendingElementConstraints; }
		const TSharedPtr<Softs::FXPBDAnisotropicBendingConstraints>& GetXAnisoBendingElementConstraints() const { return XAnisoBendingElementConstraints; }
		const TSharedPtr<Softs::FPBDAreaSpringConstraints>& GetAreaSpringConstraints() const { return AreaConstraints; }
		const TSharedPtr<Softs::FXPBDAreaSpringConstraints>& GetXAreaSpringConstraints() const { return XAreaConstraints; }
		const TSharedPtr<Softs::FPBDLongRangeConstraints>& GetLongRangeConstraints() const { return LongRangeConstraints; }
		const TSharedPtr<Softs::FPBDSphericalConstraint>& GetMaximumDistanceConstraints() const { return MaximumDistanceConstraints; }
		const TSharedPtr<Softs::FPBDSphericalBackstopConstraint>& GetBackstopConstraints() const { return BackstopConstraints; }
		const TSharedPtr<Softs::FPBDAnimDriveConstraint>& GetAnimDriveConstraints() const { return AnimDriveConstraints; }
		const TSharedPtr<Softs::FPBDCollisionSpringConstraints>& GetSelfCollisionConstraints() const { return SelfCollisionConstraints; }
		const TSharedPtr<Softs::FPBDTriangleMeshIntersections>& GetSelfIntersectionConstraints() const { return SelfIntersectionConstraints; }
		const TSharedPtr<Softs::FPBDTriangleMeshCollisions>& GetSelfCollisionInit() const { return SelfCollisionInit; }
		const TSharedPtr<Softs::FPBDSelfCollisionSphereConstraints>& GetSelfCollisionSphereConstraints() const
		{ return SelfCollisionSphereConstraints; }
		const TSharedPtr<Softs::FVelocityAndPressureField>& GetVelocityAndPressureField() const { return VelocityAndPressureField; }
		const TSharedPtr<Softs::FExternalForces>& GetExternalForces() const { return ExternalForces; }
		const TSharedPtr<Softs::FPBDSoftBodyCollisionConstraint>& GetCollisionConstraint() const { return CollisionConstraint; }
		const TSharedPtr<Softs::FMultiResConstraints>& GetMultiResConstraints() const { return MultiResConstraints; }
		// ---- End of debug functions ----

	private:
		void CreateSelfCollisionConstraints(
			const Softs::FCollectionPropertyConstFacade& ConfigProperties,
			const TMap<FString, TConstArrayView<FRealSingle>>& WeightMaps,
			const TMap<FString, const TSet<int32>*>& VertexSets,
			const TMap<FString, const TSet<int32>*>& FaceSets,
			const TMap<FString, TConstArrayView<int32>>& FaceIntMaps,
			const FTriangleMesh& TriangleMesh);
		void CreateStretchConstraints(
			const Softs::FCollectionPropertyConstFacade& ConfigProperties,
			const TMap<FString, TConstArrayView<FRealSingle>>& WeightMaps,
			const FTriangleMesh& TriangleMesh,
			const FClothingPatternData* PatternData);
		void CreateBendingConstraints(
			const Softs::FCollectionPropertyConstFacade& ConfigProperties,
			const TMap<FString, TConstArrayView<FRealSingle>>& WeightMaps,
			const FTriangleMesh& TriangleMesh,
			const FClothingPatternData* PatternData);
		void CreateAreaConstraints(
			const Softs::FCollectionPropertyConstFacade& ConfigProperties,
			const TMap<FString, TConstArrayView<FRealSingle>>& WeightMaps,
			const FTriangleMesh& TriangleMesh);
		void CreateLongRangeConstraints(
			const Softs::FCollectionPropertyConstFacade& ConfigProperties,
			const TMap<FString, TConstArrayView<FRealSingle>>& WeightMaps,
			const TArray<TConstArrayView<TTuple<int32, int32, FRealSingle>>>& Tethers,
			Softs::FSolverReal MeshScale);
		void CreateMaxDistanceConstraints(
			const Softs::FCollectionPropertyConstFacade& ConfigProperties,
			const TMap<FString, TConstArrayView<FRealSingle>>& WeightMaps,
			Softs::FSolverReal MeshScale);
		void CreateBackstopConstraints(
			const Softs::FCollectionPropertyConstFacade& ConfigProperties,
			const TMap<FString, TConstArrayView<FRealSingle>>& WeightMaps,
			Softs::FSolverReal MeshScale);
		void CreateAnimDriveConstraints(
			const Softs::FCollectionPropertyConstFacade& ConfigProperties,
			const TMap<FString, TConstArrayView<FRealSingle>>& WeightMaps);
		void CreateVelocityAndPressureField(
			const Softs::FCollectionPropertyConstFacade& ConfigProperties,
			const TMap<FString, TConstArrayView<FRealSingle>>& WeightMaps,
			const FTriangleMesh& TriangleMesh
		);
		void CreateExternalForces(
			const Softs::FCollectionPropertyConstFacade& ConfigProperties,
			const TMap<FString, TConstArrayView<FRealSingle>>& WeightMaps
		);
		void CreateCollisionConstraint(
			const Softs::FCollectionPropertyConstFacade& ConfigProperties,
			Softs::FSolverReal MeshScale );
		void CreateMultiresConstraint(
			const Softs::FCollectionPropertyConstFacade& ConfigProperties,
			const TMap<FString, TConstArrayView<FRealSingle>>& WeightMaps,
			const FTriangleMesh& TriangleMesh,
			const FTriangleMesh* MultiResCoarseLODMesh,
			const int32 MultiResCoarseLODParticleRangeId);

		void CreateForceBasedRules(const TSharedPtr<Softs::FMultiResConstraints>& FineLODMultiResConstraint);
		void CreatePBDRules();
		void CreateGSRules();
		void GetGSNumRules();


		//~ Begin deprecated constraints
		TSharedPtr<Softs::FPBDShapeConstraints> ShapeConstraints_Deprecated;
		//~ End deprecated constraints

		TSharedPtr<Softs::FPBDEdgeSpringConstraints> EdgeConstraints;
		TSharedPtr<Softs::FXPBDEdgeSpringConstraints> XEdgeConstraints;
		TSharedPtr<Softs::FXPBDStretchBiasElementConstraints> XStretchBiasConstraints;
		TSharedPtr<Softs::FXPBDAnisotropicSpringConstraints> XAnisoSpringConstraints;
		TSharedPtr<Softs::FPBDBendingSpringConstraints> BendingConstraints;
		TSharedPtr<Softs::FXPBDBendingSpringConstraints> XBendingConstraints;
		TSharedPtr<Softs::FPBDBendingConstraints> BendingElementConstraints;
		TSharedPtr<Softs::FXPBDBendingConstraints> XBendingElementConstraints;
		TSharedPtr<Softs::FXPBDAnisotropicBendingConstraints> XAnisoBendingElementConstraints;
		TSharedPtr<Softs::FPBDAreaSpringConstraints> AreaConstraints;
		TSharedPtr<Softs::FXPBDAreaSpringConstraints> XAreaConstraints;
		TSharedPtr<Softs::FPBDLongRangeConstraints> LongRangeConstraints; 
		TSharedPtr<Softs::FPBDSphericalConstraint> MaximumDistanceConstraints;
		TSharedPtr<Softs::FPBDSphericalBackstopConstraint> BackstopConstraints;
		TSharedPtr<Softs::FPBDAnimDriveConstraint> AnimDriveConstraints;
		TSharedPtr<Softs::FPBDTriangleMeshCollisions> SelfCollisionInit;
		TSharedPtr<Softs::FPBDCollisionSpringConstraints> SelfCollisionConstraints;
		TSharedPtr<Softs::FPBDTriangleMeshIntersections> SelfIntersectionConstraints;
		TSharedPtr<Softs::FPBDSelfCollisionSphereConstraints> SelfCollisionSphereConstraints;
		TSharedPtr<Softs::FGaussSeidelMainConstraint<Softs::FSolverReal, Softs::FSolverParticles>> GSMainConstraint;
		TSharedPtr<Softs::FGaussSeidelCorotatedCodimensionalConstraints<Softs::FSolverReal, Softs::FSolverParticles>> GSCorotatedCodimensionalConstraint;
		TSharedPtr<Softs::FMultiResConstraints> MultiResConstraints;
		//~ Begin Force-based solver only constraints
		Softs::FSolverVec3 SolverWindVelocity; // Set from solver and added to wind from the config
		TSharedPtr<Softs::FVelocityAndPressureField> VelocityAndPressureField;
		TSharedPtr<Softs::FExternalForces> ExternalForces;
		TSharedPtr<Softs::FPBDSoftBodyCollisionConstraint> CollisionConstraint;
		//~ End Force-based solver only constraints

		// Exactly one of these should be non-null
		Softs::FEvolution* Evolution;
		Softs::FPBDEvolution* PBDEvolution;

		const TArray<Softs::FSolverVec3>* AnimationPositions;
		const TArray<Softs::FSolverVec3>* AnimationNormals;
		const TArray<Softs::FSolverVec3>* AnimationVelocities;

		int32 ParticleOffset;
		int32 ParticleRangeId;
		int32 NumParticles;

		int32 NumConstraintInits;
		int32 NumConstraintRules;
		int32 NumPostCollisionConstraintRules;
		int32 NumPostprocessingConstraintRules;

		bool bSkipSelfCollisionInit = false;

		//~ Begin Force-based solver only fields
		FPerSolverFieldSystem* PerSolverField;
		const TArray<Softs::FSolverVec3>* Normals;
		const TArray<Softs::FSolverRigidTransform3>* LastSubframeCollisionTransformsCCD;
		TArray<bool>* CollisionParticleCollided;
		TArray<Softs::FSolverVec3>* CollisionContacts;
		TArray<Softs::FSolverVec3>* CollisionNormals;
		TArray<Softs::FSolverReal>* CollisionPhis;

		int32 NumPreSubstepInits;
		int32 NumExternalForceRules;
		int32 NumPreSubstepConstraintRules;
		int32 NumCollisionConstraintRules;
		int32 NumUpdateLinearSystemRules;
		int32 NumUpdateLinearSystemCollisionsRules;
		//~ End Force-based solver only fields

		//~ Begin PBD solver only fields
		const TArray<Softs::FSolverVec3>* OldAnimationPositions_Deprecated;
		int32 ConstraintInitOffset;
		int32 ConstraintRuleOffset;
		int32 PostCollisionConstraintRuleOffset;
		int32 PostprocessingConstraintRuleOffset;
		//~ End PBD solver only fields

		class FRuleCreator;
	};
} // namespace Chaos
