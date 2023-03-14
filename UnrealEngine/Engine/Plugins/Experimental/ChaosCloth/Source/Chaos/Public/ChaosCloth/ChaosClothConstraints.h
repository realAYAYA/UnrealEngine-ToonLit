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
	class FClothConstraints final
	{
	public:
		typedef Softs::FPBDLongRangeConstraints::EMode ETetherMode;

		FClothConstraints();
		~FClothConstraints();

		// ---- Solver interface ----
		void Initialize(
			Softs::FPBDEvolution* InEvolution,
			const TArray<Softs::FSolverVec3>& InAnimationPositions,
			const TArray<Softs::FSolverVec3>& InOldAnimationPositions,
			const TArray<Softs::FSolverVec3>& InAnimationNormals,
			int32 InParticleOffset,
			int32 InNumParticles);
		// ---- End of Solver interface ----

		// ---- Cloth interface ----
		void SetEdgeConstraints(const TArray<TVec3<int32>>& SurfaceElements, const TConstArrayView<FRealSingle>& StiffnessMultipliers, bool bUseXPBDConstraints);
		void SetBendingConstraints(const TArray<TVec2<int32>>& Edges, const TConstArrayView<FRealSingle>& StiffnessMultipliers, bool bUseXPBDConstraints);
		void SetBendingConstraints(TArray<TVec4<int32>>&& BendingElements, const TConstArrayView<FRealSingle>& StiffnessMultipliers, const TConstArrayView<FRealSingle>& BucklingStiffnessMultipliers, bool bUseXPBDConstraints);
		UE_DEPRECATED(5.1, "Use SetBendingConstraints(TArray<TVec4<int32>>&& BendingElements, const TConstArrayView<FRealSingle>& StiffnessMultipliers, const TConstArrayView<FRealSingle>& BucklingStiffnessMultipliers, bool bUseXPBDConstraints) instead.")
		void SetBendingConstraints(TArray<TVec4<int32>>&& BendingElements, Softs::FSolverReal BendingStiffness);
		void SetAreaConstraints(const TArray<TVec3<int32>>& SurfaceElements, const TConstArrayView<FRealSingle>& StiffnessMultipliers, bool bUseXPBDConstraints);
		void SetVolumeConstraints(const TArray<TVec2<int32>>& DoubleBendingEdges, Softs::FSolverReal VolumeStiffness);
		void SetVolumeConstraints(TArray<TVec3<int32>>&& SurfaceElements, Softs::FSolverReal VolumeStiffness);
		void SetLongRangeConstraints(const TArray<TConstArrayView<TTuple<int32, int32, FRealSingle>>>& Tethers,
			const TConstArrayView<FRealSingle>& TetherStiffnessMultipliers, const TConstArrayView<FRealSingle>& TetherScaleMultipliers,
			const Softs::FSolverVec2& TetherScale);
		UE_DEPRECATED(5.1, "LongRange XPBDConstraints are not supported.")
		void SetLongRangeConstraints(const TArray<TConstArrayView<TTuple<int32, int32, FRealSingle>>>& Tethers,
			const TConstArrayView<FRealSingle>& TetherStiffnessMultipliers, const TConstArrayView<FRealSingle>& TetherScaleMultipliers,
			const Softs::FSolverVec2& TetherScale, bool bUseXPBDConstraints) {
			return SetLongRangeConstraints(Tethers, TetherStiffnessMultipliers, TetherScaleMultipliers, TetherScale);
		}
		void SetMaximumDistanceConstraints(const TConstArrayView<FRealSingle>& MaxDistances);
		void SetBackstopConstraints(const TConstArrayView<FRealSingle>& BackstopDistances, const TConstArrayView<FRealSingle>& BackstopRadiuses, bool bUseLegacyBackstop);
		void SetAnimDriveConstraints(const TConstArrayView<FRealSingle>& AnimDriveStiffnessMultipliers, const TConstArrayView<FRealSingle>& AnimDriveDampingMultipliers);
		void SetShapeTargetConstraints(Softs::FSolverReal ShapeTargetStiffness);
		void SetSelfCollisionConstraints(const class FTriangleMesh& TriangleMesh, TSet<TVec2<int32>>&& DisabledCollisionElements, Softs::FSolverReal SelfCollisionThickness,
			Softs::FSolverReal SelfCollisionFriction = Softs::FPBDCollisionSpringConstraintsBase::BackCompatFrictionCoefficient, bool bGlobalIntersectionAnalysis = false,
			bool bContourMinimization = false);

		void CreateRules();
		void Enable(bool bEnable);

		void SetEdgeProperties(const Softs::FSolverVec2& EdgeStiffness);
		void SetBendingProperties(const Softs::FSolverVec2& BendingStiffness, Softs::FSolverReal BucklingRatio = 0.f, const Softs::FSolverVec2& BucklingStiffnessMultiplier = Softs::FSolverVec2::UnitVector);
		void SetAreaProperties(const Softs::FSolverVec2& AreaStiffness);
		void SetThinShellVolumeProperties(Softs::FSolverReal VolumeStiffness);
		void SetVolumeProperties(Softs::FSolverReal VolumeStiffness);
		void SetLongRangeAttachmentProperties(const Softs::FSolverVec2& TetherStiffness, const Softs::FSolverVec2& TetherScale);
		void SetMaximumDistanceProperties(Softs::FSolverReal MaxDistancesMultiplier);
		void SetAnimDriveProperties(const Softs::FSolverVec2& AnimDriveStiffness, const Softs::FSolverVec2& AnimDriveDamping);
		void SetSelfCollisionProperties(Softs::FSolverReal SelfCollisionThickness, Softs::FSolverReal SelfCollisionFriction = Softs::FPBDCollisionSpringConstraintsBase::BackCompatFrictionCoefficient, 
			bool bGlobalIntersectionAnalysis = false, bool bContourMinimization = false);
		UE_DEPRECATED(5.0, "Use SetBackstopProperties(bool, FSolverReal) instead.")
		void SetBackstopProperties(bool bEnabled) { SetBackstopProperties(bEnabled, (Softs::FSolverReal)1.); }
		void SetBackstopProperties(bool bEnabled, Softs::FSolverReal BackstopDistancesMultiplier);
		// ---- End of Cloth interface ----

		// ---- Debug functions ----
		const TSharedPtr<Softs::FPBDSpringConstraints> GetEdgeConstraints() const { return EdgeConstraints; }
		const TSharedPtr<Softs::FXPBDSpringConstraints>& GetXEdgeConstraints() const { return XEdgeConstraints; }
		const TSharedPtr<Softs::FPBDSpringConstraints>& GetBendingConstraints() const { return BendingConstraints; }  
		const TSharedPtr<Softs::FXPBDSpringConstraints>& GetXBendingConstraints() const { return XBendingConstraints; }
		const TSharedPtr<Softs::FPBDBendingConstraints>& GetBendingElementConstraints() const { return BendingElementConstraints; }
		const TSharedPtr<Softs::FXPBDBendingConstraints>& GetXBendingElementConstraints() const { return XBendingElementConstraints; }
		const TSharedPtr<Softs::FPBDAxialSpringConstraints>& GetAreaConstraints() const { return AreaConstraints; }
		const TSharedPtr<Softs::FXPBDAxialSpringConstraints>& GetXAreaConstraints() const { return XAreaConstraints; }
		const TSharedPtr<Softs::FPBDSpringConstraints>& GetThinShellVolumeConstraints() const { return ThinShellVolumeConstraints; }
		const TSharedPtr<Softs::FPBDVolumeConstraint>& GetVolumeConstraints() const { return VolumeConstraints; }
		const TSharedPtr<Softs::FPBDLongRangeConstraints>& GetLongRangeConstraints() const { return LongRangeConstraints; }
		UE_DEPRECATED(5.1,"LongRange XPBDConstraints are not supported.")
		const TSharedPtr<Softs::FXPBDLongRangeConstraints>& GetXLongRangeConstraints() const 
		{
			PRAGMA_DISABLE_DEPRECATION_WARNINGS
			return XLongRangeConstraints; 
			PRAGMA_ENABLE_DEPRECATION_WARNINGS
		}
		const TSharedPtr<Softs::FPBDSphericalConstraint>& GetMaximumDistanceConstraints() const { return MaximumDistanceConstraints; }
		const TSharedPtr<Softs::FPBDSphericalBackstopConstraint>& GetBackstopConstraints() const { return BackstopConstraints; }
		const TSharedPtr<Softs::FPBDAnimDriveConstraint>& GetAnimDriveConstraints() const { return AnimDriveConstraints; }
		const TSharedPtr<Softs::FPBDShapeConstraints>& GetShapeConstraints() const { return ShapeConstraints; }
		const TSharedPtr<Softs::FPBDCollisionSpringConstraints>& GetSelfCollisionConstraints() const { return SelfCollisionConstraints; }
		const TSharedPtr<Softs::FPBDTriangleMeshIntersections>& GetSelfIntersectionConstraints() const { return SelfIntersectionConstraints; }
		const TSharedPtr<Softs::FPBDTriangleMeshCollisions>& GetSelfCollisionInit() const { return SelfCollisionInit; }
		// ---- End of debug functions ----

	private:
		TSharedPtr<Softs::FPBDSpringConstraints> EdgeConstraints;
		TSharedPtr<Softs::FXPBDSpringConstraints> XEdgeConstraints;
		TSharedPtr<Softs::FPBDSpringConstraints> BendingConstraints;
		TSharedPtr<Softs::FXPBDSpringConstraints> XBendingConstraints;
		TSharedPtr<Softs::FPBDBendingConstraints> BendingElementConstraints;
		TSharedPtr<Softs::FXPBDBendingConstraints> XBendingElementConstraints;
		TSharedPtr<Softs::FPBDAxialSpringConstraints> AreaConstraints;
		TSharedPtr<Softs::FXPBDAxialSpringConstraints> XAreaConstraints;
		TSharedPtr<Softs::FPBDSpringConstraints> ThinShellVolumeConstraints;
		TSharedPtr<Softs::FPBDVolumeConstraint> VolumeConstraints;
		TSharedPtr<Softs::FPBDLongRangeConstraints> LongRangeConstraints; 
		UE_DEPRECATED(5.1, "LongRange XPBDConstraints are not supported.")
		TSharedPtr<Softs::FXPBDLongRangeConstraints> XLongRangeConstraints;
		TSharedPtr<Softs::FPBDSphericalConstraint> MaximumDistanceConstraints;
		TSharedPtr<Softs::FPBDSphericalBackstopConstraint> BackstopConstraints;
		TSharedPtr<Softs::FPBDAnimDriveConstraint> AnimDriveConstraints;
		TSharedPtr<Softs::FPBDShapeConstraints> ShapeConstraints;
		TSharedPtr<Softs::FPBDTriangleMeshCollisions> SelfCollisionInit;
		TSharedPtr<Softs::FPBDCollisionSpringConstraints> SelfCollisionConstraints;
		TSharedPtr<Softs::FPBDTriangleMeshIntersections> SelfIntersectionConstraints;
		
		Softs::FPBDEvolution* Evolution;
		const TArray<Softs::FSolverVec3>* AnimationPositions;
		const TArray<Softs::FSolverVec3>* OldAnimationPositions;
		const TArray<Softs::FSolverVec3>* AnimationNormals;

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
