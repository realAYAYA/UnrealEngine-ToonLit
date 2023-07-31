// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraDataInterfaceSkeletalMesh.h"

#include "Animation/SkeletalMeshActor.h"
#include "Async/ParallelFor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Canvas.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Internationalization/Internationalization.h"
#include "NDISkeletalMeshCommon.h"
#include "NiagaraEmitterInstance.h"
#include "NiagaraComponent.h"
#include "NiagaraDataInterfaceSkeletalMeshConnectivity.h"
#include "Experimental/NiagaraDataInterfaceSkeletalMeshUvMapping.h"
#include "NiagaraSettings.h"
#include "NiagaraStats.h"
#include "NiagaraSystemInstance.h"
#include "NiagaraRenderer.h"
#include "NiagaraScript.h"
#include "NiagaraShaderParametersBuilder.h"
#include "NiagaraWorldManager.h"
#include "Templates/AlignmentTemplates.h"
#include "ShaderParameterUtils.h"
#include "ShaderCore.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NiagaraDataInterfaceSkeletalMesh)

#define LOCTEXT_NAMESPACE "NiagaraDataInterfaceSkeletalMesh"

DECLARE_CYCLE_STAT(TEXT("PreSkin"), STAT_NiagaraSkel_PreSkin, STATGROUP_Niagara);

struct FNiagaraSkelMeshDIFunctionVersion
{
	enum Type
	{
		InitialVersion = 0,
		AddedRandomInfo = 1,
		CleanUpVertexSampling = 2,
		CleanupBoneSampling = 3,
		AddTangentBasisToGetSkinnedVertexData = 4,
		RemoveUvSetFromMapping = 5,
		AddedEnabledUvMapping = 6,
		AddedValidConnectivity = 7,
		AddedInputBardCoordToGetFilteredTriangleAt = 8,
		LargeWorldCoordinates = 9,
		LargeWorldCoordinates2 = 10,
		AddBoneScale = 11,

		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};
};

namespace NDISkelMeshLocal
{
	static FName NAME_GetPreSkinnedLocalBounds("GetPreSkinnedLocalBounds");

	static const TCHAR* CommonShaderFile = TEXT("/Plugin/FX/Niagara/Private/NiagaraDataInterfaceSkeletalMesh.ush");
	static const TCHAR* TemplateShaderFile = TEXT("/Plugin/FX/Niagara/Private/NiagaraDataInterfaceSkeletalMeshTemplate.ush");

	BEGIN_SHADER_PARAMETER_STRUCT(FShaderParameters,)
		SHADER_PARAMETER_SRV(Buffer<uint>,		MeshIndexBuffer)
		SHADER_PARAMETER_SRV(Buffer<float>,		MeshVertexBuffer)
		SHADER_PARAMETER_SRV(Buffer<uint>,		MeshSkinWeightBuffer)
		SHADER_PARAMETER_SRV(Buffer<uint>,		MeshSkinWeightLookupBuffer)
		SHADER_PARAMETER_SRV(Buffer<float4>,	MeshCurrBonesBuffer)
		SHADER_PARAMETER_SRV(Buffer<float4>,	MeshPrevBonesBuffer)
		SHADER_PARAMETER_SRV(Buffer<float4>,	MeshCurrSamplingBonesBuffer)
		SHADER_PARAMETER_SRV(Buffer<float4>,	MeshPrevSamplingBonesBuffer)
		SHADER_PARAMETER_SRV(Buffer<float4>,	MeshTangentBuffer)
		SHADER_PARAMETER_SRV(Buffer<float2>,	MeshTexCoordBuffer)
		SHADER_PARAMETER_SRV(Buffer<float4>,	MeshColorBuffer)
		SHADER_PARAMETER_SRV(Buffer<uint>,		MeshTriangleSamplerProbAliasBuffer)
		SHADER_PARAMETER(uint32,				MeshNumSamplingRegionTriangles)
		SHADER_PARAMETER(uint32,				MeshNumSamplingRegionVertices)
		SHADER_PARAMETER_SRV(Buffer<uint>,		MeshSamplingRegionsProbAliasBuffer)
		SHADER_PARAMETER_SRV(Buffer<uint>,		MeshSampleRegionsTriangleIndices)
		SHADER_PARAMETER_SRV(Buffer<uint>,		MeshSampleRegionsVertices)
		SHADER_PARAMETER_SRV(Buffer<uint>,		MeshTriangleMatricesOffsetBuffer)
		SHADER_PARAMETER(uint32,				MeshTriangleCount)
		SHADER_PARAMETER(uint32,				MeshVertexCount)
		SHADER_PARAMETER(uint32,				MeshWeightStride)
		SHADER_PARAMETER(uint32,				MeshSkinWeightIndexSize)
		SHADER_PARAMETER(uint32,				MeshNumTexCoord)
		SHADER_PARAMETER(uint32,				MeshNumWeights)
		SHADER_PARAMETER(int,					NumBones)
		SHADER_PARAMETER(int,					NumFilteredBones)
		SHADER_PARAMETER(int,					NumUnfilteredBones)
		SHADER_PARAMETER(int,					RandomMaxBone)
		SHADER_PARAMETER(int,					ExcludeBoneIndex)
		SHADER_PARAMETER_SRV(Buffer<uint>,		FilteredAndUnfilteredBones)
		SHADER_PARAMETER(int,					NumFilteredSockets)
		SHADER_PARAMETER(int,					FilteredSocketBoneOffset)
		SHADER_PARAMETER_SRV(Buffer<int>,		UvMappingBuffer)
		SHADER_PARAMETER(uint32,				UvMappingBufferLength)
		SHADER_PARAMETER(uint32,				UvMappingSet)
		SHADER_PARAMETER_SRV(Buffer<uint>,		ConnectivityBuffer)
		SHADER_PARAMETER(uint32,				ConnectivityBufferLength)
		SHADER_PARAMETER(uint32,				ConnectivityMaxAdjacentPerVertex)
		SHADER_PARAMETER(FMatrix44f,			InstanceTransform)
		SHADER_PARAMETER(FMatrix44f,			InstancePrevTransform)
		SHADER_PARAMETER(FQuat4f,				InstanceRotation)
		SHADER_PARAMETER(FQuat4f,				InstancePrevRotation)
		SHADER_PARAMETER(float,					InstanceInvDeltaTime)
		SHADER_PARAMETER(FVector3f,				PreSkinnedLocalBoundsCenter)
		SHADER_PARAMETER(FVector3f,				PreSkinnedLocalBoundsExtents)
		SHADER_PARAMETER(uint32,				EnabledFeatures)
	END_SHADER_PARAMETER_STRUCT()

	int32 GetProbAliasDWORDSize(int32 TriangleCount)
	{
		const ENDISkelMesh_GpuUniformSamplingFormat::Type Format = GetDefault<UNiagaraSettings>()->NDISkelMesh_GpuUniformSamplingFormat;
		switch (Format)
		{
			case ENDISkelMesh_GpuUniformSamplingFormat::Full:
				return TriangleCount * 2;
			case ENDISkelMesh_GpuUniformSamplingFormat::Limited_24_8:
			case ENDISkelMesh_GpuUniformSamplingFormat::Limited_23_9:
				return TriangleCount * 1;
			default:
				UE_LOG(LogNiagara, Fatal, TEXT("GpuUniformSamplingFormat %d is invalid"), Format);
				return 0;
		}
	}

	void PackProbAlias(uint32* Dest, const FSkeletalMeshAreaWeightedTriangleSampler& triangleSampler, int32 AliasOffset = 0)
	{
		TArrayView<const float> ProbArray = triangleSampler.GetProb();
		TArrayView<const int32> AliasArray = triangleSampler.GetAlias();

		const ENDISkelMesh_GpuUniformSamplingFormat::Type Format = GetDefault<UNiagaraSettings>()->NDISkelMesh_GpuUniformSamplingFormat;
		switch (Format)
		{
			case ENDISkelMesh_GpuUniformSamplingFormat::Full:
			{
				for (int32 i = 0; i < triangleSampler.GetNumEntries(); ++i)
				{
					const float Probability = ProbArray[i];
					const int32 Alias = AliasArray[i] + AliasOffset;
					* Dest++ = *reinterpret_cast<const uint32*>(&Probability);
					*Dest++ = Alias;
				}
				break;
			}
			case ENDISkelMesh_GpuUniformSamplingFormat::Limited_24_8:
			{
				for (int32 i = 0; i < triangleSampler.GetNumEntries(); ++i)
				{
					const float Probability = ProbArray[i];
					const int32 Alias = AliasArray[i] + AliasOffset;
					if (ensureMsgf(Alias <= 0xffffff, TEXT("Triangle Alias %d is higher than possible %d"), Alias, 0xffffff))
					{
						*Dest++ = (Alias << 8) | (int(FMath::Clamp(Probability, 0.0f, 1.0f) * 255.0f) & 0xff);
					}
					else
					{
						*Dest++ = 0;
					}
				}

				break;
			}
			case ENDISkelMesh_GpuUniformSamplingFormat::Limited_23_9:
			{
				for (int32 i = 0; i < triangleSampler.GetNumEntries(); ++i)
				{
					const float Probability = ProbArray[i];
					const int32 Alias = AliasArray[i] + AliasOffset;
					if (ensureMsgf(Alias <= 0x7fffff, TEXT("Triangle Alias %d is higher than possible %d"), Alias, 0x7fffff))
					{
						*Dest++ = (Alias << 9) | (int(FMath::Clamp(Probability, 0.0f, 1.0f) * 511.0f) & 0x1ff);
					}
					else
					{
						*Dest++ = 0;
					}
				}
				break;
			}
			default:
				UE_LOG(LogNiagara, Fatal, TEXT("GpuUniformSamplingFormat %d is invalid"), Format);
				break;
		}
	}

	// Calculate which tick group the skeletal mesh component will be ready by
	ETickingGroup GetComponentTickGroup(USkeletalMeshComponent* Component)
	{
		const ETickingGroup ComponentTickGroup = FMath::Max(Component->PrimaryComponentTick.TickGroup, Component->PrimaryComponentTick.EndTickGroup);
		const ETickingGroup PhysicsTickGroup = Component->bBlendPhysics ? FMath::Max(ComponentTickGroup, TG_EndPhysics) : ComponentTickGroup;
		const ETickingGroup ClampedTickGroup = FMath::Clamp(ETickingGroup(PhysicsTickGroup + 1), NiagaraFirstTickGroup, NiagaraLastTickGroup);
		return ClampedTickGroup;
	}
}

//////////////////////////////////////////////////////////////////////////

FSkeletalMeshSamplingRegionAreaWeightedSampler::FSkeletalMeshSamplingRegionAreaWeightedSampler()
	: Owner(nullptr)
{
}

void FSkeletalMeshSamplingRegionAreaWeightedSampler::Init(FNDISkeletalMesh_InstanceData* InOwner)
{
	Owner = InOwner;
	Initialize();
}

float FSkeletalMeshSamplingRegionAreaWeightedSampler::GetWeights(TArray<float>& OutWeights)
{
	check(Owner);

	USkeletalMesh* SkelMesh = Owner->SkeletalMesh.Get();
	if (SkelMesh == nullptr)
	{
		OutWeights.Empty();
		return 0.0f;
	}

	check(SkelMesh->IsValidLODIndex(Owner->GetLODIndex()));

	float Total = 0.0f;
	int32 NumUsedRegions = Owner->SamplingRegionIndices.Num();
	if (NumUsedRegions <= 1)
	{
		//Use 0 or 1 Sampling region. Only need additional area weighting between regions if we're sampling from multiple.
		OutWeights.Empty();
		return 0.0f;
	}

	const FSkeletalMeshSamplingInfo& SamplingInfo = SkelMesh->GetSamplingInfo();
	OutWeights.Empty(NumUsedRegions);
	for (int32 i = 0; i < NumUsedRegions; ++i)
	{
		int32 RegionIdx = Owner->SamplingRegionIndices[i];
		float T = SamplingInfo.GetRegionBuiltData(RegionIdx).AreaWeightedSampler.GetTotalWeight();
		OutWeights.Add(T);
		Total += T;
	}
	return Total;
}

//////////////////////////////////////////////////////////////////////////
FSkeletalMeshSkinningDataHandle::FSkeletalMeshSkinningDataHandle()
	: SkinningData(nullptr)
{
}

FSkeletalMeshSkinningDataHandle::FSkeletalMeshSkinningDataHandle(FSkeletalMeshSkinningDataUsage InUsage, const TSharedPtr<FSkeletalMeshSkinningData>& InSkinningData, bool bNeedsDataImmediately)
	: Usage(InUsage)
	, SkinningData(InSkinningData)
{
	if (FSkeletalMeshSkinningData* SkinData = SkinningData.Get())
	{
		SkinData->RegisterUser(Usage, bNeedsDataImmediately);
	}
}

FSkeletalMeshSkinningDataHandle::~FSkeletalMeshSkinningDataHandle()
{
	if (FSkeletalMeshSkinningData* SkinData = SkinningData.Get())
	{
		SkinData->UnregisterUser(Usage);
	}
}

FSkeletalMeshSkinningDataHandle::FSkeletalMeshSkinningDataHandle(FSkeletalMeshSkinningDataHandle&& Other) noexcept
{
	Usage = Other.Usage;
	SkinningData = Other.SkinningData;
	Other.SkinningData = nullptr;
}

FSkeletalMeshSkinningDataHandle& FSkeletalMeshSkinningDataHandle::operator=(FSkeletalMeshSkinningDataHandle&& Other) noexcept
{
	if (this != &Other)
	{
		Usage = Other.Usage;
		SkinningData = Other.SkinningData;
		Other.SkinningData = nullptr;
	}
	return *this;
}

//////////////////////////////////////////////////////////////////////////
void FSkeletalMeshSkinningData::ForceDataRefresh()
{
	FRWScopeLock Lock(RWGuard, SLT_Write);
	bForceDataRefresh = true;
}

void FSkeletalMeshSkinningData::RegisterUser(FSkeletalMeshSkinningDataUsage Usage, bool bNeedsDataImmediately)
{
	FRWScopeLock Lock(RWGuard, SLT_Write);

	USkeletalMeshComponent* SkelComp = MeshComp.Get();
	check(SkelComp);

	USkeletalMesh* SkelMesh = SkelComp->GetSkeletalMeshAsset();
	int32 LODIndex = 0;
	int32 NumLODInfo = 1;

	if (SkelMesh != nullptr)
	{
		NumLODInfo = SkelMesh->GetLODInfoArray().Num();
		LODIndex = Usage.GetLODIndex();
		check(LODIndex != INDEX_NONE);
		check(LODIndex < NumLODInfo);
	}

	LODData.SetNum(NumLODInfo);

	if (Usage.NeedBoneMatrices())
	{
		++BoneMatrixUsers;
	}

	FLODData& LOD = LODData[LODIndex];
	if (Usage.NeedPreSkinnedVerts())
	{
		++LOD.PreSkinnedVertsUsers;
		++TotalPreSkinnedVertsUsers;
	}

	if (bNeedsDataImmediately)
	{
		check(IsInGameThread());
		if (CurrBoneRefToLocals().Num() == 0 || CurrComponentTransforms().Num() == 0 )
		{
			UpdateBoneTransforms();
		}

		//Prime the prev matrices if they're missing.
		if (PrevBoneRefToLocals().Num() != CurrBoneRefToLocals().Num())
		{
			PrevBoneRefToLocals() = CurrBoneRefToLocals();
		}

		if (PrevComponentTransforms().Num() != CurrComponentTransforms().Num())
		{
			PrevComponentTransforms() = CurrComponentTransforms();
		}

		if (Usage.NeedPreSkinnedVerts() && CurrSkinnedPositions(LODIndex).Num() == 0 && SkelMesh && SkelMesh->GetLODInfo(LODIndex)->bAllowCPUAccess)
		{
			FSkeletalMeshLODRenderData& SkelMeshLODData = SkelMesh->GetResourceForRendering()->LODRenderData[LODIndex];
			FSkinWeightVertexBuffer* SkinWeightBuffer = SkelComp->GetSkinWeightBuffer(LODIndex);
			USkeletalMeshComponent::ComputeSkinnedPositions(SkelComp, CurrSkinnedPositions(LODIndex), CurrBoneRefToLocals(), SkelMeshLODData, *SkinWeightBuffer);
			USkeletalMeshComponent::ComputeSkinnedTangentBasis(SkelComp, CurrSkinnedTangentBasis(LODIndex), CurrBoneRefToLocals(), SkelMeshLODData, *SkinWeightBuffer);

			//Prime the previous positions if they're missing
			if (PrevSkinnedPositions(LODIndex).Num() != CurrSkinnedPositions(LODIndex).Num())
			{
				PrevSkinnedPositions(LODIndex) = CurrSkinnedPositions(LODIndex);
			}
			if (PrevSkinnedTangentBasis(LODIndex).Num() != CurrSkinnedTangentBasis(LODIndex).Num())
			{
				PrevSkinnedTangentBasis(LODIndex) = CurrSkinnedTangentBasis(LODIndex);
			}
		}
	}
}

void FSkeletalMeshSkinningData::UnregisterUser(FSkeletalMeshSkinningDataUsage Usage)
{
	FRWScopeLock Lock(RWGuard, SLT_Write);

	if (Usage.NeedBoneMatrices())
	{
		--BoneMatrixUsers;
	}

	int32 LODIndex = 0;

	USkeletalMeshComponent* SkelComp = MeshComp.Get();
	if (SkelComp && SkelComp->GetSkeletalMeshAsset())
	{
		LODIndex = Usage.GetLODIndex();
	}

	// The first Niagara instance that detects a change to the SkeletalMeshComponent's SkeletalMesh will execute a
	// Unregister / Register which will modify the LOD count to the correct new count.  This means that a subsequent
	// Unregister call can be pointing to a LODIndex that is no longer valid.  We can safely ignore this as we do not
	// need to decrement the counters.
	if (Usage.NeedPreSkinnedVerts())
	{
		--TotalPreSkinnedVertsUsers;
		if (LODData.IsValidIndex(LODIndex))
		{
			--LODData[LODIndex].PreSkinnedVertsUsers;
		}
	}
}

bool FSkeletalMeshSkinningData::IsUsed() const
{
	if (BoneMatrixUsers > 0)
	{
		return true;
	}

	for (const FLODData& LOD : LODData)
	{
		if (LOD.PreSkinnedVertsUsers > 0)
		{
			return true;
		}
	}

	return false;
}

void FSkeletalMeshSkinningData::UpdateBoneTransforms()
{
	USkeletalMeshComponent* SkelComp = MeshComp.Get();
	check(SkelComp);

	const USkeletalMesh* SkelMesh = SkelComp->GetSkeletalMeshAsset();
	if (SkelMesh == nullptr)
	{
		return;
	}

	TArray<FMatrix44f>& CurrBones = CurrBoneRefToLocals();
	TArray<FTransform3f>& CurrTransforms = CurrComponentTransforms();

	if (USkinnedMeshComponent* LeaderComponent = SkelComp->LeaderPoseComponent.Get())
	{
		const TArray<int32>& LeaderBoneMap = SkelComp->GetLeaderBoneMap();
		const int32 NumBones = LeaderBoneMap.Num();

		if (NumBones == 0)
		{
			// This case indicates an invalid leader pose component (e.g. no skeletal mesh)
			CurrBones.Empty(SkelMesh->GetRefSkeleton().GetNum());
			CurrBones.AddDefaulted(SkelMesh->GetRefSkeleton().GetNum());
			CurrTransforms.Empty(SkelMesh->GetRefSkeleton().GetNum());
			CurrTransforms.AddDefaulted(SkelMesh->GetRefSkeleton().GetNum());
		}
		else
		{
			CurrBones.SetNumUninitialized(NumBones);
			CurrTransforms.SetNumUninitialized(NumBones);

			const TArray<FTransform>& LeaderTransforms = LeaderComponent->GetComponentSpaceTransforms();
			for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
			{
				bool bFoundLeader = false;
				if (LeaderBoneMap.IsValidIndex(BoneIndex))
				{
					const int32 LeaderIndex = LeaderBoneMap[BoneIndex];
					if (LeaderIndex != INDEX_NONE && LeaderIndex < LeaderTransforms.Num())
					{
						bFoundLeader = true;
						CurrTransforms[BoneIndex] = (FTransform3f)LeaderTransforms[LeaderIndex];
					}
				}

				if ( !bFoundLeader)
				{
					const int32 ParentIndex = SkelMesh->GetRefSkeleton().GetParentIndex(BoneIndex);
					FTransform3f BoneTransform = (FTransform3f)SkelMesh->GetRefSkeleton().GetRefBonePose()[BoneIndex];
					if ((ParentIndex >= 0) && (ParentIndex < BoneIndex))
					{
						BoneTransform = BoneTransform * CurrTransforms[ParentIndex];
					}
					CurrTransforms[BoneIndex] = BoneTransform;
				}

				if (SkelMesh->GetRefBasesInvMatrix().IsValidIndex(BoneIndex))
				{
					CurrBones[BoneIndex] = SkelMesh->GetRefBasesInvMatrix()[BoneIndex] * CurrTransforms[BoneIndex].ToMatrixWithScale();
				}
				else
				{
					CurrBones[BoneIndex] = CurrTransforms[BoneIndex].ToMatrixWithScale();
				}
			}
		}
	}
	else
	{
		SkelComp->CacheRefToLocalMatrices(CurrBones);
		CurrTransforms = UE::LWC::ConvertArrayType<FTransform3f>(SkelComp->GetComponentSpaceTransforms());	// LWC_TODO: Perf pessimization
	}
}

