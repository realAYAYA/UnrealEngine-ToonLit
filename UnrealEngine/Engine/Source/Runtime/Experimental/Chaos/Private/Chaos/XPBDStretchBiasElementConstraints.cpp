// Copyright Epic Games, Inc. All Rights Reserved.
#include "Chaos/XPBDStretchBiasElementConstraints.h"
#include "Chaos/GraphColoring.h"
#include "Chaos/Framework/Parallel.h"
#include "Chaos/TriangleMesh.h"
#include "Chaos/Matrix.h"
#include "ChaosStats.h"

#if INTEL_ISPC
#include "XPBDStretchBiasElementConstraints.ispc.generated.h"
#endif

DECLARE_CYCLE_STAT(TEXT("Chaos XPBD Stretch Bias Constraint"), STAT_XPBD_StretchBias, STATGROUP_Chaos);

#if INTEL_ISPC && !UE_BUILD_SHIPPING
bool bChaos_XPBDStretchBiasElement_ISPC_Enabled = true;
FAutoConsoleVariableRef CVarChaosXPBDStretchBiasISPCEnabled(TEXT("p.Chaos.XPBDStretchBias.ISPC"), bChaos_XPBDStretchBiasElement_ISPC_Enabled, TEXT("Whether to use ISPC optimizations in XPBD Stretch Bias constraints"));
static_assert(sizeof(ispc::FVector4f) == sizeof(Chaos::Softs::FPAndInvM), "sizeof(ispc::FVector4f) != sizeof(Chaos::Softs::FPAndInvM");
static_assert(sizeof(ispc::FIntVector) == sizeof(Chaos::TVec3<int32>), "sizeof(ispc::FIntVector) != sizeof(Chaos::TVec3<int32>");
static_assert(sizeof(ispc::FVector2f) == sizeof(Chaos::Softs::FSolverVec2), "sizeof(ispc::FVector2f) != sizeof(Chaos::Softs::FSolverVec2");
static_assert(sizeof(ispc::FVector4f) == sizeof(Chaos::Softs::FSolverMatrix22), "sizeof(ispc::FVector2f) != sizeof(Chaos::Softs::FSolverMatrix22");
static_assert(sizeof(ispc::FVector3f) == sizeof(Chaos::Softs::FSolverVec3), "sizeof(ispc::FVector3f) != sizeof(Chaos::Softs::FSolverVec3");
#endif

