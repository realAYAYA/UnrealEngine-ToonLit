// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Core.h"
#include "Containers/ContainersFwd.h"
#include "Templates/SharedPointer.h"
#include "Chaos/PBDSoftsEvolutionFwd.h"
#include "Chaos/PBDLongRangeConstraints.h"
#include "Chaos/PBDCollisionSpringConstraintsBase.h"

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
		UE_DEPRECATED(5.2, "Use the other Initialize supplying AnimationVelocities for correct AnimDrive behavior")
		void Initialize(
			Softs::FPBDEvolution* InEvolution,
			const TArray<Softs::FSolverVec3>& InAnimationPositions,
			const TArray<Softs::FSolverVec3>& InOldAnimationPositions,
			const TArray<Softs::FSolverVec3>& InAnimationNormals,
			int32 InParticleOffset,
			int32 InNumParticles);
		void Initialize(
			Softs::FPBDEvolution* InEvolution,
			const TArray<Softs::FSolverVec3>& InInterpolatedAnimationPositions,
			const TArray<Softs::FSolverVec3>& /*InOldAnimationPositions*/, // deprecated
			const TArray<Softs::FSolverVec3>& InInterpolatedAnimationNormals,
			const TArray<Softs::FSolverVec3>& InAnimationVelocities,
			int32 InParticleOffset,
			int32 InNumParticles);
		// ---- End of Solver interface ----

		// ---- Cloth interface ----
		void AddRules(
			const Softs::FCollectionPropertyConstFacade& ConfigProperties,
			const FTriangleMesh& TriangleMesh,
			const FClothingPatternData* PatternData,
			const TMap<FString, TConstArrayView<FRealSingle>>& WeightMaps,
			const TArray<TConstArrayView<TTuple<int32, int32, FRealSingle>>>& Tethers,
			Softs::FSolverReal MeshScale,
			bool bEnabled);

		UE_DEPRECATED(5.3, "Use AddRules() with WeightMaps and optional PatternData instead.")
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
			Softs::FSolverReal MeshScale,
			Softs::FSolverReal MaxDistancesScale = (Softs::FSolverReal)1.);

		UE_DEPRECATED(5.3, "Use Update() with WeightMaps instead.")
		void Update(
			const Softs::FCollectionPropertyConstFacade& ConfigProperties,
			Softs::FSolverReal MeshScale,
			Softs::FSolverReal MaxDistancesScale = (Softs::FSolverReal)1.);

		void Enable(bool bEnable);

		UE_DEPRECATED(5.2, "Use AddRules() instead.")
		void SetEdgeConstraints(const TArray<TVec3<int32>>& SurfaceElements, const TConstArrayView<FRealSingle>& StiffnessMultipliers, bool bUseXPBDConstraints);
		UE_DEPRECATED(5.2, "Use AddRules() instead.")
		void SetXPBDEdgeConstraints(const TArray<TVec3<int32>>& SurfaceElements, const TConstArrayView<FRealSingle>& StiffnessMultipliers, const TConstArrayView<FRealSingle>& DampingRatioMultipliers);
		UE_DEPRECATED(5.2, "Use AddRules() instead.")
		void SetBendingConstraints(const TArray<TVec2<int32>>& Edges, const TConstArrayView<FRealSingle>& StiffnessMultipliers, bool bUseXPBDConstraints);
		UE_DEPRECATED(5.2, "Use AddRules() instead.")
		void SetBendingConstraints(TArray<TVec4<int32>>&& BendingElements, const TConstArrayView<FRealSingle>& StiffnessMultipliers, const TConstArrayView<FRealSingle>& BucklingStiffnessMultipliers, bool bUseXPBDConstraints);
		UE_DEPRECATED(5.2, "Use AddRules() instead.")
		void SetXPBDBendingConstraints(TArray<TVec4<int32>>&& BendingElements, const TConstArrayView<FRealSingle>& StiffnessMultipliers, const TConstArrayView<FRealSingle>& BucklingStiffnessMultipliers, const TConstArrayView<FRealSingle>& DampingRatioMultipliers);
		UE_DEPRECATED(5.1, "Use SetBendingConstraints(TArray<TVec4<int32>>&& BendingElements, const TConstArrayView<FRealSingle>& StiffnessMultipliers, const TConstArrayView<FRealSingle>& BucklingStiffnessMultipliers, bool bUseXPBDConstraints) instead.")
		void SetBendingConstraints(TArray<TVec4<int32>>&& BendingElements, Softs::FSolverReal BendingStiffness);
		UE_DEPRECATED(5.2, "Use AddRules() instead.")
		void SetAreaConstraints(const TArray<TVec3<int32>>& SurfaceElements, const TConstArrayView<FRealSingle>& StiffnessMultipliers, bool bUseXPBDConstraints);
		UE_DEPRECATED(5.2, "Use AddRules() instead.")
		void SetVolumeConstraints(const TArray<TVec2<int32>>& DoubleBendingEdges, Softs::FSolverReal VolumeStiffness);
		UE_DEPRECATED(5.2, "Use AddRules() instead.")
		void SetVolumeConstraints(TArray<TVec3<int32>>&& SurfaceElements, Softs::FSolverReal VolumeStiffness);
		UE_DEPRECATED(5.2, "Use AddRules() instead.")
		void SetLongRangeConstraints(
			const TArray<TConstArrayView<TTuple<int32,int32, FRealSingle>>>& Tethers,
			const TConstArrayView<FRealSingle>& TetherStiffnessMultipliers,
			const TConstArrayView<FRealSingle>& TetherScaleMultipliers,
			const Softs::FSolverVec2& TetherScale,
			Softs::FSolverReal MeshScale = (Softs::FSolverReal)1.);
		UE_DEPRECATED(5.1, "LongRange XPBDConstraints are not supported.")
		void SetLongRangeConstraints(
			const TArray<TConstArrayView<TTuple<int32, int32, FRealSingle>>>& Tethers,
			const TConstArrayView<FRealSingle>& TetherStiffnessMultipliers,
			const TConstArrayView<FRealSingle>& TetherScaleMultipliers,
			const Softs::FSolverVec2& TetherScale,
			bool bUseXPBDConstraints,
			Softs::FSolverReal MeshScale = (Softs::FSolverReal)1.);
		UE_DEPRECATED(5.2, "Use AddRules() instead.")
		void SetMaximumDistanceConstraints(const TConstArrayView<FRealSingle>& MaxDistances);
		UE_DEPRECATED(5.2, "Use AddRules() instead.")
		void SetBackstopConstraints(const TConstArrayView<FRealSingle>& BackstopDistances, const TConstArrayView<FRealSingle>& BackstopRadiuses, bool bUseLegacyBackstop);
		UE_DEPRECATED(5.2, "Use AddRules() instead.")
		void SetAnimDriveConstraints(const TConstArrayView<FRealSingle>& AnimDriveStiffnessMultipliers, const TConstArrayView<FRealSingle>& AnimDriveDampingMultipliers);
		UE_DEPRECATED(5.2, "Use AddRules() instead.")
		void SetShapeTargetConstraints(Softs::FSolverReal ShapeTargetStiffness);
		UE_DEPRECATED(5.2, "Use AddRules() instead.")
		void SetSelfCollisionConstraints(
			const class FTriangleMesh& TriangleMesh,
			TSet<TVec2<int32>>&& DisabledCollisionElements,
			Softs::FSolverReal SelfCollisionThickness,
			Softs::FSolverReal SelfCollisionFriction = Softs::FPBDCollisionSpringConstraintsBase::BackCompatFrictionCoefficient,
			bool bGlobalIntersectionAnalysis = false,
			bool bContourMinimization = false);

		UE_DEPRECATED(5.2, "CreateRules will soon be made private, use AddRules() instead.")
		void CreateRules();

		UE_DEPRECATED(5.2, "Use Update() instead.")
		void SetEdgeProperties(const Softs::FSolverVec2& EdgeStiffness, const Softs::FSolverVec2& DampingRatio = Softs::FSolverVec2::ZeroVector);
		UE_DEPRECATED(5.2, "Use Update() instead.")
		void SetBendingProperties(const Softs::FSolverVec2& BendingStiffness, Softs::FSolverReal BucklingRatio = 0.f, const Softs::FSolverVec2& BucklingStiffness = Softs::FSolverVec2::UnitVector, const Softs::FSolverVec2& DampingRatio = Softs::FSolverVec2::ZeroVector);
		UE_DEPRECATED(5.2, "Use Update() instead.")
		void SetAreaProperties(const Softs::FSolverVec2& AreaStiffness);
		UE_DEPRECATED(5.2, "Use Update() instead.")
		void SetThinShellVolumeProperties(Softs::FSolverReal VolumeStiffness);
		UE_DEPRECATED(5.2, "Use Update() instead.")
		void SetVolumeProperties(Softs::FSolverReal VolumeStiffness);
		UE_DEPRECATED(5.2, "Use Update() instead.")
		void SetLongRangeAttachmentProperties(
			const Softs::FSolverVec2& TetherStiffness,
			const Softs::FSolverVec2& TetherScale,
			Softs::FSolverReal MeshScale = (Softs::FSolverReal)1.);
		UE_DEPRECATED(5.2, "Use Update() instead.")
		void SetMaximumDistanceProperties(Softs::FSolverReal MaxDistancesMultiplier);
		UE_DEPRECATED(5.2, "Use Update() instead.")
		void SetAnimDriveProperties(const Softs::FSolverVec2& AnimDriveStiffness, const Softs::FSolverVec2& AnimDriveDamping);
		UE_DEPRECATED(5.2, "Use Update() instead.")
		void SetSelfCollisionProperties(Softs::FSolverReal SelfCollisionThickness, Softs::FSolverReal SelfCollisionFriction = Softs::FPBDCollisionSpringConstraintsBase::BackCompatFrictionCoefficient, 
			bool bGlobalIntersectionAnalysis = false, bool bContourMinimization = false);
		UE_DEPRECATED(5.2, "Use Update() instead.")
		void SetBackstopProperties(bool bEnabled, Softs::FSolverReal BackstopDistancesMultiplier);
		// ---- End of Cloth interface ----

		// ---- Debug functions ----
		UE_DEPRECATED(5.2, "Use GetEdgeSpringConstraints() instead.")
		const TSharedPtr<Softs::FPBDSpringConstraints>& GetEdgeConstraints() const { return EdgeConstraints_Deprecated; }
		UE_DEPRECATED(5.2, "Use GetXEdgeSpringConstraints() instead.")
		const TSharedPtr<Softs::FXPBDSpringConstraints>& GetXEdgeConstraints() const { return XEdgeConstraints_Deprecated; }
		UE_DEPRECATED(5.2, "Use GetBendingSpringConstraints() instead.")
		const TSharedPtr<Softs::FPBDSpringConstraints>& GetBendingConstraints() const { return BendingConstraints_Deprecated; }
		UE_DEPRECATED(5.2, "Use GetXBendingSpringConstraints() instead.")
		const TSharedPtr<Softs::FXPBDSpringConstraints>& GetXBendingConstraints() const { return XBendingConstraints_Deprecated; }
		UE_DEPRECATED(5.2, "Use GetAreaSpringConstraints() instead.")
		const TSharedPtr<Softs::FPBDAxialSpringConstraints>& GetAreaConstraints() const { return AreaConstraints_Deprecated; }
		UE_DEPRECATED(5.2, "Use GetXAreaSpringConstraints() instead.")
		const TSharedPtr<Softs::FXPBDAxialSpringConstraints>& GetXAreaConstraints() const { return XAreaConstraints_Deprecated; }
		UE_DEPRECATED(5.2, "ThinShellVolume constraints are no longer supported.")
		const TSharedPtr<Softs::FPBDSpringConstraints>& GetThinShellVolumeConstraints() const { return ThinShellVolumeConstraints_Deprecated; }
		UE_DEPRECATED(5.2, "Volume constraints are no longer supported.")
		const TSharedPtr<Softs::FPBDVolumeConstraint>& GetVolumeConstraints() const { return VolumeConstraints_Deprecated; }
		UE_DEPRECATED(5.1,"LongRange XPBDConstraints are no longer supported.")
		const TSharedPtr<Softs::FXPBDLongRangeConstraints>& GetXLongRangeConstraints() const { return XLongRangeConstraints_Deprecated; }

		const TSharedPtr<Softs::FPBDEdgeSpringConstraints>& GetEdgeSpringConstraints() const { return EdgeConstraints; }
		const TSharedPtr<Softs::FXPBDEdgeSpringConstraints>& GetXEdgeSpringConstraints() const { return XEdgeConstraints; }
		const TSharedPtr<Softs::FXPBDStretchBiasElementConstraints>& GetXStretchBiasConstraints() const { return XStretchBiasConstraints; }
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
		const TSharedPtr<Softs::FPBDShapeConstraints>& GetShapeConstraints() const { return ShapeConstraints; }
		const TSharedPtr<Softs::FPBDCollisionSpringConstraints>& GetSelfCollisionConstraints() const { return SelfCollisionConstraints; }
		const TSharedPtr<Softs::FPBDTriangleMeshIntersections>& GetSelfIntersectionConstraints() const { return SelfIntersectionConstraints; }
		const TSharedPtr<Softs::FPBDTriangleMeshCollisions>& GetSelfCollisionInit() const { return SelfCollisionInit; }
		// ---- End of debug functions ----

	private:
		void CreateSelfCollisionConstraints(
			const Softs::FCollectionPropertyConstFacade& ConfigProperties,
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

		//~ Begin deprecated constraints
		TSharedPtr<Softs::FPBDSpringConstraints> EdgeConstraints_Deprecated;
		TSharedPtr<Softs::FXPBDSpringConstraints> XEdgeConstraints_Deprecated;
		TSharedPtr<Softs::FPBDSpringConstraints> BendingConstraints_Deprecated;
		TSharedPtr<Softs::FXPBDSpringConstraints> XBendingConstraints_Deprecated;
		TSharedPtr<Softs::FPBDAxialSpringConstraints> AreaConstraints_Deprecated;
		TSharedPtr<Softs::FXPBDAxialSpringConstraints> XAreaConstraints_Deprecated;
		TSharedPtr<Softs::FPBDSpringConstraints> ThinShellVolumeConstraints_Deprecated;;
		TSharedPtr<Softs::FPBDVolumeConstraint> VolumeConstraints_Deprecated;
		TSharedPtr<Softs::FXPBDLongRangeConstraints> XLongRangeConstraints_Deprecated;
		//~ End deprecated constraints

		TSharedPtr<Softs::FPBDEdgeSpringConstraints> EdgeConstraints;
		TSharedPtr<Softs::FXPBDEdgeSpringConstraints> XEdgeConstraints;
		TSharedPtr<Softs::FXPBDStretchBiasElementConstraints> XStretchBiasConstraints;
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
		TSharedPtr<Softs::FPBDShapeConstraints> ShapeConstraints;
		TSharedPtr<Softs::FPBDTriangleMeshCollisions> SelfCollisionInit;
		TSharedPtr<Softs::FPBDCollisionSpringConstraints> SelfCollisionConstraints;
		TSharedPtr<Softs::FPBDTriangleMeshIntersections> SelfIntersectionConstraints;
		
		Softs::FPBDEvolution* Evolution;
		const TArray<Softs::FSolverVec3>* AnimationPositions;
		const TArray<Softs::FSolverVec3>* OldAnimationPositions_Deprecated;
		const TArray<Softs::FSolverVec3>* AnimationNormals;
		const TArray<Softs::FSolverVec3>* AnimationVelocities;

		int32 ParticleOffset;
		int32 NumParticles;
		int32 ConstraintInitOffset;
		int32 ConstraintRuleOffset;
		int32 PostCollisionConstraintRuleOffset;
		int32 NumConstraintInits;
		int32 NumConstraintRules;
		int32 NumPostCollisionConstraintRules;
	};
} // namespace Chaos