bool FSkeletalMeshSkinningData::Tick(float InDeltaSeconds, bool bRequirePreskin)
{
	FRWScopeLock Lock(RWGuard, SLT_Write);

	USkeletalMeshComponent* SkelComp = MeshComp.Get();
	check(SkelComp);
	DeltaSeconds = InDeltaSeconds;
	CurrIndex ^= 1;

	if (BoneMatrixUsers > 0)
	{
		UpdateBoneTransforms();
	}

	//Prime the prev matrices if they're missing.
	if ((PrevBoneRefToLocals().Num() != CurrBoneRefToLocals().Num()) || bForceDataRefresh)
	{
		PrevBoneRefToLocals() = CurrBoneRefToLocals();
	}

	if ((PrevComponentTransforms().Num() != CurrComponentTransforms().Num()) || bForceDataRefresh)
	{
		PrevComponentTransforms() = CurrComponentTransforms();
	}

	if (bRequirePreskin && SkelComp->GetSkeletalMeshAsset() != nullptr)
	{
		const USkeletalMesh* SkeletalMesh = SkelComp->GetSkeletalMeshAsset();
		const FSkeletalMeshRenderData* RenderData = SkeletalMesh->GetResourceForRendering();
		check(RenderData);

		for (int32 LODIndex = RenderData->PendingFirstLODIdx; LODIndex < LODData.Num(); ++LODIndex)
		{
			FLODData& LOD = LODData[LODIndex];
			if (LOD.PreSkinnedVertsUsers > 0 && SkeletalMesh->GetLODInfo(LODIndex)->bAllowCPUAccess)
			{
				// Increment ref count to prevent stream out for happening while we are processing the CPU data.
				TRefCountPtr<const FSkeletalMeshLODRenderData> SkelMeshLODData = &RenderData->LODRenderData[LODIndex];
				//TODO: If we pass the sections in the usage too, we can probably skin a minimal set of verts just for the used regions.
				FSkinWeightVertexBuffer* SkinWeightBuffer = SkelComp->GetSkinWeightBuffer(LODIndex);
				USkeletalMeshComponent::ComputeSkinnedPositions(SkelComp, CurrSkinnedPositions(LODIndex), CurrBoneRefToLocals(), *SkelMeshLODData, *SkinWeightBuffer);
				USkeletalMeshComponent::ComputeSkinnedTangentBasis(SkelComp, CurrSkinnedTangentBasis(LODIndex), CurrBoneRefToLocals(), *SkelMeshLODData, *SkinWeightBuffer);
				//check(CurrSkinnedPositions(LODIndex).Num() == SkelMeshLODData.NumVertices);
				//Prime the previous positions if they're missing
				if (PrevSkinnedPositions(LODIndex).Num() != CurrSkinnedPositions(LODIndex).Num())
				{
					PrevSkinnedPositions(LODIndex) = CurrSkinnedPositions(LODIndex);
				}
				if (PrevSkinnedTangentBasis(LODIndex).Num() != CurrSkinnedTangentBasis(LODIndex).Num())
				{
					PrevSkinnedTangentBasis(LODIndex) = CurrSkinnedTangentBasis(LODIndex);
				}
			}
		}
	}

	bForceDataRefresh = false;
	return true;
}

//////////////////////////////////////////////////////////////////////////

FSkeletalMeshSkinningDataHandle FNDI_SkeletalMesh_GeneratedData::GetCachedSkinningData(TWeakObjectPtr<USkeletalMeshComponent>& Component, FSkeletalMeshSkinningDataUsage Usage, bool bNeedsDataImmediately)
{
	check(Component.Get() != nullptr);

	// Attempt to Find data
	{
		FRWScopeLock ReadLock(CachedSkinningDataGuard, SLT_ReadOnly);
		if (TSharedPtr<FSkeletalMeshSkinningData>* Existing = CachedSkinningData.Find(Component))
		{
			return FSkeletalMeshSkinningDataHandle(Usage, *Existing, bNeedsDataImmediately);
		}
	}

	// We need to add
	FRWScopeLock WriteLock(CachedSkinningDataGuard, SLT_Write);
	return FSkeletalMeshSkinningDataHandle(
		Usage,
		CachedSkinningData.Add(Component, MakeShared<FSkeletalMeshSkinningData>(Component)),
		bNeedsDataImmediately);
}

void FNDI_SkeletalMesh_GeneratedData::Tick(ETickingGroup TickGroup, float DeltaSeconds)
{
	check(IsInGameThread());
	SCOPE_CYCLE_COUNTER(STAT_NiagaraSkel_PreSkin);

	FRWScopeLock WriteLock(CachedSkinningDataGuard, SLT_Write);

	// We may want to look at separating out how we manage the ticks here
	//-OPT: Move into different arrays per tick group, manage promotions, demotions, etc, or add ourselves as a subsequent of the component's tick
	TArray<TWeakObjectPtr<USkeletalMeshComponent>, TInlineAllocator<32>> ToRemove;
	TArray<FSkeletalMeshSkinningData*, TInlineAllocator<32>> ToTickBonesOnly;
	TArray<FSkeletalMeshSkinningData*, TInlineAllocator<32>> ToTickPreskin;
	const bool bForceTick = TickGroup == NiagaraLastTickGroup;

	ToTickBonesOnly.Reserve(CachedSkinningData.Num());
	ToTickPreskin.Reserve(CachedSkinningData.Num());

	for (TPair<TWeakObjectPtr<USkeletalMeshComponent>, TSharedPtr<FSkeletalMeshSkinningData>>& Pair : CachedSkinningData)
	{
		USkeletalMeshComponent* Component = Pair.Key.Get();
		TSharedPtr<FSkeletalMeshSkinningData>& SkinningData = Pair.Value;

		if ( TickGroup == NiagaraFirstTickGroup )
		{
			SkinningData->bHasTicked = false;
		}

		// Should remove?
		if ( (Component == nullptr) || SkinningData.IsUnique() || !SkinningData->IsUsed() )
		{
			ToRemove.Add(Pair.Key);
			continue;
		}

		if ( SkinningData->bHasTicked == true )
		{
			continue;
		}

		// Has ticked or can be ticked
		if (bForceTick == false)
		{
			const ETickingGroup PrereqTickGroup = NDISkelMeshLocal::GetComponentTickGroup(Component);
			if ( PrereqTickGroup > TickGroup )
			{
				continue;
			}
		}

		// We are going to tick this one
		SkinningData->bHasTicked = true;

		FSkeletalMeshSkinningData* SkinningDataPtr = SkinningData.Get();
		check(SkinningDataPtr);

		if (SkinningDataPtr->NeedPreSkinnedVerts())
		{
			ToTickPreskin.Add(SkinningDataPtr);
		}
		else
		{
			ToTickBonesOnly.Add(SkinningDataPtr);
		}
	}

	for (TWeakObjectPtr<USkeletalMeshComponent> Key : ToRemove)
	{
		CachedSkinningData.Remove(Key);
	}

	// First tick the meshes that don't need pre-skinning.
	// This prevents additional threading overhead when we don't need to pre-skin.
	for (int i = 0; i < ToTickBonesOnly.Num(); i++)
	{
		ToTickBonesOnly[i]->Tick(DeltaSeconds, false);
	}

	// Then tick the remaining meshes requiring pre-skinning in parallel
	if (ToTickPreskin.Num() != 0)
	{
		ParallelFor(ToTickPreskin.Num(), [&](int32 Index)
		{
			ToTickPreskin[Index]->Tick(DeltaSeconds, true);
		});
	}

	{ // handle any changes to the UV mappings
		FRWScopeLock UvMappingWriteLock(CachedUvMappingGuard, SLT_Write);

		TArray<int32, TInlineAllocator<32>> MappingsToRemove;

		const int32 MappingCount = CachedUvMapping.Num();

		for (int32 MappingIt = 0; MappingIt < MappingCount; ++MappingIt)
		{
			const TSharedPtr<FSkeletalMeshUvMapping>& UvMappingData = CachedUvMapping[MappingIt];;

			if (UvMappingData->CanBeDestroyed())
			{
				MappingsToRemove.Add(MappingIt);
			}
		}

		while (MappingsToRemove.Num())
		{
			CachedUvMapping.RemoveAtSwap(MappingsToRemove.Pop(false));
		}
	}

	{ // handle any changes to the connectivity handles
		FRWScopeLock ConnectivityWriteLock(CachedConnectivityGuard, SLT_Write);

		TArray<int32, TInlineAllocator<32>> EntriesToRemove;

		const int32 EntryCount = CachedConnectivity.Num();

		for (int32 EntryIt = 0; EntryIt < EntryCount; ++EntryIt)
		{
			const TSharedPtr<FSkeletalMeshConnectivity>& ConnectivityData = CachedConnectivity[EntryIt];;

			if (ConnectivityData->CanBeDestroyed())
			{
				EntriesToRemove.Add(EntryIt);
			}
		}

		while (EntriesToRemove.Num())
		{
			CachedConnectivity.RemoveAtSwap(EntriesToRemove.Pop(false));
		}
	}
}

FSkeletalMeshUvMappingHandle FNDI_SkeletalMesh_GeneratedData::GetCachedUvMapping(TWeakObjectPtr<USkeletalMesh>& MeshObject, int32 InLodIndex, int32 InUvSetIndex, FMeshUvMappingUsage Usage, bool bNeedsDataImmediately)
{
	check(MeshObject.Get() != nullptr);

	if (!FSkeletalMeshUvMapping::IsValidMeshObject(MeshObject, InLodIndex, InUvSetIndex))
	{
		return FSkeletalMeshUvMappingHandle();
	}

	// Attempt to Find data
	auto MappingMatchPredicate = [&](const TSharedPtr<FSkeletalMeshUvMapping>& UvMapping)
	{
		return UvMapping->Matches(MeshObject, InLodIndex, InUvSetIndex);
	};

	{
		FRWScopeLock ReadLock(CachedUvMappingGuard, SLT_ReadOnly);
		if (TSharedPtr<FSkeletalMeshUvMapping>* Existing = CachedUvMapping.FindByPredicate(MappingMatchPredicate))
		{
			return FSkeletalMeshUvMappingHandle(Usage, *Existing, bNeedsDataImmediately);
		}
	}

	// We need to add
	FRWScopeLock WriteLock(CachedUvMappingGuard, SLT_Write);
	if (TSharedPtr<FSkeletalMeshUvMapping>* Existing = CachedUvMapping.FindByPredicate(MappingMatchPredicate))
	{
		return FSkeletalMeshUvMappingHandle(Usage, *Existing, bNeedsDataImmediately);
	}

	return FSkeletalMeshUvMappingHandle(
		Usage,
		CachedUvMapping.Add_GetRef(MakeShared<FSkeletalMeshUvMapping>(MeshObject, InLodIndex, InUvSetIndex)),
		bNeedsDataImmediately);
}


FSkeletalMeshConnectivityHandle FNDI_SkeletalMesh_GeneratedData::GetCachedConnectivity(TWeakObjectPtr<USkeletalMesh>& MeshObject, int32 InLodIndex, FSkeletalMeshConnectivityUsage Usage, bool bNeedsDataImmediately)
{
	check(MeshObject.Get() != nullptr);

	if (!FSkeletalMeshConnectivity::IsValidMeshObject(MeshObject, InLodIndex))
	{
		return FSkeletalMeshConnectivityHandle();
	}

	// Attempt to Find data
	auto ConnectivityMatchPredicate = [&](const TSharedPtr<FSkeletalMeshConnectivity>& Connectivity)
	{
		return Connectivity->CanBeUsed(MeshObject, InLodIndex);
	};


	{
		FRWScopeLock ReadLock(CachedConnectivityGuard, SLT_ReadOnly);
		if (TSharedPtr<FSkeletalMeshConnectivity>* Existing = CachedConnectivity.FindByPredicate(ConnectivityMatchPredicate))
		{
			return FSkeletalMeshConnectivityHandle(Usage, *Existing, bNeedsDataImmediately);
		}
	}

	// We need to add
	FRWScopeLock WriteLock(CachedConnectivityGuard, SLT_Write);
	if (TSharedPtr<FSkeletalMeshConnectivity>* Existing = CachedConnectivity.FindByPredicate(ConnectivityMatchPredicate))
	{
		return FSkeletalMeshConnectivityHandle(Usage, *Existing, bNeedsDataImmediately);
	}

	return FSkeletalMeshConnectivityHandle(
		Usage,
		CachedConnectivity.Add_GetRef(MakeShared<FSkeletalMeshConnectivity>(MeshObject, InLodIndex)),
		bNeedsDataImmediately);
}

//////////////////////////////////////////////////////////////////////////
// FStaticMeshGpuSpawnBuffer


FSkeletalMeshGpuSpawnStaticBuffers::~FSkeletalMeshGpuSpawnStaticBuffers()
{
	//ValidSections.Empty();
}

void FSkeletalMeshGpuSpawnStaticBuffers::Initialise(FNDISkeletalMesh_InstanceData* InstData, const FSkeletalMeshLODRenderData& SkeletalMeshLODRenderData, const FSkeletalMeshSamplingLODBuiltData* MeshSamplingLODBuiltData, FNiagaraSystemInstance* SystemInstance)
{
	SkeletalMeshSamplingLODBuiltData = nullptr;
	bUseGpuUniformlyDistributedSampling = false;

	LODRenderData = nullptr;
	TriangleCount = 0;
	VertexCount = 0;

	NumFilteredBones = 0;
	NumUnfilteredBones = 0;
	FilteredAndUnfilteredBonesArray.Empty();
	NumFilteredSockets = 0;
	FilteredSocketBoneOffset = 0;

	if (InstData)
	{
		SkeletalMeshSamplingLODBuiltData = MeshSamplingLODBuiltData;
		bUseGpuUniformlyDistributedSampling = InstData->bIsGpuUniformlyDistributedSampling;

		LODRenderData = &SkeletalMeshLODRenderData;
		TriangleCount = SkeletalMeshLODRenderData.MultiSizeIndexContainer.GetIndexBuffer()->Num() / 3;
		VertexCount = SkeletalMeshLODRenderData.GetNumVertices();
		
		// TODO: Bring these back when we can know if they are for sure sampling from them. Disabled for now to suppress log spam
		//if (TriangleCount == 0)
		//{
		//	UE_LOG(LogNiagara, Warning, TEXT("FSkeletalMeshGpuSpawnStaticBuffers> TriangleCount(0) is invalid for SkelMesh(%s) System(%s)"), *GetFullNameSafe(InstData->SkeletalMesh.Get()), *GetFullNameSafe(SystemInstance->GetSystem()));
		//}
		//if (VertexCount == 0)
		//{
		//	UE_LOG(LogNiagara, Warning, TEXT("FSkeletalMeshGpuSpawnStaticBuffers> VertexCount(0) is invalid for SkelMesh(%s) System(%s)"), *GetFullNameSafe(InstData->SkeletalMesh.Get()), *GetFullNameSafe(SystemInstance->GetSystem()));
		//}

		if (bUseGpuUniformlyDistributedSampling)
		{
			check(SkeletalMeshSamplingLODBuiltData != nullptr);
			const int32 NumAreaSamples = SkeletalMeshSamplingLODBuiltData->AreaWeightedTriangleSampler.GetNumEntries();
			if (NumAreaSamples != TriangleCount)
			{
				UE_LOG(LogNiagara, Warning, TEXT("FSkeletalMeshGpuSpawnStaticBuffers> AreaWeighted Triangle Sampling Count (%d) does not match triangle count (%d), disabling uniform sampling for SkelMesh(%s) System(%s)"), NumAreaSamples, TriangleCount, *GetFullNameSafe(InstData->SkeletalMesh.Get()), *GetFullNameSafe(SystemInstance->GetSystem()));
				bUseGpuUniformlyDistributedSampling = false;
			}
		}

		// Copy filtered Bones / Socket data into arrays that the renderer will use to create read buffers
		//-TODO: Exclude setting up these arrays if we don't sample from them
		NumFilteredBones = InstData->NumFilteredBones;
		NumUnfilteredBones = InstData->NumUnfilteredBones;
		ExcludedBoneIndex = InstData->ExcludedBoneIndex;

		FilteredAndUnfilteredBonesArray.Reserve(InstData->FilteredAndUnfilteredBones.Num());
		for (uint16 v : InstData->FilteredAndUnfilteredBones)
		{
			FilteredAndUnfilteredBonesArray.Add(v);
		}

		NumFilteredSockets = InstData->FilteredSocketInfo.Num();
		FilteredSocketBoneOffset = InstData->FilteredSocketBoneOffset;

		// Create triangle / vertex region sampling data
		if (InstData->SamplingRegionIndices.Num() > 0)
		{
			const FSkeletalMeshSamplingInfo& SamplingInfo = InstData->SkeletalMesh->GetSamplingInfo();

			// Count required regions
			bSamplingRegionsAllAreaWeighted = true;
			NumSamplingRegionTriangles = 0;
			NumSamplingRegionVertices = 0;

			for (const int32 RegionIndex : InstData->SamplingRegionIndices)
			{
				const FSkeletalMeshSamplingRegionBuiltData& SamplingRegionBuildData = SamplingInfo.GetRegionBuiltData(RegionIndex);
				NumSamplingRegionTriangles += SamplingRegionBuildData.TriangleIndices.Num();
				NumSamplingRegionVertices += SamplingRegionBuildData.Vertices.Num();
				bSamplingRegionsAllAreaWeighted &= SamplingRegionBuildData.AreaWeightedSampler.GetNumEntries() == SamplingRegionBuildData.TriangleIndices.Num();
			}

			// Build buffers
			SampleRegionsProbAlias.AddUninitialized(NDISkelMeshLocal::GetProbAliasDWORDSize(NumSamplingRegionTriangles));
			SampleRegionsTriangleIndicies.Reserve(NumSamplingRegionTriangles);
			SampleRegionsVertices.Reserve(NumSamplingRegionVertices);

			int32 RegionOffset = 0;
			int32 PABufferOffset = 0;
			for (const int32 RegionIndex : InstData->SamplingRegionIndices)
			{
				const FSkeletalMeshSamplingRegionBuiltData& SamplingRegionBuildData = SamplingInfo.GetRegionBuiltData(RegionIndex);
				if (bSamplingRegionsAllAreaWeighted)
				{
					NDISkelMeshLocal::PackProbAlias(SampleRegionsProbAlias.GetData() + PABufferOffset, SamplingRegionBuildData.AreaWeightedSampler, RegionOffset);
					PABufferOffset += NDISkelMeshLocal::GetProbAliasDWORDSize(SamplingRegionBuildData.AreaWeightedSampler.GetNumEntries());
				}
				for (int v : SamplingRegionBuildData.TriangleIndices)
				{
					SampleRegionsTriangleIndicies.Add(v / 3);
				}
				for (int v : SamplingRegionBuildData.Vertices)
				{
					SampleRegionsVertices.Add(v);
				}
				RegionOffset += SamplingRegionBuildData.TriangleIndices.Num();
			}
		}
	}
}

