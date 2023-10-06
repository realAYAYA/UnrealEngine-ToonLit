// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/CollectionPropertyFacade.h"
#include "Chaos/PBDSoftsEvolutionFwd.h"
#include "Chaos/PBDSoftsSolverParticles.h"
#include "Chaos/PBDStiffness.h"
#include "Chaos/ParticleRule.h"
#include "ChaosStats.h"

namespace Chaos
{
class FTriangleMesh;
}

namespace Chaos::Softs
{

class FXPBDStretchBiasElementConstraints
{
public:
	// Stiffness is in kg cm / s^2 for stretch, kg cm^2 / s^2 for Bias
	static constexpr FSolverReal MinStiffness = (FSolverReal)1e-4; // Stiffness below this will be considered 0 since all of our calculations are actually based on 1 / stiffness.
	static constexpr FSolverReal MaxStiffness = (FSolverReal)1e7;
	static constexpr FSolverReal MinDamping = (FSolverReal)0.;
	static constexpr FSolverReal MaxDamping = (FSolverReal)1000.;
	static constexpr bool bDefaultUse3dRestLengths = true;
	static constexpr FSolverReal MinWarpWeftScale = (FSolverReal)0.; 
	static constexpr FSolverReal MaxWarpWeftScale = (FSolverReal)1e7; // No particular reason for this number. Just can't imagine wanting something bigger?
	static constexpr FSolverReal DefaultWarpWeftScale = (FSolverReal)1.;

	static bool IsEnabled(const FCollectionPropertyConstFacade& PropertyCollection)
	{
		return IsXPBDAnisoStretchStiffnessWarpEnabled(PropertyCollection, false);
	}

	CHAOS_API FXPBDStretchBiasElementConstraints(const FSolverParticles& InParticles,
		int32 ParticleOffset,
		int32 ParticleCount,
		const FTriangleMesh& TriangleMesh,
		const TArray<TVec3<FVec2f>>& FaceVertexUVs,
		const TMap<FString, TConstArrayView<FRealSingle>>& WeightMaps,
		const FCollectionPropertyConstFacade& PropertyCollection,
		bool bTrimKinematicConstraints = false);

	CHAOS_API FXPBDStretchBiasElementConstraints(const FSolverParticles& InParticles,
		int32 ParticleOffset,
		int32 ParticleCount,
		const FTriangleMesh& TriangleMesh,
		const TArray<TVec3<FVec2f>>& FaceVertexUVs,
		const TConstArrayView<FRealSingle>& StiffnessWarpMultipliers,
		const TConstArrayView<FRealSingle>& StiffnessWeftMultipliers,
		const TConstArrayView<FRealSingle>& StiffnessBiasMultipliers,
		const TConstArrayView<FRealSingle>& DampingMultipliers,
		const TConstArrayView<FRealSingle>& WarpScaleMultipliers,
		const TConstArrayView<FRealSingle>& WeftScaleMultipliers,
		const FSolverVec2& InStiffnessWarp,
		const FSolverVec2& InStiffnessWeft,
		const FSolverVec2& InStiffnessBias,
		const FSolverVec2& InDampingRatio,
		const FSolverVec2& InWarpScale,
		const FSolverVec2& InWeftScale,
		bool bUse3dRestLengths,
		bool bTrimKinematicConstraints = false);

	virtual ~FXPBDStretchBiasElementConstraints() {}

	void Init()
	{ 
		Lambdas.Reset();
		Lambdas.AddZeroed(Constraints.Num());
	}

	CHAOS_API void SetProperties(
		const FCollectionPropertyConstFacade& PropertyCollection,
		const TMap<FString, TConstArrayView<FRealSingle>>& WeightMaps);

	void SetProperties(const FSolverVec2& InStiffnessWarp, const FSolverVec2& InStiffnessWeft, const FSolverVec2& InStiffnessBias, const FSolverVec2& InDampingRatio, const FSolverVec2& InWarpScale, const FSolverVec2& InWeftScale)
	{
		StiffnessWarp.SetWeightedValue(InStiffnessWarp, MaxStiffness);
		StiffnessWeft.SetWeightedValue(InStiffnessWeft, MaxStiffness);
		StiffnessBias.SetWeightedValue(InStiffnessBias, MaxStiffness);
		DampingRatio.SetWeightedValue(InDampingRatio.ClampAxes(MinDamping, MaxDamping));
		WarpScale.SetWeightedValue(InWarpScale.ClampAxes(MinWarpWeftScale, MaxWarpWeftScale));
		WeftScale.SetWeightedValue(InWeftScale.ClampAxes(MinWarpWeftScale, MaxWarpWeftScale));
	}

