// Copyright Epic Games, Inc. All Rights Reserved.
#include "ChaosCloth/ChaosClothingSimulationMesh.h"
#include "ChaosCloth/ChaosClothingSimulationSolver.h"
#include "ChaosCloth/ChaosWeightMapTarget.h"
#include "ChaosCloth/ChaosClothPrivate.h"
#include "Chaos/PBDSoftsSolverParticles.h" // Needed for PAndInvM in WrapDeformLOD
#include "ClothingSimulation.h"
#include "ClothingAsset.h"
#include "Containers/ArrayView.h"
#include "Components/SkeletalMeshComponent.h"
#include "Async/ParallelFor.h"
#if INTEL_ISPC
#include "ChaosClothingSimulationMesh.ispc.generated.h"
#endif

#if INTEL_ISPC && !UE_BUILD_SHIPPING
bool bChaos_SkinPhysicsMesh_ISPC_Enabled = CHAOS_SKIN_PHYSICS_MESH_ISPC_ENABLED_DEFAULT;
FAutoConsoleVariableRef CVarChaosSkinPhysicsMeshISPCEnabled(TEXT("p.Chaos.SkinPhysicsMesh.ISPC"), bChaos_SkinPhysicsMesh_ISPC_Enabled, TEXT("Whether to use ISPC optimizations on skinned physics meshes"));

static_assert(sizeof(ispc::FVector3f) == sizeof(Chaos::Softs::FSolverVec3), "sizeof(ispc::FVector3f) != sizeof(Chaos::Softs::FSolverVec3)");
static_assert(sizeof(ispc::FVector3f) == sizeof(FVector3f), "sizeof(ispc::FVector3f) != sizeof(FVector3f)");
static_assert(sizeof(ispc::FMatrix44f) == sizeof(FMatrix44f), "sizeof(ispc::FMatrix44f) != sizeof(FMatrix44f)");
static_assert(sizeof(ispc::FTransform3f) == sizeof(FTransform3f), "sizeof(ispc::FTransform3f) != sizeof(FTransform3f)");
static_assert(sizeof(ispc::FClothVertBoneData) == sizeof(FClothVertBoneData), "sizeof(ispc::FClothVertBoneData) != sizeof(Chaos::FClothVertBoneData)");
#endif

DECLARE_CYCLE_STAT(TEXT("Chaos Cloth Skin Physics Mesh"), STAT_ChaosClothSkinPhysicsMesh, STATGROUP_ChaosCloth);
DECLARE_CYCLE_STAT(TEXT("Chaos Cloth Wrap Deform Mesh"), STAT_ChaosClothWrapDeformMesh, STATGROUP_ChaosCloth);
DECLARE_CYCLE_STAT(TEXT("Chaos Cloth Wrap Deform Cloth LOD"), STAT_ChaosClothWrapDeformClothLOD, STATGROUP_ChaosCloth);