void FSkeletalMeshGpuSpawnStaticBuffers::InitRHI()
{
	// As of today, the UI does not allow to cull specific section of a mesh so this data could be generated on the Mesh. But Section culling might be added later?
	// Also see https://jira.it.epicgames.net/browse/UE-69376 : we would need to know if GPU sampling of the mesh surface is needed or not on the mesh to be able to do that.
	// ALso today we do not know if an interface is create from a CPU or GPU emitter. So always allocate for now. Follow up in https://jira.it.epicgames.net/browse/UE-69375.

	MeshIndexBufferSRV = LODRenderData->MultiSizeIndexContainer.GetIndexBuffer()->GetSRV();
	MeshVertexBufferSRV = LODRenderData->StaticVertexBuffers.PositionVertexBuffer.GetSRV();
	MeshTangentBufferSRV = LODRenderData->StaticVertexBuffers.StaticMeshVertexBuffer.GetTangentsSRV();
	bMeshValid = MeshIndexBufferSRV.IsValid() && MeshVertexBufferSRV.IsValid() && MeshTangentBufferSRV.IsValid();
	NumWeights = LODRenderData->SkinWeightVertexBuffer.GetMaxBoneInfluences();

	if ( bMeshValid == false )
	{
		MeshIndexBufferSRV = FNiagaraRenderer::GetDummyUIntBuffer();
		MeshVertexBufferSRV = FNiagaraRenderer::GetDummyFloatBuffer();
		MeshTangentBufferSRV = FNiagaraRenderer::GetDummyFloat4Buffer();
		NumWeights = 0;
		TriangleCount = 0;
		VertexCount = 0;
		bUseGpuUniformlyDistributedSampling = false;
		NumSamplingRegionTriangles = 0;
		bSamplingRegionsAllAreaWeighted = false;

		SampleRegionsProbAlias.Empty();
		SampleRegionsTriangleIndicies.Empty();
		SampleRegionsVertices.Empty();
	}

	MeshColorBufferSRV = LODRenderData->StaticVertexBuffers.ColorVertexBuffer.GetColorComponentsSRV();
	bHasMeshColors = bMeshValid && MeshColorBufferSRV.IsValid();
	if (bHasMeshColors == false)
	{
		MeshColorBufferSRV = FNiagaraRenderer::GetDummyFloat4Buffer();
	}

	MeshTexCoordBufferSRV = LODRenderData->StaticVertexBuffers.StaticMeshVertexBuffer.GetTexCoordsSRV();
	if (MeshTexCoordBufferSRV.IsValid())
	{
		NumTexCoord = LODRenderData->StaticVertexBuffers.StaticMeshVertexBuffer.GetNumTexCoords();
	}
	else
	{
		MeshTexCoordBufferSRV = FNiagaraRenderer::GetDummyFloat2Buffer();
		NumTexCoord = 0;
	}

	const uint32 SectionCount = LODRenderData->RenderSections.Num();

#if STATS
	ensure(GPUMemoryUsage == 0);
#endif

	if (bUseGpuUniformlyDistributedSampling)
	{
		const FSkeletalMeshAreaWeightedTriangleSampler& triangleSampler = SkeletalMeshSamplingLODBuiltData->AreaWeightedTriangleSampler;
		check(TriangleCount == triangleSampler.GetNumEntries());

		FRHIResourceCreateInfo CreateInfo(TEXT("FSkeletalMeshGpuSpawnStaticBuffers"));
		uint32 SizeByte = NDISkelMeshLocal::GetProbAliasDWORDSize(TriangleCount) * sizeof(uint32);
		BufferTriangleUniformSamplerProbAliasRHI = RHICreateBuffer(SizeByte, BUF_Static | BUF_VertexBuffer | BUF_ShaderResource, 0, ERHIAccess::VertexOrIndexBuffer | ERHIAccess::SRVMask, CreateInfo);
		uint32* PackedData = (uint32*)RHILockBuffer(BufferTriangleUniformSamplerProbAliasRHI, 0, SizeByte, RLM_WriteOnly);
		NDISkelMeshLocal::PackProbAlias(PackedData, triangleSampler);
		RHIUnlockBuffer(BufferTriangleUniformSamplerProbAliasRHI);
		BufferTriangleUniformSamplerProbAliasSRV = RHICreateShaderResourceView(BufferTriangleUniformSamplerProbAliasRHI, sizeof(uint32), PF_R32_UINT);
#if STATS
		GPUMemoryUsage += SizeByte;
#endif
	}
	else
	{
		BufferTriangleUniformSamplerProbAliasSRV = FNiagaraRenderer::GetDummyUIntBuffer();
	}

	// Prepare sampling regions (if we have any)
	SampleRegionsProbAliasSRV = FNiagaraRenderer::GetDummyUIntBuffer();
	SampleRegionsTriangleIndicesSRV = FNiagaraRenderer::GetDummyUIntBuffer();
	SampleRegionsVerticesSRV = FNiagaraRenderer::GetDummyUIntBuffer();
	if (NumSamplingRegionTriangles > 0)
	{
		FRHIResourceCreateInfo CreateInfo(TEXT("SampleRegionsProbAliasBuffer"));
		if (bSamplingRegionsAllAreaWeighted)
		{
			CreateInfo.ResourceArray = &SampleRegionsProbAlias;
			SampleRegionsProbAliasBuffer = RHICreateVertexBuffer(SampleRegionsProbAlias.Num() * SampleRegionsProbAlias.GetTypeSize(), BUF_Static | BUF_ShaderResource, CreateInfo);
			SampleRegionsProbAliasSRV = RHICreateShaderResourceView(SampleRegionsProbAliasBuffer, sizeof(uint32), PF_R32_UINT);
#if STATS
			GPUMemoryUsage += SampleRegionsProbAlias.Num() * SampleRegionsProbAlias.GetTypeSize();
#endif
		}
		else
		{
			SampleRegionsProbAliasSRV = FNiagaraRenderer::GetDummyUIntBuffer();
		}

		CreateInfo.DebugName = (TEXT("SampleRegionsTriangleIndicesBuffer"));
		CreateInfo.ResourceArray = &SampleRegionsTriangleIndicies;
		SampleRegionsTriangleIndicesBuffer = RHICreateVertexBuffer(SampleRegionsTriangleIndicies.Num() * SampleRegionsTriangleIndicies.GetTypeSize(), BUF_Static | BUF_ShaderResource, CreateInfo);
		SampleRegionsTriangleIndicesSRV = RHICreateShaderResourceView(SampleRegionsTriangleIndicesBuffer, sizeof(int32), PF_R32_UINT);
#if STATS
		GPUMemoryUsage += SampleRegionsTriangleIndicies.Num() * SampleRegionsTriangleIndicies.GetTypeSize();
#endif

		CreateInfo.DebugName = (TEXT("SampleRegionsVerticesBuffer"));
		CreateInfo.ResourceArray = &SampleRegionsVertices;
		SampleRegionsVerticesBuffer = RHICreateVertexBuffer(SampleRegionsVertices.Num() * SampleRegionsVertices.GetTypeSize(), BUF_Static | BUF_ShaderResource, CreateInfo);
		SampleRegionsVerticesSRV = RHICreateShaderResourceView(SampleRegionsVerticesBuffer, sizeof(int32), PF_R32_UINT);
#if STATS
		GPUMemoryUsage += SampleRegionsVertices.Num() * SampleRegionsVertices.GetTypeSize();
#endif
	}
	else
	{
		SampleRegionsProbAliasSRV = FNiagaraRenderer::GetDummyUIntBuffer();
		SampleRegionsTriangleIndicesSRV = FNiagaraRenderer::GetDummyUIntBuffer();
		SampleRegionsVerticesSRV = FNiagaraRenderer::GetDummyUIntBuffer();
	}

	// Prepare the vertex matrix lookup offset for each of the sections. This is needed because per vertex BlendIndicies are stored relatively to each Section used matrices.
	// And these offset per section need to point to the correct matrix according to each section BoneMap.
	// There is not section selection/culling in the interface so technically we could compute that array in the pipeline.
	if ( bMeshValid )
	{
		FRHIResourceCreateInfo CreateInfo(TEXT("FSkeletalMeshGpuSpawnStaticBuffers"));
		BufferTriangleMatricesOffsetRHI = RHICreateBuffer(VertexCount * sizeof(uint32), BUF_Static | BUF_VertexBuffer | BUF_ShaderResource, 0, ERHIAccess::VertexOrIndexBuffer | ERHIAccess::SRVMask, CreateInfo);
		uint32* MatricesOffsets = (uint32*)RHILockBuffer(BufferTriangleMatricesOffsetRHI, 0, VertexCount * sizeof(uint32), RLM_WriteOnly);
		uint32 AccumulatedMatrixOffset = 0;
		for (uint32 s = 0; s < SectionCount; ++s)
		{
			const FSkelMeshRenderSection& Section = LODRenderData->RenderSections[s];
			const uint32 SectionBaseVertexIndex = Section.BaseVertexIndex;
			const uint32 SectionNumVertices = Section.NumVertices;
			for (uint32 SectionVertex = 0; SectionVertex < SectionNumVertices; ++SectionVertex)
			{
				MatricesOffsets[SectionBaseVertexIndex + SectionVertex] = AccumulatedMatrixOffset;
			}
			AccumulatedMatrixOffset += Section.BoneMap.Num();
		}
		RHIUnlockBuffer(BufferTriangleMatricesOffsetRHI);
		BufferTriangleMatricesOffsetSRV = RHICreateShaderResourceView(BufferTriangleMatricesOffsetRHI, sizeof(uint32), PF_R32_UINT);
#if STATS
		GPUMemoryUsage += VertexCount * sizeof(uint32);
#endif
	}
	else
	{
		BufferTriangleMatricesOffsetSRV = FNiagaraRenderer::GetDummyUIntBuffer();
	}

	// Create arrays for filtered bones / sockets
	if ( FilteredAndUnfilteredBonesArray.Num() > 0 )
	{
		FRHIResourceCreateInfo CreateInfo(TEXT("FilteredAndUnfilteredBonesBuffer"));
		CreateInfo.ResourceArray = &FilteredAndUnfilteredBonesArray;

		FilteredAndUnfilteredBonesBuffer = RHICreateVertexBuffer(FilteredAndUnfilteredBonesArray.Num() * FilteredAndUnfilteredBonesArray.GetTypeSize(), BUF_Static | BUF_ShaderResource, CreateInfo);
		FilteredAndUnfilteredBonesSRV = RHICreateShaderResourceView(FilteredAndUnfilteredBonesBuffer, sizeof(uint16), PF_R16_UINT);
	}
	else
	{
		FilteredAndUnfilteredBonesSRV = FNiagaraRenderer::GetDummyUIntBuffer();
	}
#if STATS
	GPUMemoryUsage += FilteredAndUnfilteredBonesArray.Num() * FilteredAndUnfilteredBonesArray.GetTypeSize();
#endif

#if STATS
	INC_MEMORY_STAT_BY(STAT_NiagaraGPUDataInterfaceMemory, GPUMemoryUsage);
#endif
}

void FSkeletalMeshGpuSpawnStaticBuffers::ReleaseRHI()
{
#if STATS
	DEC_MEMORY_STAT_BY(STAT_NiagaraGPUDataInterfaceMemory, GPUMemoryUsage);
	GPUMemoryUsage = 0;
#endif

	BufferTriangleMatricesOffsetRHI.SafeRelease();
	BufferTriangleMatricesOffsetSRV.SafeRelease();

	BufferTriangleUniformSamplerProbAliasRHI.SafeRelease();
	BufferTriangleUniformSamplerProbAliasSRV.SafeRelease();

	SampleRegionsProbAliasBuffer.SafeRelease();
	SampleRegionsProbAliasSRV.SafeRelease();
	SampleRegionsTriangleIndicesBuffer.SafeRelease();
	SampleRegionsTriangleIndicesSRV.SafeRelease();
	SampleRegionsVerticesBuffer.SafeRelease();
	SampleRegionsVerticesSRV.SafeRelease();

	FilteredAndUnfilteredBonesBuffer.SafeRelease();
	FilteredAndUnfilteredBonesSRV.SafeRelease();

	MeshVertexBufferSRV.SafeRelease();
	MeshIndexBufferSRV.SafeRelease();
	MeshTangentBufferSRV.SafeRelease();
	MeshTexCoordBufferSRV.SafeRelease();
	MeshColorBufferSRV.SafeRelease();
}

//////////////////////////////////////////////////////////////////////////
//FSkeletalMeshGpuSpawnProxy

FSkeletalMeshGpuDynamicBufferProxy::FSkeletalMeshGpuDynamicBufferProxy()
{
	//
}

FSkeletalMeshGpuDynamicBufferProxy::~FSkeletalMeshGpuDynamicBufferProxy()
{
	//
}

void FSkeletalMeshGpuDynamicBufferProxy::Initialise(const FReferenceSkeleton& RefSkel, const FSkeletalMeshLODRenderData& SkeletalMeshLODRenderData, uint32 InSamplingSocketCount)
{
	SectionBoneCount = 0;
	for (const FSkelMeshRenderSection& Section : SkeletalMeshLODRenderData.RenderSections)
	{
		SectionBoneCount += Section.BoneMap.Num();
	}

	SamplingBoneCount = RefSkel.GetNum();
	SamplingSocketCount = InSamplingSocketCount;
}

void FSkeletalMeshGpuDynamicBufferProxy::InitRHI()
{
#if STATS
	ensure(GPUMemoryUsage == 0);
#endif
	for (FSkeletalBuffer& Buffer : RWBufferBones)
	{
		FRHIResourceCreateInfo CreateInfo(TEXT("SkeletalMeshGpuDynamicBuffer"));
		Buffer.SectionBuffer = RHICreateVertexBuffer(sizeof(FVector4f) * 3 * SectionBoneCount, BUF_ShaderResource | BUF_Dynamic, CreateInfo);
		Buffer.SectionSRV = RHICreateShaderResourceView(Buffer.SectionBuffer, sizeof(FVector4f), PF_A32B32G32R32F);

		Buffer.SamplingBuffer = RHICreateVertexBuffer(sizeof(FVector4f) * 3 * (SamplingBoneCount + SamplingSocketCount), BUF_ShaderResource | BUF_Dynamic, CreateInfo);
		Buffer.SamplingSRV = RHICreateShaderResourceView(Buffer.SamplingBuffer, sizeof(FVector4f), PF_A32B32G32R32F);

#if STATS
		GPUMemoryUsage += sizeof(FVector4f) * 3 * SectionBoneCount;
		GPUMemoryUsage += sizeof(FVector4f) * 2 * (SamplingBoneCount + SamplingSocketCount);
#endif
	}
#if STATS
	INC_MEMORY_STAT_BY(STAT_NiagaraGPUDataInterfaceMemory, GPUMemoryUsage);
#endif
}

void FSkeletalMeshGpuDynamicBufferProxy::ReleaseRHI()
{
#if STATS
	DEC_MEMORY_STAT_BY(STAT_NiagaraGPUDataInterfaceMemory, GPUMemoryUsage);
	GPUMemoryUsage = 0;
#endif
	for (FSkeletalBuffer& Buffer : RWBufferBones)
	{
		Buffer.SectionBuffer.SafeRelease();
		Buffer.SectionSRV.SafeRelease();

		Buffer.SamplingBuffer.SafeRelease();
		Buffer.SamplingSRV.SafeRelease();
	}
}

void FSkeletalMeshGpuDynamicBufferProxy::NewFrame(const FNDISkeletalMesh_InstanceData* InstanceData, int32 LODIndex)
{
	// Grab Skeletal Component / Mesh, we must have a mesh at minimum to set the data
	USkeletalMeshComponent* SkelComp = nullptr;
	USkeletalMesh* SkelMesh = nullptr;
	if (InstanceData != nullptr)
	{
		SkelComp = Cast<USkeletalMeshComponent>(InstanceData->SceneComponent.Get());
		if ( SkelComp != nullptr )
		{
			SkelMesh = SkelComp->GetSkeletalMeshAsset();
		}
		if (SkelMesh == nullptr)
		{
			SkelMesh = InstanceData->SkeletalMesh.Get();
		}
	}

	if ( SkelMesh == nullptr )
	{
		return;
	}

	static_assert(sizeof(FVector4f) == 4 * sizeof(float), "FVector4f should match 4 * floats");

	TArray<FVector4f> AllSectionsRefToLocalMatrices;
	TArray<FVector4f> BoneSamplingData;

	auto FillBuffers =
		[&](const TArray<FTransform>& BoneTransforms, const FReferenceSkeleton* ReferenceSkeleton)
		{
			check(BoneTransforms.Num() == SamplingBoneCount);

			// Fill AllSectionsRefToLocalMatrices
			TIndirectArray<FSkeletalMeshLODRenderData>& LODRenderDataArray = SkelMesh->GetResourceForRendering()->LODRenderData;
			check(0 <= LODIndex && LODIndex < LODRenderDataArray.Num());
			FSkeletalMeshLODRenderData& LODRenderData = LODRenderDataArray[LODIndex];
			TArray<FSkelMeshRenderSection>& Sections = LODRenderData.RenderSections;

			// Count number of matrices we want before appending all of them according to the per section mapping from BoneMap
			uint32 Float4Count = 0;
			for (const FSkelMeshRenderSection& Section : Sections)
			{
				Float4Count += Section.BoneMap.Num() * 3;
			}
			check(Float4Count == 3 * SectionBoneCount);
			AllSectionsRefToLocalMatrices.AddUninitialized(Float4Count);

			Float4Count = 0;
			for (const FSkelMeshRenderSection& Section : Sections)
			{
				const uint32 MatrixCount = Section.BoneMap.Num();
				for (uint32 m=0; m < MatrixCount; ++m)
				{
					const int32 BoneIndex = Section.BoneMap[m];
					const FTransform& BoneTransform = BoneTransforms[BoneIndex];
					// LWC_TODO: precision loss
					const FMatrix44f BoneMatrix = SkelMesh->GetRefBasesInvMatrix().IsValidIndex(BoneIndex) ? SkelMesh->GetRefBasesInvMatrix()[BoneIndex] * (FMatrix44f)BoneTransform.ToMatrixWithScale() : (FMatrix44f)BoneTransform.ToMatrixWithScale();
					BoneMatrix.To3x4MatrixTranspose(&AllSectionsRefToLocalMatrices[Float4Count].X);
					Float4Count += 3;
				}
			}

			// Fill BoneSamplingData
			BoneSamplingData.Reserve((SamplingBoneCount + SamplingSocketCount) * 2);
			for (int i=0; i < BoneTransforms.Num(); ++i )
			{
				const FTransform& BoneTransform = BoneTransforms[i];
				const FQuat Rotation = BoneTransform.GetRotation();
				const int32 ParentIndex = ReferenceSkeleton ? ReferenceSkeleton->GetParentIndex(i) : -1;
				BoneSamplingData.Emplace((FVector3f)BoneTransform.GetLocation()); // LWC_TODO: precision loss
				BoneSamplingData.Emplace(Rotation.X, Rotation.Y, Rotation.Z, Rotation.W);
				BoneSamplingData.Emplace_GetRef((FVector3f)BoneTransform.GetScale3D()).W = reinterpret_cast<const float&>(ParentIndex); // LWC_TODO: precision loss
			}

			// Append sockets
			for (const FTransform3f& SocketTransform : InstanceData->GetFilteredSocketsCurrBuffer())
			{
				const FQuat4f Rotation = SocketTransform.GetRotation();
				const int32 ParentIndex = -1;
				BoneSamplingData.Emplace(SocketTransform.GetLocation());
				BoneSamplingData.Emplace(Rotation.X, Rotation.Y, Rotation.Z, Rotation.W);
				BoneSamplingData.Emplace_GetRef(SocketTransform.GetScale3D()).W = reinterpret_cast<const float&>(ParentIndex);
			}
		};

	// If we have a component pull transforms from component otherwise grab from skel mesh
	if (SkelComp)
	{
		if (USkinnedMeshComponent* LeaderComponent = SkelComp->LeaderPoseComponent.Get())
		{
			const FReferenceSkeleton* ReferenceSkeleton = nullptr;
			const TArray<int32>& LeaderBoneMap = SkelComp->GetLeaderBoneMap();
			const int32 NumBones = LeaderBoneMap.Num();

			TArray<FTransform> TempBoneTransforms;
			TempBoneTransforms.Reserve(SamplingBoneCount);

			if (NumBones == 0)
			{
				// This case indicates an invalid leader pose component (e.g. no skeletal mesh)
				TempBoneTransforms.AddDefaulted(SamplingBoneCount);
			}
			else
			{
				ReferenceSkeleton = &SkelMesh->GetRefSkeleton();
				const TArray<FTransform>& LeaderTransforms = LeaderComponent->GetComponentSpaceTransforms();
				for (int32 BoneIndex=0; BoneIndex < NumBones; ++BoneIndex)
				{
					if (LeaderBoneMap.IsValidIndex(BoneIndex))
					{
						const int32 LeaderIndex = LeaderBoneMap[BoneIndex];
						if (LeaderIndex != INDEX_NONE && LeaderIndex < LeaderTransforms.Num())
						{
							TempBoneTransforms.Add(LeaderTransforms[LeaderIndex]);
							continue;
						}
					}

					const int32 ParentIndex  = ReferenceSkeleton->GetParentIndex(BoneIndex);
					FTransform BoneTransform = ReferenceSkeleton->GetRefBonePose()[BoneIndex];
					if (TempBoneTransforms.IsValidIndex(ParentIndex))
					{
						BoneTransform = BoneTransform * TempBoneTransforms[ParentIndex];
					}
					TempBoneTransforms.Add(BoneTransform);
				}
			}
			FillBuffers(TempBoneTransforms, ReferenceSkeleton);
		}
		else
		{
			const TArray<FTransform>& ComponentTransforms = SkelComp->GetComponentSpaceTransforms();
			if (ComponentTransforms.Num() > 0)
			{
				FillBuffers(ComponentTransforms, &SkelMesh->GetRefSkeleton());
			}
			else
			{
				// Trying to catch cause of this case in the wild. Not supposed to be possible with a valid skeletal mesh
				ensureMsgf(false, TEXT("NiagaraSkelMeshDI: Mesh has no ComponentSpaceTransforms. Component - %s (Registered: %s, Flags: %d), Mesh - %s (Flags: %d)"),
					*GetFullNameSafe(SkelComp), SkelComp->IsRegistered() ? TEXT("Yes") : TEXT("No"), SkelComp->GetFlags(), *GetFullNameSafe(SkelMesh), SkelMesh->GetFlags());

				TArray<FTransform> TempBoneTransforms;
				TempBoneTransforms.AddDefaulted(SamplingBoneCount);
				FillBuffers(TempBoneTransforms, nullptr);
			}
		}
	}
	else
	{
		//-TODO: Opt and combine with MaterPoseComponent
		const FReferenceSkeleton* ReferenceSkeleton = &SkelMesh->GetRefSkeleton();
		TArray<FTransform> TempBoneTransforms;
		TempBoneTransforms.Reserve(SamplingBoneCount);

		const TArray<FTransform>& RefTransforms = SkelMesh->GetRefSkeleton().GetRefBonePose();
		for (int32 i=0; i < RefTransforms.Num(); ++i)
		{
			FTransform BoneTransform = RefTransforms[i];
			const int32 ParentIndex = ReferenceSkeleton->GetParentIndex(i);
			if (TempBoneTransforms.IsValidIndex(ParentIndex))
			{
				BoneTransform = BoneTransform * TempBoneTransforms[ParentIndex];
			}
			TempBoneTransforms.Add(BoneTransform);
		}

		FillBuffers(TempBoneTransforms, ReferenceSkeleton);
	}

	FSkeletalMeshGpuDynamicBufferProxy* ThisProxy = this;
	ENQUEUE_RENDER_COMMAND(UpdateSpawnInfoForSkinnedMesh)(
		[ThisProxy, AllSectionsRefToLocalMatrices = MoveTemp(AllSectionsRefToLocalMatrices), BoneSamplingData = MoveTemp(BoneSamplingData)](FRHICommandListImmediate& RHICmdList) mutable
		{
			ThisProxy->CurrentBoneBufferId = (ThisProxy->CurrentBoneBufferId + 1) % BufferBoneCount;
			ThisProxy->bPrevBoneGpuBufferValid = ThisProxy->bBoneGpuBufferValid;
			ThisProxy->bBoneGpuBufferValid = true;

			// Copy bone remap data matrices
			{
				const uint32 NumBytes = AllSectionsRefToLocalMatrices.Num() * sizeof(FVector4f);
				void* DstData = RHILockBuffer(ThisProxy->GetRWBufferBone().SectionBuffer, 0, NumBytes, RLM_WriteOnly);
				FMemory::Memcpy(DstData, AllSectionsRefToLocalMatrices.GetData(), NumBytes);
				RHIUnlockBuffer(ThisProxy->GetRWBufferBone().SectionBuffer);
			}

			// Copy bone sampling data
			{
				const uint32 NumBytes = BoneSamplingData.Num() * sizeof(FVector4f);
				FVector4f* DstData = reinterpret_cast<FVector4f*>(RHILockBuffer(ThisProxy->GetRWBufferBone().SamplingBuffer, 0, NumBytes, RLM_WriteOnly));
				FMemory::Memcpy(DstData, BoneSamplingData.GetData(), NumBytes);
				RHIUnlockBuffer(ThisProxy->GetRWBufferBone().SamplingBuffer);
			}
		}
	);
}

//////////////////////////////////////////////////////////////////////////

