// Copyright Epic Games, Inc. All Rights Reserved.
#include "Chaos/XPBDAnisotropicBendingConstraints.h"
#include "Chaos/XPBDBendingConstraints.h"
#include "Chaos/GraphColoring.h"
#include "Chaos/Framework/Parallel.h"
#include "Chaos/TriangleMesh.h"
#include "ChaosStats.h"

#if INTEL_ISPC
#include "XPBDAnisotropicBendingConstraints.ispc.generated.h"
#endif

DECLARE_CYCLE_STAT(TEXT("Chaos XPBD Anisotropic Bending Constraint"), STAT_XPBD_AnisoBending, STATGROUP_Chaos);

#if INTEL_ISPC && !UE_BUILD_SHIPPING
static_assert(sizeof(ispc::FVector4f) == sizeof(Chaos::Softs::FPAndInvM), "sizeof(ispc::FVector4f) != sizeof(Chaos::Softs::FPAndInvM");
static_assert(sizeof(ispc::FIntVector4) == sizeof(Chaos::TVec4<int32>), "sizeof(ispc::FIntVector4) != sizeof(Chaos::TVec4<int32>");
#endif

namespace Chaos::Softs {

// @todo(chaos): the parallel threshold (or decision to run parallel) should probably be owned by the solver and passed to the constraint container
extern int32 Chaos_XPBDBending_ParallelConstraintCount;

FXPBDAnisotropicBendingConstraints::FXPBDAnisotropicBendingConstraints(const FSolverParticles& InParticles,
	int32 InParticleOffset,
	int32 InParticleCount,
	const FTriangleMesh& TriangleMesh,
	const TArray<TVec3<FVec2f>>& FaceVertexPatternPositions,
	const TMap<FString, TConstArrayView<FRealSingle>>& WeightMaps,
	const FCollectionPropertyConstFacade& PropertyCollection,
	bool bTrimKinematicConstraints)
	: Base(
		InParticles,
		InParticleOffset,
		InParticleCount,
		TriangleMesh.GetUniqueAdjacentElements(),
		WeightMaps.FindRef(GetXPBDAnisoBendingStiffnessWarpString(PropertyCollection, XPBDAnisoBendingStiffnessWarpName.ToString())),
		WeightMaps.FindRef(GetXPBDAnisoBucklingStiffnessWarpString(PropertyCollection, XPBDAnisoBucklingStiffnessWarpName.ToString())),
		GetRestAngleMapFromCollection(WeightMaps, PropertyCollection),
		FSolverVec2(GetWeightedFloatXPBDAnisoBendingStiffnessWarp(PropertyCollection, MaxStiffness)),
		(FSolverReal)GetXPBDAnisoBucklingRatio(PropertyCollection, 0.f),
		FSolverVec2(GetWeightedFloatXPBDAnisoBucklingStiffnessWarp(PropertyCollection, MaxStiffness)),
		GetRestAngleValueFromCollection(PropertyCollection),
		(ERestAngleConstructionType)GetXPBDAnisoRestAngleType(PropertyCollection, (int32)ERestAngleConstructionType::Use3DRestAngles),
		bTrimKinematicConstraints,
		MaxStiffness)
	, StiffnessWeft(
		FSolverVec2(GetWeightedFloatXPBDAnisoBendingStiffnessWeft(PropertyCollection, MaxStiffness)),
		WeightMaps.FindRef(GetXPBDAnisoBendingStiffnessWeftString(PropertyCollection, XPBDAnisoBendingStiffnessWeftName.ToString())),
		TConstArrayView<TVec2<int32>>(ConstraintSharedEdges),
		ParticleOffset,
		ParticleCount,
		FPBDStiffness::DefaultTableSize,
		FPBDStiffness::DefaultParameterFitBase,
		MaxStiffness)
	, StiffnessBias(
		FSolverVec2(GetWeightedFloatXPBDAnisoBendingStiffnessBias(PropertyCollection, MaxStiffness)),
		WeightMaps.FindRef(GetXPBDAnisoBendingStiffnessBiasString(PropertyCollection, XPBDAnisoBendingStiffnessBiasName.ToString())),
		TConstArrayView<TVec2<int32>>(ConstraintSharedEdges),
		ParticleOffset,
		ParticleCount,
		FPBDStiffness::DefaultTableSize,
		FPBDStiffness::DefaultParameterFitBase,
		MaxStiffness)
	, BucklingStiffnessWeft(
		FSolverVec2(GetWeightedFloatXPBDAnisoBucklingStiffnessWeft(PropertyCollection, MaxStiffness)),
		WeightMaps.FindRef(GetXPBDAnisoBucklingStiffnessWeftString(PropertyCollection, XPBDAnisoBucklingStiffnessWeftName.ToString())),
		TConstArrayView<TVec2<int32>>(ConstraintSharedEdges),
		ParticleOffset,
		ParticleCount,
		FPBDStiffness::DefaultTableSize,
		FPBDStiffness::DefaultParameterFitBase,
		MaxStiffness)
	, BucklingStiffnessBias(
		FSolverVec2(GetWeightedFloatXPBDAnisoBucklingStiffnessBias(PropertyCollection, MaxStiffness)),
		WeightMaps.FindRef(GetXPBDAnisoBucklingStiffnessBiasString(PropertyCollection, XPBDAnisoBucklingStiffnessBiasName.ToString())),
		TConstArrayView<TVec2<int32>>(ConstraintSharedEdges),
		ParticleOffset,
		ParticleCount,
		FPBDStiffness::DefaultTableSize,
		FPBDStiffness::DefaultParameterFitBase,
		MaxStiffness)
	, DampingRatio(
		FSolverVec2(GetWeightedFloatXPBDAnisoBendingDamping(PropertyCollection, MinDamping)).ClampAxes(MinDamping, MaxDamping),
		WeightMaps.FindRef(GetXPBDAnisoBendingDampingString(PropertyCollection, XPBDAnisoBendingDampingName.ToString())),
		TConstArrayView<TVec2<int32>>(ConstraintSharedEdges),
		ParticleOffset,
		ParticleCount)
	, WarpWeftBiasBaseMultipliers(GenerateWarpWeftBiasBaseMultipliers(FaceVertexPatternPositions, TriangleMesh))
	, XPBDAnisoBendingStiffnessWarpIndex(PropertyCollection)
	, XPBDAnisoBendingStiffnessWeftIndex(PropertyCollection)
	, XPBDAnisoBendingStiffnessBiasIndex(PropertyCollection)
	, XPBDAnisoBendingDampingIndex(PropertyCollection)
	, XPBDAnisoBucklingRatioIndex(PropertyCollection)
	, XPBDAnisoBucklingStiffnessWarpIndex(PropertyCollection)
	, XPBDAnisoBucklingStiffnessWeftIndex(PropertyCollection)
	, XPBDAnisoBucklingStiffnessBiasIndex(PropertyCollection)
	, XPBDAnisoFlatnessRatioIndex(PropertyCollection)
	, XPBDAnisoRestAngleIndex(PropertyCollection)
	, XPBDAnisoRestAngleTypeIndex(PropertyCollection)
{
	Lambdas.Init((FSolverReal)0., Constraints.Num());
	InitColor(InParticles);
}

FXPBDAnisotropicBendingConstraints::FXPBDAnisotropicBendingConstraints(const FSolverParticles& InParticles,
	int32 ParticleOffset,
	int32 ParticleCount,
	const FTriangleMesh& TriangleMesh,
	const TArray<TVec3<FVec2f>>& FaceVertexPatternPositions,
	const TConstArrayView<FRealSingle>& StiffnessWarpMultipliers,
	const TConstArrayView<FRealSingle>& StiffnessWeftMultipliers,
	const TConstArrayView<FRealSingle>& StiffnessBiasMultipliers,
	const TConstArrayView<FRealSingle>& BucklingStiffnessWarpMultipliers,
	const TConstArrayView<FRealSingle>& BucklingStiffnessWeftMultipliers,
	const TConstArrayView<FRealSingle>& BucklingStiffnessBiasMultipliers,
	const TConstArrayView<FRealSingle>& DampingMultipliers,
	const FSolverVec2& InStiffnessWarp,
	const FSolverVec2& InStiffnessWeft,
	const FSolverVec2& InStiffnessBias,
	const FSolverReal InBucklingRatio,
	const FSolverVec2& InBucklingStiffnessWarp,
	const FSolverVec2& InBucklingStiffnessWeft,
	const FSolverVec2& InBucklingStiffnessBias,
	const FSolverVec2& InDampingRatio,
	bool bTrimKinematicConstraints)
	: Base(
		InParticles,
		ParticleOffset,
		ParticleCount,
		TriangleMesh.GetUniqueAdjacentElements(),
		StiffnessWarpMultipliers,
		BucklingStiffnessWarpMultipliers,
		InStiffnessWarp,
		InBucklingRatio,
		InBucklingStiffnessWarp,
		bTrimKinematicConstraints,
		MaxStiffness)
	, StiffnessWeft(
		InStiffnessWeft,
		StiffnessWeftMultipliers,
		TConstArrayView<TVec2<int32>>(ConstraintSharedEdges),
		ParticleOffset,
		ParticleCount,
		FPBDStiffness::DefaultTableSize,
		FPBDStiffness::DefaultParameterFitBase,
		MaxStiffness)
	, StiffnessBias(
		InStiffnessBias,
		StiffnessBiasMultipliers,
		TConstArrayView<TVec2<int32>>(ConstraintSharedEdges),
		ParticleOffset,
		ParticleCount,
		FPBDStiffness::DefaultTableSize,
		FPBDStiffness::DefaultParameterFitBase,
		MaxStiffness)
	, BucklingStiffnessWeft(
		InBucklingStiffnessWeft,
		BucklingStiffnessWeftMultipliers,
		TConstArrayView<TVec2<int32>>(ConstraintSharedEdges),
		ParticleOffset,
		ParticleCount,
		FPBDStiffness::DefaultTableSize,
		FPBDStiffness::DefaultParameterFitBase,
		MaxStiffness)
	, BucklingStiffnessBias(
		InBucklingStiffnessBias,
		BucklingStiffnessBiasMultipliers,
		TConstArrayView<TVec2<int32>>(ConstraintSharedEdges),
		ParticleOffset,
		ParticleCount,
		FPBDStiffness::DefaultTableSize,
		FPBDStiffness::DefaultParameterFitBase,
		MaxStiffness)
	, DampingRatio(
		InDampingRatio.ClampAxes((FSolverReal)0., (FSolverReal)1.),
		DampingMultipliers,
		TConstArrayView<TVec2<int32>>(ConstraintSharedEdges),
		ParticleOffset,
		ParticleCount)
	, WarpWeftBiasBaseMultipliers(GenerateWarpWeftBiasBaseMultipliers(FaceVertexPatternPositions, TriangleMesh))
	, XPBDAnisoBendingStiffnessWarpIndex(ForceInit)
	, XPBDAnisoBendingStiffnessWeftIndex(ForceInit)
	, XPBDAnisoBendingStiffnessBiasIndex(ForceInit)
	, XPBDAnisoBendingDampingIndex(ForceInit)
	, XPBDAnisoBucklingRatioIndex(ForceInit)
	, XPBDAnisoBucklingStiffnessWarpIndex(ForceInit)
	, XPBDAnisoBucklingStiffnessWeftIndex(ForceInit)
	, XPBDAnisoBucklingStiffnessBiasIndex(ForceInit)
	, XPBDAnisoFlatnessRatioIndex(ForceInit)
	, XPBDAnisoRestAngleIndex(ForceInit)
	, XPBDAnisoRestAngleTypeIndex(ForceInit)
{
	Lambdas.Init((FSolverReal)0., Constraints.Num());
	InitColor(InParticles);
}

TArray<FSolverVec3> FXPBDAnisotropicBendingConstraints::GenerateWarpWeftBiasBaseMultipliers(const TArray<TVec3<FVec2f>>& FaceVertexPatternPositions, const FTriangleMesh& TriangleMesh) const
{
	// This is easier to do by faces. Average the multipliers for faces that share the same edge.
	TMap<TVec2<int32> /*Edge*/, TArray<FSolverVec3> /*BaseMultipliers*/> EdgeToMultiplierMap;
	auto SortedEdge = [](int32 P0, int32 P1) { return P0 <= P1 ? TVec2<int32>(P0, P1) : TVec2<int32>(P1, P0); };
	auto Multiplier = [](const FVec2f& UV0, const FVec2f& UV1)
	{
		// Input UVs: X = Weft, Y = Warp direction
		// Transform to X = Weft, Y = Warp, Z = Bias
		// NOTE: Weft-dominant bend is along a vertical edge (i.e., larger V(Y) diff)). Warp-dominant bend is along a horizontal edge.
		const FSolverVec2 UVDiff = UV1 - UV0;
		const FSolverVec2 UVDiffAbs = UVDiff.GetAbs();
		FSolverVec3 UVDiffTransformed;
		if (UVDiffAbs.X > UVDiffAbs.Y)
		{
			// Stiffness is blend between warp and bias
			UVDiffTransformed  = FSolverVec3((FSolverReal)0.f, UVDiffAbs.X - UVDiffAbs.Y, UVDiffAbs.Y);
		}
		else
		{
			// Stiffness is blend between weft and bias
			UVDiffTransformed = FSolverVec3(UVDiffAbs.Y - UVDiffAbs.X, (FSolverReal)0.f, UVDiffAbs.X);
		}
		const FSolverReal Denom = UVDiffTransformed.X + UVDiffTransformed.Y + UVDiffTransformed.Z;
		return Denom > UE_SMALL_NUMBER ? UVDiffTransformed / Denom : FSolverVec3(0.f, 1.f, 0.f); // Default to Warp if zero length
	};

	const TArray<TVec3<int32>>& Elements = TriangleMesh.GetElements();
	for (int32 ElemIdx = 0; ElemIdx < Elements.Num(); ++ElemIdx)
	{
		const TVec3<int32>& Element = Elements[ElemIdx];
		const TVec3<FVec2f>& UVs = FaceVertexPatternPositions[ElemIdx];
		EdgeToMultiplierMap.FindOrAdd(SortedEdge(Element[0], Element[1])).Add(Multiplier(UVs[0], UVs[1]));
		EdgeToMultiplierMap.FindOrAdd(SortedEdge(Element[1], Element[2])).Add(Multiplier(UVs[1], UVs[2]));
		EdgeToMultiplierMap.FindOrAdd(SortedEdge(Element[2], Element[0])).Add(Multiplier(UVs[2], UVs[0]));
	}

	TArray<FSolverVec3> WarpWeftBaseMultiplierResult;
	WarpWeftBaseMultiplierResult.SetNumUninitialized(Constraints.Num());
	for (int32 ConstraintIdx = 0; ConstraintIdx < Constraints.Num(); ++ConstraintIdx)
	{
		const TArray<FSolverVec3>& EdgeMultipliers = EdgeToMultiplierMap.FindChecked(SortedEdge(ConstraintSharedEdges[ConstraintIdx][0], ConstraintSharedEdges[ConstraintIdx][1]));
		FSolverVec3 AveragedEdgeMultiplier((FSolverReal)0.);
		if (ensure(EdgeMultipliers.Num()))
		{
			for (const FSolverVec3& EdgeMult : EdgeMultipliers)
			{
				AveragedEdgeMultiplier += EdgeMult;
			}
			AveragedEdgeMultiplier /= (FSolverReal)EdgeMultipliers.Num();
		}
		WarpWeftBaseMultiplierResult[ConstraintIdx] = AveragedEdgeMultiplier;
	}
	return WarpWeftBaseMultiplierResult;
}

void FXPBDAnisotropicBendingConstraints::InitColor(const FSolverParticles& InParticles)
{
	// In dev builds we always color so we can tune the system without restarting. See Apply()
#if UE_BUILD_SHIPPING || UE_BUILD_TEST
	if (Constraints.Num() > Chaos_XPBDBending_ParallelConstraintCount)
#endif
	{
		const TArray<TArray<int32>> ConstraintsPerColor = FGraphColoring::ComputeGraphColoring(Constraints, InParticles, ParticleOffset, ParticleOffset + ParticleCount);

		// Reorder constraints based on color so each array in ConstraintsPerColor contains contiguous elements.
		TArray<TVec4<int32>> ReorderedConstraints; 
		TArray<TVec2<int32>> ReorderedConstraintSharedEdges;
		TArray<FSolverReal> ReorderedRestAngles;
		TArray<FSolverVec3> ReorderedWarpWeftBiasBaseMultipliers;
		TArray<int32> OrigToReorderedIndices; // used to reorder stiffness indices
		ReorderedConstraints.SetNumUninitialized(Constraints.Num());
		ReorderedConstraintSharedEdges.SetNumUninitialized(Constraints.Num());
		ReorderedRestAngles.SetNumUninitialized(Constraints.Num());
		ReorderedWarpWeftBiasBaseMultipliers.SetNumUninitialized(Constraints.Num());
		OrigToReorderedIndices.SetNumUninitialized(Constraints.Num());

		ConstraintsPerColorStartIndex.Reset(ConstraintsPerColor.Num() + 1);

		int32 ReorderedIndex = 0;
		for (const TArray<int32>& ConstraintsBatch : ConstraintsPerColor)
		{
			ConstraintsPerColorStartIndex.Add(ReorderedIndex);
			for (const int32& BatchConstraint : ConstraintsBatch)
			{
				const int32 OrigIndex = BatchConstraint;
				ReorderedConstraints[ReorderedIndex] = Constraints[OrigIndex];
				ReorderedConstraintSharedEdges[ReorderedIndex] = ConstraintSharedEdges[OrigIndex];
				ReorderedRestAngles[ReorderedIndex] = RestAngles[OrigIndex];
				ReorderedWarpWeftBiasBaseMultipliers[ReorderedIndex] = WarpWeftBiasBaseMultipliers[OrigIndex];
				OrigToReorderedIndices[OrigIndex] = ReorderedIndex;
				++ReorderedIndex;
			}
		}
		ConstraintsPerColorStartIndex.Add(ReorderedIndex);

		Constraints = MoveTemp(ReorderedConstraints);
		ConstraintSharedEdges = MoveTemp(ReorderedConstraintSharedEdges);
		RestAngles = MoveTemp(ReorderedRestAngles);
		WarpWeftBiasBaseMultipliers = MoveTemp(ReorderedWarpWeftBiasBaseMultipliers);
		Stiffness.ReorderIndices(OrigToReorderedIndices);
		StiffnessWeft.ReorderIndices(OrigToReorderedIndices);
		StiffnessBias.ReorderIndices(OrigToReorderedIndices);
		BucklingStiffness.ReorderIndices(OrigToReorderedIndices);
		BucklingStiffnessWeft.ReorderIndices(OrigToReorderedIndices);
		BucklingStiffnessBias.ReorderIndices(OrigToReorderedIndices);
		DampingRatio.ReorderIndices(OrigToReorderedIndices);
	}
}

void FXPBDAnisotropicBendingConstraints::SetProperties(
	const FCollectionPropertyConstFacade& PropertyCollection,
	const TMap<FString, TConstArrayView<FRealSingle>>& WeightMaps)
{
	if (IsXPBDAnisoBendingStiffnessWarpMutable(PropertyCollection))
	{
		const FSolverVec2 WeightedValue(GetWeightedFloatXPBDAnisoBendingStiffnessWarp(PropertyCollection));
		if (IsXPBDAnisoBendingStiffnessWarpStringDirty(PropertyCollection))
		{
			const FString& WeightMapName = GetXPBDAnisoBendingStiffnessWarpString(PropertyCollection);
			Stiffness = FPBDStiffness(
				WeightedValue,
				WeightMaps.FindRef(WeightMapName),
				TConstArrayView<TVec2<int32>>(ConstraintSharedEdges),
				ParticleOffset,
				ParticleCount,
				FPBDStiffness::DefaultTableSize,
				FPBDStiffness::DefaultParameterFitBase,
				MaxStiffness);
		}
		else
		{
			Stiffness.SetWeightedValue(WeightedValue, MaxStiffness);
		}
	}
	if (IsXPBDAnisoBendingStiffnessWeftMutable(PropertyCollection))
	{
		const FSolverVec2 WeightedValue(GetWeightedFloatXPBDAnisoBendingStiffnessWeft(PropertyCollection));
		if (IsXPBDAnisoBendingStiffnessWeftStringDirty(PropertyCollection))
		{
			const FString& WeightMapName = GetXPBDAnisoBendingStiffnessWeftString(PropertyCollection);
			StiffnessWeft = FPBDStiffness(
				WeightedValue,
				WeightMaps.FindRef(WeightMapName),
				TConstArrayView<TVec2<int32>>(ConstraintSharedEdges),
				ParticleOffset,
				ParticleCount,
				FPBDStiffness::DefaultTableSize,
				FPBDStiffness::DefaultParameterFitBase,
				MaxStiffness);
		}
		else
		{
			StiffnessWeft.SetWeightedValue(WeightedValue, MaxStiffness);
		}
	}
	if (IsXPBDAnisoBendingStiffnessBiasMutable(PropertyCollection))
	{
		const FSolverVec2 WeightedValue(GetWeightedFloatXPBDAnisoBendingStiffnessBias(PropertyCollection));
		if (IsXPBDAnisoBendingStiffnessBiasStringDirty(PropertyCollection))
		{
			const FString& WeightMapName = GetXPBDAnisoBendingStiffnessBiasString(PropertyCollection);
			StiffnessBias = FPBDStiffness(
				WeightedValue,
				WeightMaps.FindRef(WeightMapName),
				TConstArrayView<TVec2<int32>>(ConstraintSharedEdges),
				ParticleOffset,
				ParticleCount,
				FPBDStiffness::DefaultTableSize,
				FPBDStiffness::DefaultParameterFitBase,
				MaxStiffness);
		}
		else
		{
			StiffnessBias.SetWeightedValue(WeightedValue, MaxStiffness);
		}
	}
	if (IsXPBDAnisoBucklingRatioMutable(PropertyCollection))
	{
		BucklingRatio = FMath::Clamp(GetXPBDAnisoBucklingRatio(PropertyCollection), (FSolverReal)0., (FSolverReal)1.);
	}
	if (IsXPBDAnisoBucklingStiffnessWarpMutable(PropertyCollection))
	{
		const FSolverVec2 WeightedValue(GetWeightedFloatXPBDAnisoBucklingStiffnessWarp(PropertyCollection));
		if (IsXPBDAnisoBucklingStiffnessWarpStringDirty(PropertyCollection))
		{
			const FString& WeightMapName = GetXPBDAnisoBucklingStiffnessWarpString(PropertyCollection);
			BucklingStiffness = FPBDStiffness(
				WeightedValue,
				WeightMaps.FindRef(WeightMapName),
				TConstArrayView<TVec2<int32>>(ConstraintSharedEdges),
				ParticleOffset,
				ParticleCount,
				FPBDStiffness::DefaultTableSize,
				FPBDStiffness::DefaultParameterFitBase,
				MaxStiffness);
		}
		else
		{
			BucklingStiffness.SetWeightedValue(WeightedValue, MaxStiffness);
		}
	}
	if (IsXPBDAnisoBucklingStiffnessWeftMutable(PropertyCollection))
	{
		const FSolverVec2 WeightedValue(GetWeightedFloatXPBDAnisoBucklingStiffnessWeft(PropertyCollection));
		if (IsXPBDAnisoBucklingStiffnessWeftStringDirty(PropertyCollection))
		{
			const FString& WeightMapName = GetXPBDAnisoBucklingStiffnessWeftString(PropertyCollection);
			BucklingStiffnessWeft = FPBDStiffness(
				WeightedValue,
				WeightMaps.FindRef(WeightMapName),
				TConstArrayView<TVec2<int32>>(ConstraintSharedEdges),
				ParticleOffset,
				ParticleCount,
				FPBDStiffness::DefaultTableSize,
				FPBDStiffness::DefaultParameterFitBase,
				MaxStiffness);
		}
		else
		{
			BucklingStiffnessWeft.SetWeightedValue(WeightedValue, MaxStiffness);
		}
	}
	if (IsXPBDAnisoBucklingStiffnessBiasMutable(PropertyCollection))
	{
		const FSolverVec2 WeightedValue(GetWeightedFloatXPBDAnisoBucklingStiffnessBias(PropertyCollection));
		if (IsXPBDAnisoBucklingStiffnessBiasStringDirty(PropertyCollection))
		{
			const FString& WeightMapName = GetXPBDAnisoBucklingStiffnessBiasString(PropertyCollection);
			BucklingStiffnessBias = FPBDStiffness(
				WeightedValue,
				WeightMaps.FindRef(WeightMapName),
				TConstArrayView<TVec2<int32>>(ConstraintSharedEdges),
				ParticleOffset,
				ParticleCount,
				FPBDStiffness::DefaultTableSize,
				FPBDStiffness::DefaultParameterFitBase,
				MaxStiffness);
		}
		else
		{
			BucklingStiffnessBias.SetWeightedValue(WeightedValue, MaxStiffness);
		}
	}
	if (IsXPBDAnisoBendingDampingMutable(PropertyCollection))
	{
		const FSolverVec2 WeightedValue = FSolverVec2(GetXPBDAnisoBendingDamping(PropertyCollection)).ClampAxes(MinDamping, MaxDamping);
		if (IsXPBDAnisoBendingDampingStringDirty(PropertyCollection))
		{
			const FString& WeightMapName = GetXPBDAnisoBendingDampingString(PropertyCollection);
			DampingRatio = FPBDWeightMap(
				WeightedValue,
				WeightMaps.FindRef(WeightMapName),
				TConstArrayView<TVec2<int32>>(ConstraintSharedEdges),
				ParticleOffset,
				ParticleCount);
		}
		else
		{
			DampingRatio.SetWeightedValue(WeightedValue);
		}
	}
}

void FXPBDAnisotropicBendingConstraints::ApplyHelper(FSolverParticles& Particles, const FSolverReal Dt, const int32 ConstraintIndex, const FSolverVec3& ExpStiffnessValues, 
	const FSolverVec3& ExpBucklingStiffnessValues, const FSolverReal DampingRatioValue) const
{
	const TVec4<int32>& Constraint = Constraints[ConstraintIndex];
	const int32 i1 = Constraint[0];
	const int32 i2 = Constraint[1];
	const int32 i3 = Constraint[2];
	const int32 i4 = Constraint[3];
	const FSolverVec3& WarpWeftBiasBaseMultiplier = WarpWeftBiasBaseMultipliers[ConstraintIndex];

	const FSolverReal BiphasicStiffnessValue = IsBuckled[ConstraintIndex] ?
		WarpWeftBiasBaseMultiplier.Dot(ExpStiffnessValues) : WarpWeftBiasBaseMultiplier.Dot(ExpBucklingStiffnessValues);

	if (BiphasicStiffnessValue < MinStiffness || (Particles.InvM(i1) == (FSolverReal)0. && Particles.InvM(i2) == (FSolverReal)0. && Particles.InvM(i3) == (FSolverReal)0. && Particles.InvM(i4) == (FSolverReal)0.))
	{
		return;
	}

	const FSolverReal CombinedInvMass = Particles.InvM(i1) + Particles.InvM(i2) + Particles.InvM(i3) + Particles.InvM(i4);
	const FSolverReal Damping = (FSolverReal)2.f * DampingRatioValue * FMath::Sqrt(BiphasicStiffnessValue / CombinedInvMass);

	const FSolverReal Angle = CalcAngle(Particles.P(i1), Particles.P(i2), Particles.P(i3), Particles.P(i4));
	const TStaticArray<FSolverVec3, 4> Grads = Base::GetGradients(Particles, ConstraintIndex);

	const FSolverVec3 V1TimesDt = Particles.P(i1) - Particles.X(i1);
	const FSolverVec3 V2TimesDt = Particles.P(i2) - Particles.X(i2);
	const FSolverVec3 V3TimesDt = Particles.P(i3) - Particles.X(i3);
	const FSolverVec3 V4TimesDt = Particles.P(i4) - Particles.X(i4);

	FSolverReal& Lambda = Lambdas[ConstraintIndex];
	const FSolverReal Alpha = (FSolverReal)1.f / (BiphasicStiffnessValue * Dt * Dt);
	const FSolverReal Gamma = Alpha * Damping * Dt;

	const FSolverReal DampingTerm = Gamma * (FSolverVec3::DotProduct(V1TimesDt, Grads[0]) + FSolverVec3::DotProduct(V2TimesDt, Grads[1]) + FSolverVec3::DotProduct(V3TimesDt, Grads[2]) + FSolverVec3::DotProduct(V4TimesDt, Grads[3]));

	const FSolverReal Denom = ((FSolverReal)1.f + Gamma) * (Particles.InvM(i1) * Grads[0].SizeSquared() + Particles.InvM(i2) * Grads[1].SizeSquared() + Particles.InvM(i3) * Grads[2].SizeSquared() + Particles.InvM(i4) * Grads[3].SizeSquared()) + Alpha;
	const FSolverReal DLambda = (Angle - RestAngles[ConstraintIndex] - Alpha * Lambda + DampingTerm) / Denom;

	Particles.P(i1) -= DLambda * Particles.InvM(i1) * Grads[0];
	Particles.P(i2) -= DLambda * Particles.InvM(i2) * Grads[1];
	Particles.P(i3) -= DLambda * Particles.InvM(i3) * Grads[2];
	Particles.P(i4) -= DLambda * Particles.InvM(i4) * Grads[3];
	Lambda += DLambda;
}

void FXPBDAnisotropicBendingConstraints::Apply(FSolverParticles& Particles, const FSolverReal Dt) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FXPBDAnisotropicBendingConstraints_Apply);
	SCOPE_CYCLE_COUNTER(STAT_XPBD_AnisoBending);

	const bool StiffnessHasWeightMap = Stiffness.HasWeightMap();
	const bool StiffnessWeftHasWeightMap = StiffnessWeft.HasWeightMap();
	const bool StiffnessBiasHasWeightMap = StiffnessBias.HasWeightMap();
	const bool BucklingStiffnessHasWeightMap = BucklingStiffness.HasWeightMap();
	const bool BucklingStiffnessWeftHasWeightMap = BucklingStiffnessWeft.HasWeightMap();
	const bool BucklingStiffnessBiasHasWeightMap = BucklingStiffnessBias.HasWeightMap();
	const bool DampingHasWeightMap = DampingRatio.HasWeightMap();

	if (ConstraintsPerColorStartIndex.Num() > 0 && Constraints.Num() > Chaos_XPBDBending_ParallelConstraintCount)
	{
		const int32 ConstraintColorNum = ConstraintsPerColorStartIndex.Num() - 1;
		if (!StiffnessHasWeightMap && !StiffnessWeftHasWeightMap && !StiffnessBiasHasWeightMap &&
			!BucklingStiffnessHasWeightMap && !BucklingStiffnessWeftHasWeightMap && !BucklingStiffnessBiasHasWeightMap &&
			!DampingHasWeightMap)
		{
			const FSolverVec3 ExpStiffnessValue((FSolverReal)StiffnessWeft, (FSolverReal)Stiffness, (FSolverReal)StiffnessBias);
			const FSolverVec3 ExpBucklingValue((FSolverReal)BucklingStiffnessWeft, (FSolverReal)BucklingStiffness, (FSolverReal)BucklingStiffnessBias);
			const FSolverReal DampingRatioValue = (FSolverReal)DampingRatio;

			if (ExpStiffnessValue.Max() < MinStiffness && ExpBucklingValue.Max() < MinStiffness)
			{
				return;
			}

#if INTEL_ISPC
			if (bRealTypeCompatibleWithISPC && bChaos_XPBDBending_ISPC_Enabled)
			{
				if (DampingRatioValue > 0)
				{
					for (int32 ConstraintColorIndex = 0; ConstraintColorIndex < ConstraintColorNum; ++ConstraintColorIndex)
					{
						const int32 ColorStart = ConstraintsPerColorStartIndex[ConstraintColorIndex];
						const int32 ColorSize = ConstraintsPerColorStartIndex[ConstraintColorIndex + 1] - ColorStart;
						ispc::ApplyXPBDAnisotropicBendingConstraintsWithDamping(
							(ispc::FVector4f*)Particles.GetPAndInvM().GetData(),
							(const ispc::FVector3f*)Particles.X().GetData(),
							(ispc::FIntVector4*)&Constraints.GetData()[ColorStart],
							&RestAngles.GetData()[ColorStart],
							&IsBuckled.GetData()[ColorStart],
							(const ispc::FVector3f*)&WarpWeftBiasBaseMultipliers.GetData()[ColorStart],
							&Lambdas.GetData()[ColorStart],
							Dt,
							MinStiffness,
							reinterpret_cast<const ispc::FVector3f&>(ExpStiffnessValue),
							reinterpret_cast<const ispc::FVector3f&>(ExpBucklingValue),
							DampingRatioValue,
							ColorSize);
					}
				}
				else
				{
					for (int32 ConstraintColorIndex = 0; ConstraintColorIndex < ConstraintColorNum; ++ConstraintColorIndex)
					{
						const int32 ColorStart = ConstraintsPerColorStartIndex[ConstraintColorIndex];
						const int32 ColorSize = ConstraintsPerColorStartIndex[ConstraintColorIndex + 1] - ColorStart;
						ispc::ApplyXPBDAnisotropicBendingConstraints(
							(ispc::FVector4f*)Particles.GetPAndInvM().GetData(),
							(ispc::FIntVector4*)&Constraints.GetData()[ColorStart],
							&RestAngles.GetData()[ColorStart],
							&IsBuckled.GetData()[ColorStart],
							(const ispc::FVector3f*)&WarpWeftBiasBaseMultipliers.GetData()[ColorStart],
							&Lambdas.GetData()[ColorStart],
							Dt,
							MinStiffness,
							reinterpret_cast<const ispc::FVector3f&>(ExpStiffnessValue),
							reinterpret_cast<const ispc::FVector3f&>(ExpBucklingValue),
							ColorSize);
					}
				}
			}
			else
#endif
			{
				for (int32 ConstraintColorIndex = 0; ConstraintColorIndex < ConstraintColorNum; ++ConstraintColorIndex)
				{
					const int32 ColorStart = ConstraintsPerColorStartIndex[ConstraintColorIndex];
					const int32 ColorSize = ConstraintsPerColorStartIndex[ConstraintColorIndex + 1] - ColorStart;
					PhysicsParallelFor(ColorSize, [&](const int32 Index)
					{
						const int32 ConstraintIndex = ColorStart + Index;
						ApplyHelper(Particles, Dt, ConstraintIndex, ExpStiffnessValue, ExpBucklingValue, DampingRatioValue);
					});
				}
			}
		}
		else  // Has weight maps
		{
#if INTEL_ISPC
			if (bRealTypeCompatibleWithISPC && bChaos_XPBDBending_ISPC_Enabled)
			{
				if (DampingHasWeightMap || (FSolverReal)DampingRatio > 0)
				{
					for (int32 ConstraintColorIndex = 0; ConstraintColorIndex < ConstraintColorNum; ++ConstraintColorIndex)
					{
						const int32 ColorStart = ConstraintsPerColorStartIndex[ConstraintColorIndex];
						const int32 ColorSize = ConstraintsPerColorStartIndex[ConstraintColorIndex + 1] - ColorStart;
						ispc::ApplyXPBDAnisotropicBendingConstraintsWithDampingAndMaps(
							(ispc::FVector4f*)Particles.GetPAndInvM().GetData(),
							(const ispc::FVector3f*)Particles.X().GetData(),
							(ispc::FIntVector4*)&Constraints.GetData()[ColorStart],
							&RestAngles.GetData()[ColorStart],
							&IsBuckled.GetData()[ColorStart],
							(const ispc::FVector3f*)&WarpWeftBiasBaseMultipliers.GetData()[ColorStart],
							&Lambdas.GetData()[ColorStart],
							Dt,
							MinStiffness,
							StiffnessHasWeightMap,
							StiffnessHasWeightMap ? &Stiffness.GetIndices().GetData()[ColorStart] : nullptr,
							&Stiffness.GetTable().GetData()[0],
							StiffnessWeftHasWeightMap,
							StiffnessWeftHasWeightMap ? &StiffnessWeft.GetIndices().GetData()[ColorStart] : nullptr,
							&StiffnessWeft.GetTable().GetData()[0],
							StiffnessBiasHasWeightMap,
							StiffnessBiasHasWeightMap ? &StiffnessBias.GetIndices().GetData()[ColorStart] : nullptr,
							&StiffnessBias.GetTable().GetData()[0],
							BucklingStiffnessHasWeightMap,
							BucklingStiffnessHasWeightMap ? &BucklingStiffness.GetIndices().GetData()[ColorStart] : nullptr,
							&BucklingStiffness.GetTable().GetData()[0],
							BucklingStiffnessWeftHasWeightMap,
							BucklingStiffnessWeftHasWeightMap ? &BucklingStiffnessWeft.GetIndices().GetData()[ColorStart] : nullptr,
							&BucklingStiffnessWeft.GetTable().GetData()[0],
							BucklingStiffnessBiasHasWeightMap,
							BucklingStiffnessBiasHasWeightMap ? &BucklingStiffnessBias.GetIndices().GetData()[ColorStart] : nullptr,
							&BucklingStiffnessBias.GetTable().GetData()[0],
							DampingHasWeightMap,
							DampingHasWeightMap ? &DampingRatio.GetIndices().GetData()[ColorStart] : nullptr,
							&DampingRatio.GetTable().GetData()[0],
							ColorSize);
					}
				}
				else
				{
					for (int32 ConstraintColorIndex = 0; ConstraintColorIndex < ConstraintColorNum; ++ConstraintColorIndex)
					{
						const int32 ColorStart = ConstraintsPerColorStartIndex[ConstraintColorIndex];
						const int32 ColorSize = ConstraintsPerColorStartIndex[ConstraintColorIndex + 1] - ColorStart;
						ispc::ApplyXPBDAnisotropicBendingConstraintsWithMaps(
							(ispc::FVector4f*)Particles.GetPAndInvM().GetData(),
							(ispc::FIntVector4*)&Constraints.GetData()[ColorStart],
							&RestAngles.GetData()[ColorStart],
							&IsBuckled.GetData()[ColorStart],
							(const ispc::FVector3f*)&WarpWeftBiasBaseMultipliers.GetData()[ColorStart],
							&Lambdas.GetData()[ColorStart],
							Dt,
							MinStiffness,
							StiffnessHasWeightMap,
							StiffnessHasWeightMap ? &Stiffness.GetIndices().GetData()[ColorStart] : nullptr,
							&Stiffness.GetTable().GetData()[0],
							StiffnessWeftHasWeightMap,
							StiffnessWeftHasWeightMap ? &StiffnessWeft.GetIndices().GetData()[ColorStart] : nullptr,
							&StiffnessWeft.GetTable().GetData()[0],
							StiffnessBiasHasWeightMap,
							StiffnessBiasHasWeightMap ? &StiffnessBias.GetIndices().GetData()[ColorStart] : nullptr,
							&StiffnessBias.GetTable().GetData()[0],
							BucklingStiffnessHasWeightMap,
							BucklingStiffnessHasWeightMap ? &BucklingStiffness.GetIndices().GetData()[ColorStart] : nullptr,
							&BucklingStiffness.GetTable().GetData()[0],
							BucklingStiffnessWeftHasWeightMap,
							BucklingStiffnessWeftHasWeightMap ? &BucklingStiffnessWeft.GetIndices().GetData()[ColorStart] : nullptr,
							&BucklingStiffnessWeft.GetTable().GetData()[0],
							BucklingStiffnessBiasHasWeightMap,
							BucklingStiffnessBiasHasWeightMap ? &BucklingStiffnessBias.GetIndices().GetData()[ColorStart] : nullptr,
							&BucklingStiffnessBias.GetTable().GetData()[0],
							ColorSize);
					}
				}
			}
			else
#endif
			{
				const FSolverReal StiffnessNoMap = (FSolverReal)Stiffness;
				const FSolverReal StiffnessWeftNoMap = (FSolverReal)StiffnessWeft;
				const FSolverReal StiffnessBiasNoMap = (FSolverReal)StiffnessBias;
				const FSolverReal BucklingStiffnessNoMap = (FSolverReal)BucklingStiffness;
				const FSolverReal BucklingStiffnessWeftNoMap = (FSolverReal)BucklingStiffnessWeft;
				const FSolverReal BucklingStiffnessBiasNoMap = (FSolverReal)BucklingStiffnessBias;
				const FSolverReal DampingNoMap = (FSolverReal)DampingRatio;
				for (int32 ConstraintColorIndex = 0; ConstraintColorIndex < ConstraintColorNum; ++ConstraintColorIndex)
				{
					const int32 ColorStart = ConstraintsPerColorStartIndex[ConstraintColorIndex];
					const int32 ColorSize = ConstraintsPerColorStartIndex[ConstraintColorIndex + 1] - ColorStart;
					PhysicsParallelFor(ColorSize, [&](const int32 Index)
					{
						const int32 ConstraintIndex = ColorStart + Index;
						const FSolverReal ExpStiffnessValue = StiffnessHasWeightMap ? Stiffness[ConstraintIndex] : StiffnessNoMap;
						const FSolverReal ExpStiffnessWeftValue = StiffnessWeftHasWeightMap ? StiffnessWeft[ConstraintIndex] : StiffnessWeftNoMap;
						const FSolverReal ExpStiffnessBiasValue = StiffnessBiasHasWeightMap ? StiffnessBias[ConstraintIndex] : StiffnessBiasNoMap;
						const FSolverReal ExpBucklingValue = BucklingStiffnessHasWeightMap ? BucklingStiffness[ConstraintIndex] : BucklingStiffnessNoMap;
						const FSolverReal ExpBucklingWeftValue = BucklingStiffnessWeftHasWeightMap ? BucklingStiffnessWeft[ConstraintIndex] : BucklingStiffnessWeftNoMap;
						const FSolverReal ExpBucklingBiasValue = BucklingStiffnessBiasHasWeightMap ? BucklingStiffnessBias[ConstraintIndex] : BucklingStiffnessBiasNoMap;
						const FSolverReal DampingRatioValue = DampingHasWeightMap ? DampingRatio[ConstraintIndex] : DampingNoMap;
						ApplyHelper(Particles, Dt, ConstraintIndex, FSolverVec3(ExpStiffnessWeftValue, ExpStiffnessValue, ExpStiffnessBiasValue),
							FSolverVec3(ExpBucklingWeftValue, ExpBucklingValue, ExpBucklingBiasValue), DampingRatioValue);
					});
				}
			}
		}
	}
	else
	{
		if (!StiffnessHasWeightMap && !StiffnessWeftHasWeightMap && !StiffnessBiasHasWeightMap &&
			!BucklingStiffnessHasWeightMap && !BucklingStiffnessWeftHasWeightMap && !BucklingStiffnessBiasHasWeightMap &&
			!DampingHasWeightMap)
		{
			const FSolverVec3 ExpStiffnessValue((FSolverReal)StiffnessWeft, (FSolverReal)Stiffness, (FSolverReal)StiffnessBias);
			const FSolverVec3 ExpBucklingValue((FSolverReal)BucklingStiffnessWeft, (FSolverReal)BucklingStiffness, (FSolverReal)BucklingStiffnessBias);
			const FSolverReal DampingRatioValue = (FSolverReal)DampingRatio;

			if (ExpStiffnessValue.Max() < MinStiffness && ExpBucklingValue.Max() < MinStiffness)
			{
				return;
			}

			for (int32 ConstraintIndex = 0; ConstraintIndex < Constraints.Num(); ++ConstraintIndex)
			{
				ApplyHelper(Particles, Dt, ConstraintIndex, ExpStiffnessValue, ExpBucklingValue, DampingRatioValue);
			}
		}
		else
		{
			const FSolverReal StiffnessNoMap = (FSolverReal)Stiffness;
			const FSolverReal StiffnessWeftNoMap = (FSolverReal)StiffnessWeft;
			const FSolverReal StiffnessBiasNoMap = (FSolverReal)StiffnessBias;
			const FSolverReal BucklingStiffnessNoMap = (FSolverReal)BucklingStiffness;
			const FSolverReal BucklingStiffnessWeftNoMap = (FSolverReal)BucklingStiffnessWeft;
			const FSolverReal BucklingStiffnessBiasNoMap = (FSolverReal)BucklingStiffnessBias;
			const FSolverReal DampingNoMap = (FSolverReal)DampingRatio;
			for (int32 ConstraintIndex = 0; ConstraintIndex < Constraints.Num(); ++ConstraintIndex)
			{
				const FSolverReal ExpStiffnessValue = StiffnessHasWeightMap ? Stiffness[ConstraintIndex] : StiffnessNoMap;
				const FSolverReal ExpStiffnessWeftValue = StiffnessWeftHasWeightMap ? StiffnessWeft[ConstraintIndex] : StiffnessWeftNoMap;
				const FSolverReal ExpStiffnessBiasValue = StiffnessBiasHasWeightMap ? StiffnessBias[ConstraintIndex] : StiffnessBiasNoMap;
				const FSolverReal ExpBucklingValue = BucklingStiffnessHasWeightMap ? BucklingStiffness[ConstraintIndex] : BucklingStiffnessNoMap;
				const FSolverReal ExpBucklingWeftValue = BucklingStiffnessWeftHasWeightMap ? BucklingStiffnessWeft[ConstraintIndex] : BucklingStiffnessWeftNoMap;
				const FSolverReal ExpBucklingBiasValue = BucklingStiffnessBiasHasWeightMap ? BucklingStiffnessBias[ConstraintIndex] : BucklingStiffnessBiasNoMap;
				const FSolverReal DampingRatioValue = DampingHasWeightMap ? DampingRatio[ConstraintIndex] : DampingNoMap;
				ApplyHelper(Particles, Dt, ConstraintIndex, FSolverVec3(ExpStiffnessWeftValue, ExpStiffnessValue, ExpStiffnessBiasValue),
					FSolverVec3(ExpBucklingWeftValue, ExpBucklingValue, ExpBucklingBiasValue), DampingRatioValue);
			}
		}
	}
}

}  // End namespace Chaos::Softs
