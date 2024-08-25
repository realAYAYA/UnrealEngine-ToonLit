// Copyright Epic Games, Inc. All Rights Reserved.
#include "Chaos/PBDSpringConstraints.h"
#include "Chaos/PBDSoftsSolverParticles.h"
#include "Chaos/PBDRigidParticles.h"
#include "Chaos/GraphColoring.h"
#include "Chaos/Framework/Parallel.h"
#include "ChaosStats.h"
#include "ChaosLog.h"
#include "HAL/IConsoleManager.h"

#if INTEL_ISPC
#include "PBDSpringConstraints.ispc.generated.h"
#endif

DECLARE_CYCLE_STAT(TEXT("Chaos PBD Spring Constraint"), STAT_PBD_Spring, STATGROUP_Chaos);

#if INTEL_ISPC && !UE_BUILD_SHIPPING
static_assert(sizeof(ispc::FVector4f) == sizeof(Chaos::Softs::FPAndInvM), "sizeof(ispc::FVector4f) != sizeof(Chaos::Softs::FPAndInvM)");
static_assert(sizeof(ispc::FIntVector2) == sizeof(Chaos::TVec2<int32>), "sizeof(ispc::FIntVector2) != sizeof(Chaos::TVec2<int32>)");

bool bChaos_Spring_ISPC_Enabled = true;
FAutoConsoleVariableRef CVarChaosSpringISPCEnabled(TEXT("p.Chaos.Spring.ISPC"), bChaos_Spring_ISPC_Enabled, TEXT("Whether to use ISPC optimizations in Spring constraints"));
#endif

namespace Chaos::Softs {

// @todo(chaos): the parallel threshold (or decision to run parallel) should probably be owned by the solver and passed to the constraint container
static int32 Chaos_Spring_ParallelConstraintCount = 100;
#if !UE_BUILD_SHIPPING
FAutoConsoleVariableRef CVarChaosSpringParallelConstraintCount(TEXT("p.Chaos.Spring.ParallelConstraintCount"), Chaos_Spring_ParallelConstraintCount, TEXT("If we have more constraints than this, use parallel-for in Apply."));
#endif

template<typename SolverParticlesOrRange>
void FPBDSpringConstraints::InitColor(const SolverParticlesOrRange& Particles)
{
	// In dev builds we always color so we can tune the system without restarting. See Apply()
#if UE_BUILD_SHIPPING || UE_BUILD_TEST
	if (Constraints.Num() > Chaos_Spring_ParallelConstraintCount)
#endif
	{
		const TArray<TArray<int32>> ConstraintsPerColor = FGraphColoring::ComputeGraphColoringParticlesOrRange(Constraints, Particles, ParticleOffset, ParticleOffset + ParticleCount);
		
		// Reorder constraints based on color so each array in ConstraintsPerColor contains contiguous elements.
		TArray<TVec2<int32>> ReorderedConstraints;
		TArray<FSolverReal> ReorderedDists;
		TArray<int32> OrigToReorderedIndices; // used to reorder stiffness indices
		ReorderedConstraints.SetNumUninitialized(Constraints.Num());
		ReorderedDists.SetNumUninitialized(Dists.Num());
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
				ReorderedDists[ReorderedIndex] = Dists[OrigIndex];
				OrigToReorderedIndices[OrigIndex] = ReorderedIndex;

				++ReorderedIndex;
			}
		}
		ConstraintsPerColorStartIndex.Add(ReorderedIndex);

		Constraints = MoveTemp(ReorderedConstraints);
		Dists = MoveTemp(ReorderedDists);
		Stiffness.ReorderIndices(OrigToReorderedIndices);
	}
}
template CHAOS_API void FPBDSpringConstraints::InitColor(const FSolverParticles& Particles);
template CHAOS_API void FPBDSpringConstraints::InitColor(const FSolverParticlesRange& Particles);

template<typename SolverParticlesOrRange>
void FPBDSpringConstraints::ApplyHelper(SolverParticlesOrRange& Particles, const FSolverReal Dt, const int32 ConstraintIndex, const FSolverReal ExpStiffnessValue) const
{
	const TVec2<int32>& Constraint = Constraints[ConstraintIndex];
	const int32 i1 = Constraint[0];
	const int32 i2 = Constraint[1];
	const FSolverVec3 Delta =  Base::GetDelta(Particles, ConstraintIndex, ExpStiffnessValue);
	if (Particles.InvM(i1) > (FSolverReal)0.)
	{
		Particles.P(i1) -= Particles.InvM(i1) * Delta;
	}
	if (Particles.InvM(i2) > (FSolverReal)0.)
	{
		Particles.P(i2) += Particles.InvM(i2) * Delta;
	}
}