void FNiagaraDataInterfaceProxySkeletalMesh::ConsumePerInstanceDataFromGameThread(void* PerInstanceData, const FNiagaraSystemInstanceID& Instance)
{
	FNiagaraDISkeletalMeshPassedDataToRT* SourceData = static_cast<FNiagaraDISkeletalMeshPassedDataToRT*>(PerInstanceData);

	FNiagaraDataInterfaceProxySkeletalMeshData& Data = SystemInstancesToData.FindOrAdd(Instance);

	Data.bIsGpuUniformlyDistributedSampling = SourceData->bIsGpuUniformlyDistributedSampling;
	Data.bUnlimitedBoneInfluences = SourceData->bUnlimitedBoneInfluences;
	Data.DeltaSeconds = SourceData->DeltaSeconds;
	Data.DynamicBuffer = SourceData->DynamicBuffer;
	Data.MeshWeightStrideByte = SourceData->MeshWeightStrideByte;
	Data.MeshSkinWeightIndexSizeByte = SourceData->MeshSkinWeightIndexSizeByte;
	Data.PrevTransform = SourceData->PrevTransform;
	Data.StaticBuffers = SourceData->StaticBuffers;
	Data.Transform = SourceData->Transform;
	Data.PreSkinnedLocalBoundsCenter = SourceData->PreSkinnedLocalBoundsCenter;
	Data.PreSkinnedLocalBoundsExtents = SourceData->PreSkinnedLocalBoundsExtents;

	Data.MeshSkinWeightBuffer = SourceData->MeshSkinWeightBuffer;
	Data.MeshSkinWeightLookupBuffer = SourceData->MeshSkinWeightLookupBuffer;

	Data.UvMappingBuffer = SourceData->UvMappingBuffer;
	Data.UvMappingSet = SourceData->UvMappingSet;

	Data.ConnectivityBuffer = SourceData->ConnectivityBuffer;

	SourceData->~FNiagaraDISkeletalMeshPassedDataToRT();
}

//////////////////////////////////////////////////////////////////////////
//FNDISkeletalMesh_InstanceData

void UNiagaraDataInterfaceSkeletalMesh::ProvidePerInstanceDataForRenderThread(void* DataForRenderThread, void* PerInstanceData, const FNiagaraSystemInstanceID& SystemInstance)
{
	FNiagaraDISkeletalMeshPassedDataToRT* Data = static_cast<FNiagaraDISkeletalMeshPassedDataToRT*>(DataForRenderThread);
	FNDISkeletalMesh_InstanceData* SourceData = static_cast<FNDISkeletalMesh_InstanceData*>(PerInstanceData);

	Data->bIsGpuUniformlyDistributedSampling = SourceData->bIsGpuUniformlyDistributedSampling;
	Data->bUnlimitedBoneInfluences = SourceData->bUnlimitedBoneInfluences;
	Data->DeltaSeconds = SourceData->DeltaSeconds;
	Data->DynamicBuffer = SourceData->MeshGpuSpawnDynamicBuffers;
	Data->MeshWeightStrideByte = SourceData->MeshWeightStrideByte;
	Data->MeshSkinWeightIndexSizeByte = SourceData->MeshSkinWeightIndexSizeByte;
	Data->PrevTransform = FMatrix44f(SourceData->PrevTransform);	// LWC_TODO: Precision loss
	Data->StaticBuffers = SourceData->MeshGpuSpawnStaticBuffers;
	Data->Transform = FMatrix44f(SourceData->Transform);			// LWC_TODO: Precision loss
	Data->PreSkinnedLocalBoundsCenter = SourceData->PreSkinnedLocalBoundsCenter;
	Data->PreSkinnedLocalBoundsExtents = SourceData->PreSkinnedLocalBoundsExtents;

	Data->MeshSkinWeightBuffer = SourceData->MeshSkinWeightBuffer;
	Data->MeshSkinWeightLookupBuffer = SourceData->MeshSkinWeightLookupBuffer;

	Data->UvMappingBuffer = SourceData->UvMapping.GetQuadTreeProxy();
	Data->UvMappingSet = SourceData->UvMapping.GetUvSetIndex();

	Data->ConnectivityBuffer = SourceData->Connectivity.GetProxy();
}

USkeletalMesh* UNiagaraDataInterfaceSkeletalMesh::GetSkeletalMesh(FNiagaraSystemInstance* SystemInstance, USceneComponent* AttachComponent, TWeakObjectPtr<USceneComponent>& SceneComponent, USkeletalMeshComponent*& FoundSkelComp, FNDISkeletalMesh_InstanceData* InstData)
{
	auto IsValidComponent = [&](const USkeletalMeshComponent* Component, bool bAllowNullMesh=false)
	{
		return IsValid(Component) && (bAllowNullMesh || IsValid(Component->GetSkeletalMeshAsset())) &&
			(ComponentTags.IsEmpty()
			|| ComponentTags.ContainsByPredicate([&](const FName& Tag) { return Tag == NAME_None || Component->ComponentHasTag(Tag); }));
	};

	// Helper to scour an actor (or its parents) for a valid skeletal mesh component
	auto FindActorSkelMeshComponent = [&](AActor* Actor, bool bRecurseParents = false) -> USkeletalMeshComponent*
	{
		if (ASkeletalMeshActor* SkelMeshActor = Cast<ASkeletalMeshActor>(Actor))
		{
			USkeletalMeshComponent* Comp = SkelMeshActor->GetSkeletalMeshComponent();
			if (IsValidComponent(Comp, true))
			{
				return Comp;
			}
		}

		// Fall back on any valid component on the actor
		while (Actor)
		{
			if (USceneComponent* ActorRootComponent = Actor->GetRootComponent())
			{
				TArray<USceneComponent*> ActorComponents;
				ActorRootComponent->GetChildrenComponents(true /*bIncludeAllDescendants*/, ActorComponents);
				ActorComponents.Insert(ActorRootComponent, 0);

				for (USceneComponent* SceneComponent : ActorComponents)
				{
					if (USkeletalMeshComponent* SkeletalComp = Cast<USkeletalMeshComponent>(SceneComponent))
					{
						if (IsValidComponent(SkeletalComp))
						{
							return SkeletalComp;
						}
					}
				}
			}

			if (bRecurseParents)
			{
				Actor = Actor->GetParentActor();
			}
			else
			{
				break;
			}
		}

		return nullptr;
	};

	const bool bTrySource = SourceMode == ENDISkeletalMesh_SourceMode::Default || SourceMode == ENDISkeletalMesh_SourceMode::Source;
	const bool bTryAttachParent = SourceMode == ENDISkeletalMesh_SourceMode::Default || SourceMode == ENDISkeletalMesh_SourceMode::AttachParent;

	if (MeshUserParameter.Parameter.IsValid() && InstData && SystemInstance != nullptr)
	{
		// Initialize the binding and retrieve the object. If a valid object is bound, we'll try and retrieve the SkelMesh component from it.
		// If it's not valid yet, we'll reset and do this again when/if a valid object is set on the binding
		UObject* UserParamObject = InstData->UserParamBinding.Init(SystemInstance->GetInstanceParameters(), MeshUserParameter.Parameter);
		InstData->CachedUserParam = UserParamObject;
		if (UserParamObject)
		{
			if (USkeletalMeshComponent* UserSkelMeshComp = Cast<USkeletalMeshComponent>(UserParamObject))
			{
				if (IsValid(UserSkelMeshComp))
				{
					FoundSkelComp = UserSkelMeshComp;
				}
			}
			else if (AActor* Actor = Cast<AActor>(UserParamObject))
			{
				FoundSkelComp = FindActorSkelMeshComponent(Actor);
			}
			else
			{
				//We have a valid, non-null UObject parameter type but it is not a type we can use to get a skeletal mesh from.
				UE_LOG(LogNiagara, Warning, TEXT("SkeletalMesh data interface using object parameter with invalid type. Skeletal Mesh Data Interfaces can only get a valid mesh from SkeletalMeshComponents, SkeletalMeshActors or Actors."));
				UE_LOG(LogNiagara, Warning, TEXT("Invalid Parameter : %s"), *UserParamObject->GetFullName());
				UE_LOG(LogNiagara, Warning, TEXT("Niagara Component : %s"), *GetFullNameSafe(Cast<UNiagaraComponent>(AttachComponent)));
				UE_LOG(LogNiagara, Warning, TEXT("System : %s"), *GetFullNameSafe(SystemInstance->GetSystem()));
			}
		}
		else
		{
			// The binding exists, but no object is bound. Not warning here in case the user knows what they're doing.
		}
	}
	else if (bTrySource && IsValid(SourceComponent))
	{
		FoundSkelComp = SourceComponent;
	}
	else if (bTrySource && SoftSourceActor.Get())
	{
		FoundSkelComp = FindActorSkelMeshComponent(SoftSourceActor.Get());
	}
	else if (bTryAttachParent && AttachComponent)
	{
		// First, try to find the mesh component up the attachment hierarchy
		for (USceneComponent* Curr = AttachComponent; Curr; Curr = Curr->GetAttachParent())
		{
			USkeletalMeshComponent* ParentComp = Cast<USkeletalMeshComponent>(Curr);
			if (IsValidComponent(ParentComp, true))
			{
				FoundSkelComp = ParentComp;
				break;
			}
		}

		if (!FoundSkelComp)
		{
			// Next, try to find one in our outer chain
			USkeletalMeshComponent* OuterComp = AttachComponent->GetTypedOuter<USkeletalMeshComponent>();
			if (IsValidComponent(OuterComp, true))
			{
				FoundSkelComp = OuterComp;
			}
			else if (AActor* Actor = AttachComponent->GetAttachmentRootActor())
			{
				// Final fall-back, look for any mesh component on our root actor or any of its parents
				FoundSkelComp = FindActorSkelMeshComponent(Actor, true);
			}
		}
	}

	USkeletalMesh* Mesh = nullptr;
	SceneComponent = nullptr;
	if (FoundSkelComp)
	{
		Mesh = FoundSkelComp->GetSkeletalMeshAsset();
		SceneComponent = FoundSkelComp;
	}
#if WITH_EDITORONLY_DATA
	else if (!SystemInstance || !SystemInstance->GetWorld()->IsGameWorld())
	{
		// NOTE: We don't fall back on the preview mesh if we have a valid skeletal mesh component referenced
		Mesh = PreviewMesh.LoadSynchronous();
	}
#endif

	return Mesh;
}

USkeletalMesh* UNiagaraDataInterfaceSkeletalMesh::GetSkeletalMesh(UNiagaraComponent* Component)
{
	// NOTE: We don't need the system instance when not initializing instance data, and when using a UNiagaraComponent, it is always the attach component
	TWeakObjectPtr<USceneComponent> SceneComponent;
	USkeletalMeshComponent* FoundSkelComp = nullptr;
	return GetSkeletalMesh(nullptr, Component, SceneComponent, FoundSkelComp, nullptr);
}

bool FNDISkeletalMesh_InstanceData::Init(UNiagaraDataInterfaceSkeletalMesh* Interface, FNiagaraSystemInstance* SystemInstance)
{
	check(Interface);
	check(SystemInstance);

	// Initialize members
	SceneComponent = nullptr;
	CachedAttachParent = nullptr;
	CachedUserParam = nullptr;
	SkeletalMesh = nullptr;
	Transform = FMatrix::Identity;
	TransformInverseTransposed = FMatrix::Identity;
	PrevTransform = FMatrix::Identity;
	DeltaSeconds = SystemInstance->GetWorld()->GetDeltaSeconds();
	ChangeId = Interface->ChangeId;
	bIsGpuUniformlyDistributedSampling = false;
	bUnlimitedBoneInfluences = false;
	MeshWeightStrideByte = 0;
	MeshSkinWeightIndexSizeByte = 0;
	MeshGpuSpawnStaticBuffers = nullptr;
	MeshGpuSpawnDynamicBuffers = nullptr;
	bAllowCPUMeshDataAccess = false;
	PreSkinnedLocalBoundsCenter = FVector3f::ZeroVector;
	PreSkinnedLocalBoundsExtents = FVector3f::ZeroVector;

	// Get skel mesh and confirm have valid data
	USkeletalMeshComponent* NewSkelComp = nullptr;
	USceneComponent* AttachComponent = SystemInstance->GetAttachComponent();
	USkeletalMesh* Mesh = Interface->GetSkeletalMesh(SystemInstance, AttachComponent, SceneComponent, NewSkelComp, this);

	SkeletalMesh = Mesh;
	bMeshValid = Mesh != nullptr;
	bComponentValid = SceneComponent.IsValid();

	FTransform ComponentTransform = (bComponentValid ? SceneComponent->GetComponentToWorld() : SystemInstance->GetWorldTransform());
	ComponentTransform.AddToTranslation(FVector(SystemInstance->GetLWCTile()) * -FLargeWorldRenderScalar::GetTileSize());
	Transform = ComponentTransform.ToMatrixWithScale();
	TransformInverseTransposed = Transform.Inverse().GetTransposed();
	PrevTransform = Transform;

	if (AttachComponent)
	{
		CachedAttachParent = AttachComponent->GetAttachParent();
	}

	bResetOnLODStreamedIn = false;
	CachedLODIdx = 0;
	CachedLODData.SafeRelease();

	//Setup where to spawn from
	SamplingRegionIndices.Empty();
	bool bAllRegionsAreAreaWeighting = true;

	if (Mesh)
	{
		// Determine the LOD index and sampling region indices
		const FStreamableRenderResourceState& SRRState = Mesh->GetStreamableResourceState();		
		const int32 NumValidLODs = FMath::Min(SRRState.NumRequestedLODs, SRRState.NumResidentLODs);
		if (NumValidLODs > 0)
		{
			const int32 CurrentFirstLOD = SRRState.LODCountToAssetFirstLODIdx(NumValidLODs);
			const int32 DesiredLODIndex = Interface->CalculateLODIndexAndSamplingRegions(Mesh, SamplingRegionIndices, bAllRegionsAreAreaWeighting);
			if (DesiredLODIndex != INDEX_NONE)
			{
				if (DesiredLODIndex >= CurrentFirstLOD)
				{
					CachedLODIdx = DesiredLODIndex;
				}
				else
				{
					CachedLODIdx = CurrentFirstLOD;
					bResetOnLODStreamedIn = true;
				}

				// Attempt to cache the LOD
				if (FSkeletalMeshRenderData* RenderData = Mesh->GetResourceForRendering())
				{
					if (RenderData->LODRenderData.IsValidIndex(CachedLODIdx))
					{
						CachedLODData = &RenderData->LODRenderData[CachedLODIdx];
					}
					
					if (!ensure(CachedLODData.IsValid()))
					{
						// NOTE: Assumption here is that the LOD render data is cacheable from GameThread as long as it's considered resident by
						// the StreamableRenderResourceState on GameThread. If this warning gets hit, that assumption has become incorrect.
						UE_LOG(LogNiagara, Log, TEXT("SkeletalMesh data interface failed to cache LOD %d. Sampling will fail. %s"), CachedLODIdx, *Interface->GetFullName());
						Mesh = nullptr;
					}
				}
				else
				{
					// Warn and continue as if the component has no mesh
					UE_LOG(LogNiagara, Log, TEXT("SkeletalMesh data interface with no Render data. Sampling will fail. %s"), *Interface->GetFullName());
					Mesh = nullptr;
				}
			}
			else
			{
				return false;
			}
		}
		else
		{
			// Warn and continue as if the component has no mesh
			UE_LOG(LogNiagara, Log, TEXT("SkeletalMesh data interface with no resident LODs. Sampling will fail. %s"), *Interface->GetFullName());
			Mesh = nullptr;
		}

		if (Mesh == nullptr)
		{
			CachedLODIdx = 0;
			bResetOnLODStreamedIn = false;
		}
#if WITH_EDITOR
		else
		{
			// HACK! This only works on systems created by a Niagara component...should maybe move somewhere else to cover non-component systems
			if (UNiagaraComponent* NiagaraComponent = Cast<UNiagaraComponent>(AttachComponent))
			{
				Mesh->GetOnMeshChanged().AddUObject(NiagaraComponent, &UNiagaraComponent::ReinitializeSystem);
				if (USkeleton* Skeleton = Mesh->GetSkeleton())
				{
					Skeleton->RegisterOnSkeletonHierarchyChanged(USkeleton::FOnSkeletonHierarchyChanged::CreateUObject(NiagaraComponent, &UNiagaraComponent::ReinitializeSystem));
				}
			}
		}
#endif

		if ( Mesh != nullptr )
		{
			const FBoxSphereBounds LocalBounds = Mesh->GetBounds();
			PreSkinnedLocalBoundsCenter = FVector3f(LocalBounds.Origin);
			PreSkinnedLocalBoundsExtents = FVector3f(LocalBounds.BoxExtent);
		}
	}

	check(CachedLODIdx >= 0);

	//Grab a handle to the skinning data if we have a component to skin.
	const ENDISkeletalMesh_SkinningMode SkinningMode = Interface->SkinningMode;
	FSkeletalMeshSkinningDataUsage Usage(
		CachedLODIdx,
		SkinningMode == ENDISkeletalMesh_SkinningMode::SkinOnTheFly || SkinningMode == ENDISkeletalMesh_SkinningMode::PreSkin,
		SkinningMode == ENDISkeletalMesh_SkinningMode::PreSkin
	);

	// GetSkeletalMeshGeneratedData() is meant to match with the required lod, so don't access unless we are using it.
	if (NewSkelComp)
	{
		// TODO: This change is temporary to work around a crash that happens when you change the
		// source mesh on a system which is running in the level from the details panel.
		// bool bNeedsDataImmediately = SystemInstance->IsSolo();
		const bool bNeedsDataImmediately = true;

		TWeakObjectPtr<USkeletalMeshComponent> SkelWeakCompPtr = NewSkelComp;
		FNDI_SkeletalMesh_GeneratedData& GeneratedData = SystemInstance->GetWorldManager()->EditGeneratedData<FNDI_SkeletalMesh_GeneratedData>();
		SkinningData = GeneratedData.GetCachedSkinningData(SkelWeakCompPtr, Usage, bNeedsDataImmediately);
	}
	else
	{
		SkinningData = FSkeletalMeshSkinningDataHandle(Usage, nullptr, false);
	}

	// support for UV mapping
	{
		bool UsedByCpuUvMapping = false;
		bool UsedByGpuUvMapping = false;

		SystemInstance->EvaluateBoundFunction(FSkeletalMeshInterfaceHelper::GetTriangleCoordAtUVName, UsedByCpuUvMapping, UsedByGpuUvMapping);
		SystemInstance->EvaluateBoundFunction(FSkeletalMeshInterfaceHelper::GetTriangleCoordInAabbName, UsedByCpuUvMapping, UsedByGpuUvMapping);

		const bool MeshValid = SkeletalMesh.IsValid();
		const bool SupportUvMappingCpu = UsedByCpuUvMapping && MeshValid;
		const bool SupportUvMappingGpu = UsedByGpuUvMapping && MeshValid && Interface->IsUsedWithGPUEmitter();

		FMeshUvMappingUsage UvMappingUsage(SupportUvMappingCpu, SupportUvMappingGpu);

		if (UvMappingUsage.IsValid())
		{
			const bool bNeedsDataImmediately = true;

			FNDI_SkeletalMesh_GeneratedData& GeneratedData = SystemInstance->GetWorldManager()->EditGeneratedData<FNDI_SkeletalMesh_GeneratedData>();
			UvMapping = GeneratedData.GetCachedUvMapping(SkeletalMesh, CachedLODIdx, Interface->UvSetIndex, UvMappingUsage, bNeedsDataImmediately);
		}
		else
		{
			UvMapping = FSkeletalMeshUvMappingHandle(UvMappingUsage, nullptr, false);
		}
	}

	// mesh connectivity
	{
		bool UsedByCpuConnectivity = false;
		bool UsedByGpuConnectivity = false;

		SystemInstance->EvaluateBoundFunction(FSkeletalMeshInterfaceHelper::GetAdjacentTriangleIndexName, UsedByCpuConnectivity, UsedByGpuConnectivity);
		SystemInstance->EvaluateBoundFunction(FSkeletalMeshInterfaceHelper::GetTriangleNeighborName, UsedByCpuConnectivity, UsedByGpuConnectivity);

		const bool MeshValid = SkeletalMesh.IsValid();
		const bool SupportConnectivityCpu = UsedByCpuConnectivity && MeshValid;
		const bool SupportConnectivityGpu = UsedByGpuConnectivity && MeshValid && Interface->IsUsedWithGPUEmitter();

		FSkeletalMeshConnectivityUsage ConnectivityUsage(SupportConnectivityCpu, SupportConnectivityGpu);

		if (ConnectivityUsage.IsValid())
		{
			const bool bNeedsDataImmediately = true;

			FNDI_SkeletalMesh_GeneratedData& GeneratedData = SystemInstance->GetWorldManager()->EditGeneratedData<FNDI_SkeletalMesh_GeneratedData>();
			Connectivity = GeneratedData.GetCachedConnectivity(SkeletalMesh, CachedLODIdx, ConnectivityUsage, bNeedsDataImmediately);
		}
		else
		{
			Connectivity = FSkeletalMeshConnectivityHandle(ConnectivityUsage, nullptr, false);
		}
	}

	//Init area weighting sampler for Sampling regions.
	if (SamplingRegionIndices.Num() > 1 && bAllRegionsAreAreaWeighting)
	{
		//We are sampling from multiple area weighted regions so setup the inter-region weighting sampler.
		SamplingRegionAreaWeightedSampler.Init(this);
	}

	if (Mesh)
	{
		check(CachedLODData);

		bAllowCPUMeshDataAccess = true; // Assume accessibility until proven otherwise below
		const FSkinWeightVertexBuffer* SkinWeightBuffer = GetSkinWeights();
		check(SkinWeightBuffer);

		// Check for the validity of the Mesh's cpu data.
		if (Mesh->GetLODInfo(CachedLODIdx)->bAllowCPUAccess)
		{
			const bool LODDataNumVerticesCorrect = CachedLODData->GetNumVertices() > 0;
			const bool LODDataPositonNumVerticesCorrect = CachedLODData->StaticVertexBuffers.PositionVertexBuffer.GetNumVertices() > 0;
			const bool SkinWeightBufferNumVerticesCorrect = SkinWeightBuffer->GetNumVertices() > 0;
			const bool bIndexBufferValid = CachedLODData->MultiSizeIndexContainer.IsIndexBufferValid();
			const bool bIndexBufferFound = bIndexBufferValid && (CachedLODData->MultiSizeIndexContainer.GetIndexBuffer() != nullptr);
			const bool bIndexBufferNumCorrect = bIndexBufferFound && (CachedLODData->MultiSizeIndexContainer.GetIndexBuffer()->Num() > 0);

			bAllowCPUMeshDataAccess = LODDataNumVerticesCorrect &&
				LODDataPositonNumVerticesCorrect &&
				SkinWeightBufferNumVerticesCorrect &&
				bIndexBufferValid &&
				bIndexBufferFound &&
				bIndexBufferNumCorrect;
		}
		else
		{
			bAllowCPUMeshDataAccess = false;
		}

		// Generate excluded root bone index (if any)
		FReferenceSkeleton& RefSkel = Mesh->GetRefSkeleton();
		ExcludedBoneIndex = INDEX_NONE;
		if (Interface->bExcludeBone && !Interface->ExcludeBoneName.IsNone())
		{
			ExcludedBoneIndex = RefSkel.FindBoneIndex(Interface->ExcludeBoneName);
			if (ExcludedBoneIndex == INDEX_NONE)
			{
				UE_LOG(LogNiagara, Warning, TEXT("Skeletal Mesh Data Interface '%s' is missing bone '%s' this is ok but may not exclude what you want Mesh '%s' Component '%s'"), *Interface->GetFullName(), *Interface->ExcludeBoneName.ToString(), *Mesh->GetFullName(), *SceneComponent->GetFullName());
			}
		}

		// Gather filtered bones information
		if (Interface->FilteredBones.Num() > 0)
		{
			if (RefSkel.GetNum() > TNumericLimits<uint16>::Max())
			{
				UE_LOG(LogNiagara, Warning, TEXT("Skeletal Mesh Data Interface '%s' requires more bones '%d' than we currently support '%d' Mesh '%s' Component '%s'"), *Interface->GetFullName(), RefSkel.GetNum(), TNumericLimits<uint16>::Max(), *Mesh->GetFullName(), *SceneComponent->GetFullName());
				return false;
			}

			//-TODO: If the DI does not use unfiltered bones we can skip adding them here...
			TStringBuilder<256> MissingFilteredBones;

			FilteredAndUnfilteredBones.Reserve(RefSkel.GetNum());

			// Append filtered bones to the array first
			for (const FName& BoneName : Interface->FilteredBones)
			{
				const int32 Bone = RefSkel.FindBoneIndex(BoneName);
				if (Bone == INDEX_NONE)
				{
					if ( FNiagaraUtilities::LogVerboseWarnings() )
					{
						if (MissingFilteredBones.Len() > 0)
						{
							MissingFilteredBones.Append(TEXT(", "));
						}
						MissingFilteredBones.Append(BoneName.ToString());
					}
				}
				else
				{
					ensure(Bone <= TNumericLimits<uint16>::Max());
					FilteredAndUnfilteredBones.Add(Bone);
					++NumFilteredBones;
				}
			}

			// Append unfiltered bones to the array
			for (int32 i = 0; i < RefSkel.GetNum(); ++i)
			{
				// Don't append excluded bone
				if (i == ExcludedBoneIndex)
				{
					continue;
				}

				bool bExists = false;
				for (int32 j = 0; j < NumFilteredBones; ++j)
				{
					if (FilteredAndUnfilteredBones[j] == i)
					{
						bExists = true;
						break;
					}
				}
				if (!bExists)
				{
					FilteredAndUnfilteredBones.Add(i);
					++NumUnfilteredBones;
				}
			}

			if (MissingFilteredBones.Len() > 0)
			{
				UE_LOG(LogNiagara, Warning, TEXT("Skeletal Mesh Data Interface is trying to sample from filtered bones that don't exist in it's skeleton. Mesh(%s) Bones(%s) System(%s)"), *GetFullNameSafe(Mesh), MissingFilteredBones.ToString(), *GetFullNameSafe(SystemInstance->GetSystem()));
			}
		}
		else
		{
			// Note: We do not allocate space in the array as that wastes memory, we handle this special case when reading from unfiltered
			NumUnfilteredBones = RefSkel.GetNum();
		}

		// Gather filtered socket information
		{
			TArray<FName>& FilteredSockets = Interface->FilteredSockets;
			FilteredSocketInfo.SetNum(FilteredSockets.Num());

			//-TODO: We may need to handle skinning mode changes here
			if (NewSkelComp != nullptr)
			{
				for (int32 i = 0; i < FilteredSocketInfo.Num(); ++i)
				{
					FTransform SocketTransform;
					NewSkelComp->GetSocketInfoByName(FilteredSockets[i], SocketTransform, FilteredSocketInfo[i].BoneIdx);
					FilteredSocketInfo[i].Transform = (FTransform3f)SocketTransform;
				}
			}
			else
			{
				for (int32 i = 0; i < FilteredSocketInfo.Num(); ++i)
				{
					FilteredSocketInfo[i].Transform = FTransform3f(FMatrix44f(Mesh->GetComposedRefPoseMatrix(FilteredSockets[i])));
					FilteredSocketInfo[i].BoneIdx = INDEX_NONE;
				}
			}

			FilteredSocketBoneOffset = Mesh->GetRefSkeleton().GetNum();

			FilteredSocketTransformsIndex = 0;
			FilteredSocketTransforms[0].Reset(FilteredSockets.Num());
			FilteredSocketTransforms[0].AddDefaulted(FilteredSockets.Num());
			UpdateFilteredSocketTransforms();
			for (int32 i = 1; i < FilteredSocketTransforms.Num(); ++i)
			{
				FilteredSocketTransforms[i].Reset(FilteredSockets.Num());
				FilteredSocketTransforms[i].Append(FilteredSocketTransforms[0]);
			}

			if ( FNiagaraUtilities::LogVerboseWarnings() )
			{
				TStringBuilder<512> MissingSockets;
				for (FName SocketName : FilteredSockets)
				{
					if (Mesh->FindSocket(SocketName) == nullptr)
					{
						if (MissingSockets.Len() != 0)
						{
							MissingSockets.Append(TEXT(", "));
						}
						MissingSockets.Append(SocketName.ToString());
					}
				}

				if (MissingSockets.Len() > 0)
				{
					UE_LOG(LogNiagara, Warning, TEXT("Skeletal Mesh Data Interface is trying to sample from filtered sockets that don't exist in it's skeleton. Mesh(%s) Sockets(%s) System(%s)"), *GetFullNameSafe(Mesh), MissingSockets.ToString(), *GetFullNameSafe(SystemInstance->GetSystem()));
				}
			}
		}

		if (Interface->IsUsedWithGPUEmitter())
		{
			GPUSkinBoneInfluenceType BoneInfluenceType = SkinWeightBuffer->GetBoneInfluenceType();
			bUnlimitedBoneInfluences = (BoneInfluenceType == GPUSkinBoneInfluenceType::UnlimitedBoneInfluence);
			MeshWeightStrideByte = SkinWeightBuffer->GetConstantInfluencesVertexStride();
			MeshSkinWeightIndexSizeByte = SkinWeightBuffer->GetBoneIndexByteSize();
			MeshSkinWeightBuffer = SkinWeightBuffer->GetDataVertexBuffer();
			//check(MeshSkinWeightBufferSrv->IsValid()); // not available in this stream
			MeshSkinWeightLookupBuffer = SkinWeightBuffer->GetLookupVertexBuffer();

			FSkeletalMeshLODInfo* LODInfo = Mesh->GetLODInfo(CachedLODIdx);
			bIsGpuUniformlyDistributedSampling = LODInfo->bSupportUniformlyDistributedSampling && bAllRegionsAreAreaWeighting;

			if (Mesh->HasActiveClothingAssets())
			{
				UE_LOG(LogNiagara, Warning, TEXT("Skeletal Mesh %s has cloth asset on it: spawning from it might not work properly."), *Mesh->GetName());
			}

			const auto MaxInfluenceType = GetDefault<UNiagaraSettings>()->NDISkelMesh_GpuMaxInfluences;
			int32 MaxInfluenceCount = -1;
			if (MaxInfluenceType == ENDISkelMesh_GpuMaxInfluences::Type::AllowMax4)
			{
				MaxInfluenceCount = 4;
			}
			else if (MaxInfluenceType == ENDISkelMesh_GpuMaxInfluences::Type::AllowMax8) //-V517
			{
				MaxInfluenceCount = 8;
			}
			else
			{
				checkf(MaxInfluenceType == ENDISkelMesh_GpuMaxInfluences::Type::Unlimited, TEXT("Unknown value for NDISkelMesh_GpuMaxInfluences: %d"), MaxInfluenceType);
			}

			if (MaxInfluenceCount > 0 && (static_cast<uint32>(MaxInfluenceCount) < CachedLODData->GetVertexBufferMaxBoneInfluences()))
			{
				UE_LOG(LogNiagara, Warning, TEXT("Skeletal Mesh %s has bones extra influence: spawning from it might not work properly."), *Mesh->GetName());
			}

			const FSkeletalMeshSamplingInfo& SamplingInfo = Mesh->GetSamplingInfo();
			const FSkeletalMeshSamplingLODBuiltData* MeshSamplingBuiltData = SamplingInfo.GetBuiltData().WholeMeshBuiltData.IsValidIndex(CachedLODIdx) ? &SamplingInfo.GetBuiltData().WholeMeshBuiltData[CachedLODIdx] : nullptr;
			bIsGpuUniformlyDistributedSampling &= MeshSamplingBuiltData != nullptr;

			MeshGpuSpawnStaticBuffers = new FSkeletalMeshGpuSpawnStaticBuffers();
			MeshGpuSpawnStaticBuffers->Initialise(this, *CachedLODData, MeshSamplingBuiltData, SystemInstance);
			BeginInitResource(MeshGpuSpawnStaticBuffers);

			MeshGpuSpawnDynamicBuffers = new FSkeletalMeshGpuDynamicBufferProxy();
			MeshGpuSpawnDynamicBuffers->Initialise(RefSkel, *CachedLODData, FilteredSocketInfo.Num());
			BeginInitResource(MeshGpuSpawnDynamicBuffers);
		}
	}

	return true;
}