namespace Chaos
{

FClothingSimulationMesh::FClothingSimulationMesh(const UClothingAssetCommon* InAsset, const USkeletalMeshComponent* InSkeletalMeshComponent)
	: Asset(InAsset)
	, SkeletalMeshComponent(InSkeletalMeshComponent)
{
}

FClothingSimulationMesh::~FClothingSimulationMesh()
{
}

int32 FClothingSimulationMesh::GetNumLODs() const
{
	return Asset ? Asset->LodData.Num() : 0;
}

int32 FClothingSimulationMesh::GetLODIndex() const
{
	int32 LODIndex = INDEX_NONE;

	if (Asset && SkeletalMeshComponent)
	{
		const int32 MeshLODIndex = SkeletalMeshComponent->GetPredictedLODLevel();
		if (Asset->LodMap.IsValidIndex(MeshLODIndex))
		{
			const int32 ClothLODIndex = Asset->LodMap[MeshLODIndex];
			if (Asset->LodData.IsValidIndex(ClothLODIndex))
			{
				LODIndex = ClothLODIndex;
			}
		}
	}
	return LODIndex;
}

int32 FClothingSimulationMesh::GetNumPoints(int32 LODIndex) const
{
	return (Asset && Asset->LodData.IsValidIndex(LODIndex)) ?
		Asset->LodData[LODIndex].PhysicalMeshData.Vertices.Num() :
		0;
}

TConstArrayView<uint32> FClothingSimulationMesh::GetIndices(int32 LODIndex) const
{
	return (Asset && Asset->LodData.IsValidIndex(LODIndex)) ?
		TConstArrayView<uint32>(Asset->LodData[LODIndex].PhysicalMeshData.Indices) :
		TConstArrayView<uint32>();
}

TArray<TConstArrayView<FRealSingle>> FClothingSimulationMesh::GetWeightMaps(int32 LODIndex) const
{
	TArray<TConstArrayView<FRealSingle>> WeightMaps;
	if (Asset && Asset->LodData.IsValidIndex(LODIndex))
	{
		const FClothLODDataCommon& ClothLODData = Asset->LodData[LODIndex];
		const FClothPhysicalMeshData& ClothPhysicalMeshData = ClothLODData.PhysicalMeshData;

		const UEnum* const ChaosWeightMapTargetEnum = StaticEnum<EChaosWeightMapTarget>();
		const int32 NumWeightMaps = (int32)ChaosWeightMapTargetEnum->GetMaxEnumValue() + 1;

		WeightMaps.SetNum(NumWeightMaps);

		for (int32 EnumIndex = 0; EnumIndex < ChaosWeightMapTargetEnum->NumEnums(); ++EnumIndex)
		{
			const int32 TargetIndex = (int32)ChaosWeightMapTargetEnum->GetValueByIndex(EnumIndex);
			if (const FPointWeightMap* const WeightMap = ClothPhysicalMeshData.FindWeightMap(TargetIndex))
			{
				WeightMaps[TargetIndex] = WeightMap->Values;
			}
		}
	}
	return WeightMaps;
}

TArray<TConstArrayView<TTuple<int32, int32, float>>> FClothingSimulationMesh::GetTethers(int32 LODIndex, bool bUseGeodesicTethers) const
{
	TArray<TConstArrayView<TTuple<int32, int32, float>>> Tethers;
	if (Asset && Asset->LodData.IsValidIndex(LODIndex))
	{
		const FClothLODDataCommon& ClothLODData = Asset->LodData[LODIndex];
		const FClothPhysicalMeshData& ClothPhysicalMeshData = ClothLODData.PhysicalMeshData;
		const FClothTetherData& ClothTetherData = bUseGeodesicTethers ? ClothPhysicalMeshData.GeodesicTethers : ClothPhysicalMeshData.EuclideanTethers;
		
		const int32 NumTetherBatches = ClothTetherData.Tethers.Num();
		Tethers.Reserve(NumTetherBatches);
		for (int32 Index = 0; Index < NumTetherBatches; ++Index)
		{
			Tethers.Emplace(TConstArrayView<TTuple<int32, int32, float>>(ClothTetherData.Tethers[Index]));
		}
	}
	return Tethers;
}

int32 FClothingSimulationMesh::GetReferenceBoneIndex() const
{
	return Asset ? Asset->ReferenceBoneIndex : INDEX_NONE;
}

FRigidTransform3 FClothingSimulationMesh::GetReferenceBoneTransform() const
{
	if (SkeletalMeshComponent)
	{
		if (const FClothingSimulationContextCommon* const Context =
			static_cast<const FClothingSimulationContextCommon*>(SkeletalMeshComponent->GetClothingSimulationContext()))
		{
			const int32 ReferenceBoneIndex = GetReferenceBoneIndex();
			const TArray<FTransform>& BoneTransforms = Context->BoneTransforms;

			return BoneTransforms.IsValidIndex(ReferenceBoneIndex) ?
				BoneTransforms[ReferenceBoneIndex] * Context->ComponentToWorld :
				Context->ComponentToWorld;
		}
	}
	return FRigidTransform3::Identity;
}

Softs::FSolverReal FClothingSimulationMesh::GetScale() const
{
	if (SkeletalMeshComponent)
	{
		if (const FClothingSimulationContextCommon* const Context =
			static_cast<const FClothingSimulationContextCommon*>(SkeletalMeshComponent->GetClothingSimulationContext()))
		{
			return (Softs::FSolverReal)Context->ComponentToWorld.GetScale3D().GetMax();
		}
	}
	return (Softs::FSolverReal)1.;
}

bool FClothingSimulationMesh::WrapDeformLOD(
	int32 PrevLODIndex,
	int32 LODIndex,
	const FSolverVec3* Normals,
	const FSolverVec3* Positions,
	FSolverVec3* OutPositions) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FClothingSimulationMesh_WrapDeformLOD);
	SCOPE_CYCLE_COUNTER(STAT_ChaosClothWrapDeformMesh);

	const int32 NumLODsPassed = FMath::Abs(LODIndex - PrevLODIndex);
	if (NumLODsPassed != 1 || !Asset || !Asset->LodData.IsValidIndex(PrevLODIndex) || !Asset->LodData.IsValidIndex(LODIndex))
	{
		return false;
	}

	const FClothLODDataCommon& LODData = Asset->LodData[LODIndex];
	const int32 NumPoints = LODData.PhysicalMeshData.Vertices.Num();
	const TArray<FMeshToMeshVertData>& SkinData = (PrevLODIndex < LODIndex) ?
		LODData.TransitionUpSkinData :
		LODData.TransitionDownSkinData;

	for (int32 Index = 0; Index < NumPoints; ++Index)  // TODO: Profile for parallel for
	{
		const FMeshToMeshVertData& VertData = SkinData[Index];

		const int32 VertIndex0 = (int32)VertData.SourceMeshVertIndices[0];  // Note: The source is uint16. Watch out for large mesh sections!
		const int32 VertIndex1 = (int32)VertData.SourceMeshVertIndices[1];
		const int32 VertIndex2 = (int32)VertData.SourceMeshVertIndices[2];

		OutPositions[Index] = 
			Positions[VertIndex0] * (FSolverReal)VertData.PositionBaryCoordsAndDist.X + Normals[VertIndex0] * (FSolverReal)VertData.PositionBaryCoordsAndDist.W +
			Positions[VertIndex1] * (FSolverReal)VertData.PositionBaryCoordsAndDist.Y + Normals[VertIndex1] * (FSolverReal)VertData.PositionBaryCoordsAndDist.W +
			Positions[VertIndex2] * (FSolverReal)VertData.PositionBaryCoordsAndDist.Z + Normals[VertIndex2] * (FSolverReal)VertData.PositionBaryCoordsAndDist.W;
	}

	return true;
}