template<typename SolverParticlesOrRange>
void FPBDSpringConstraints::Apply(SolverParticlesOrRange& Particles, const FSolverReal Dt) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPBDSpringConstraints_Apply);
	SCOPE_CYCLE_COUNTER(STAT_PBD_Spring);
	if ((ConstraintsPerColorStartIndex.Num() > 1) && (Constraints.Num() > Chaos_Spring_ParallelConstraintCount))
	{
		const int32 ConstraintColorNum = ConstraintsPerColorStartIndex.Num() - 1;
		if (!Stiffness.HasWeightMap())
		{
			const FSolverReal ExpStiffnessValue = (FSolverReal)Stiffness;

#if INTEL_ISPC
			if (bRealTypeCompatibleWithISPC && bChaos_Spring_ISPC_Enabled)
			{
				for (int32 ConstraintColorIndex = 0; ConstraintColorIndex < ConstraintColorNum; ++ConstraintColorIndex)
				{
					const int32 ColorStart = ConstraintsPerColorStartIndex[ConstraintColorIndex];
					const int32 ColorSize = ConstraintsPerColorStartIndex[ConstraintColorIndex + 1] - ColorStart;
					ispc::ApplySpringConstraints(
						(ispc::FVector4f*)Particles.GetPAndInvM().GetData(),
						(ispc::FIntVector2*)&Constraints.GetData()[ColorStart],
						&Dists.GetData()[ColorStart],
						ExpStiffnessValue,
						ColorSize);
				}
			}
			else
#endif
			{
				for (int32 ConstraintColorIndex = 0; ConstraintColorIndex < ConstraintColorNum; ++ConstraintColorIndex)
				{
					const int32 ColorStart = ConstraintsPerColorStartIndex[ConstraintColorIndex];
					const int32 ColorSize = ConstraintsPerColorStartIndex[ConstraintColorIndex + 1] - ColorStart;
					PhysicsParallelFor(ColorSize, [this, &Particles, Dt, ColorStart, ExpStiffnessValue](const int32 Index)
					{
						const int32 ConstraintIndex = ColorStart + Index;
						ApplyHelper(Particles, Dt, ConstraintIndex, ExpStiffnessValue);
					});
				}
			}
		}
		else  // Has weight maps
		{
#if INTEL_ISPC
			if (bRealTypeCompatibleWithISPC && bChaos_Spring_ISPC_Enabled)
			{
				for (int32 ConstraintColorIndex = 0; ConstraintColorIndex < ConstraintColorNum; ++ConstraintColorIndex)
				{
					const int32 ColorStart = ConstraintsPerColorStartIndex[ConstraintColorIndex];
					const int32 ColorSize = ConstraintsPerColorStartIndex[ConstraintColorIndex + 1] - ColorStart;
					ispc::ApplySpringConstraintsWithWeightMaps(
						(ispc::FVector4f*)Particles.GetPAndInvM().GetData(),
						(ispc::FIntVector2*) & Constraints.GetData()[ColorStart],
						&Dists.GetData()[ColorStart],
						&Stiffness.GetIndices().GetData()[ColorStart],
						&Stiffness.GetTable().GetData()[0],
						ColorSize);
				}
			}
			else
#endif
			{
				for (int32 ConstraintColorIndex = 0; ConstraintColorIndex < ConstraintColorNum; ++ConstraintColorIndex)
				{
					const int32 ColorStart = ConstraintsPerColorStartIndex[ConstraintColorIndex];
					const int32 ColorSize = ConstraintsPerColorStartIndex[ConstraintColorIndex + 1] - ColorStart;
					PhysicsParallelFor(ColorSize, [this, &Particles, Dt, ColorStart](const int32 Index)
					{
						const int32 ConstraintIndex = ColorStart + Index;
						const FSolverReal ExpStiffnessValue = Stiffness[ConstraintIndex];
						ApplyHelper(Particles, Dt, ConstraintIndex, ExpStiffnessValue);
					});
				}
			}
		}
	}
	else
	{
		if (!Stiffness.HasWeightMap())
		{
			const FSolverReal ExpStiffnessValue = (FSolverReal)Stiffness;
			for (int32 ConstraintIndex = 0; ConstraintIndex < Constraints.Num(); ++ConstraintIndex)
			{
				ApplyHelper(Particles, Dt, ConstraintIndex, ExpStiffnessValue);
			}
		}
		else
		{
			for (int32 ConstraintIndex = 0; ConstraintIndex < Constraints.Num(); ++ConstraintIndex)
			{
				const FSolverReal ExpStiffnessValue = Stiffness[ConstraintIndex];
				ApplyHelper(Particles, Dt, ConstraintIndex, ExpStiffnessValue);
			}
		}
	}
}
template CHAOS_API void FPBDSpringConstraints::Apply(FSolverParticles& Particles, const FSolverReal Dt) const;
template CHAOS_API void FPBDSpringConstraints::Apply(FSolverParticlesRange& Particles, const FSolverReal Dt) const;

void FPBDEdgeSpringConstraints::SetProperties(
	const FCollectionPropertyConstFacade& PropertyCollection,
	const TMap<FString, TConstArrayView<FRealSingle>>& WeightMaps)
{
	if (IsEdgeSpringStiffnessMutable(PropertyCollection))
	{
		const FSolverVec2 WeightedValue(GetWeightedFloatEdgeSpringStiffness(PropertyCollection));
		if (IsEdgeSpringStiffnessStringDirty(PropertyCollection))
		{
			const FString& WeightMapName = GetEdgeSpringStiffnessString(PropertyCollection);
			Stiffness = FPBDStiffness(
				WeightedValue,
				WeightMaps.FindRef(WeightMapName),
				TConstArrayView<TVec2<int32>>(Constraints),
				ParticleOffset,
				ParticleCount);
		}
		else
		{
			Stiffness.SetWeightedValue(WeightedValue);
		}
	}
}

void FPBDBendingSpringConstraints::SetProperties(
	const FCollectionPropertyConstFacade& PropertyCollection,
	const TMap<FString, TConstArrayView<FRealSingle>>& WeightMaps)
{
	if (IsBendingSpringStiffnessMutable(PropertyCollection))
	{
		const FSolverVec2 WeightedValue(GetWeightedFloatBendingSpringStiffness(PropertyCollection));
		if (IsBendingSpringStiffnessStringDirty(PropertyCollection))
		{
			const FString& WeightMapName = GetBendingSpringStiffnessString(PropertyCollection);
			Stiffness = FPBDStiffness(
				WeightedValue,
				WeightMaps.FindRef(WeightMapName),
				TConstArrayView<TVec2<int32>>(Constraints),
				ParticleOffset,
				ParticleCount);
		}
		else
		{
			Stiffness.SetWeightedValue(WeightedValue);
		}
	}
}

} // End namespace Chaos::Softs