namespace Chaos::Softs {

// @todo(chaos): the parallel threshold (or decision to run parallel) should probably be owned by the solver and passed to the constraint container
static int32 Chaos_XPBDStretchBias_ParallelConstraintCount = 100;

FXPBDStretchBiasElementConstraints::FXPBDStretchBiasElementConstraints(const FSolverParticles& InParticles,
	int32 InParticleOffset,
	int32 InParticleCount,
	const FTriangleMesh& TriangleMesh,
	const TArray<TVec3<FVec2f>>& FaceVertexUVs,
	const TMap<FString, TConstArrayView<FRealSingle>>& WeightMaps,
	const FCollectionPropertyConstFacade& PropertyCollection,
	bool bTrimKinematicConstraints)
	: ParticleOffset(InParticleOffset)
	, ParticleCount(InParticleCount)
	, StiffnessWarp(
		FSolverVec2(GetWeightedFloatXPBDAnisoStretchStiffnessWarp(PropertyCollection, MaxStiffness)),
		WeightMaps.FindRef(GetXPBDAnisoStretchStiffnessWarpString(PropertyCollection, XPBDAnisoStretchStiffnessWarpName.ToString())),
		TConstArrayView<TVec3<int32>>(Constraints),
		ParticleOffset,
		ParticleCount,
		FPBDStiffness::DefaultTableSize,
		FPBDStiffness::DefaultParameterFitBase,
		MaxStiffness)
	, StiffnessWeft(
		FSolverVec2(GetWeightedFloatXPBDAnisoStretchStiffnessWeft(PropertyCollection, MaxStiffness)),
		WeightMaps.FindRef(GetXPBDAnisoStretchStiffnessWeftString(PropertyCollection, XPBDAnisoStretchStiffnessWeftName.ToString())),
		TConstArrayView<TVec3<int32>>(Constraints),
		ParticleOffset,
		ParticleCount,
		FPBDStiffness::DefaultTableSize,
		FPBDStiffness::DefaultParameterFitBase,
		MaxStiffness)
	, StiffnessBias(
		FSolverVec2(GetWeightedFloatXPBDAnisoStretchStiffnessBias(PropertyCollection, MaxStiffness)),
		WeightMaps.FindRef(GetXPBDAnisoStretchStiffnessBiasString(PropertyCollection, XPBDAnisoStretchStiffnessBiasName.ToString())),
		TConstArrayView<TVec3<int32>>(Constraints),
		ParticleOffset,
		ParticleCount,
		FPBDStiffness::DefaultTableSize,
		FPBDStiffness::DefaultParameterFitBase,
		MaxStiffness)
	, DampingRatio(
		FSolverVec2(GetWeightedFloatXPBDAnisoStretchDamping(PropertyCollection, MinDamping)).ClampAxes(MinDamping, MaxDamping),
		WeightMaps.FindRef(GetXPBDAnisoStretchDampingString(PropertyCollection, XPBDAnisoStretchDampingName.ToString())),
		TConstArrayView<TVec3<int32>>(Constraints),
		ParticleOffset,
		ParticleCount)
	, WarpScale(
		FSolverVec2(GetWeightedFloatXPBDAnisoStretchWarpScale(PropertyCollection, DefaultWarpWeftScale)).ClampAxes(MinWarpWeftScale, MaxWarpWeftScale),
		WeightMaps.FindRef(GetXPBDAnisoStretchWarpScaleString(PropertyCollection, XPBDAnisoStretchWarpScaleName.ToString())),
		TConstArrayView<TVec3<int32>>(Constraints),
		ParticleOffset,
		ParticleCount)
	, WeftScale(
		FSolverVec2(GetWeightedFloatXPBDAnisoStretchWeftScale(PropertyCollection, DefaultWarpWeftScale)).ClampAxes(MinWarpWeftScale, MaxWarpWeftScale),
		WeightMaps.FindRef(GetXPBDAnisoStretchWeftScaleString(PropertyCollection, XPBDAnisoStretchWeftScaleName.ToString())),
		TConstArrayView<TVec3<int32>>(Constraints),
		ParticleOffset,
		ParticleCount)
	, XPBDAnisoStretchUse3dRestLengthsIndex(PropertyCollection)
	, XPBDAnisoStretchStiffnessWarpIndex(PropertyCollection)
	, XPBDAnisoStretchStiffnessWeftIndex(PropertyCollection)
	, XPBDAnisoStretchStiffnessBiasIndex(PropertyCollection)
	, XPBDAnisoStretchDampingIndex(PropertyCollection)
	, XPBDAnisoStretchWarpScaleIndex(PropertyCollection)
	, XPBDAnisoStretchWeftScaleIndex(PropertyCollection)
{
	Lambdas.Init(FSolverVec3(0.), Constraints.Num());
	InitConstraintsAndRestData(InParticles, TriangleMesh, FaceVertexUVs, GetXPBDAnisoStretchUse3dRestLengths(PropertyCollection, bDefaultUse3dRestLengths), bTrimKinematicConstraints);
	InitColor(InParticles);
}

FXPBDStretchBiasElementConstraints::FXPBDStretchBiasElementConstraints(const FSolverParticles& InParticles,
	int32 InParticleOffset,
	int32 InParticleCount,
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
	bool bTrimKinematicConstraints)
	: ParticleOffset(InParticleOffset)
	, ParticleCount(InParticleCount)
	, StiffnessWarp(
		InStiffnessWarp,
		StiffnessWarpMultipliers,
		TConstArrayView<TVec3<int32>>(Constraints),
		ParticleOffset,
		ParticleCount,
		FPBDStiffness::DefaultTableSize,
		FPBDStiffness::DefaultParameterFitBase,
		MaxStiffness)
	, StiffnessWeft(
		InStiffnessWeft,
		StiffnessWeftMultipliers,
		TConstArrayView<TVec3<int32>>(Constraints),
		ParticleOffset,
		ParticleCount,
		FPBDStiffness::DefaultTableSize,
		FPBDStiffness::DefaultParameterFitBase,
		MaxStiffness)
	, StiffnessBias(
		InStiffnessBias,
		StiffnessBiasMultipliers,
		TConstArrayView<TVec3<int32>>(Constraints),
		ParticleOffset,
		ParticleCount,
		FPBDStiffness::DefaultTableSize,
		FPBDStiffness::DefaultParameterFitBase,
		MaxStiffness)
	, DampingRatio(
		InDampingRatio,
		DampingMultipliers,
		TConstArrayView<TVec3<int32>>(Constraints),
		ParticleOffset,
		ParticleCount)
	, WarpScale(
		InWarpScale,
		WarpScaleMultipliers,
		TConstArrayView<TVec3<int32>>(Constraints),
		ParticleOffset,
		ParticleCount)
	, WeftScale(
		InWeftScale,
		WeftScaleMultipliers,
		TConstArrayView<TVec3<int32>>(Constraints),
		ParticleOffset,
		ParticleCount)
	, XPBDAnisoStretchUse3dRestLengthsIndex(ForceInit)
	, XPBDAnisoStretchStiffnessWarpIndex(ForceInit)
	, XPBDAnisoStretchStiffnessWeftIndex(ForceInit)
	, XPBDAnisoStretchStiffnessBiasIndex(ForceInit)
	, XPBDAnisoStretchDampingIndex(ForceInit)
	, XPBDAnisoStretchWarpScaleIndex(ForceInit)
	, XPBDAnisoStretchWeftScaleIndex(ForceInit)
{
	Lambdas.Init(FSolverVec3(0.), Constraints.Num());
	InitConstraintsAndRestData(InParticles, TriangleMesh, FaceVertexUVs, bUse3dRestLengths, bTrimKinematicConstraints);
	InitColor(InParticles);
}

void FXPBDStretchBiasElementConstraints::InitConstraintsAndRestData(const FSolverParticles& InParticles, const FTriangleMesh& TriangleMesh,
	const TArray<TVec3<FSolverVec2>>& FaceVertexUVs, const bool bUse3dRestLengths, const bool bTrimKinematicConstraints)
{
	const TArray<TVec3<int32>>& Elements = TriangleMesh.GetElements();
	check(Elements.Num() == FaceVertexUVs.Num());

	Constraints.Reserve(Elements.Num());
	RestStretchLengths.Reserve(Elements.Num());
	DeltaUVInverse.Reserve(Elements.Num());
	StiffnessScales.Reserve(Elements.Num());

	for (int32 ElemIdx = 0; ElemIdx < Elements.Num(); ++ElemIdx)
	{
		const TVec3<int32>& Constraint = Elements[ElemIdx];

		if (bTrimKinematicConstraints && InParticles.InvM(Constraint[0]) == (FSolverReal)0. && InParticles.InvM(Constraint[1]) == (FSolverReal)0. && InParticles.InvM(Constraint[2]) == (FSolverReal)0.)
		{
			continue;
		}

		Constraints.Add(Constraint);

		const FSolverVec2& UV0 = FaceVertexUVs[ElemIdx][0];
		const FSolverVec2& UV1 = FaceVertexUVs[ElemIdx][1];
		const FSolverVec2& UV2 = FaceVertexUVs[ElemIdx][2];

		const FSolverVec2 UV01 = UV1 - UV0;
		const FSolverVec2 UV02 = UV2 - UV0;


		if (bUse3dRestLengths)
		{
			const FSolverVec2 DeltaUNormalized = FSolverVec2(UV01[0], UV02[0]).GetSafeNormal();
			const FSolverVec2 DeltaVNormalized = FSolverVec2(UV01[1], UV02[1]).GetSafeNormal();
			const FSolverReal DeltaUVDet = DeltaUNormalized[0] * DeltaVNormalized[1] - DeltaVNormalized[0] * DeltaUNormalized[1];

			const FSolverMatrix22& DeltaUVInv = FMath::Abs(DeltaUVDet) < UE_SMALL_NUMBER ? DeltaUVInverse.Add_GetRef(FSolverMatrix22(1.f, 0.f, 0.f, 1.f)) :
				DeltaUVInverse.Add_GetRef(FSolverMatrix22(DeltaVNormalized[1] / DeltaUVDet, -DeltaVNormalized[0] / DeltaUVDet, -DeltaUNormalized[1] / DeltaUVDet, DeltaUNormalized[0] / DeltaUVDet));

			const FSolverVec3& X0 = InParticles.X(Constraint[0]);
			const FSolverVec3& X1 = InParticles.X(Constraint[1]);
			const FSolverVec3& X2 = InParticles.X(Constraint[2]);
			const FSolverVec3 X01 = X1 - X0;
			const FSolverVec3 X02 = X2 - X0;

			const FSolverVec3 DXDu = X01 * DeltaUVInv.M[0] + X02 * DeltaUVInv.M[1];
			const FSolverVec3 DXDv = X01 * DeltaUVInv.M[2] + X02 * DeltaUVInv.M[3];
			const FSolverVec2& RestStretch = RestStretchLengths.Add_GetRef(FSolverVec2(DXDu.Length(), DXDv.Length()));

			const FSolverReal StiffnessScaleU = RestStretch[0] < UE_SMALL_NUMBER ? 1.f : 1.f / RestStretch[0];
			const FSolverReal StiffnessScaleV = RestStretch[1] < UE_SMALL_NUMBER ? 1.f : 1.f / RestStretch[1];
			const FSolverReal BiasScale = FMath::Sqrt((FSolverReal).5 * FSolverVec3::CrossProduct(X01, X02).Length()); // Sqrt of triangle area

			StiffnessScales.Add(FSolverVec3(StiffnessScaleU, StiffnessScaleV, BiasScale));
		}
		else
		{
			const FSolverVec2 DeltaUUnnormalized = FSolverVec2(UV01[0], UV02[0]);
			const FSolverVec2 DeltaVUnnormalized = FSolverVec2(UV01[1], UV02[1]);
			const FSolverReal DeltaUVDet = DeltaUUnnormalized[0] * DeltaVUnnormalized[1] - DeltaVUnnormalized[0] * DeltaUUnnormalized[1];
			const FSolverMatrix22& DeltaUVInv = FMath::Abs(DeltaUVDet) < UE_SMALL_NUMBER ? DeltaUVInverse.Add_GetRef(FSolverMatrix22(1.f, 0.f, 0.f, 1.f)) :
				DeltaUVInverse.Add_GetRef(FSolverMatrix22(DeltaVUnnormalized[1] / DeltaUVDet, -DeltaVUnnormalized[0] / DeltaUVDet, -DeltaUUnnormalized[1] / DeltaUVDet, DeltaUUnnormalized[0] / DeltaUVDet));
			RestStretchLengths.Add(FSolverVec2(1.f));

			const FSolverReal DeltaULen = DeltaUUnnormalized.Length();
			const FSolverReal DeltaVLen = DeltaVUnnormalized.Length();

			const FSolverReal StiffnessScaleU = DeltaULen < UE_SMALL_NUMBER ? 1.f : 1.f / DeltaULen;
			const FSolverReal StiffnessScaleV = DeltaVLen < UE_SMALL_NUMBER ? 1.f : 1.f / DeltaVLen;
			const FSolverReal BiasScale = FMath::Sqrt((FSolverReal).5 * FMath::Abs(UV01[0] * UV02[1] - UV01[1] * UV02[0]));
			StiffnessScales.Add(FSolverVec3(StiffnessScaleU, StiffnessScaleV, BiasScale));
		}
	}
}

void FXPBDStretchBiasElementConstraints::InitColor(const FSolverParticles& InParticles)
{
	ConstraintsPerColorStartIndex.Reset();

	// In dev builds we always color so we can tune the system without restarting. See Apply()
#if UE_BUILD_SHIPPING || UE_BUILD_TEST
	if (Constraints.Num() > Chaos_XPBDStretchBias_ParallelConstraintCount)
#endif
	{
		const TArray<TArray<int32>> ConstraintsPerColor = FGraphColoring::ComputeGraphColoring(Constraints, InParticles, ParticleOffset, ParticleOffset + ParticleCount);

		// Reorder constraints based on color so each array in ConstraintsPerColor contains contiguous elements.
		TArray<TVec3<int32>> ReorderedConstraints;
		TArray<FSolverVec2> ReorderedRestStretchLengths;
		TArray<FSolverMatrix22> ReorderedDeltaUVInverse;
		TArray<FSolverVec3> ReorderedStiffnessScales;
		TArray<int32> OrigToReorderedIndices; // used to reorder stiffness indices
		ReorderedConstraints.SetNumUninitialized(Constraints.Num());
		ReorderedRestStretchLengths.SetNumUninitialized(Constraints.Num());
		ReorderedDeltaUVInverse.SetNumUninitialized(Constraints.Num());
		ReorderedStiffnessScales.SetNumUninitialized(Constraints.Num());
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
				ReorderedRestStretchLengths[ReorderedIndex] = RestStretchLengths[OrigIndex];
				ReorderedDeltaUVInverse[ReorderedIndex] = DeltaUVInverse[OrigIndex];
				ReorderedStiffnessScales[ReorderedIndex] = StiffnessScales[OrigIndex];
				OrigToReorderedIndices[OrigIndex] = ReorderedIndex;

				++ReorderedIndex;
			}
		}
		check(ReorderedIndex == Constraints.Num());
		ConstraintsPerColorStartIndex.Add(ReorderedIndex);