bool FClothingSimulationMesh::WrapDeformLOD(
	int32 PrevLODIndex,
	int32 LODIndex,
	const Softs::FSolverVec3* Normals,
	const Softs::FPAndInvM* PositionAndInvMs,
	const Softs::FSolverVec3* Velocities,
	Softs::FPAndInvM* OutPositionAndInvMs0,
	Softs::FSolverVec3* OutPositions1,
	Softs::FSolverVec3* OutVelocities) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FClothingSimulationMesh_WrapDeformLOD);
	SCOPE_CYCLE_COUNTER(STAT_ChaosClothWrapDeformClothLOD);

	const int32 NumLODsPassed = FMath::Abs(LODIndex - PrevLODIndex);
	if (NumLODsPassed != 1 || !Asset || !Asset->LodData.IsValidIndex(PrevLODIndex) || !Asset->LodData.IsValidIndex(LODIndex))
	{
		return false;
	}

	const FClothLODDataCommon& LODData = Asset->LodData[LODIndex];
	const int32 NumPoints = LODData.PhysicalMeshData.Vertices.Num();
	const TArray<FMeshToMeshVertData>& SkinData = (PrevLODIndex < LODIndex) ?
		LODData.TransitionUpSkinData :
		LODData.TransitionDownSkinData;

	for (int32 Index = 0; Index < NumPoints; ++Index)  // TODO: Profile for parallel for
	{
		const FMeshToMeshVertData& VertData = SkinData[Index];

		const int32 VertIndex0 = (int32)VertData.SourceMeshVertIndices[0];  // Note: The source is uint16. Watch out for large mesh sections!
		const int32 VertIndex1 = (int32)VertData.SourceMeshVertIndices[1];
		const int32 VertIndex2 = (int32)VertData.SourceMeshVertIndices[2];

		OutPositionAndInvMs0[Index].P = OutPositions1[Index] =
			PositionAndInvMs[VertIndex0].P * (FSolverReal)VertData.PositionBaryCoordsAndDist.X + Normals[VertIndex0] * (FSolverReal)VertData.PositionBaryCoordsAndDist.W +
			PositionAndInvMs[VertIndex1].P * (FSolverReal)VertData.PositionBaryCoordsAndDist.Y + Normals[VertIndex1] * (FSolverReal)VertData.PositionBaryCoordsAndDist.W +
			PositionAndInvMs[VertIndex2].P * (FSolverReal)VertData.PositionBaryCoordsAndDist.Z + Normals[VertIndex2] * (FSolverReal)VertData.PositionBaryCoordsAndDist.W;

		OutVelocities[Index] = 
			Velocities[VertIndex0] * (FSolverReal)VertData.PositionBaryCoordsAndDist.X +
			Velocities[VertIndex1] * (FSolverReal)VertData.PositionBaryCoordsAndDist.Y +
			Velocities[VertIndex2] * (FSolverReal)VertData.PositionBaryCoordsAndDist.Z;
	}

	return true;
}