	// Update stiffness table, as well as the simulation stiffness exponent
	void ApplyProperties(const FSolverReal /*Dt*/, const int32 /*NumIterations*/)
	{
		StiffnessWarp.ApplyXPBDValues(MaxStiffness);
		StiffnessWeft.ApplyXPBDValues(MaxStiffness);
		StiffnessBias.ApplyXPBDValues(MaxStiffness);
		DampingRatio.ApplyValues();
		WarpScale.ApplyValues();
		WeftScale.ApplyValues();
	}

	CHAOS_API void Apply(FSolverParticles& Particles, const FSolverReal Dt) const;
	CHAOS_API void CalculateUVStretch(const int32 ConstraintIndex, const FSolverVec3& P0, const FSolverVec3& P1, const FSolverVec3& P2, FSolverVec3& DXDu, FSolverVec3& DXDv) const;
	
	const TArray<TVec3<int32>>& GetConstraints() const { return Constraints; }
	const TArray<FSolverVec2> GetRestStretchLengths() const { return RestStretchLengths; }
	FSolverVec2 GetWarpWeftScale(const int32 ConstraintIndex) const 
	{ 
		return FSolverVec2(WarpScale.HasWeightMap() ? WarpScale[ConstraintIndex] : (FSolverReal)WarpScale,
			WeftScale.HasWeightMap() ? WeftScale[ConstraintIndex] : (FSolverReal)WeftScale);
	}
	const TArray<int32>& GetConstraintsPerColorStartIndex() const { return ConstraintsPerColorStartIndex; }

private:
	CHAOS_API void InitConstraintsAndRestData(const FSolverParticles& InParticles, const FTriangleMesh& TriangleMesh,
		const TArray<TVec3<FVec2f>>& FaceVertexUVs, const bool bUse3dRestLengths, const bool bTrimKinematicConstraints);
	CHAOS_API void InitColor(const FSolverParticles& InParticles);
	CHAOS_API void ApplyHelper(FSolverParticles& Particles, const FSolverReal Dt, const int32 ConstraintIndex, const FSolverVec3& ExpStiffnessValue, const FSolverReal DampingRatioValue, const FSolverReal WarpScaleValue, const FSolverReal WeftScaleValue) const;

	TArray<TVec3<int32>> Constraints;
	const int32 ParticleOffset;
	const int32 ParticleCount;
	FPBDStiffness StiffnessWarp;
	FPBDStiffness StiffnessWeft;
	FPBDStiffness StiffnessBias;
	FPBDWeightMap DampingRatio;
	FPBDWeightMap WarpScale;
	FPBDWeightMap WeftScale;
	mutable TArray<FSolverVec3> Lambdas; // separate for stretchU, stretchV, Bias
	TArray<int32> ConstraintsPerColorStartIndex; // Constraints are ordered so each batch is contiguous. This is ColorNum + 1 length so it can be used as start and end.

	TArray<FSolverMatrix22> DeltaUVInverse; // Used to convert from DeltaX to dX/dU and dX/dV
	TArray<FSolverVec2> RestStretchLengths;
	TArray<FSolverVec3> StiffnessScales; // Used to make everything resolution independent.

	UE_CHAOS_DECLARE_PROPERTYCOLLECTION_NAME(XPBDAnisoStretchUse3dRestLengths, bool);
	UE_CHAOS_DECLARE_PROPERTYCOLLECTION_NAME(XPBDAnisoStretchStiffnessWarp, float);
	UE_CHAOS_DECLARE_PROPERTYCOLLECTION_NAME(XPBDAnisoStretchStiffnessWeft, float);
	UE_CHAOS_DECLARE_PROPERTYCOLLECTION_NAME(XPBDAnisoStretchStiffnessBias, float);
	UE_CHAOS_DECLARE_PROPERTYCOLLECTION_NAME(XPBDAnisoStretchDamping, float);
	UE_CHAOS_DECLARE_PROPERTYCOLLECTION_NAME(XPBDAnisoStretchWarpScale, float);
	UE_CHAOS_DECLARE_PROPERTYCOLLECTION_NAME(XPBDAnisoStretchWeftScale, float);
};

}  // End namespace Chaos::Softs

// Support ISPC enable/disable in non-shipping builds
#if !INTEL_ISPC
const bool bChaos_XPBDStretchBiasElement_ISPC_Enabled = false;
#elif UE_BUILD_SHIPPING
const bool bChaos_XPBDStretchBiasElement_ISPC_Enabled = true;
#else
extern CHAOS_API bool bChaos_XPBDStretchBiasElement_ISPC_Enabled;
#endif