bool FNDISkeletalMesh_InstanceData::ResetRequired(UNiagaraDataInterfaceSkeletalMesh* Interface, FNiagaraSystemInstance* SystemInstance) const
{
	// Reset if the scene component we've cached has been invalidated
	USceneComponent* Comp = SceneComponent.Get();
	if (bComponentValid && !Comp)
	{
		return true;
	}

	// Reset if any mesh was bound on init, but is now invalidated
	USkeletalMesh* SkelMesh = SkeletalMesh.Get();
	if (bMeshValid && !SkelMesh)
	{
		return true;
	}

	if (Interface->MeshUserParameter.Parameter.IsValid())
	{
		// Reset if the user object ptr has been changed to look at a new object
		if (UserParamBinding.GetValue() != CachedUserParam)
		{
			return true;
		}
	}
	else if (Interface->GetSourceComponent())
	{
		// Reset if the source component changed (or there wasn't one and now there is)
		if (Interface->GetSourceComponent() != Comp)
		{
			return true;
		}
	}
	else if (USceneComponent* AttachComponent = SystemInstance->GetAttachComponent())
	{
		// Reset if we detect any attachment change.
		// TODO: This check is not really comprehensive. What we really need to know is if the mesh we cached comes from a skeletal mesh component in our
		// attachment hierarchy, and if that hierarchy has changed in the chain between the system instance's attach component and the cached component,
		// therefore potentially invalidating the cached component and mesh as our best choice.
		if (CachedAttachParent != AttachComponent->GetAttachParent())
		{
			// The scene component our system instance was associated with has changed attachment, so we need to reinit
			return true;
		}
	}

	// Reset if the LOD we relied on was streamed out, or if the LOD we need could now be available.
	if (SkelMesh != nullptr)
	{
		const FStreamableRenderResourceState& SRRState = SkelMesh->GetStreamableResourceState();
		const int32 NumValidLODs = FMath::Min(SRRState.NumRequestedLODs, SRRState.NumResidentLODs);
		if (NumValidLODs == 0)
		{
			return true;
		}

		const int32 CurrentFirstLOD = SRRState.LODCountToAssetFirstLODIdx(NumValidLODs);
		if (CurrentFirstLOD > CachedLODIdx || (CurrentFirstLOD < CachedLODIdx && bResetOnLODStreamedIn))
		{
			return true;
		}
	}

	// Reset if the skeletal mesh on the cached skeletal mesh component changed.
	if (USkeletalMeshComponent* SkelComp = Cast<USkeletalMeshComponent>(Comp))
	{
		if (SkelComp->GetSkeletalMeshAsset() != SkelMesh)
		{
			if (SkinningData.SkinningData.IsValid())
			{
				SkinningData.SkinningData.Get()->ForceDataRefresh();
			}
			return true;
		}
	}

	// Reset if any parameters changed on the data interface
	if (Interface->ChangeId != ChangeId)
	{
		return true;
	}

	return false;
}

bool FNDISkeletalMesh_InstanceData::Tick(UNiagaraDataInterfaceSkeletalMesh* Interface, FNiagaraSystemInstance* SystemInstance, float InDeltaSeconds)
{
	if (ResetRequired(Interface, SystemInstance))
	{
		return true;
	}
	else
	{
		DeltaSeconds = InDeltaSeconds;

		PrevTransform = Transform;
		FTransform ComponentTransform = (SceneComponent.IsValid() ? SceneComponent->GetComponentToWorld() : SystemInstance->GetWorldTransform());
		ComponentTransform.AddToTranslation(FVector(SystemInstance->GetLWCTile()) * -FLargeWorldRenderScalar::GetTileSize());
		Transform = ComponentTransform.ToMatrixWithScale();
		TransformInverseTransposed = Transform.Inverse().GetTransposed();

		// Cache socket transforms to avoid potentially calculating them multiple times during the VM
		FilteredSocketTransformsIndex = (FilteredSocketTransformsIndex + 1) % FilteredSocketTransforms.Num();
		UpdateFilteredSocketTransforms();

		if (MeshGpuSpawnDynamicBuffers)
		{
			MeshGpuSpawnDynamicBuffers->NewFrame(this, GetLODIndex());
		}

		return false;
	}
}

void FNDISkeletalMesh_InstanceData::UpdateFilteredSocketTransforms()
{
	USkeletalMeshComponent* SkelComp = Cast<USkeletalMeshComponent>(SceneComponent.Get());
	TArray<FTransform3f>& WriteBuffer = GetFilteredSocketsWriteBuffer();

	for (int32 i = 0; i < FilteredSocketInfo.Num(); ++i)
	{
		const FCachedSocketInfo& SocketInfo = FilteredSocketInfo[i];
		const FTransform& BoneTransform = SocketInfo.BoneIdx != INDEX_NONE ? SkelComp->GetBoneTransform(SocketInfo.BoneIdx, FTransform::Identity) : FTransform::Identity;
		WriteBuffer[i] = SocketInfo.Transform * FTransform3f(BoneTransform);
	}
}

bool FNDISkeletalMesh_InstanceData::HasColorData()
{
	return CachedLODData && CachedLODData->StaticVertexBuffers.ColorVertexBuffer.GetNumVertices() != 0;
}

void FNDISkeletalMesh_InstanceData::Release()
{
	if (MeshGpuSpawnStaticBuffers || MeshGpuSpawnDynamicBuffers)
	{
		ENQUEUE_RENDER_COMMAND(DeleteResource)(
			[RT_StaticBuffers=MeshGpuSpawnStaticBuffers, RT_DynamicBuffers=MeshGpuSpawnDynamicBuffers](FRHICommandListImmediate& RHICmdList)
			{
				if (RT_StaticBuffers)
				{
					RT_StaticBuffers->ReleaseResource();
					delete RT_StaticBuffers;
				}
				if (RT_DynamicBuffers)
				{
					RT_DynamicBuffers->ReleaseResource();
					delete RT_DynamicBuffers;
				}
			}
		);
		MeshGpuSpawnStaticBuffers = nullptr;
		MeshGpuSpawnDynamicBuffers = nullptr;
	}
}

//Instance Data END
//////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////
// UNiagaraDataInterfaceSkeletalMesh

UNiagaraDataInterfaceSkeletalMesh::UNiagaraDataInterfaceSkeletalMesh(FObjectInitializer const& ObjectInitializer)
	: Super(ObjectInitializer)
	  , SourceMode(ENDISkeletalMesh_SourceMode::Default)
#if WITH_EDITORONLY_DATA
	  , PreviewMesh(nullptr)
#endif
	  , SoftSourceActor(nullptr)
	  , SourceComponent(nullptr)
      , SkinningMode(ENDISkeletalMesh_SkinningMode::SkinOnTheFly)
	  , WholeMeshLOD(INDEX_NONE)
	  , ChangeId(0)
{
	FNiagaraTypeDefinition Def(UObject::StaticClass());
	MeshUserParameter.Parameter.SetType(Def);

	static const FName RootBoneName("root");
	ExcludeBoneName = RootBoneName;
	bExcludeBone = false;

	Proxy.Reset(new FNiagaraDataInterfaceProxySkeletalMesh());
}


void UNiagaraDataInterfaceSkeletalMesh::PostInitProperties()
{
	Super::PostInitProperties();

	//Can we register data interfaces as regular types and fold them into the FNiagaraVariable framework for UI and function calls etc?
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		ENiagaraTypeRegistryFlags DIFlags =
			ENiagaraTypeRegistryFlags::AllowAnyVariable |
			ENiagaraTypeRegistryFlags::AllowParameter;
		FNiagaraTypeRegistry::Register(FNiagaraTypeDefinition(GetClass()), DIFlags);

		//Still some issues with using custom structs. Convert node for example throws a wobbler. TODO after GDC.
		ENiagaraTypeRegistryFlags CoordFlags =
			ENiagaraTypeRegistryFlags::AllowAnyVariable |
			ENiagaraTypeRegistryFlags::AllowParameter;
		FNiagaraTypeRegistry::Register(FMeshTriCoordinate::StaticStruct(), CoordFlags);
	}
}

void UNiagaraDataInterfaceSkeletalMesh::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	if(USkeletalMesh* LocalPreviewMesh = PreviewMesh.Get())
	{
		LocalPreviewMesh->ConditionalPostLoad();
	}
#endif
#if WITH_EDITORONLY_DATA
	if (Source_DEPRECATED != nullptr)
	{
		SoftSourceActor = Source_DEPRECATED;
	}
#endif
}

#if WITH_EDITOR
void UNiagaraDataInterfaceSkeletalMesh::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	ChangeId++;

	if (PropertyChangedEvent.Property &&
		PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UNiagaraDataInterfaceSkeletalMesh, SourceMode) &&
		SourceMode != ENDISkeletalMesh_SourceMode::Default &&
		SourceMode != ENDISkeletalMesh_SourceMode::Source)
	{
		// Clear out any source that is set to prevent unnecessary references, since we won't even consider them
		SoftSourceActor = nullptr;
		SourceComponent = nullptr;
	}
}

bool UNiagaraDataInterfaceSkeletalMesh::CanEditChange(const FProperty* InProperty) const
{
	if (!Super::CanEditChange(InProperty))
	{
		return false;
	}

	if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UNiagaraDataInterfaceSkeletalMesh, SoftSourceActor) &&
		SourceMode != ENDISkeletalMesh_SourceMode::Default &&
		SourceMode != ENDISkeletalMesh_SourceMode::Source)
	{
		// Disable "Source" if it won't be considered
		return false;
	}

	return true;
}

#endif //WITH_EDITOR