// Inline function used to force the unrolling of the skinning loop, LWC: note skinning is all done in float to match the asset data type
FORCEINLINE static void AddInfluence(FVector3f& OutPosition, FVector3f& OutNormal, const FVector3f& RefParticle, const FVector3f& RefNormal, const FMatrix44f& BoneMatrix, const float Weight)
{
	OutPosition += BoneMatrix.TransformPosition(RefParticle) * Weight;
	OutNormal += BoneMatrix.TransformVector(RefNormal) * Weight;
}

void FClothingSimulationMesh::SkinPhysicsMesh(int32 LODIndex, const FVec3& LocalSpaceLocation, FSolverVec3* OutPositions, FSolverVec3* OutNormals) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FClothingSimulationMesh_SkinPhysicsMesh);
	SCOPE_CYCLE_COUNTER(STAT_ChaosClothSkinPhysicsMesh);
	SCOPE_CYCLE_COUNTER(STAT_ClothSkinPhysMesh);

	check(Asset && Asset->LodData.IsValidIndex(LODIndex));
	const FClothPhysicalMeshData& PhysicalMeshData = Asset->LodData[LODIndex].PhysicalMeshData;
	UE_CLOG(PhysicalMeshData.MaxBoneWeights > 12, LogChaosCloth, Warning, TEXT("The cloth physics mesh skinning code can't cope with more than 12 bone influences."));

	const uint32 NumPoints = PhysicalMeshData.Vertices.Num();

	check(SkeletalMeshComponent && SkeletalMeshComponent->GetClothingSimulationContext());
	const FClothingSimulationContextCommon* const Context = static_cast<const FClothingSimulationContextCommon*>(SkeletalMeshComponent->GetClothingSimulationContext());
	FTransform ComponentToLocalSpaceReal = Context->ComponentToWorld;
	ComponentToLocalSpaceReal.AddToTranslation(-LocalSpaceLocation);
	const FTransform3f ComponentToLocalSpace(ComponentToLocalSpaceReal);  // LWC: Now in local space, therefore it is safe to use single precision which is the asset data format

	const int32* const RESTRICT BoneMap = Asset->UsedBoneIndices.GetData();
	const FMatrix44f* const RESTRICT BoneMatrices = Context->RefToLocals.GetData();
		
#if INTEL_ISPC
	if (bChaos_SkinPhysicsMesh_ISPC_Enabled)
	{
		ispc::SkinPhysicsMesh(
			(ispc::FVector3f*)OutPositions,
			(ispc::FVector3f*)OutNormals,
			(ispc::FVector3f*)PhysicalMeshData.Vertices.GetData(),
			(ispc::FVector3f*)PhysicalMeshData.Normals.GetData(),
			(ispc::FClothVertBoneData*)PhysicalMeshData.BoneData.GetData(),
			BoneMap,
			(ispc::FMatrix44f*)BoneMatrices,
			(ispc::FTransform3f&)ComponentToLocalSpace,
			NumPoints);
	}
	else