		Constraints = MoveTemp(ReorderedConstraints);
		RestStretchLengths = MoveTemp(ReorderedRestStretchLengths);
		DeltaUVInverse = MoveTemp(ReorderedDeltaUVInverse);
		StiffnessScales = MoveTemp(ReorderedStiffnessScales);
		StiffnessWarp.ReorderIndices(OrigToReorderedIndices);
		StiffnessWeft.ReorderIndices(OrigToReorderedIndices);
		StiffnessBias.ReorderIndices(OrigToReorderedIndices);
		DampingRatio.ReorderIndices(OrigToReorderedIndices);
	}

	// Ensure we have valid color indices
	if (ConstraintsPerColorStartIndex.Num() < 2)
	{
		ConstraintsPerColorStartIndex.Reset(2);
		ConstraintsPerColorStartIndex.Add(0);
		ConstraintsPerColorStartIndex.Add(Constraints.Num());
	}
}

void FXPBDStretchBiasElementConstraints::SetProperties(
	const FCollectionPropertyConstFacade& PropertyCollection,
	const TMap<FString, TConstArrayView<FRealSingle>>& WeightMaps)
{
	if (IsXPBDAnisoStretchStiffnessWarpMutable(PropertyCollection))
	{
		const FSolverVec2 WeightedValue(GetWeightedFloatXPBDAnisoStretchStiffnessWarp(PropertyCollection));
		if (IsXPBDAnisoStretchStiffnessWarpStringDirty(PropertyCollection))
		{
			const FString& WeightMapName = GetXPBDAnisoStretchStiffnessWarpString(PropertyCollection);
			StiffnessWarp = FPBDStiffness(
				WeightedValue,
				WeightMaps.FindRef(WeightMapName),
				TConstArrayView<TVec3<int32>>(Constraints),
				ParticleOffset,
				ParticleCount,
				FPBDStiffness::DefaultTableSize,
				FPBDStiffness::DefaultParameterFitBase,
				MaxStiffness);
		}
		else
		{
			StiffnessWarp.SetWeightedValue(WeightedValue, MaxStiffness);
		}
	}
	if (IsXPBDAnisoStretchStiffnessWeftMutable(PropertyCollection))
	{
		const FSolverVec2 WeightedValue(GetWeightedFloatXPBDAnisoStretchStiffnessWeft(PropertyCollection));
		if (IsXPBDAnisoStretchStiffnessWeftStringDirty(PropertyCollection))
		{
			const FString& WeightMapName = GetXPBDAnisoStretchStiffnessWeftString(PropertyCollection);
			StiffnessWeft = FPBDStiffness(
				WeightedValue,
				WeightMaps.FindRef(WeightMapName),
				TConstArrayView<TVec3<int32>>(Constraints),
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
	if (IsXPBDAnisoStretchStiffnessBiasMutable(PropertyCollection))
	{
		const FSolverVec2 WeightedValue(GetWeightedFloatXPBDAnisoStretchStiffnessBias(PropertyCollection));
		if (IsXPBDAnisoStretchStiffnessBiasStringDirty(PropertyCollection))
		{
			const FString& WeightMapName = GetXPBDAnisoStretchStiffnessBiasString(PropertyCollection);
			StiffnessBias = FPBDStiffness(
				WeightedValue,
				WeightMaps.FindRef(WeightMapName),
				TConstArrayView<TVec3<int32>>(Constraints),
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
	if (IsXPBDAnisoStretchDampingMutable(PropertyCollection))
	{
		const FSolverVec2 WeightedValue = FSolverVec2(GetWeightedFloatXPBDAnisoStretchDamping(PropertyCollection)).ClampAxes(MinDamping, MaxDamping);
		if (IsXPBDAnisoStretchDampingStringDirty(PropertyCollection))
		{
			const FString& WeightMapName = GetXPBDAnisoStretchDampingString(PropertyCollection);
			DampingRatio = FPBDWeightMap(
				WeightedValue,
				WeightMaps.FindRef(WeightMapName),
				TConstArrayView<TVec3<int32>>(Constraints),
				ParticleOffset,
				ParticleCount);
		}
		else
		{
			DampingRatio.SetWeightedValue(WeightedValue);
		}
	}
	if (IsXPBDAnisoStretchWarpScaleMutable(PropertyCollection))
	{
		const FSolverVec2 WeightedValue = FSolverVec2(GetWeightedFloatXPBDAnisoStretchWarpScale(PropertyCollection)).ClampAxes(MinWarpWeftScale, MaxWarpWeftScale);
		if (IsXPBDAnisoStretchWarpScaleStringDirty(PropertyCollection))
		{
			const FString& WeightMapName = GetXPBDAnisoStretchWarpScaleString(PropertyCollection);
			WarpScale = FPBDWeightMap(
				WeightedValue,
				WeightMaps.FindRef(WeightMapName),
				TConstArrayView<TVec3<int32>>(Constraints),
				ParticleOffset,
				ParticleCount);
		}
		else
		{
			WarpScale.SetWeightedValue(WeightedValue);
		}
	}
	if (IsXPBDAnisoStretchWeftScaleMutable(PropertyCollection))
	{
		const FSolverVec2 WeightedValue = FSolverVec2(GetWeightedFloatXPBDAnisoStretchWeftScale(PropertyCollection)).ClampAxes(MinWarpWeftScale, MaxWarpWeftScale);
		if (IsXPBDAnisoStretchWeftScaleStringDirty(PropertyCollection))
		{
			const FString& WeightMapName = GetXPBDAnisoStretchWeftScaleString(PropertyCollection);
			WeftScale = FPBDWeightMap(
				WeightedValue,
				WeightMaps.FindRef(WeightMapName),
				TConstArrayView<TVec3<int32>>(Constraints),
				ParticleOffset,
				ParticleCount);
		}
		else
		{
			WeftScale.SetWeightedValue(WeightedValue);
		}
	}
}

void FXPBDStretchBiasElementConstraints::Apply(FSolverParticles& Particles, const FSolverReal Dt) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FXPBDStretchBiasElementConstraints_Apply);
	SCOPE_CYCLE_COUNTER(STAT_XPBD_StretchBias);

	const bool StiffnessWarpHasWeightMap = StiffnessWarp.HasWeightMap();
	const bool StiffnessWeftHasWeightMap = StiffnessWeft.HasWeightMap();
	const bool StiffnessBiasHasWeightMap = StiffnessBias.HasWeightMap();
	const bool DampingHasWeightMap = DampingRatio.HasWeightMap();
	const bool WarpScaleHasWeightMap = WarpScale.HasWeightMap();
	const bool WeftScaleHasWeightMap = WeftScale.HasWeightMap();

	const FSolverReal StiffnessWarpNoMap = (FSolverReal)StiffnessWarp;
	const FSolverReal StiffnessWeftNoMap = (FSolverReal)StiffnessWeft;
	const FSolverReal StiffnessBiasNoMap = (FSolverReal)StiffnessBias;
	const FSolverReal DampingNoMap = (FSolverReal)DampingRatio;
	const FSolverReal WarpScaleNoMap = (FSolverReal)WarpScale;
	const FSolverReal WeftScaleNoMap = (FSolverReal)WeftScale;

	if (ConstraintsPerColorStartIndex.Num() > 1 && Constraints.Num() > Chaos_XPBDStretchBias_ParallelConstraintCount)
	{
		const int32 ConstraintColorNum = ConstraintsPerColorStartIndex.Num() - 1;
		if (!StiffnessWarpHasWeightMap && !StiffnessWeftHasWeightMap && !StiffnessBiasHasWeightMap && !DampingHasWeightMap && !WarpScaleHasWeightMap && !WeftScaleHasWeightMap)
		{
			const FSolverVec3 ExpStiffnessValue(StiffnessWeftNoMap, StiffnessWarpNoMap, StiffnessBiasNoMap);
			if (ExpStiffnessValue.Max() < MinStiffness)
			{
				return;
			}

#if INTEL_ISPC
			if (bRealTypeCompatibleWithISPC && bChaos_XPBDStretchBiasElement_ISPC_Enabled)
			{
				if (DampingNoMap > 0)
				{
					for (int32 ConstraintColorIndex = 0; ConstraintColorIndex < ConstraintColorNum; ++ConstraintColorIndex)
					{
						const int32 ColorStart = ConstraintsPerColorStartIndex[ConstraintColorIndex];
						const int32 ColorSize = ConstraintsPerColorStartIndex[ConstraintColorIndex + 1] - ColorStart;
						ispc::ApplyXPBDStretchBiasConstraintsWithDamping(
							(ispc::FVector4f*)Particles.GetPAndInvM().GetData(),
							(const ispc::FVector3f*)Particles.X().GetData(),
							(ispc::FIntVector*)&Constraints.GetData()[ColorStart],
							(ispc::FVector2f*)&RestStretchLengths.GetData()[ColorStart],
							(ispc::FVector4f*)&DeltaUVInverse.GetData()[ColorStart],
							(ispc::FVector3f*)&StiffnessScales.GetData()[ColorStart],
							(ispc::FVector3f*)&Lambdas.GetData()[ColorStart],
							Dt,
							MinStiffness,
							reinterpret_cast<const ispc::FVector3f&>(ExpStiffnessValue),
							DampingNoMap,
							WarpScaleNoMap,
							WeftScaleNoMap,
							ColorSize);
					}

				}
				else
				{
					for (int32 ConstraintColorIndex = 0; ConstraintColorIndex < ConstraintColorNum; ++ConstraintColorIndex)
					{
						const int32 ColorStart = ConstraintsPerColorStartIndex[ConstraintColorIndex];
						const int32 ColorSize = ConstraintsPerColorStartIndex[ConstraintColorIndex + 1] - ColorStart;
						ispc::ApplyXPBDStretchBiasConstraints(
							(ispc::FVector4f*)Particles.GetPAndInvM().GetData(),
							(ispc::FIntVector*)&Constraints.GetData()[ColorStart],
							(ispc::FVector2f*)&RestStretchLengths.GetData()[ColorStart],
							(ispc::FVector4f*)&DeltaUVInverse.GetData()[ColorStart],
							(ispc::FVector3f*)&StiffnessScales.GetData()[ColorStart],
							(ispc::FVector3f*)&Lambdas.GetData()[ColorStart],
							Dt,
							MinStiffness,
							reinterpret_cast<const ispc::FVector3f&>(ExpStiffnessValue),
							WarpScaleNoMap,
							WeftScaleNoMap,
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
						ApplyHelper(Particles, Dt, ConstraintIndex, ExpStiffnessValue, DampingNoMap, WarpScaleNoMap, WeftScaleNoMap);
					});
				}
			}			
		}
		else // has weight maps
		{

#if INTEL_ISPC
			if (bRealTypeCompatibleWithISPC && bChaos_XPBDStretchBiasElement_ISPC_Enabled)
			{
				if (DampingHasWeightMap || DampingNoMap > 0)
				{
					for (int32 ConstraintColorIndex = 0; ConstraintColorIndex < ConstraintColorNum; ++ConstraintColorIndex)
					{
						const int32 ColorStart = ConstraintsPerColorStartIndex[ConstraintColorIndex];
						const int32 ColorSize = ConstraintsPerColorStartIndex[ConstraintColorIndex + 1] - ColorStart;
						ispc::ApplyXPBDStretchBiasConstraintsWithDampingAndMaps(
							(ispc::FVector4f*)Particles.GetPAndInvM().GetData(),
							(const ispc::FVector3f*)Particles.X().GetData(),
							(ispc::FIntVector*)&Constraints.GetData()[ColorStart],
							(ispc::FVector2f*)&RestStretchLengths.GetData()[ColorStart],
							(ispc::FVector4f*)&DeltaUVInverse.GetData()[ColorStart],
							(ispc::FVector3f*)&StiffnessScales.GetData()[ColorStart],
							(ispc::FVector3f*)&Lambdas.GetData()[ColorStart],
							Dt,
							MinStiffness,
							StiffnessWarpHasWeightMap,
							StiffnessWarpHasWeightMap ? &StiffnessWarp.GetIndices().GetData()[ColorStart] : nullptr,
							&StiffnessWarp.GetTable().GetData()[0],
							StiffnessWeftHasWeightMap,
							StiffnessWeftHasWeightMap ? &StiffnessWeft.GetIndices().GetData()[ColorStart] : nullptr,
							&StiffnessWeft.GetTable().GetData()[0],
							StiffnessBiasHasWeightMap,
							StiffnessBiasHasWeightMap ? &StiffnessBias.GetIndices().GetData()[ColorStart] : nullptr,
							&StiffnessBias.GetTable().GetData()[0],
							DampingHasWeightMap,
							DampingHasWeightMap ? &DampingRatio.GetIndices().GetData()[ColorStart] : nullptr,
							&DampingRatio.GetTable().GetData()[0],
							WarpScaleHasWeightMap,
							WarpScaleHasWeightMap ? &WarpScale.GetIndices().GetData()[ColorStart] : nullptr,
							&WarpScale.GetTable().GetData()[0],
							WeftScaleHasWeightMap,
							WeftScaleHasWeightMap ? &WeftScale.GetIndices().GetData()[ColorStart] : nullptr,
							&WeftScale.GetTable().GetData()[0],
							ColorSize);
					}

				}
				else
				{
					for (int32 ConstraintColorIndex = 0; ConstraintColorIndex < ConstraintColorNum; ++ConstraintColorIndex)
					{
						const int32 ColorStart = ConstraintsPerColorStartIndex[ConstraintColorIndex];
						const int32 ColorSize = ConstraintsPerColorStartIndex[ConstraintColorIndex + 1] - ColorStart;
						ispc::ApplyXPBDStretchBiasConstraintsWithMaps(
							(ispc::FVector4f*)Particles.GetPAndInvM().GetData(),
							(ispc::FIntVector*)&Constraints.GetData()[ColorStart],
							(ispc::FVector2f*)&RestStretchLengths.GetData()[ColorStart],
							(ispc::FVector4f*)&DeltaUVInverse.GetData()[ColorStart],
							(ispc::FVector3f*)&StiffnessScales.GetData()[ColorStart],
							(ispc::FVector3f*)&Lambdas.GetData()[ColorStart],
							Dt,
							MinStiffness,
							StiffnessWarpHasWeightMap,
							StiffnessWarpHasWeightMap ? &StiffnessWarp.GetIndices().GetData()[ColorStart] : nullptr,
							&StiffnessWarp.GetTable().GetData()[0],
							StiffnessWeftHasWeightMap,
							StiffnessWeftHasWeightMap ? &StiffnessWeft.GetIndices().GetData()[ColorStart] : nullptr,
							&StiffnessWeft.GetTable().GetData()[0],
							StiffnessBiasHasWeightMap,
							StiffnessBiasHasWeightMap ? &StiffnessBias.GetIndices().GetData()[ColorStart] : nullptr,
							&StiffnessBias.GetTable().GetData()[0],
							WarpScaleHasWeightMap,
							WarpScaleHasWeightMap ? &WarpScale.GetIndices().GetData()[ColorStart] : nullptr,
							&WarpScale.GetTable().GetData()[0],
							WeftScaleHasWeightMap,
							WeftScaleHasWeightMap ? &WeftScale.GetIndices().GetData()[ColorStart] : nullptr,
							&WeftScale.GetTable().GetData()[0],
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
						const FSolverReal ExpStiffnessWarpValue = StiffnessWarpHasWeightMap ? StiffnessWarp[ConstraintIndex] : StiffnessWarpNoMap;
						const FSolverReal ExpStiffnessWeftValue = StiffnessWeftHasWeightMap ? StiffnessWeft[ConstraintIndex] : StiffnessWeftNoMap;
						const FSolverReal ExpStiffnessBiasValue = StiffnessBiasHasWeightMap ? StiffnessBias[ConstraintIndex] : StiffnessBiasNoMap;
						const FSolverReal DampingRatioValue = DampingHasWeightMap ? DampingRatio[ConstraintIndex] : DampingNoMap;
						const FSolverReal WarpScaleValue = WarpScaleHasWeightMap ? WarpScale[ConstraintIndex] : WarpScaleNoMap;
						const FSolverReal WeftScaleValue = WeftScaleHasWeightMap ? WeftScale[ConstraintIndex] : WeftScaleNoMap;
						ApplyHelper(Particles, Dt, ConstraintIndex, FSolverVec3(ExpStiffnessWeftValue, ExpStiffnessWarpValue, ExpStiffnessBiasValue),DampingRatioValue, WarpScaleValue, WeftScaleValue);
					});
				}
			}
		}
	}
	else
	{
		if (!StiffnessWarpHasWeightMap && !StiffnessWeftHasWeightMap && !StiffnessBiasHasWeightMap && !DampingHasWeightMap)
		{
			const FSolverVec3 ExpStiffnessValue(StiffnessWarpNoMap, StiffnessWeftNoMap, StiffnessBiasNoMap);
			if (ExpStiffnessValue.Max() < MinStiffness)
			{
				return;
			}
			for (int32 ConstraintIndex = 0; ConstraintIndex < Constraints.Num(); ++ConstraintIndex)
			{
				ApplyHelper(Particles, Dt, ConstraintIndex, ExpStiffnessValue, DampingNoMap, WarpScaleNoMap, WeftScaleNoMap);
			}
		}
		else
		{
			for (int32 ConstraintIndex = 0; ConstraintIndex < Constraints.Num(); ++ConstraintIndex)
			{
				const FSolverReal ExpStiffnessWarpValue = StiffnessWarpHasWeightMap ? StiffnessWarp[ConstraintIndex] : StiffnessWarpNoMap;
				const FSolverReal ExpStiffnessWeftValue = StiffnessWeftHasWeightMap ? StiffnessWeft[ConstraintIndex] : StiffnessWeftNoMap;
				const FSolverReal ExpStiffnessBiasValue = StiffnessBiasHasWeightMap ? StiffnessBias[ConstraintIndex] : StiffnessBiasNoMap;
				const FSolverReal DampingRatioValue = DampingHasWeightMap ? DampingRatio[ConstraintIndex] : DampingNoMap;
				const FSolverReal WarpScaleValue = WarpScaleHasWeightMap ? WarpScale[ConstraintIndex] : WarpScaleNoMap;
				const FSolverReal WeftScaleValue = WeftScaleHasWeightMap ? WeftScale[ConstraintIndex] : WeftScaleNoMap;
				ApplyHelper(Particles, Dt, ConstraintIndex, FSolverVec3(ExpStiffnessWeftValue, ExpStiffnessWarpValue, ExpStiffnessBiasValue), DampingRatioValue, WarpScaleValue, WeftScaleValue);
			}
		}
	}
}

void FXPBDStretchBiasElementConstraints::CalculateUVStretch(const int32 ConstraintIndex, const FSolverVec3& P0, const FSolverVec3& P1, const FSolverVec3& P2, FSolverVec3& DXDu, FSolverVec3& DXDv) const
{
	const FSolverMatrix22& DeltaUVInv = DeltaUVInverse[ConstraintIndex];

	const FSolverVec3 P01 = P1 - P0;
	const FSolverVec3 P02 = P2 - P0;
	DXDu = P01 * DeltaUVInv.M[0] + P02 * DeltaUVInv.M[1];
	DXDv = P01 * DeltaUVInv.M[2] + P02 * DeltaUVInv.M[3];
}

static FSolverReal CalcDLambda(const FSolverReal StiffnessValue, const FSolverReal Dt, const FSolverReal Lambda, const FSolverReal Damping,
	const FSolverReal InvM1, const FSolverReal InvM2, const FSolverReal InvM3,
	const FSolverVec3& V1TimesDt, const FSolverVec3& V2TimesDt, const FSolverVec3& V3TimesDt,
	const FSolverReal C, const FSolverVec3& dC_dX1, const FSolverVec3& dC_dX2, const FSolverVec3& dC_dX3)
{
	if (StiffnessValue < FXPBDStretchBiasElementConstraints::MinStiffness)
	{
		return (FSolverReal)0.f;
	}
	const FSolverReal Alpha = (FSolverReal)1.f / (StiffnessValue * Dt * Dt);
	const FSolverReal Gamma = Alpha * Damping * Dt;
	const FSolverReal DampingTerm = Gamma * (FSolverVec3::DotProduct(V1TimesDt, dC_dX1) + FSolverVec3::DotProduct(V2TimesDt, dC_dX2) + FSolverVec3::DotProduct(V3TimesDt, dC_dX3));
	const FSolverReal Denom = ((FSolverReal)1.f + Gamma) * (InvM1 * dC_dX1.SizeSquared() + InvM2 * dC_dX2.SizeSquared() + InvM3 * dC_dX3.SizeSquared()) + Alpha;
	return (C - Alpha * Lambda + DampingTerm) / Denom;
}

void FXPBDStretchBiasElementConstraints::ApplyHelper(FSolverParticles& Particles, const FSolverReal Dt, const int32 ConstraintIndex, const FSolverVec3& ExpStiffnessValue, const FSolverReal DampingRatioValue, const FSolverReal WarpScaleValue, const FSolverReal WeftScaleValue) const
{
	const TVec3<int32>& Constraint = Constraints[ConstraintIndex];
	const int32 i0 = Constraint[0];
	const int32 i1 = Constraint[1];
	const int32 i2 = Constraint[2];
	if (Particles.InvM(i0) == (FSolverReal)0. && Particles.InvM(i1) == (FSolverReal)0. && Particles.InvM(i2) == (FSolverReal)0.)
	{
		return;
	}

	const FSolverReal CombinedInvMass = Particles.InvM(i0) + Particles.InvM(i1) + Particles.InvM(i2);

	FSolverVec3 DXDu, DXDv;
	CalculateUVStretch(ConstraintIndex, Particles.P(i0), Particles.P(i1), Particles.P(i2), DXDu, DXDv);

	const FSolverReal DXDuLength = DXDu.Length();
	const FSolverReal DXDvLength = DXDv.Length();
	auto SafeRecip = [](const FSolverReal Len, const FSolverReal Fallback)
	{
		if (Len > UE_SMALL_NUMBER)
		{
			return 1.f / Len;
		}
		return Fallback;
	};
	const FSolverReal OneOverDXDuLen = SafeRecip(DXDuLength, 0.f);
	const FSolverReal OneOverDXDvLen = SafeRecip(DXDvLength, 0.f);

	const FSolverVec3 DXDuNormalized = DXDu * OneOverDXDuLen;
	const FSolverVec3 DXDvNormalized = DXDv * OneOverDXDvLen;

	// constraints
	const FSolverReal Cu = (DXDuLength - RestStretchLengths[ConstraintIndex][0]*WeftScaleValue); // stretch in weft direction
	const FSolverReal Cv = (DXDvLength - RestStretchLengths[ConstraintIndex][1]*WarpScaleValue); // stretch in warp direction
	const FSolverReal Cs = FSolverVec3::DotProduct(DXDuNormalized, DXDvNormalized);


	const FSolverMatrix22& DeltaUVInv = DeltaUVInverse[ConstraintIndex];

	const FSolverVec3 GradCu1 = DXDuNormalized * DeltaUVInv.M[0];
	const FSolverVec3 GradCu2 = DXDuNormalized * DeltaUVInv.M[1];
	const FSolverVec3 GradCu0 = -GradCu1 - GradCu2;
	const FSolverVec3 GradCv1 = DXDvNormalized * DeltaUVInv.M[2];
	const FSolverVec3 GradCv2 = DXDvNormalized * DeltaUVInv.M[3];
	const FSolverVec3 GradCv0 = -GradCv1 - GradCv2;
	const FSolverVec3 GradCs1 = (DXDvNormalized * OneOverDXDuLen * DeltaUVInv.M[0] + DXDuNormalized * OneOverDXDvLen * DeltaUVInv.M[2]);
	const FSolverVec3 GradCs2 = (DXDvNormalized * OneOverDXDuLen * DeltaUVInv.M[1] + DXDuNormalized * OneOverDXDvLen * DeltaUVInv.M[3]);
	const FSolverVec3 GradCs0 = -GradCs1 - GradCs2;

	const FSolverVec3 V0TimesDt = Particles.P(i0) - Particles.X(i0);
	const FSolverVec3 V1TimesDt = Particles.P(i1) - Particles.X(i1);
	const FSolverVec3 V2TimesDt = Particles.P(i2) - Particles.X(i2);

	// These scale factors make everything resolution independent
	const FSolverVec3 FinalStiffnesses = StiffnessScales[ConstraintIndex] * ExpStiffnessValue;

	const FSolverVec3 Damping = (FSolverReal)2.f * DampingRatioValue * FMath::InvSqrt(CombinedInvMass) *
		FSolverVec3(FMath::Sqrt(FinalStiffnesses[0]), FMath::Sqrt(FinalStiffnesses[1]), FMath::Sqrt(FinalStiffnesses[2]));

	FSolverVec3& Lambda = Lambdas[ConstraintIndex];
	FSolverVec3 DLambda;
	DLambda[0] = CalcDLambda(FinalStiffnesses[0], Dt, Lambda[0], Damping[0], Particles.InvM(i0), Particles.InvM(i1), Particles.InvM(i2),
		V0TimesDt, V1TimesDt, V2TimesDt, Cu, GradCu0, GradCu1, GradCu2);
	DLambda[1] = CalcDLambda(FinalStiffnesses[1], Dt, Lambda[1], Damping[1], Particles.InvM(i0), Particles.InvM(i1), Particles.InvM(i2),
		V0TimesDt, V1TimesDt, V2TimesDt, Cv, GradCv0, GradCv1, GradCv2);
	DLambda[2] = CalcDLambda(FinalStiffnesses[2], Dt, Lambda[2], Damping[2], Particles.InvM(i0), Particles.InvM(i1), Particles.InvM(i2),
		V0TimesDt, V1TimesDt, V2TimesDt, Cs, GradCs0, GradCs1, GradCs2);

	Particles.P(i0) -= Particles.InvM(i0) * (DLambda[0] * GradCu0 + DLambda[1] * GradCv0 + DLambda[2] * GradCs0);
	Particles.P(i1) -= Particles.InvM(i1) * (DLambda[0] * GradCu1 + DLambda[1] * GradCv1 + DLambda[2] * GradCs1);
	Particles.P(i2) -= Particles.InvM(i2) * (DLambda[0] * GradCu2 + DLambda[1] * GradCv2 + DLambda[2] * GradCs2);

	Lambda += DLambda;
}

}  // End namespace Chaos::Softs