void UNiagaraDataInterfaceSkeletalMesh::GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions)
{
	const int32 FirstFunction = OutFunctions.Num();

	{
		FNiagaraFunctionSignature& Sig = OutFunctions.AddDefaulted_GetRef();
		Sig.Name = NDISkelMeshLocal::NAME_GetPreSkinnedLocalBounds;
		Sig.Inputs.Emplace(FNiagaraTypeDefinition(GetClass()), TEXT("SkeletalMesh"));
		Sig.Outputs.Emplace(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Center"));
		Sig.Outputs.Emplace(FNiagaraTypeDefinition::GetVec3Def(), TEXT("ExtentsMin"));
		Sig.Outputs.Emplace(FNiagaraTypeDefinition::GetVec3Def(), TEXT("ExtentsMax"));
		Sig.Outputs.Emplace(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Extents"));
		Sig.Outputs.Emplace(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Half Extents"));
		Sig.SetDescription(LOCTEXT("GetPreSkinnedLocalBoundsDesc", "Returns the local bounds of the Skeletal Mesh"));
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
	}

	GetTriangleSamplingFunctions(OutFunctions);
	GetVertexSamplingFunctions(OutFunctions);
	GetSkeletonSamplingFunctions(OutFunctions);

#if WITH_EDITORONLY_DATA
	for ( int i=FirstFunction; i < OutFunctions.Num(); ++i )
	{
		OutFunctions[i].FunctionVersion = FNiagaraSkelMeshDIFunctionVersion::LatestVersion;
	}
#endif
}

void UNiagaraDataInterfaceSkeletalMesh::GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction &OutFunc)
{
	FNDISkeletalMesh_InstanceData* InstData = (FNDISkeletalMesh_InstanceData*)InstanceData;

	if (!InstData)
	{
		OutFunc = FVMExternalFunction();
		return;
	}

	// Bind misc functions
	if (BindingInfo.Name == NDISkelMeshLocal::NAME_GetPreSkinnedLocalBounds)
	{
		OutFunc = FVMExternalFunction::CreateUObject(this, &UNiagaraDataInterfaceSkeletalMesh::VMGetPreSkinnedLocalBounds);
		return;
	}

	// Bind skeleton sampling function
	BindSkeletonSamplingFunction(BindingInfo, InstData, OutFunc);
	if (OutFunc.IsBound())
	{
		return;
	}

	// Bind triangle sampling function
	BindTriangleSamplingFunction(BindingInfo, InstData, OutFunc);
	if (OutFunc.IsBound())
	{
		if (!InstData->bAllowCPUMeshDataAccess)
		{
			UE_LOG(LogNiagara, Log, TEXT("Skeletal Mesh Data Interface is trying to use triangle sampling function '%s', but either no CPU access is set on the mesh or the data is invalid. Interface: %s"),
				*BindingInfo.Name.ToString(), *GetFullName());
		}
		return;
	}

	// Bind vertex sampling function
	BindVertexSamplingFunction(BindingInfo, InstData, OutFunc);
	if (OutFunc.IsBound())
	{
		if (!InstData->bAllowCPUMeshDataAccess)
		{
			UE_LOG(LogNiagara, Log, TEXT("Skeletal Mesh Data Interface is trying to use vertex sampling function '%s' but either no CPU access is set on the mesh, or the data is invalid. Interface: %s"),
				*BindingInfo.Name.ToString(), *GetFullName());
		}
		return;
	}
}


bool UNiagaraDataInterfaceSkeletalMesh::CopyToInternal(UNiagaraDataInterface* Destination) const
{
	if (!Super::CopyToInternal(Destination))
	{
		return false;
	}

	UNiagaraDataInterfaceSkeletalMesh* OtherTyped = CastChecked<UNiagaraDataInterfaceSkeletalMesh>(Destination);
	OtherTyped->UnbindSourceDelegates();
	OtherTyped->SourceMode = SourceMode;
	OtherTyped->SoftSourceActor = SoftSourceActor;
	OtherTyped->MeshUserParameter = MeshUserParameter;
	OtherTyped->SourceComponent = SourceComponent;
	OtherTyped->SkinningMode = SkinningMode;
	OtherTyped->SamplingRegions = SamplingRegions;
	OtherTyped->WholeMeshLOD = WholeMeshLOD;
	OtherTyped->FilteredBones = FilteredBones;
	OtherTyped->FilteredSockets = FilteredSockets;
	OtherTyped->bExcludeBone = bExcludeBone;
	OtherTyped->ExcludeBoneName = ExcludeBoneName;
	OtherTyped->bRequireCurrentFrameData = bRequireCurrentFrameData;
	OtherTyped->UvSetIndex = UvSetIndex;
#if WITH_EDITORONLY_DATA
	OtherTyped->PreviewMesh = PreviewMesh;
#endif
	OtherTyped->BindSourceDelegates();

	return true;
}

bool UNiagaraDataInterfaceSkeletalMesh::Equals(const UNiagaraDataInterface* Other) const
{
	if (!Super::Equals(Other))
	{
		return false;
	}
	const UNiagaraDataInterfaceSkeletalMesh* OtherTyped = CastChecked<const UNiagaraDataInterfaceSkeletalMesh>(Other);
	return OtherTyped->SourceMode == SourceMode &&
#if WITH_EDITORONLY_DATA
		OtherTyped->PreviewMesh == PreviewMesh &&
#endif
		OtherTyped->SoftSourceActor == SoftSourceActor &&
		OtherTyped->MeshUserParameter == MeshUserParameter &&
		OtherTyped->SourceComponent == SourceComponent &&
		OtherTyped->SkinningMode == SkinningMode &&
		OtherTyped->SamplingRegions == SamplingRegions &&
		OtherTyped->WholeMeshLOD == WholeMeshLOD &&
		OtherTyped->FilteredBones == FilteredBones &&
		OtherTyped->FilteredSockets == FilteredSockets &&
		OtherTyped->bExcludeBone == bExcludeBone &&
		OtherTyped->ExcludeBoneName == ExcludeBoneName &&
		OtherTyped->UvSetIndex == UvSetIndex &&
		OtherTyped->bRequireCurrentFrameData == bRequireCurrentFrameData;
}

bool UNiagaraDataInterfaceSkeletalMesh::InitPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance)
{
	FNDISkeletalMesh_InstanceData* Inst = new (PerInstanceData) FNDISkeletalMesh_InstanceData();
	check(IsAligned(PerInstanceData, 16));
	return Inst->Init(this, SystemInstance);
}

void UNiagaraDataInterfaceSkeletalMesh::DestroyPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance)
{
	FNDISkeletalMesh_InstanceData* Inst = (FNDISkeletalMesh_InstanceData*)PerInstanceData;

#if WITH_EDITOR
	if (USkeletalMesh* SkeletalMesh = Inst->SkeletalMesh.Get())
	{
		if (UNiagaraComponent* NiagaraComponent = Cast<UNiagaraComponent>(SystemInstance->GetAttachComponent()))
		{
			SkeletalMesh->GetOnMeshChanged().RemoveAll(NiagaraComponent);
			if (USkeleton* Skeleton = SkeletalMesh->GetSkeleton())
			{
				Skeleton->UnregisterOnSkeletonHierarchyChanged(NiagaraComponent);
			}
		}
	}
#endif

	ENQUEUE_RENDER_COMMAND(FNiagaraDestroySkeletalMeshInstanceData) (
		[RT_Proxy=GetProxyAs<FNiagaraDataInterfaceProxySkeletalMesh>(), InstanceID=SystemInstance->GetId()](FRHICommandListImmediate& CmdList)
		{
			RT_Proxy->SystemInstancesToData.Remove(InstanceID);
		}
	);

	Inst->Release();
	Inst->~FNDISkeletalMesh_InstanceData();
}

bool UNiagaraDataInterfaceSkeletalMesh::PerInstanceTick(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance, float InDeltaSeconds)
{
	FNDISkeletalMesh_InstanceData* Inst = (FNDISkeletalMesh_InstanceData*)PerInstanceData;
	return Inst->Tick(this, SystemInstance, InDeltaSeconds);
}

#if WITH_NIAGARA_DEBUGGER
void UNiagaraDataInterfaceSkeletalMesh::DrawDebugHud(UCanvas* Canvas, FNiagaraSystemInstance* SystemInstance, FString& VariableDataString, bool bVerbose) const
{
	FNDISkeletalMesh_InstanceData* InstanceData_GT = SystemInstance->FindTypedDataInterfaceInstanceData<FNDISkeletalMesh_InstanceData>(this);
	if (InstanceData_GT == nullptr)
	{
		return;
	}

	USceneComponent* SceneComponent = InstanceData_GT->SceneComponent.Get();
	USkeletalMesh* SkeletalMesh = InstanceData_GT->SkeletalMesh.Get();
	VariableDataString = FString::Printf(TEXT("Skeleton(%s) SkelComp(%s)"), *GetNameSafe(SkeletalMesh), *GetNameSafe(SceneComponent));

	if ( bVerbose && SceneComponent && SkeletalMesh )
	{
		FSkeletalMeshAccessorHelper MeshAccessor;
		MeshAccessor.Init<TNDISkelMesh_FilterModeNone, TNDISkelMesh_AreaWeightingOff>(InstanceData_GT);
		if (MeshAccessor.AreBonesAccessible())
		{
			const TArray<FTransform3f>& BoneTransforms = MeshAccessor.SkinningData->CurrComponentTransforms();
			const FMatrix& InstanceTransform = InstanceData_GT->Transform;

			for (const FTransform3f& BoneTransform : BoneTransforms)
			{
				const FVector BoneLocation = InstanceTransform.TransformPosition(FVector(BoneTransform.GetLocation()));
				const FVector ScreenPos = Canvas->Project(BoneLocation, false);
				if (ScreenPos.Z <= 0.0f)
				{
					continue;
				}

				Canvas->Canvas->DrawNGon(FVector2D(ScreenPos), FColor::Red, 8, 4.0f);
			}
		}
	}
}
#endif

#if WITH_EDITOR
void UNiagaraDataInterfaceSkeletalMesh::GetFeedback(UNiagaraSystem* Asset, UNiagaraComponent* Component, TArray<FNiagaraDataInterfaceError>& OutErrors,
	TArray<FNiagaraDataInterfaceFeedback>& OutWarnings, TArray<FNiagaraDataInterfaceFeedback>& OutInfo)
{
	if (Asset == nullptr)
	{
		return;
	}

	bool bHasCPUAccessWarning = false;
	bool bHasNoMeshAssignedWarning = false;
	USkeletalMesh* SkelMesh = GetSkeletalMesh(Component);

	// Collect Errors
#if WITH_EDITORONLY_DATA
	if (SkelMesh != nullptr)
	{
		bool bHasCPUAccess = true;
		for (const FSkeletalMeshLODInfo& LODInfo : SkelMesh->GetLODInfoArray())
		{
			if (!LODInfo.bAllowCPUAccess)
			{
				bHasCPUAccess = false;
				break;
			}
		}

		// Check for the possibility that this mesh won't behave properly because of no CPU access
		if (!bHasCPUAccess)
		{
			// Collect all scripts used by the system
			// NOTE: We don't descriminate between CPU or GPU scripts here because while GPU access will "Just Work"
			// on some platforms, other platforms (like Mobile or OpenGL) do not create a shader resource view for the
			// buffers unless the CPU access flag is enabled.
			TArray<UNiagaraScript*> Scripts;
			Scripts.Add(Asset->GetSystemSpawnScript());
			Scripts.Add(Asset->GetSystemUpdateScript());
			for (auto&& EmitterHandle : Asset->GetEmitterHandles())
			{
				EmitterHandle.GetEmitterData()->GetScripts(Scripts, false);
			}

			// Now check if any script uses functions that require CPU access
			// TODO: This isn't complete enough. It doesn't guarantee that the DI used by these functions are THIS DI.
			// Finding that information out is currently non-trivial so just pop a warning with the possibility of false
			// positives
			TArray<FNiagaraFunctionSignature> Functions;
			GetTriangleSamplingFunctions(Functions);
			GetVertexSamplingFunctions(Functions);

			bHasCPUAccessWarning = [this, &Scripts, &Functions]()
			{
				for (const auto Script : Scripts)
				{
					for (const auto& DIInfo : Script->GetVMExecutableData().DataInterfaceInfo)
					{
						if (DIInfo.MatchesClass(GetClass()))
						{
							for (const auto& Func : DIInfo.RegisteredFunctions)
							{
								auto Filter = [&Func](const FNiagaraFunctionSignature& CPUSig)
								{
									return CPUSig.Name == Func.Name;
								};
								if (Functions.FindByPredicate(Filter))
								{
									return true;
								}
							}
						}
					}
				}
				return false;
			}();
		}
	}
	else
	{
		bHasNoMeshAssignedWarning = true;
	}

	// Report Errors/Warnings
	if (SkelMesh && bHasCPUAccessWarning)
	{
		FNiagaraDataInterfaceFeedback CPUAccessNotAllowedWarning(
			FText::Format(LOCTEXT("CPUAccessNotAllowedError", "This mesh may need CPU access in order to be used properly (even when used by GPU emitters). ({0})"), FText::FromString(SkelMesh->GetName())),
			LOCTEXT("CPUAccessNotAllowedErrorSummary", "CPU access error"),
			FNiagaraDataInterfaceFix::CreateLambda([=]()
				{
					SkelMesh->Modify();
					for (FSkeletalMeshLODInfo& LODInfo : SkelMesh->GetLODInfoArray())
					{
						LODInfo.bAllowCPUAccess = true;
					}
					return true;
				}));

		OutWarnings.Add(CPUAccessNotAllowedWarning);
	}
#endif

	if (SoftSourceActor.Get() == nullptr && bHasNoMeshAssignedWarning)
	{
		FNiagaraDataInterfaceFeedback NoMeshAssignedError(LOCTEXT("NoMeshAssignedError", "This Data Interface should be assigned a skeletal mesh to operate correctly."),
			LOCTEXT("NoMeshAssignedErrorSummary", "No mesh assigned warning"),
			FNiagaraDataInterfaceFix());

		OutWarnings.Add(NoMeshAssignedError);
	}

	// Look for bones being used that are LOD'ed out
	if ( (SkelMesh != nullptr) && (SkelMesh->GetResourceForRendering() != nullptr) )
	{
		FSkeletalMeshRenderData* SkelResource = SkelMesh->GetResourceForRendering();
		auto IsBoneRequiredInAllLODs =
			[&](const FName BoneName)
			{
				const int32 BoneIndex = SkelMesh->GetRefSkeleton().FindBoneIndex(BoneName);
				if (BoneIndex == INDEX_NONE)
				{
					return false;
				}

				for ( const FSkeletalMeshLODRenderData& LODData : SkelResource->LODRenderData )
				{
					if ( !LODData.RequiredBones.Contains(BoneIndex) )
					{
						return false;
					}
				}
				return true;
			};

		if (FilteredBones.Num() > 0)
		{
			FString MissingBoneList;
			for (FName Bone : FilteredBones)
			{
				if ( !IsBoneRequiredInAllLODs(Bone) )
				{
					MissingBoneList.Append(TEXT("\n"));
					Bone.AppendString(MissingBoneList);
				}
			}

			if (MissingBoneList.Len() > 0 )
			{
				OutWarnings.Emplace(
					FText::Format(LOCTEXT("BonesLODOutError", "Filtered Bones may not animate in all LODs, this can lead to incorrect results when animating at those LOD levels.\n{0}"), FText::FromString(MissingBoneList)),
					LOCTEXT("BonesLODOutErrorSummary", "Filtered bones may not animate in all LODs."),
					FNiagaraDataInterfaceFix()
				);
			}
		}
	}
}

//Deprecated functions we check for and advise on updates in ValidateFunction
static const FName GetTriPositionName_DEPRECATED("GetTriPosition");
static const FName GetTriPositionWSName_DEPRECATED("GetTriPositionWS");
static const FName GetTriPositionVelocityAndNormalName_DEPRECATED("GetTriPositionVelocityAndNormal");
static const FName GetTriPositionVelocityAndNormalWSName_DEPRECATED("GetTriPositionVelocityAndNormalWS");
static const FName GetTriPositionVelocityAndNormalBinormalTangentName_DEPRECATED("GetTriPositionVelocityAndNormalBinormalTangent");
static const FName GetTriPositionVelocityAndNormalBinormalTangentWSName_DEPRECATED("GetTriPositionVelocityAndNormalBinormalTangentWS");

void UNiagaraDataInterfaceSkeletalMesh::ValidateFunction(const FNiagaraFunctionSignature& Function, TArray<FText>& OutValidationErrors)
{
	TArray<FNiagaraFunctionSignature> DIFuncs;
	GetFunctions(DIFuncs);

	if (!DIFuncs.Contains(Function))
	{
		TArray<FNiagaraFunctionSignature> SkinnedDataDeprecatedFunctions;
		{
			FNiagaraFunctionSignature Sig;
			Sig.Name = GetTriPositionName_DEPRECATED;
			Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("SkeletalMesh")));
			Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(FMeshTriCoordinate::StaticStruct()), TEXT("Coord")));
			Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Position")));
			Sig.bMemberFunction = true;
			Sig.bRequiresContext = false;
			SkinnedDataDeprecatedFunctions.Add(Sig);
		}

		{
			FNiagaraFunctionSignature Sig;
			Sig.Name = GetTriPositionWSName_DEPRECATED;
			Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("SkeletalMesh")));
			Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(FMeshTriCoordinate::StaticStruct()), TEXT("Coord")));
			Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Position")));
			Sig.bMemberFunction = true;
			Sig.bRequiresContext = false;
			SkinnedDataDeprecatedFunctions.Add(Sig);
		}

		{
			FNiagaraFunctionSignature Sig;
			Sig.Name = GetTriPositionVelocityAndNormalName_DEPRECATED;
			Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("SkeletalMesh")));
			Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(FMeshTriCoordinate::StaticStruct()), TEXT("Coord")));
			Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Position")));
			Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Velocity")));
			Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Normal")));
			Sig.bMemberFunction = true;
			Sig.bRequiresContext = false;
			SkinnedDataDeprecatedFunctions.Add(Sig);
		}

		{
			FNiagaraFunctionSignature Sig;
			Sig.Name = GetTriPositionVelocityAndNormalWSName_DEPRECATED;
			Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("SkeletalMesh")));
			Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(FMeshTriCoordinate::StaticStruct()), TEXT("Coord")));
			Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Position")));
			Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Velocity")));
			Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Normal")));
			Sig.bMemberFunction = true;
			Sig.bRequiresContext = false;
			SkinnedDataDeprecatedFunctions.Add(Sig);
		}

		{
			FNiagaraFunctionSignature Sig;
			Sig.Name = GetTriPositionVelocityAndNormalBinormalTangentName_DEPRECATED;
			Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("SkeletalMesh")));
			Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(FMeshTriCoordinate::StaticStruct()), TEXT("Coord")));
			Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("UV Set")));
			Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Position")));
			Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Velocity")));
			Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Normal")));
			Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Binormal")));
			Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Tangent")));
			Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec2Def(), TEXT("UV")));
			Sig.bMemberFunction = true;
			Sig.bRequiresContext = false;
			SkinnedDataDeprecatedFunctions.Add(Sig);
		}

		{
			FNiagaraFunctionSignature Sig;
			Sig.Name = GetTriPositionVelocityAndNormalBinormalTangentWSName_DEPRECATED;
			Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("SkeletalMesh")));
			Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(FMeshTriCoordinate::StaticStruct()), TEXT("Coord")));
			Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("UV Set")));
			Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Position")));
			Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Velocity")));
			Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Normal")));
			Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Binormal")));
			Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Tangent")));
			Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec2Def(), TEXT("UV")));
			Sig.bMemberFunction = true;
			Sig.bRequiresContext = false;
			SkinnedDataDeprecatedFunctions.Add(Sig);
		}

		if (SkinnedDataDeprecatedFunctions.Contains(Function))
		{
			OutValidationErrors.Add(FText::Format(LOCTEXT("SkinnedDataFunctionDeprecationMsgFmt", "Skeletal Mesh DI Function {0} has been deprecated. Use GetSinnedTriangleData or GetSkinnedTriangleDataWS instead.\n"), FText::FromName(Function.Name)));
		}
		else
		{
			Super::ValidateFunction(Function, OutValidationErrors);
		}
	}
}

#endif

#if WITH_EDITORONLY_DATA
bool UNiagaraDataInterfaceSkeletalMesh::AppendCompileHash(FNiagaraCompileHashVisitor* InVisitor) const
{
	if (!Super::AppendCompileHash(InVisitor))
		return false;

	InVisitor->UpdateString(TEXT("NiagaraDataInterfaceSkeletalMeshHLSLSource"), GetShaderFileHash(NDISkelMeshLocal::CommonShaderFile, EShaderPlatform::SP_PCD3D_SM5).ToString());
	InVisitor->UpdateString(TEXT("NiagaraDataInterfaceSkeletalMeshTemplateHLSLSource"), GetShaderFileHash(NDISkelMeshLocal::TemplateShaderFile, EShaderPlatform::SP_PCD3D_SM5).ToString());
	InVisitor->UpdateShaderParameters<NDISkelMeshLocal::FShaderParameters>();

	InVisitor->UpdatePOD(TEXT("NDISkelmesh_Influences"), int(GetDefault<UNiagaraSettings>()->NDISkelMesh_GpuMaxInfluences));
	InVisitor->UpdatePOD(TEXT("NDISkelmesh_ProbAliasFormat"), int(GetDefault<UNiagaraSettings>()->NDISkelMesh_GpuUniformSamplingFormat));

	return true;
}
#endif

#if WITH_EDITOR
void UNiagaraDataInterfaceSkeletalMesh::ModifyCompilationEnvironment(EShaderPlatform ShaderPlatform, struct FShaderCompilerEnvironment& OutEnvironment) const
{
	Super::ModifyCompilationEnvironment(ShaderPlatform, OutEnvironment);

	OutEnvironment.SetDefine(TEXT("DISKELMESH_BONE_INFLUENCES"), int(GetDefault<UNiagaraSettings>()->NDISkelMesh_GpuMaxInfluences));
	OutEnvironment.SetDefine(TEXT("DISKELMESH_PROBALIAS_FORMAT"), int(GetDefault<UNiagaraSettings>()->NDISkelMesh_GpuUniformSamplingFormat));
	OutEnvironment.SetDefine(TEXT("DISKELMESH_ADJ_INDEX_FORMAT"), int(GetDefault<UNiagaraSettings>()->NDISkelMesh_AdjacencyTriangleIndexFormat));
}
#endif

#if WITH_EDITORONLY_DATA
void UNiagaraDataInterfaceSkeletalMesh::GetCommonHLSL(FString& OutHLSL)
{
	OutHLSL.Appendf(TEXT("#include \"%s\"\n"), NDISkelMeshLocal::CommonShaderFile);
	OutHLSL.Append(TEXT("#include \"/Plugin/FX/Niagara/Private/Experimental/NiagaraUvMappingUtils.ush\"\n"));
}

void UNiagaraDataInterfaceSkeletalMesh::GetParameterDefinitionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL)
{
	TMap<FString, FStringFormatArg> TemplateArgs =
	{
		{TEXT("ParameterName"),	ParamInfo.DataInterfaceHLSLSymbol},
	};

	FString TemplateFile;
	LoadShaderSourceFile(NDISkelMeshLocal::TemplateShaderFile, EShaderPlatform::SP_PCD3D_SM5, &TemplateFile, nullptr);
	OutHLSL += FString::Format(*TemplateFile, TemplateArgs);
}