#endif
	{
		static const uint32 MinParallelVertices = 500;  // 500 seems to be the lowest threshold still giving gains even on profiled assets that are only using a small number of influences

		ParallelFor(NumPoints, [&PhysicalMeshData, &ComponentToLocalSpace, BoneMap, BoneMatrices, &OutPositions, &OutNormals](uint32 VertIndex)
		{
			const uint16* const RESTRICT BoneIndices = PhysicalMeshData.BoneData[VertIndex].BoneIndices;
			const float* const RESTRICT BoneWeights = PhysicalMeshData.BoneData[VertIndex].BoneWeights;
	
			// WARNING - HORRIBLE UNROLLED LOOP + JUMP TABLE BELOW
			// done this way because this is a pretty tight and performance critical loop. essentially
			// rather than checking each influence we can just jump into this switch and fall through
			// everything to compose the final skinned data
			const FVector3f& RefParticle = PhysicalMeshData.Vertices[VertIndex];
			const FVector3f& RefNormal = PhysicalMeshData.Normals[VertIndex];

			FVector3f Position(ForceInitToZero);
			FVector3f Normal(ForceInitToZero);

			switch (PhysicalMeshData.BoneData[VertIndex].NumInfluences)
			{
			case 12: AddInfluence(Position, Normal, RefParticle, RefNormal, BoneMatrices[BoneMap[BoneIndices[11]]], BoneWeights[11]);  // Intentional fall through
			case 11: AddInfluence(Position, Normal, RefParticle, RefNormal, BoneMatrices[BoneMap[BoneIndices[10]]], BoneWeights[10]);  // Intentional fall through
			case 10: AddInfluence(Position, Normal, RefParticle, RefNormal, BoneMatrices[BoneMap[BoneIndices[ 9]]], BoneWeights[ 9]);  // Intentional fall through
			case  9: AddInfluence(Position, Normal, RefParticle, RefNormal, BoneMatrices[BoneMap[BoneIndices[ 8]]], BoneWeights[ 8]);  // Intentional fall through
			case  8: AddInfluence(Position, Normal, RefParticle, RefNormal, BoneMatrices[BoneMap[BoneIndices[ 7]]], BoneWeights[ 7]);  // Intentional fall through
			case  7: AddInfluence(Position, Normal, RefParticle, RefNormal, BoneMatrices[BoneMap[BoneIndices[ 6]]], BoneWeights[ 6]);  // Intentional fall through
			case  6: AddInfluence(Position, Normal, RefParticle, RefNormal, BoneMatrices[BoneMap[BoneIndices[ 5]]], BoneWeights[ 5]);  // Intentional fall through
			case  5: AddInfluence(Position, Normal, RefParticle, RefNormal, BoneMatrices[BoneMap[BoneIndices[ 4]]], BoneWeights[ 4]);  // Intentional fall through
			case  4: AddInfluence(Position, Normal, RefParticle, RefNormal, BoneMatrices[BoneMap[BoneIndices[ 3]]], BoneWeights[ 3]);  // Intentional fall through
			case  3: AddInfluence(Position, Normal, RefParticle, RefNormal, BoneMatrices[BoneMap[BoneIndices[ 2]]], BoneWeights[ 2]);  // Intentional fall through
			case  2: AddInfluence(Position, Normal, RefParticle, RefNormal, BoneMatrices[BoneMap[BoneIndices[ 1]]], BoneWeights[ 1]);  // Intentional fall through
			case  1: AddInfluence(Position, Normal, RefParticle, RefNormal, BoneMatrices[BoneMap[BoneIndices[ 0]]], BoneWeights[ 0]);  // Intentional fall through
			default: break;
			}

			OutPositions[VertIndex] = FSolverVec3(ComponentToLocalSpace.TransformPosition(Position));
			OutNormals[VertIndex] = FSolverVec3(ComponentToLocalSpace.TransformVector(Normal).GetSafeNormal());

		}, NumPoints > MinParallelVertices ? EParallelForFlags::None : EParallelForFlags::ForceSingleThread);
	}
}

void FClothingSimulationMesh::Update(
	FClothingSimulationSolver* Solver,
	int32 PrevLODIndex,
	int32 LODIndex,
	int32 PrevOffset,
	int32 Offset)
{
	check(Solver);

	// Exit if any inputs are missing or not ready, and if the LOD is invalid
	if (!Asset || !Asset->LodData.IsValidIndex(LODIndex) || !SkeletalMeshComponent || !SkeletalMeshComponent->GetClothingSimulationContext())
	{
		return;
	}

	// Skin current LOD positions
	const FVec3& LocalSpaceLocation = Solver->GetLocalSpaceLocation();
	FSolverVec3* const OutPositions = Solver->GetAnimationPositions(Offset);
	FSolverVec3* const OutNormals = Solver->GetAnimationNormals(Offset);
	
	SkinPhysicsMesh(LODIndex, LocalSpaceLocation, OutPositions, OutNormals);

	// Update old positions after LOD Switching
	if (LODIndex != PrevLODIndex)
	{
		// TODO: Using the more accurate skinning method here would require double buffering the context at the skeletal mesh level
		const FSolverVec3* const SrcWrapNormals = Solver->GetAnimationNormals(PrevOffset);  // No need to keep an old normals array around, since the LOD has just changed
		const FSolverVec3* const SrcWrapPositions = Solver->GetOldAnimationPositions(PrevOffset);
		FSolverVec3* const OutOldPositions = Solver->GetOldAnimationPositions(Offset);

		const bool bValidWrap = WrapDeformLOD(PrevLODIndex, LODIndex, SrcWrapNormals, SrcWrapPositions, OutOldPositions);
	
		if (!bValidWrap)
		{
			// The previous LOD is invalid, reset old positions with the new LOD
			const FClothPhysicalMeshData& PhysicalMeshData = Asset->LodData[LODIndex].PhysicalMeshData;
			const int32 NumPoints = PhysicalMeshData.Vertices.Num();

			for (int32 Index = 0; Index < NumPoints; ++Index)
			{
				OutOldPositions[Index] = OutPositions[Index];
			}
		}
	}
}

}  // End namespace Chaos