bool UNiagaraDataInterfaceSkeletalMesh::GetFunctionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, const FNiagaraDataInterfaceGeneratedFunction& FunctionInfo, int FunctionInstanceIndex, FString& OutHLSL)
{
	static const TSet<FName> ValidGpuFunctions =
	{
		FSkeletalMeshInterfaceHelper::GetTriCoordVerticesName,
		FSkeletalMeshInterfaceHelper::GetTriangleCountName,
		FSkeletalMeshInterfaceHelper::GetFilteredTriangleCountName,
		//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		// Bone Sampling
		FSkeletalMeshInterfaceHelper::GetSkinnedBoneDataName,
		FSkeletalMeshInterfaceHelper::GetSkinnedBoneDataInterpolatedName,
		FSkeletalMeshInterfaceHelper::GetSkinnedBoneDataWSName,
		FSkeletalMeshInterfaceHelper::GetSkinnedBoneDataWSInterpolatedName,
		//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		// Vertex Sampling
		FSkeletalMeshInterfaceHelper::GetVertexDataName,
		FSkeletalMeshInterfaceHelper::GetSkinnedVertexDataName,
		FSkeletalMeshInterfaceHelper::GetSkinnedVertexDataWSName,
		FSkeletalMeshInterfaceHelper::GetVertexColorName,
		FSkeletalMeshInterfaceHelper::GetVertexUVName,
		FSkeletalMeshInterfaceHelper::IsValidVertexName,
		FSkeletalMeshInterfaceHelper::RandomVertexName,
		FSkeletalMeshInterfaceHelper::GetVertexCountName,
		FSkeletalMeshInterfaceHelper::IsValidFilteredVertexName,
		FSkeletalMeshInterfaceHelper::RandomFilteredVertexName,
		FSkeletalMeshInterfaceHelper::GetFilteredVertexCountName,
		FSkeletalMeshInterfaceHelper::GetFilteredVertexAtName,
		//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		// Filtered Bone
		FSkeletalMeshInterfaceHelper::IsValidBoneName,
		FSkeletalMeshInterfaceHelper::RandomBoneName,
		FSkeletalMeshInterfaceHelper::GetBoneCountName,
		FSkeletalMeshInterfaceHelper::GetParentBoneName,
		FSkeletalMeshInterfaceHelper::GetFilteredBoneCountName,
		FSkeletalMeshInterfaceHelper::GetFilteredBoneAtName,
		FSkeletalMeshInterfaceHelper::RandomFilteredBoneName,
		FSkeletalMeshInterfaceHelper::GetUnfilteredBoneCountName,
		FSkeletalMeshInterfaceHelper::GetUnfilteredBoneAtName,
		FSkeletalMeshInterfaceHelper::RandomUnfilteredBoneName,
		//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		// Filtered Socket
		FSkeletalMeshInterfaceHelper::GetFilteredSocketCountName,
		FSkeletalMeshInterfaceHelper::GetFilteredSocketBoneAtName,
		FSkeletalMeshInterfaceHelper::GetFilteredSocketTransformName,
		FSkeletalMeshInterfaceHelper::RandomFilteredSocketName,
		//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		// Misc bone functions
		FSkeletalMeshInterfaceHelper::RandomFilteredSocketOrBoneName,
		FSkeletalMeshInterfaceHelper::GetFilteredSocketOrBoneCountName,
		FSkeletalMeshInterfaceHelper::GetFilteredSocketOrBoneAtName,
		//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		// Uv Mapping
		FSkeletalMeshInterfaceHelper::GetAdjacentTriangleIndexName,
		FSkeletalMeshInterfaceHelper::GetTriangleNeighborName,
	};

	if ( ValidGpuFunctions.Contains(FunctionInfo.DefinitionName) )
	{
		return true;
	}

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Special cases due to using FMeshTriCoordinate
	TMap<FString, FStringFormatArg> FormatArgs =
	{
		{TEXT("InstanceFunctionName"),	FunctionInfo.InstanceName},
		{TEXT("ParameterName"),			ParamInfo.DataInterfaceHLSLSymbol},
	};

	const TCHAR* FuncFormat = nullptr;
	if (FunctionInfo.DefinitionName == FSkeletalMeshInterfaceHelper::IsValidTriCoordName)
	{
		FuncFormat = TEXT("void {InstanceFunctionName}(in MeshTriCoordinate InCoord, out bool bIsValid) { IsValidTriCoord_{ParameterName}(InCoord.Tri, bIsValid); }\n");
	}
	else if (FunctionInfo.DefinitionName == FSkeletalMeshInterfaceHelper::RandomTriangleName)
	{
		FuncFormat = TEXT("void {InstanceFunctionName}(NiagaraRandInfo InRandomInfo, out MeshTriCoordinate OutCoord) { RandomTriangle_{ParameterName}(InRandomInfo.Seed1, InRandomInfo.Seed2, InRandomInfo.Seed3, OutCoord.Tri, OutCoord.BaryCoord); }\n");
	}
	else if (FunctionInfo.DefinitionName == FSkeletalMeshInterfaceHelper::RandomTriCoordName)
	{
		FuncFormat = TEXT("void {InstanceFunctionName}(NiagaraRandInfo InRandomInfo, out MeshTriCoordinate OutCoord) { RandomTriangle_{ParameterName}(InRandomInfo.Seed1, InRandomInfo.Seed2, InRandomInfo.Seed3, OutCoord.Tri, OutCoord.BaryCoord); }\n");
	}
	else if (FunctionInfo.DefinitionName == FSkeletalMeshInterfaceHelper::RandomFilteredTriangleName)
	{
		FuncFormat = TEXT("void {InstanceFunctionName}(NiagaraRandInfo InRandomInfo, out MeshTriCoordinate OutCoord) { RandomFilteredTriangle_{ParameterName}(InRandomInfo.Seed1, InRandomInfo.Seed2, InRandomInfo.Seed3, OutCoord.Tri, OutCoord.BaryCoord); }\n");
	}
	else if (FunctionInfo.DefinitionName == FSkeletalMeshInterfaceHelper::GetFilteredTriangleAtName)
	{
		FuncFormat = TEXT("void {InstanceFunctionName}(int FilteredIndex, in float3 BaryCoord, out MeshTriCoordinate OutCoord) { GetFilteredTriangleAt_{ParameterName}(FilteredIndex, OutCoord.Tri); OutCoord.BaryCoord = BaryCoord; }\n");
	}
	else if (FunctionInfo.DefinitionName == FSkeletalMeshInterfaceHelper::GetTriangleDataName)
	{
		FuncFormat = TEXT("void {InstanceFunctionName}(in MeshTriCoordinate InCoord, out float3 OutPosition, out float3 OutNormal, out float3 OutBinormal, out float3 OutTangent) { GetPointOnTriangle_{ParameterName}(InCoord.Tri, InCoord.BaryCoord, OutPosition, OutTangent, OutBinormal, OutNormal); }\n");
	}
	else if (FunctionInfo.DefinitionName == FSkeletalMeshInterfaceHelper::GetSkinnedTriangleDataWSName)
	{
		FuncFormat = TEXT("void {InstanceFunctionName}(in MeshTriCoordinate InCoord, out float3 OutPosition, out float3 OutVelocity, out float3 OutNormal, out float3 OutBinormal, out float3 OutTangent) { GetSkinnedTriangleDataWS_{ParameterName}(InCoord.Tri, InCoord.BaryCoord, OutPosition, OutVelocity, OutNormal, OutBinormal, OutTangent); }\n");
	}
	else if (FunctionInfo.DefinitionName == FSkeletalMeshInterfaceHelper::GetSkinnedTriangleDataWSInterpName)
	{
		FuncFormat = TEXT("void {InstanceFunctionName}(in MeshTriCoordinate InCoord, in float InInterp, out float3 OutPosition, out float3 OutVelocity, out float3 OutNormal, out float3 OutBinormal, out float3 OutTangent) { GetSkinnedTriangleDataInterpolatedWS_{ParameterName}(InCoord.Tri, InCoord.BaryCoord, InInterp, OutPosition, OutVelocity, OutNormal, OutBinormal, OutTangent); }\n");
	}
	else if (FunctionInfo.DefinitionName == FSkeletalMeshInterfaceHelper::GetSkinnedTriangleDataName)
	{
		FuncFormat = TEXT("void {InstanceFunctionName}(in MeshTriCoordinate InCoord, out float3 OutPosition, out float3 OutVelocity, out float3 OutNormal, out float3 OutBinormal, out float3 OutTangent) { GetSkinnedTriangleData_{ParameterName}(InCoord.Tri, InCoord.BaryCoord, OutPosition, OutVelocity, OutNormal, OutBinormal, OutTangent); }\n");
	}
	else if (FunctionInfo.DefinitionName == FSkeletalMeshInterfaceHelper::GetSkinnedTriangleDataInterpName)
	{
		FuncFormat = TEXT("void {InstanceFunctionName}(in MeshTriCoordinate InCoord, in float InInterp, out float3 OutPosition, out float3 OutVelocity, out float3 OutNormal, out float3 OutBinormal, out float3 OutTangent) { GetSkinnedTriangleDataInterpolated_{ParameterName}(InCoord.Tri, InCoord.BaryCoord, InInterp, OutPosition, OutVelocity, OutNormal, OutBinormal, OutTangent); }\n");
	}
	else if (FunctionInfo.DefinitionName == FSkeletalMeshInterfaceHelper::GetTriUVName)
	{
		FuncFormat = TEXT("void {InstanceFunctionName}(in MeshTriCoordinate InCoord, in int InUVSet, out float2 OutUV) { GetTriUV_{ParameterName}(InCoord.Tri, InCoord.BaryCoord, InUVSet, OutUV); }\n");
	}
	else if (FunctionInfo.DefinitionName == FSkeletalMeshInterfaceHelper::GetTriColorName)
	{
		FuncFormat = TEXT("void {InstanceFunctionName}(in MeshTriCoordinate InCoord, out float4 OutColor) { GetTriColor_{ParameterName}(InCoord.Tri, InCoord.BaryCoord, OutColor); }\n");
	}
	else if (FunctionInfo.DefinitionName == FSkeletalMeshInterfaceHelper::GetTriangleCoordAtUVName)
	{
		FuncFormat = TEXT("void {InstanceFunctionName}(in bool InEnabled, in float2 InUV, in float InTolerance, out MeshTriCoordinate OutCoord, out bool OutIsValid) { GetTriangleCoordAtUV_{ParameterName}(InEnabled, InUV, InTolerance, OutCoord.Tri, OutCoord.BaryCoord, OutIsValid); }\n");
	}
	else if (FunctionInfo.DefinitionName == FSkeletalMeshInterfaceHelper::GetTriangleCoordInAabbName)
	{
		FuncFormat = TEXT("void {InstanceFunctionName}(in bool InEnabled, in float2 InUvMin, in float2 InUvMax, out MeshTriCoordinate OutCoord, out bool OutIsValid) { GetTriangleCoordInAabb_{ParameterName}(InEnabled, InUvMin, InUvMax, OutCoord.Tri, OutCoord.BaryCoord, OutIsValid); }\n");
	}

	if ( FuncFormat != nullptr )
	{
		OutHLSL += FString::Format(FuncFormat, FormatArgs);
		return true;
	}

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Unsupported functionality
	return false;
}

bool UNiagaraDataInterfaceSkeletalMesh::UpgradeFunctionCall(FNiagaraFunctionSignature& FunctionSignature)
{
	bool bWasChanged = false;

	// Early out for version matching
	if (FunctionSignature.FunctionVersion == FNiagaraSkelMeshDIFunctionVersion::LatestVersion)
	{
		return bWasChanged;
	}

	// Renamed some functions and added Random Info to Various functions for consistency
	if (FunctionSignature.FunctionVersion < FNiagaraSkelMeshDIFunctionVersion::AddedRandomInfo)
	{
		static const TPair<FName, FName> FunctionRenames[] =
		{
			MakeTuple(FName("IsValidBone"), FSkeletalMeshInterfaceHelper::IsValidBoneName),
			MakeTuple(FName("RandomSpecificBone"), FSkeletalMeshInterfaceHelper::RandomFilteredBoneName),
			MakeTuple(FName("GetSpecificBoneCount"), FSkeletalMeshInterfaceHelper::GetFilteredBoneCountName),
			MakeTuple(FName("GetSpecificBone"), FSkeletalMeshInterfaceHelper::GetFilteredBoneAtName),
			MakeTuple(FName("RandomSpecificSocketBone"), FSkeletalMeshInterfaceHelper::RandomFilteredSocketName),
			MakeTuple(FName("GetSpecificSocketCount"), FSkeletalMeshInterfaceHelper::GetFilteredSocketCountName),
			MakeTuple(FName("GetSpecificSocketTransform"), FSkeletalMeshInterfaceHelper::GetFilteredSocketTransformName),
			MakeTuple(FName("GetSpecificSocketBone"), FSkeletalMeshInterfaceHelper::GetFilteredSocketBoneAtName),
			MakeTuple(FName("RandomFilteredSocketBone"), FSkeletalMeshInterfaceHelper::RandomFilteredSocketName),
		};

		for (const auto& RenamePair : FunctionRenames)
		{
			if (FunctionSignature.Name == RenamePair.Key)
			{
				FunctionSignature.Name = RenamePair.Value;
				bWasChanged = true;
				break;
			}
		}

		if (FunctionSignature.Name == FSkeletalMeshInterfaceHelper::RandomTriCoordName)
		{
			if (FunctionSignature.Inputs.Num() == 1)
			{
				FunctionSignature.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(FNiagaraRandInfo::StaticStruct()), TEXT("RandomInfo")));
				bWasChanged = true;
			}
		}
		else if (FunctionSignature.Name == FSkeletalMeshInterfaceHelper::RandomFilteredBoneName)
		{
			FunctionSignature.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(FNiagaraRandInfo::StaticStruct()), TEXT("RandomInfo")));
			bWasChanged = true;
		}
		else if (FunctionSignature.Name == FSkeletalMeshInterfaceHelper::RandomFilteredSocketName)
		{
			FunctionSignature.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(FNiagaraRandInfo::StaticStruct()), TEXT("RandomInfo")));
			bWasChanged = true;
		}
		else if (FunctionSignature.Name == FSkeletalMeshInterfaceHelper::RandomVertexName)
		{
			FunctionSignature.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(FNiagaraRandInfo::StaticStruct()), TEXT("RandomInfo")));
			bWasChanged = true;
		}
	}

	// Vertex sampling clean up
	if (FunctionSignature.FunctionVersion < FNiagaraSkelMeshDIFunctionVersion::CleanUpVertexSampling)
	{
		static const TPair<FName, FName> FunctionRenames[] =
		{
			MakeTuple(FName("IsValidVertex"), FSkeletalMeshInterfaceHelper::IsValidVertexName),
			MakeTuple(FName("RandomVertex"), FSkeletalMeshInterfaceHelper::RandomFilteredVertexName),
		};

		for (const auto& RenamePair : FunctionRenames)
		{
			if (FunctionSignature.Name == RenamePair.Key)
			{
				FunctionSignature.Name = RenamePair.Value;
				bWasChanged = true;
				break;
			}
		}
	}

	// Clean up CleanupBoneSampling
	if (FunctionSignature.FunctionVersion < FNiagaraSkelMeshDIFunctionVersion::CleanupBoneSampling)
	{
		static const TPair<FName, FName> FunctionRenames[] =
		{
			MakeTuple(FName("GetFilteredSocketBone"), FSkeletalMeshInterfaceHelper::GetFilteredSocketBoneAtName),
		};

		for (const auto& RenamePair : FunctionRenames)
		{
			if (FunctionSignature.Name == RenamePair.Key)
			{
				FunctionSignature.Name = RenamePair.Value;
				bWasChanged = true;
				break;
			}
		}
	}

	// Added tangent basis to GetSkinnedVertexData
	if (FunctionSignature.FunctionVersion < FNiagaraSkelMeshDIFunctionVersion::AddTangentBasisToGetSkinnedVertexData)
	{
		if ((FunctionSignature.Name == FSkeletalMeshInterfaceHelper::GetSkinnedVertexDataName) ||
			(FunctionSignature.Name == FSkeletalMeshInterfaceHelper::GetSkinnedVertexDataWSName))
		{
			if ( ensure(FunctionSignature.Outputs.Num() == 2) )
			{
				FunctionSignature.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Normal")));
				FunctionSignature.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Binormal")));
				FunctionSignature.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Tangent")));
				bWasChanged = true;
			}
		}
	}

	// Added a new Tolerance parameter to GetTriangleCoordAtUV
	if (FunctionSignature.FunctionVersion < FNiagaraSkelMeshDIFunctionVersion::RemoveUvSetFromMapping)
	{
		if (FunctionSignature.Name == FSkeletalMeshInterfaceHelper::GetTriangleCoordAtUVName)
		{
			for (int32 InputIndex = FunctionSignature.Inputs.Num() - 1; InputIndex >= 0; --InputIndex)
			{
				if (FunctionSignature.Inputs[InputIndex].GetName() == TEXT("UV Set"))
				{
					FunctionSignature.Inputs.RemoveAt(InputIndex);
				}
			}

			FNiagaraVariable ToleranceVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Tolerance"));
			ToleranceVariable.SetValue(KINDA_SMALL_NUMBER);

			FunctionSignature.Inputs.Add(ToleranceVariable);
			bWasChanged = true;
		}
	}

	if (FunctionSignature.FunctionVersion < FNiagaraSkelMeshDIFunctionVersion::AddedEnabledUvMapping)
	{
		if ((FunctionSignature.Name == FSkeletalMeshInterfaceHelper::GetTriangleCoordAtUVName)
			|| (FunctionSignature.Name == FSkeletalMeshInterfaceHelper::GetTriangleCoordInAabbName))
		{
			FNiagaraVariable EnabledVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("Enabled"));
			EnabledVariable.SetValue(true);
			FunctionSignature.Inputs.Insert(EnabledVariable, 1);
			bWasChanged = true;
		}
	}

	if (FunctionSignature.FunctionVersion < FNiagaraSkelMeshDIFunctionVersion::AddedValidConnectivity)
	{
		if ((FunctionSignature.Name == FSkeletalMeshInterfaceHelper::GetAdjacentTriangleIndexName)
			|| (FunctionSignature.Name == FSkeletalMeshInterfaceHelper::GetTriangleNeighborName))
		{
			FunctionSignature.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("IsValid")));
			bWasChanged = true;
		}
	}

	if (FunctionSignature.FunctionVersion < FNiagaraSkelMeshDIFunctionVersion::AddedInputBardCoordToGetFilteredTriangleAt)
	{
		if ( FunctionSignature.Name == FSkeletalMeshInterfaceHelper::GetFilteredTriangleAtName )
		{
			FunctionSignature.Inputs.Add_GetRef(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("BaryCoord"))).SetValue(FVector3f(1.0f / 3.0f));
			bWasChanged = true;
		}
	}

	if (FunctionSignature.FunctionVersion < FNiagaraSkelMeshDIFunctionVersion::LargeWorldCoordinates2)
	{
		if (
			(FunctionSignature.Name == FSkeletalMeshInterfaceHelper::GetTriangleDataName) ||
			(FunctionSignature.Name == FSkeletalMeshInterfaceHelper::GetSkinnedTriangleDataName) ||
			(FunctionSignature.Name == FSkeletalMeshInterfaceHelper::GetSkinnedTriangleDataWSName) ||
			(FunctionSignature.Name == FSkeletalMeshInterfaceHelper::GetSkinnedTriangleDataInterpName) ||
			(FunctionSignature.Name == FSkeletalMeshInterfaceHelper::GetSkinnedTriangleDataWSInterpName) ||
			(FunctionSignature.Name == FSkeletalMeshInterfaceHelper::GetSkinnedBoneDataName) ||
			(FunctionSignature.Name == FSkeletalMeshInterfaceHelper::GetSkinnedBoneDataWSName) ||
			(FunctionSignature.Name == FSkeletalMeshInterfaceHelper::GetSkinnedBoneDataInterpolatedName) ||
			(FunctionSignature.Name == FSkeletalMeshInterfaceHelper::GetSkinnedBoneDataWSInterpolatedName) ||
			(FunctionSignature.Name == FSkeletalMeshInterfaceHelper::GetVertexDataName) ||
			(FunctionSignature.Name == FSkeletalMeshInterfaceHelper::GetSkinnedVertexDataName) ||
			(FunctionSignature.Name == FSkeletalMeshInterfaceHelper::GetSkinnedVertexDataWSName) )
		{
			check(FunctionSignature.Outputs[0].GetName() == TEXT("Position"));
			check(FunctionSignature.Outputs[0].GetType() == FNiagaraTypeDefinition::GetVec3Def() || FunctionSignature.Outputs[0].GetType() == FNiagaraTypeDefinition::GetPositionDef());
			FunctionSignature.Outputs[0].SetType(FNiagaraTypeDefinition::GetPositionDef());
			bWasChanged = true;
		}
	}

	if (FunctionSignature.FunctionVersion < FNiagaraSkelMeshDIFunctionVersion::AddBoneScale)
	{
		if (
			(FunctionSignature.Name == FSkeletalMeshInterfaceHelper::GetSkinnedBoneDataName) ||
			(FunctionSignature.Name == FSkeletalMeshInterfaceHelper::GetSkinnedBoneDataWSName) ||
			(FunctionSignature.Name == FSkeletalMeshInterfaceHelper::GetSkinnedBoneDataInterpolatedName) ||
			(FunctionSignature.Name == FSkeletalMeshInterfaceHelper::GetSkinnedBoneDataWSInterpolatedName) )
		{
			// Some bogus content appears to exist which only contains Position & Velocity outputs
			if ((FunctionSignature.Outputs.Num() <= 2) && (FunctionSignature.Outputs[1].GetName() == TEXT("Velocity")))
			{
				FunctionSignature.Outputs.EmplaceAt(1, FNiagaraTypeDefinition::GetQuatDef(), TEXT("Rotation"));
			}

			check(FunctionSignature.Outputs[2].GetName() == TEXT("Velocity"));
			FunctionSignature.Outputs.EmplaceAt(2, FNiagaraTypeDefinition::GetVec3Def(), TEXT("Scale"));
			bWasChanged = true;
		}
	}

	// Set latest version
	FunctionSignature.FunctionVersion = FNiagaraSkelMeshDIFunctionVersion::LatestVersion;

	return bWasChanged;
}
#endif

void UNiagaraDataInterfaceSkeletalMesh::BuildShaderParameters(FNiagaraShaderParametersBuilder& ShaderParametersBuilder) const
{
	ShaderParametersBuilder.AddNestedStruct<NDISkelMeshLocal::FShaderParameters>();
}

void UNiagaraDataInterfaceSkeletalMesh::SetShaderParameters(const FNiagaraDataInterfaceSetShaderParametersContext& Context) const
{
	FNiagaraDataInterfaceProxySkeletalMesh& DIProxy = Context.GetProxy<FNiagaraDataInterfaceProxySkeletalMesh>();
	FNiagaraDataInterfaceProxySkeletalMeshData* InstanceData = DIProxy.SystemInstancesToData.Find(Context.GetSystemInstanceID());

	NDISkelMeshLocal::FShaderParameters* ShaderParameters = Context.GetParameterNestedStruct<NDISkelMeshLocal::FShaderParameters>();
	if (InstanceData && InstanceData->StaticBuffers)
	{
		FSkeletalMeshGpuSpawnStaticBuffers* StaticBuffers = InstanceData->StaticBuffers;

		// Bind mesh buffers
		ShaderParameters->MeshVertexBuffer = StaticBuffers->GetBufferPositionSRV();
		ShaderParameters->MeshIndexBuffer = StaticBuffers->GetBufferIndexSRV();
		ShaderParameters->MeshTangentBuffer = StaticBuffers->GetBufferTangentSRV();
		ShaderParameters->MeshTexCoordBuffer = StaticBuffers->GetBufferTexCoordSRV();
		ShaderParameters->MeshColorBuffer = StaticBuffers->GetBufferColorSRV();

		ShaderParameters->MeshNumTexCoord = StaticBuffers->GetNumTexCoord();
		ShaderParameters->MeshTriangleCount = StaticBuffers->GetTriangleCount();
		ShaderParameters->MeshVertexCount = StaticBuffers->GetVertexCount();

		// Set triangle sampling buffer
		ShaderParameters->MeshTriangleSamplerProbAliasBuffer = StaticBuffers->GetBufferTriangleUniformSamplerProbAliasSRV();

		// Set triangle sampling region buffer
		ShaderParameters->MeshNumSamplingRegionTriangles = StaticBuffers->GetNumSamplingRegionTriangles();
		ShaderParameters->MeshNumSamplingRegionVertices = StaticBuffers->GetNumSamplingRegionVertices();
		ShaderParameters->MeshSamplingRegionsProbAliasBuffer = StaticBuffers->GetSampleRegionsProbAliasSRV();
		ShaderParameters->MeshSampleRegionsTriangleIndices = StaticBuffers->GetSampleRegionsTriangleIndicesSRV();
		ShaderParameters->MeshSampleRegionsVertices = StaticBuffers->GetSampleRegionsVerticesSRV();

		ShaderParameters->MeshSkinWeightBuffer = FNiagaraRenderer::GetSrvOrDefaultUInt(InstanceData->MeshSkinWeightBuffer->GetSRV());
		ShaderParameters->MeshSkinWeightLookupBuffer = FNiagaraRenderer::GetSrvOrDefaultUInt(InstanceData->MeshSkinWeightLookupBuffer->GetSRV());

		ShaderParameters->MeshWeightStride = InstanceData->MeshWeightStrideByte / 4;
		ShaderParameters->MeshSkinWeightIndexSize = InstanceData->MeshSkinWeightIndexSizeByte;

		uint32 EnabledFeaturesBits = 0;
		EnabledFeaturesBits |= StaticBuffers->IsUseGpuUniformlyDistributedSampling() ? 1 : 0;
		EnabledFeaturesBits |= StaticBuffers->IsSamplingRegionsAllAreaWeighted() ? 2 : 0;
		EnabledFeaturesBits |= InstanceData->bUnlimitedBoneInfluences ? 4 : 0;
		EnabledFeaturesBits |= StaticBuffers->HasMeshColors() ? 8 : 0;

		FSkeletalMeshGpuDynamicBufferProxy* DynamicBuffers = InstanceData->DynamicBuffer;
		check(DynamicBuffers);
		if (DynamicBuffers->DoesBoneDataExist())
		{
			ShaderParameters->MeshNumWeights = StaticBuffers->GetNumWeights();
			ShaderParameters->MeshCurrBonesBuffer = DynamicBuffers->GetRWBufferBone().SectionSRV;
			ShaderParameters->MeshPrevBonesBuffer = DynamicBuffers->GetRWBufferPrevBone().SectionSRV;
			ShaderParameters->MeshCurrSamplingBonesBuffer = DynamicBuffers->GetRWBufferBone().SamplingSRV;
			ShaderParameters->MeshPrevSamplingBonesBuffer = DynamicBuffers->GetRWBufferPrevBone().SamplingSRV;
			ShaderParameters->MeshTriangleMatricesOffsetBuffer = StaticBuffers->GetBufferTriangleMatricesOffsetSRV();
		}
		// Bind dummy data for validation purposes only.  Code will not execute due to "EnabledFeatures" bits but validation can not determine that.
		else
		{
			ShaderParameters->MeshNumWeights = 0;
			ShaderParameters->MeshCurrBonesBuffer = FNiagaraRenderer::GetDummyFloat4Buffer();
			ShaderParameters->MeshPrevBonesBuffer = FNiagaraRenderer::GetDummyFloat4Buffer();
			ShaderParameters->MeshCurrSamplingBonesBuffer = FNiagaraRenderer::GetDummyFloat4Buffer();
			ShaderParameters->MeshPrevSamplingBonesBuffer = FNiagaraRenderer::GetDummyFloat4Buffer();
			ShaderParameters->MeshTriangleMatricesOffsetBuffer = FNiagaraRenderer::GetDummyUIntBuffer();
		}

		ShaderParameters->NumBones = DynamicBuffers->GetNumBones();

		ShaderParameters->NumFilteredBones = StaticBuffers->GetNumFilteredBones();
		ShaderParameters->NumUnfilteredBones = StaticBuffers->GetNumUnfilteredBones();
		ShaderParameters->RandomMaxBone = StaticBuffers->GetExcludedBoneIndex() >= 0 ? DynamicBuffers->GetNumBones() - 2 : DynamicBuffers->GetNumBones() - 1;
		ShaderParameters->ExcludeBoneIndex = StaticBuffers->GetExcludedBoneIndex();
		ShaderParameters->FilteredAndUnfilteredBones = StaticBuffers->GetFilteredAndUnfilteredBonesSRV();

		ShaderParameters->NumFilteredSockets = StaticBuffers->GetNumFilteredSockets();
		ShaderParameters->FilteredSocketBoneOffset = StaticBuffers->GetFilteredSocketBoneOffset();

		if (InstanceData->UvMappingBuffer)
		{
			ShaderParameters->UvMappingBuffer = InstanceData->UvMappingBuffer->GetSrv();
			ShaderParameters->UvMappingBufferLength = InstanceData->UvMappingBuffer->GetBufferSize();
			ShaderParameters->UvMappingSet = InstanceData->UvMappingSet;
		}
		else
		{
			ShaderParameters->UvMappingBuffer = FNiagaraRenderer::GetDummyIntBuffer();
			ShaderParameters->UvMappingBufferLength = 0;
			ShaderParameters->UvMappingSet = 0;
		}

		if (InstanceData->ConnectivityBuffer)
		{
			const uint32 NumBufferElements = FMath::DivideAndRoundUp<uint32>(InstanceData->ConnectivityBuffer->GetBufferSize(), sizeof(uint32));
			ShaderParameters->ConnectivityBuffer = InstanceData->ConnectivityBuffer->GetSrv();
			ShaderParameters->ConnectivityBufferLength = NumBufferElements;
			ShaderParameters->ConnectivityMaxAdjacentPerVertex = InstanceData->ConnectivityBuffer->MaxAdjacentTriangleCount;
		}
		else
		{
			ShaderParameters->ConnectivityBuffer = FNiagaraRenderer::GetDummyUIntBuffer();
			ShaderParameters->ConnectivityBufferLength = 0;
			ShaderParameters->ConnectivityMaxAdjacentPerVertex = 0;
		}

		ShaderParameters->InstanceTransform = InstanceData->Transform;
		ShaderParameters->InstancePrevTransform = InstanceData->PrevTransform;
		ShaderParameters->InstanceRotation = InstanceData->Transform.GetMatrixWithoutScale().ToQuat();
		ShaderParameters->InstancePrevRotation = InstanceData->PrevTransform.GetMatrixWithoutScale().ToQuat();
		ShaderParameters->InstanceInvDeltaTime = 1.0f / InstanceData->DeltaSeconds;

		ShaderParameters->PreSkinnedLocalBoundsCenter = InstanceData->PreSkinnedLocalBoundsCenter;
		ShaderParameters->PreSkinnedLocalBoundsExtents = InstanceData->PreSkinnedLocalBoundsExtents;

		ShaderParameters->EnabledFeatures = EnabledFeaturesBits;
	}
	else
	{
		// Bind dummy buffers
		ShaderParameters->MeshVertexBuffer = FNiagaraRenderer::GetDummyFloatBuffer();
		ShaderParameters->MeshIndexBuffer = FNiagaraRenderer::GetDummyUIntBuffer();
		ShaderParameters->MeshTangentBuffer = FNiagaraRenderer::GetDummyFloat4Buffer();

		ShaderParameters->MeshNumTexCoord = 0;
		ShaderParameters->MeshTexCoordBuffer = FNiagaraRenderer::GetDummyFloat2Buffer();
		ShaderParameters->MeshColorBuffer = FNiagaraRenderer::GetDummyFloat4Buffer();
		ShaderParameters->MeshTriangleCount = 0;
		ShaderParameters->MeshVertexCount = 0;
		ShaderParameters->MeshTriangleSamplerProbAliasBuffer = FNiagaraRenderer::GetDummyUIntBuffer();

		ShaderParameters->MeshNumSamplingRegionTriangles = 0;
		ShaderParameters->MeshNumSamplingRegionVertices = 0;
		ShaderParameters->MeshSamplingRegionsProbAliasBuffer = FNiagaraRenderer::GetDummyUIntBuffer();
		ShaderParameters->MeshSampleRegionsTriangleIndices = FNiagaraRenderer::GetDummyUIntBuffer();
		ShaderParameters->MeshSampleRegionsVertices = FNiagaraRenderer::GetDummyUIntBuffer();

		ShaderParameters->MeshSkinWeightBuffer = FNiagaraRenderer::GetDummyUIntBuffer();
		ShaderParameters->MeshSkinWeightLookupBuffer = FNiagaraRenderer::GetDummyUIntBuffer();

		ShaderParameters->MeshWeightStride = 0;
		ShaderParameters->MeshSkinWeightIndexSize = 0;
		ShaderParameters->MeshNumWeights = 0;

		ShaderParameters->MeshCurrBonesBuffer = FNiagaraRenderer::GetDummyFloat4Buffer();
		ShaderParameters->MeshPrevBonesBuffer = FNiagaraRenderer::GetDummyFloat4Buffer();
		ShaderParameters->MeshCurrSamplingBonesBuffer = FNiagaraRenderer::GetDummyFloat4Buffer();
		ShaderParameters->MeshPrevSamplingBonesBuffer = FNiagaraRenderer::GetDummyFloat4Buffer();
		ShaderParameters->MeshTriangleMatricesOffsetBuffer = FNiagaraRenderer::GetDummyUIntBuffer();

		ShaderParameters->NumBones = 0;
		ShaderParameters->NumFilteredBones = 0;
		ShaderParameters->NumUnfilteredBones = 0;
		ShaderParameters->RandomMaxBone = 0;
		ShaderParameters->ExcludeBoneIndex = 0;
		ShaderParameters->FilteredAndUnfilteredBones = FNiagaraRenderer::GetDummyUIntBuffer();
		ShaderParameters->NumFilteredSockets = 0;
		ShaderParameters->FilteredSocketBoneOffset = 0;

		ShaderParameters->UvMappingBuffer = FNiagaraRenderer::GetDummyIntBuffer();
		ShaderParameters->UvMappingBufferLength = 0;
		ShaderParameters->UvMappingSet = 0;

		ShaderParameters->ConnectivityBuffer = FNiagaraRenderer::GetDummyUIntBuffer();
		ShaderParameters->ConnectivityBufferLength = 0;
		ShaderParameters->ConnectivityMaxAdjacentPerVertex = 0;

		ShaderParameters->InstanceTransform = FMatrix44f::Identity;
		ShaderParameters->InstancePrevTransform = FMatrix44f::Identity;
		ShaderParameters->InstanceRotation = FQuat4f::Identity;
		ShaderParameters->InstancePrevRotation = FQuat4f::Identity;
		ShaderParameters->InstanceInvDeltaTime = 0.0f;

		ShaderParameters->PreSkinnedLocalBoundsCenter = FVector3f::ZeroVector;
		ShaderParameters->PreSkinnedLocalBoundsExtents = FVector3f::ZeroVector;

		ShaderParameters->EnabledFeatures = 0;
	}
}

void UNiagaraDataInterfaceSkeletalMesh::BindSourceDelegates()
{
	if (AActor* Source = SoftSourceActor.Get())
	{
		Source->OnEndPlay.AddDynamic(this, &UNiagaraDataInterfaceSkeletalMesh::OnSourceEndPlay);
	}
	else if (SourceComponent)
	{
		UE_CLOG(!UObjectBaseUtility::IsPendingKillEnabled(), 
			LogNiagara, Warning, TEXT("%s: Unable to bind OnEndPlay for actor-less source component %s, this may extend the lifetime of the component"), 
			*GetFullName(), *SourceComponent->GetPathName());
	}
}

void UNiagaraDataInterfaceSkeletalMesh::UnbindSourceDelegates()
{
	if (AActor* Source = SoftSourceActor.Get())
	{
		Source->OnEndPlay.RemoveAll(this);
	}
}

void UNiagaraDataInterfaceSkeletalMesh::OnSourceEndPlay(AActor* InSource, EEndPlayReason::Type Reason)
{
	// Increment change id in case we're able to find a new source component 
	++ChangeId;
	SoftSourceActor = nullptr;
	SourceComponent = nullptr;
}

void UNiagaraDataInterfaceSkeletalMesh::SetSourceComponentFromBlueprints(USkeletalMeshComponent* ComponentToUse)
{
	// NOTE: When ChangeId changes the next tick will be skipped and a reset of the per-instance data will be initiated.
	++ChangeId;
	UnbindSourceDelegates();
	SourceComponent = ComponentToUse;
	SoftSourceActor = ComponentToUse->GetOwner();
	BindSourceDelegates();
}

void UNiagaraDataInterfaceSkeletalMesh::SetSamplingRegionsFromBlueprints(const TArray<FName>& InSamplingRegions)
{
	// NOTE: When ChangeId changes the next tick will be skipped and a reset of the per-instance data will be initiated.
	++ChangeId;
	SamplingRegions = InSamplingRegions;
}

void UNiagaraDataInterfaceSkeletalMesh::SetFilteredBonesFromBlueprints(const TArray<FName>& InFilteredBones)
{
	// NOTE: When ChangeId changes the next tick will be skipped and a reset of the per-instance data will be initiated.
	++ChangeId;
	FilteredBones = InFilteredBones;
}

void UNiagaraDataInterfaceSkeletalMesh::SetFilteredSocketsFromBlueprints(const TArray<FName>& InFilteredSockets)
{
	// NOTE: When ChangeId changes the next tick will be skipped and a reset of the per-instance data will be initiated.
	++ChangeId;
	FilteredSockets = InFilteredSockets;
}

void UNiagaraDataInterfaceSkeletalMesh::SetWholeMeshLODFromBlueprints(int32 InWholeMeshLOD)
{
	// NOTE: When ChangeId changes the next tick will be skipped and a reset of the per-instance data will be initiated.
	++ChangeId;
	WholeMeshLOD = InWholeMeshLOD;
}

ETickingGroup UNiagaraDataInterfaceSkeletalMesh::CalculateTickGroup(const void* PerInstanceData) const
{
	const FNDISkeletalMesh_InstanceData* InstData = static_cast<const FNDISkeletalMesh_InstanceData*>(PerInstanceData);
	USkeletalMeshComponent* Component = Cast<USkeletalMeshComponent>(InstData->SceneComponent.Get());
	if (Component && bRequireCurrentFrameData)
	{
		return NDISkelMeshLocal::GetComponentTickGroup(Component);
	}
	return NiagaraFirstTickGroup;
}


int32 UNiagaraDataInterfaceSkeletalMesh::CalculateLODIndexAndSamplingRegions(USkeletalMesh* InMesh, TArray<int32>& OutSamplingRegionIndices, bool& OutAllRegionsAreAreaWeighting) const
{
	check(InMesh);

	if (!SamplingRegions.Num())
	{
		//If we have no regions, sample the whole mesh at the specified LOD.
		if (WholeMeshLOD == INDEX_NONE)
		{
			return InMesh->GetLODNum() - 1;
		}
		else
		{
			return FMath::Clamp(WholeMeshLOD, 0, InMesh->GetLODNum() - 1);
		}
	}
	else
	{
		int32 LastRegionLODIndex = INDEX_NONE;

		//Sampling from regions. Gather the indices of the regions we'll sample from.
		const FSkeletalMeshSamplingInfo& SamplingInfo = InMesh->GetSamplingInfo();
		for (FName RegionName : SamplingRegions)
		{
			const int32 RegionIdx = SamplingInfo.IndexOfRegion(RegionName);
			if (RegionIdx != INDEX_NONE)
			{
				const FSkeletalMeshSamplingRegion& Region = SamplingInfo.GetRegion(RegionIdx);
				const FSkeletalMeshSamplingRegionBuiltData& RegionBuiltData = SamplingInfo.GetRegionBuiltData(RegionIdx);

				const int32 RegionLODIndex = Region.LODIndex == INDEX_NONE ?
					(InMesh->GetLODNum() - 1) :
					(FMath::Clamp(Region.LODIndex, 0, InMesh->GetLODNum() - 1));

				if (LastRegionLODIndex == INDEX_NONE)
				{
					LastRegionLODIndex = RegionLODIndex;
				}
				else if (RegionLODIndex != LastRegionLODIndex)
				{
					UE_LOG(LogNiagara, Warning, TEXT("Skeletal Mesh Data Interface is trying to use regions on LODs levels that are either streamed or cooked out. This is currently unsupported.\nInterface: %s\nMesh: %s\nRegion: %s"),
						*GetFullName(),
						*InMesh->GetFullName(),
						*RegionName.ToString());
					return INDEX_NONE;
				}

				if (RegionBuiltData.TriangleIndices.Num() > 0)
				{
					OutSamplingRegionIndices.Add(RegionIdx);
					OutAllRegionsAreAreaWeighting &= Region.bSupportUniformlyDistributedSampling;
				}
				else
				{
					UE_LOG(LogNiagara, Warning, TEXT("Skeletal Mesh Data Interface is trying to use a region with no associated triangles.\nLOD: %d\nInterface: %s\nMesh: %s\nRegion: %s"),
						 RegionLODIndex,
						*GetFullName(),
						*InMesh->GetFullName(),
						*RegionName.ToString());

					return INDEX_NONE;
				}
			}
			else
			{
				UE_LOG(LogNiagara, Warning, TEXT("Skeletal Mesh Data Interface is trying to use a region on a mesh that does not provide this region.\nInterface: %s\nMesh: %s\nRegion: %s"),
					*GetFullName(),
					*InMesh->GetFullName(),
					*RegionName.ToString());

				return INDEX_NONE;
			}
		}
		return LastRegionLODIndex;
	}
}

void UNiagaraDataInterfaceSkeletalMesh::VMGetPreSkinnedLocalBounds(FVectorVMExternalFunctionContext& Context)
{
	VectorVM::FUserPtrHandler<FNDISkeletalMesh_InstanceData> InstData(Context);
	FNDIOutputParam<FVector3f>	OutCenter(Context);
	FNDIOutputParam<FVector3f>	OutExtentsMin(Context);
	FNDIOutputParam<FVector3f>	OutExtentsMax(Context);
	FNDIOutputParam<FVector3f>	OutExtents(Context);
	FNDIOutputParam<FVector3f>	OutHalfExtents(Context);

	const FVector3f Center = InstData->PreSkinnedLocalBoundsCenter;
	const FVector3f Extents = InstData->PreSkinnedLocalBoundsExtents;
	const FVector3f HalfExtents = Extents * 0.5f;
	const FVector3f ExtentsMin = Center - HalfExtents;
	const FVector3f ExtentsMax = Center + HalfExtents;

	for (int32 i = 0; i < Context.GetNumInstances(); ++i)
	{
		OutCenter.SetAndAdvance(Center);
		OutExtentsMin.SetAndAdvance(ExtentsMin);
		OutExtentsMax.SetAndAdvance(ExtentsMax);
		OutExtents.SetAndAdvance(Extents);
		OutHalfExtents.SetAndAdvance(HalfExtents);
	}
}

//UNiagaraDataInterfaceSkeletalMesh END
//////////////////////////////////////////////////////////////////////////

template<>
void FSkeletalMeshAccessorHelper::Init<TNDISkelMesh_FilterModeSingle, TNDISkelMesh_AreaWeightingOff>(FNDISkeletalMesh_InstanceData* InstData)
{
	Comp = Cast<USkeletalMeshComponent>(InstData->SceneComponent.Get());
	Mesh = InstData->SkeletalMesh.Get();
	LODData = InstData->CachedLODData;
	SkinWeightBuffer = InstData->GetSkinWeights();
	IndexBuffer = LODData ? LODData->MultiSizeIndexContainer.GetIndexBuffer() : nullptr;
	SkinningData = InstData->SkinningData.SkinningData.Get();
	Usage = InstData->SkinningData.Usage;

	if (Mesh)
	{
		const FSkeletalMeshSamplingInfo& SamplingInfo = Mesh->GetSamplingInfo();
		SamplingRegion = &SamplingInfo.GetRegion(InstData->SamplingRegionIndices[0]);
		SamplingRegionBuiltData = &SamplingInfo.GetRegionBuiltData(InstData->SamplingRegionIndices[0]);
	}
	else
	{
		SamplingRegion = nullptr;
		SamplingRegionBuiltData = nullptr;
	}

	if (SkinningData != nullptr)
	{
		SkinningData->EnterRead();
	}
}

template<>
void FSkeletalMeshAccessorHelper::Init<TNDISkelMesh_FilterModeSingle, TNDISkelMesh_AreaWeightingOn>(FNDISkeletalMesh_InstanceData* InstData)
{
	Comp = Cast<USkeletalMeshComponent>(InstData->SceneComponent.Get());
	Mesh = InstData->SkeletalMesh.Get();
	LODData = InstData->CachedLODData;
	SkinWeightBuffer = InstData->GetSkinWeights();
	IndexBuffer = LODData ? LODData->MultiSizeIndexContainer.GetIndexBuffer() : nullptr;
	SkinningData = InstData->SkinningData.SkinningData.Get();
	Usage = InstData->SkinningData.Usage;

	if (Mesh)
	{
		const FSkeletalMeshSamplingInfo& SamplingInfo = Mesh->GetSamplingInfo();
		SamplingRegion = &SamplingInfo.GetRegion(InstData->SamplingRegionIndices[0]);
		SamplingRegionBuiltData = &SamplingInfo.GetRegionBuiltData(InstData->SamplingRegionIndices[0]);
	}
	else
	{
		SamplingRegion = nullptr;
		SamplingRegionBuiltData = nullptr;
	}

	if (SkinningData != nullptr)
	{
		SkinningData->EnterRead();
	}
}

#undef LOCTEXT_NAMESPACE
