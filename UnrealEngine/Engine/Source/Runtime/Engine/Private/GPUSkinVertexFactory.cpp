// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GPUVertexFactory.cpp: GPU skin vertex factory implementation
=============================================================================*/

#include "GPUSkinVertexFactory.h"
#include "SceneView.h"
#include "MeshBatch.h"
#include "GPUSkinCache.h"
#include "ShaderParameterUtils.h"
#include "MeshMaterialShader.h"
#include "SkeletalRenderGPUSkin.h"
#include "PlatformInfo.h"
#include "Interfaces/ITargetPlatform.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "Logging/LogMacros.h"
#include "Misc/CoreMisc.h"
#include "RenderGraphResources.h"

#include "Engine/RendererSettings.h"
#if INTEL_ISPC
#include "GPUSkinVertexFactory.ispc.generated.h"
#endif

// Changing this is currently unsupported after content has been chunked with the previous setting
// Changing this causes a full shader recompile
static int32 GCVarMaxGPUSkinBones = FGPUBaseSkinVertexFactory::GHardwareMaxGPUSkinBones;
static FAutoConsoleVariableRef CVarMaxGPUSkinBones(
	TEXT("Compat.MAX_GPUSKIN_BONES"),
	GCVarMaxGPUSkinBones,
	TEXT("Max number of bones that can be skinned on the GPU in a single draw call. This setting clamp the per platform project setting URendererSettings::MaxSkinBones. Cannot be changed at runtime."),
	ECVF_ReadOnly);

static int32 GCVarSupport16BitBoneIndex = 0;
static FAutoConsoleVariableRef CVarSupport16BitBoneIndex(
	TEXT("r.GPUSkin.Support16BitBoneIndex"),
	GCVarSupport16BitBoneIndex,
	TEXT("If enabled, a new mesh imported will use 8 bit (if <=256 bones) or 16 bit (if > 256 bones) bone indices for rendering."),
	ECVF_ReadOnly);

// Whether to use 2 bones influence instead of default 4 for GPU skinning
// Changing this causes a full shader recompile
static TAutoConsoleVariable<int32> CVarGPUSkinLimit2BoneInfluences(
	TEXT("r.GPUSkin.Limit2BoneInfluences"),
	0,	
	TEXT("Whether to use 2 bones influence instead of default 4/8 for GPU skinning. Cannot be changed at runtime."),
	ECVF_ReadOnly);

static int32 GCVarUnlimitedBoneInfluences = 0;
static FAutoConsoleVariableRef CVarUnlimitedBoneInfluences(
	TEXT("r.GPUSkin.UnlimitedBoneInfluences"),
	GCVarUnlimitedBoneInfluences,
	TEXT("Whether to use unlimited bone influences instead of default 4/8 for GPU skinning. Cannot be changed at runtime."),
	ECVF_ReadOnly);

static int32 GCVarUnlimitedBoneInfluencesThreshold = EXTRA_BONE_INFLUENCES;
static FAutoConsoleVariableRef CVarUnlimitedBoneInfluencesThreshold(
	TEXT("r.GPUSkin.UnlimitedBoneInfluencesThreshold"),
	GCVarUnlimitedBoneInfluencesThreshold,
	TEXT("Unlimited Bone Influences Threshold to use unlimited bone influences buffer if r.GPUSkin.UnlimitedBoneInfluences is enabled. Should be unsigned int. Cannot be changed at runtime."),
	ECVF_ReadOnly);

static TAutoConsoleVariable<bool> CVarMobileEnableCloth(
	TEXT("r.Mobile.EnableCloth"),
	true,
	TEXT("If enabled, compile cloth shader permutations and render simulated cloth on mobile platforms and Windows ES3.1. Cannot be changed at runtime"),
	ECVF_ReadOnly);

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FAPEXClothUniformShaderParameters,"APEXClothParam");

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FBoneMatricesUniformShaderParameters,"Bones");

static FBoneMatricesUniformShaderParameters GBoneUniformStruct;

#define IMPLEMENT_GPUSKINNING_VERTEX_FACTORY_TYPE_INTERNAL(FactoryClass, ShaderFilename, Flags) \
	template <GPUSkinBoneInfluenceType BoneInfluenceType> FVertexFactoryType FactoryClass<BoneInfluenceType>::StaticType( \
	BoneInfluenceType == DefaultBoneInfluence ? TEXT(#FactoryClass) TEXT("Default") : TEXT(#FactoryClass) TEXT("Unlimited"), \
	TEXT(ShaderFilename), \
	Flags | EVertexFactoryFlags::SupportsPrimitiveIdStream, \
	IMPLEMENT_VERTEX_FACTORY_VTABLE(FactoryClass<BoneInfluenceType>) \
	); \
	template <GPUSkinBoneInfluenceType BoneInfluenceType> inline FVertexFactoryType* FactoryClass<BoneInfluenceType>::GetType() const { return &StaticType; }


#define IMPLEMENT_GPUSKINNING_VERTEX_FACTORY_TYPE(FactoryClass, ShaderFilename, Flags) \
	IMPLEMENT_GPUSKINNING_VERTEX_FACTORY_TYPE_INTERNAL(FactoryClass, ShaderFilename, Flags) \
	template class FactoryClass<DefaultBoneInfluence>;	\
	template class FactoryClass<UnlimitedBoneInfluence>;

#define IMPLEMENT_GPUSKINNING_VERTEX_FACTORY_PARAMETER_TYPE(FactoryClass, Frequency, ParameterType) \
	IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(FactoryClass<DefaultBoneInfluence>, Frequency, ParameterType); \
	IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(FactoryClass<UnlimitedBoneInfluence>, Frequency, ParameterType)

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
static TAutoConsoleVariable<int32> CVarVelocityTest(
	TEXT("r.VelocityTest"),
	0,
	TEXT("Allows to enable some low level testing code for the velocity rendering (Affects object motion blur and TemporalAA).")
	TEXT(" 0: off (default)")
	TEXT(" 1: add random data to the buffer where we store skeletal mesh bone data to test if the code (good to test in PAUSED as well)."),
	ECVF_Cheat | ECVF_RenderThreadSafe);
#endif // if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

#if !defined(GPU_SKIN_COPY_BONES_ISPC_ENABLED_DEFAULT)
#define GPU_SKIN_COPY_BONES_ISPC_ENABLED_DEFAULT 1
#endif

// Support run-time toggling on supported platforms in non-shipping configurations
#if !INTEL_ISPC || UE_BUILD_SHIPPING
static constexpr bool bGPUSkin_CopyBones_ISPC_Enabled = INTEL_ISPC && GPU_SKIN_COPY_BONES_ISPC_ENABLED_DEFAULT;
#else
static bool bGPUSkin_CopyBones_ISPC_Enabled = GPU_SKIN_COPY_BONES_ISPC_ENABLED_DEFAULT;
static FAutoConsoleVariableRef CVarGPUSkinCopyBonesISPCEnabled(TEXT("r.GPUSkin.CopyBones.ISPC"), bGPUSkin_CopyBones_ISPC_Enabled, TEXT("Whether to use ISPC optimizations when copying bones for GPU skinning"));
#endif

#if INTEL_ISPC
static_assert(sizeof(ispc::FMatrix44f) == sizeof(FMatrix44f), "sizeof(ispc::FMatrix44f) != sizeof(FMatrix44f)");
static_assert(sizeof(ispc::FMatrix3x4) == sizeof(FMatrix3x4), "sizeof(ispc::FMatrix3x4) != sizeof(FMatrix3x4)");
#endif

// ---
// These should match USE_BONES_SRV_BUFFER
static inline bool SupportsBonesBufferSRV(EShaderPlatform Platform)
{
	// at some point we might switch GL to uniform buffers
	return true;
}

static inline bool SupportsBonesBufferSRV(ERHIFeatureLevel::Type InFeatureLevel)
{
	// at some point we might switch GL to uniform buffers
	return true;
}
// ---


/*-----------------------------------------------------------------------------
 FSharedPoolPolicyData
 -----------------------------------------------------------------------------*/
uint32 FSharedPoolPolicyData::GetPoolBucketIndex(uint32 Size)
{
	unsigned long Lower = 0;
	unsigned long Upper = NumPoolBucketSizes;
	unsigned long Middle;
	
	do
	{
		Middle = ( Upper + Lower ) >> 1;
		if( Size <= BucketSizes[Middle-1] )
		{
			Upper = Middle;
		}
		else
		{
			Lower = Middle;
		}
	}
	while( Upper - Lower > 1 );
	
	check( Size <= BucketSizes[Lower] );
	check( (Lower == 0 ) || ( Size > BucketSizes[Lower-1] ) );
	
	return Lower;
}

uint32 FSharedPoolPolicyData::GetPoolBucketSize(uint32 Bucket)
{
	check(Bucket < NumPoolBucketSizes);
	return BucketSizes[Bucket];
}

uint32 FSharedPoolPolicyData::BucketSizes[NumPoolBucketSizes] = {
	16, 48, 96, 192, 384, 768, 1536, 
	3072, 4608, 6144, 7680, 9216, 12288, 
	65536, 131072, 262144, 786432, 1572864 // these 5 numbers are added for large cloth simulation vertices, supports up to 65,536 verts
};

/*-----------------------------------------------------------------------------
 FBoneBufferPoolPolicy
 -----------------------------------------------------------------------------*/
FVertexBufferAndSRV FBoneBufferPoolPolicy::CreateResource(CreationArguments Args)
{
	uint32 BufferSize = GetPoolBucketSize(GetPoolBucketIndex(Args));
	// in VisualStudio the copy constructor call on the return argument can be optimized out
	// see https://msdn.microsoft.com/en-us/library/ms364057.aspx#nrvo_cpp05_topic3
	FVertexBufferAndSRV Buffer;
	FRHIResourceCreateInfo CreateInfo(TEXT("FBoneBufferPoolPolicy"));
	Buffer.VertexBufferRHI = RHICreateVertexBuffer( BufferSize, (BUF_Dynamic | BUF_ShaderResource), CreateInfo );
	Buffer.VertexBufferSRV = RHICreateShaderResourceView( Buffer.VertexBufferRHI, sizeof(FVector4f), PF_A32B32G32R32F );
	return Buffer;
}

FSharedPoolPolicyData::CreationArguments FBoneBufferPoolPolicy::GetCreationArguments(const FVertexBufferAndSRV& Resource)
{
	return Resource.VertexBufferRHI->GetSize();
}

void FBoneBufferPoolPolicy::FreeResource(FVertexBufferAndSRV Resource)
{
}

FVertexBufferAndSRV FClothBufferPoolPolicy::CreateResource(CreationArguments Args)
{
	uint32 BufferSize = GetPoolBucketSize(GetPoolBucketIndex(Args));
	// in VisualStudio the copy constructor call on the return argument can be optimized out
	// see https://msdn.microsoft.com/en-us/library/ms364057.aspx#nrvo_cpp05_topic3
	FVertexBufferAndSRV Buffer;
	FRHIResourceCreateInfo CreateInfo(TEXT("FClothBufferPoolPolicy"));
	Buffer.VertexBufferRHI = RHICreateVertexBuffer( BufferSize, (BUF_Dynamic | BUF_ShaderResource), CreateInfo );
	Buffer.VertexBufferSRV = RHICreateShaderResourceView( Buffer.VertexBufferRHI, sizeof(FVector2f), PF_G32R32F );
	return Buffer;
}

/*-----------------------------------------------------------------------------
 FBoneBufferPool
 -----------------------------------------------------------------------------*/
FBoneBufferPool::~FBoneBufferPool()
{
}

TStatId FBoneBufferPool::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FBoneBufferPool, STATGROUP_Tickables);
}

FClothBufferPool::~FClothBufferPool()
{
}

TStatId FClothBufferPool::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FClothBufferPool, STATGROUP_Tickables);
}

TConsoleVariableData<int32>* FGPUBaseSkinVertexFactory::FShaderDataType::MaxBonesVar = NULL;
uint32 FGPUBaseSkinVertexFactory::FShaderDataType::MaxGPUSkinBones = 0;

static TAutoConsoleVariable<int32> CVarRHICmdDeferSkeletalLockAndFillToRHIThread(
	TEXT("r.RHICmdDeferSkeletalLockAndFillToRHIThread"),
	0,
	TEXT("If > 0, then do the bone and cloth copies on the RHI thread. Experimental option."));

static bool DeferSkeletalLockAndFillToRHIThread()
{
	return IsRunningRHIInSeparateThread() && CVarRHICmdDeferSkeletalLockAndFillToRHIThread.GetValueOnRenderThread() > 0;
}

bool FGPUBaseSkinVertexFactory::FShaderDataType::UpdateBoneData(FRHICommandListImmediate& RHICmdList, const TArray<FMatrix44f>& ReferenceToLocalMatrices,
	const TArray<FBoneIndexType>& BoneMap, uint32 RevisionNumber, bool bPrevious, ERHIFeatureLevel::Type InFeatureLevel, bool bUseSkinCache)
{
	// stat disabled by default due to low-value/high-frequency
	//QUICK_SCOPE_CYCLE_COUNTER(STAT_FGPUBaseSkinVertexFactory_UpdateBoneData);

	const uint32 NumBones = BoneMap.Num();
	check(NumBones <= MaxGPUSkinBones);
	FMatrix3x4* ChunkMatrices = nullptr;

	FVertexBufferAndSRV* CurrentBoneBuffer = 0;

	if (SupportsBonesBufferSRV(InFeatureLevel))
	{
		check(IsInRenderingThread());
		
		// make sure current revision is up-to-date
		SetCurrentRevisionNumber(RevisionNumber);

		CurrentBoneBuffer = &GetBoneBufferForWriting(bPrevious);

		static FSharedPoolPolicyData PoolPolicy;
		uint32 NumVectors = NumBones*3;
		check(NumVectors <= (MaxGPUSkinBones*3));
		uint32 VectorArraySize = NumVectors * sizeof(FVector4f);
		uint32 PooledArraySize = BoneBufferPool.PooledSizeForCreationArguments(VectorArraySize);

		if(!IsValidRef(*CurrentBoneBuffer) || PooledArraySize != CurrentBoneBuffer->VertexBufferRHI->GetSize())
		{
			if(IsValidRef(*CurrentBoneBuffer))
			{
				BoneBufferPool.ReleasePooledResource(*CurrentBoneBuffer);
			}
			*CurrentBoneBuffer = BoneBufferPool.CreatePooledResource(VectorArraySize);
			check(IsValidRef(*CurrentBoneBuffer));
		}
		if(NumBones)
		{
			if (!bUseSkinCache && DeferSkeletalLockAndFillToRHIThread())
			{
				FRHIBuffer* VertexBuffer = CurrentBoneBuffer->VertexBufferRHI;
				RHICmdList.EnqueueLambda([VertexBuffer, VectorArraySize, &ReferenceToLocalMatrices, &BoneMap](FRHICommandListImmediate& InRHICmdList)
				{
					QUICK_SCOPE_CYCLE_COUNTER(STAT_FRHICommandUpdateBoneBuffer_Execute);
					FMatrix3x4* LambdaChunkMatrices = (FMatrix3x4*)InRHICmdList.LockBuffer(VertexBuffer, 0, VectorArraySize, RLM_WriteOnly);
					//FMatrix3x4 is sizeof() == 48
					// PLATFORM_CACHE_LINE_SIZE (128) / 48 = 2.6
					//  sizeof(FMatrix) == 64
					// PLATFORM_CACHE_LINE_SIZE (128) / 64 = 2
					const uint32 LocalNumBones = BoneMap.Num();
					check(LocalNumBones > 0 && LocalNumBones < 256); // otherwise maybe some bad threading on BoneMap, maybe we need to copy that
					const int32 PreFetchStride = 2; // FPlatformMisc::Prefetch stride
					for (uint32 BoneIdx = 0; BoneIdx < LocalNumBones; BoneIdx++)
					{
						const FBoneIndexType RefToLocalIdx = BoneMap[BoneIdx];
						check(ReferenceToLocalMatrices.IsValidIndex(RefToLocalIdx)); // otherwise maybe some bad threading on BoneMap, maybe we need to copy that
						FPlatformMisc::Prefetch(ReferenceToLocalMatrices.GetData() + RefToLocalIdx + PreFetchStride);
						FPlatformMisc::Prefetch(ReferenceToLocalMatrices.GetData() + RefToLocalIdx + PreFetchStride, PLATFORM_CACHE_LINE_SIZE);

						FMatrix3x4& BoneMat = LambdaChunkMatrices[BoneIdx];
						const FMatrix44f& RefToLocal = ReferenceToLocalMatrices[RefToLocalIdx];
						RefToLocal.To3x4MatrixTranspose((float*)BoneMat.M);
					}
					InRHICmdList.UnlockBuffer(VertexBuffer);
				});

				RHICmdList.RHIThreadFence(true);

				return true;
			}
			ChunkMatrices = (FMatrix3x4*)RHILockBuffer(CurrentBoneBuffer->VertexBufferRHI, 0, VectorArraySize, RLM_WriteOnly);
		}
	}
	else
	{
		if(NumBones)
		{
			check(NumBones * sizeof(FMatrix3x4) <= sizeof(GBoneUniformStruct));
			ChunkMatrices = (FMatrix3x4*)&GBoneUniformStruct;
		}
	}

	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FGPUBaseSkinVertexFactory_ShaderDataType_UpdateBoneData_CopyBones);
		//FMatrix3x4 is sizeof() == 48
		// PLATFORM_CACHE_LINE_SIZE (128) / 48 = 2.6
		//  sizeof(FMatrix) == 64
		// PLATFORM_CACHE_LINE_SIZE (128) / 64 = 2

		if (bGPUSkin_CopyBones_ISPC_Enabled)
		{
#if INTEL_ISPC
			ispc::UpdateBoneData_CopyBones(
				(ispc::FMatrix3x4*)&ChunkMatrices[0],
				(ispc::FMatrix44f*)&ReferenceToLocalMatrices[0],
				BoneMap.GetData(),
				NumBones);
#endif
		}
		else
		{
			const int32 PreFetchStride = 2; // FPlatformMisc::Prefetch stride
			for (uint32 BoneIdx = 0; BoneIdx < NumBones; BoneIdx++)
			{
				const FBoneIndexType RefToLocalIdx = BoneMap[BoneIdx];
				FPlatformMisc::Prefetch(ReferenceToLocalMatrices.GetData() + RefToLocalIdx + PreFetchStride);
				FPlatformMisc::Prefetch(ReferenceToLocalMatrices.GetData() + RefToLocalIdx + PreFetchStride, PLATFORM_CACHE_LINE_SIZE);

				FMatrix3x4& BoneMat = ChunkMatrices[BoneIdx];
				const FMatrix44f& RefToLocal = ReferenceToLocalMatrices[RefToLocalIdx];
				RefToLocal.To3x4MatrixTranspose((float*)BoneMat.M);
			}
		}
	}
	if (SupportsBonesBufferSRV(InFeatureLevel))
	{
		if (NumBones)
		{
			check(CurrentBoneBuffer);
			RHIUnlockBuffer(CurrentBoneBuffer->VertexBufferRHI);
		}
	}
	else
	{
		UniformBuffer = RHICreateUniformBuffer(&GBoneUniformStruct, FBoneMatricesUniformShaderParameters::StaticStructMetadata.GetLayoutPtr(), UniformBuffer_MultiFrame);
	}
	return false;
}

int32 FGPUBaseSkinVertexFactory::GetMinimumPerPlatformMaxGPUSkinBonesValue()
{
	const bool bUseGlobalMaxGPUSkinBones = (GCVarMaxGPUSkinBones != FGPUBaseSkinVertexFactory::GHardwareMaxGPUSkinBones);
	//Use the default value in case there is no valid target platform
	int32 MaxGPUSkinBones = GetDefault<URendererSettings>()->MaxSkinBones.GetValue();
#if WITH_EDITORONLY_DATA && WITH_EDITOR
	for (const TPair<FName, int32>& PlatformData : GetDefault<URendererSettings>()->MaxSkinBones.PerPlatform)
	{
		MaxGPUSkinBones = FMath::Min(MaxGPUSkinBones, PlatformData.Value);
	}
#endif
	if (bUseGlobalMaxGPUSkinBones)
	{
		MaxGPUSkinBones = FMath::Min(MaxGPUSkinBones, GCVarMaxGPUSkinBones);
	}
	return MaxGPUSkinBones;
}

int32 FGPUBaseSkinVertexFactory::GetMaxGPUSkinBones(const ITargetPlatform* TargetPlatform /*= nullptr*/)
{
	const bool bUseGlobalMaxGPUSkinBones = (GCVarMaxGPUSkinBones != FGPUBaseSkinVertexFactory::GHardwareMaxGPUSkinBones);
	if (bUseGlobalMaxGPUSkinBones)
	{
		static bool bIsLogged = false;
		if (!bIsLogged)
		{
			UE_LOG(LogSkeletalMesh, Display, TEXT("The Engine config variable [SystemSettings] Compat.MAX_GPUSKIN_BONES (%d) is deprecated, please remove the variable from any engine .ini file. Instead use the per platform project settings - Engine - Rendering - Skinning - Maximum bones per sections. Until the variable is remove we will clamp the per platform value"),
				   GCVarMaxGPUSkinBones);
			bIsLogged = true;
		}
	}
	//Use the default value in case there is no valid target platform
	int32 MaxGPUSkinBones = GetDefault<URendererSettings>()->MaxSkinBones.GetValue();
	
#if WITH_EDITOR
	const ITargetPlatform* TargetPlatformTmp = TargetPlatform;
	if (!TargetPlatformTmp)
	{
		//Get the running platform if the caller did not supply a platform
		ITargetPlatformManagerModule& TargetPlatformManager = GetTargetPlatformManagerRef();
		TargetPlatformTmp = TargetPlatformManager.GetRunningTargetPlatform();
	}
	if (TargetPlatformTmp)
	{
		//Get the platform value
		MaxGPUSkinBones = GetDefault<URendererSettings>()->MaxSkinBones.GetValueForPlatform(*TargetPlatformTmp->IniPlatformName());
	}
#endif

	if (bUseGlobalMaxGPUSkinBones)
	{
		//Make sure we do not go over the global ini console variable GCVarMaxGPUSkinBones
		MaxGPUSkinBones = FMath::Min(MaxGPUSkinBones, GCVarMaxGPUSkinBones);
		
	}

	//We cannot go under MAX_TOTAL_INFLUENCES
	MaxGPUSkinBones = FMath::Max(MaxGPUSkinBones, MAX_TOTAL_INFLUENCES);

	if (GCVarSupport16BitBoneIndex > 0)
	{
		// 16-bit bone index is supported
		return MaxGPUSkinBones;
	}
	else
	{
		// 16-bit bone index is not supported, clamp the max bones to 8-bit
		return FMath::Min(MaxGPUSkinBones, 256);
	}
}

bool FGPUBaseSkinVertexFactory::UseUnlimitedBoneInfluences(uint32 MaxBoneInfluences)
{
	const bool bUnlimitedBoneInfluence = (GCVarUnlimitedBoneInfluences!=0);
	const uint32 UnlimitedBoneInfluencesThreshold = (uint32) GCVarUnlimitedBoneInfluencesThreshold;
	return bUnlimitedBoneInfluence && MaxBoneInfluences > UnlimitedBoneInfluencesThreshold;
}

bool FGPUBaseSkinVertexFactory::GetUnlimitedBoneInfluences()
{
	return (GCVarUnlimitedBoneInfluences!=0);
}

void FGPUBaseSkinVertexFactory::SetData(const FGPUSkinDataType* InData)
{
	check(InData);

	if (!Data)
	{
		Data = MakeUnique<FGPUSkinDataType>();
	}

	*Data = *InData;
	UpdateRHI();
}

void FGPUBaseSkinVertexFactory::CopyDataTypeForPassthroughFactory(FGPUSkinPassthroughVertexFactory* PassthroughVertexFactory)
{
	FGPUSkinPassthroughVertexFactory::FDataType DestDataType;
	check(Data.IsValid());

	DestDataType.PositionComponent = Data->PositionComponent;
	DestDataType.TangentBasisComponents[0] = Data->TangentBasisComponents[0];
	DestDataType.TangentBasisComponents[1] = Data->TangentBasisComponents[1];
	DestDataType.TextureCoordinates = Data->TextureCoordinates;
	DestDataType.ColorComponent = Data->ColorComponent;
	DestDataType.PreSkinPositionComponent = Data->PositionComponent;
	DestDataType.PositionComponentSRV = Data->PositionComponentSRV;
	DestDataType.PreSkinPositionComponentSRV = Data->PositionComponentSRV;
	DestDataType.TangentsSRV = Data->TangentsSRV;
	DestDataType.ColorComponentsSRV = Data->ColorComponentsSRV;
	DestDataType.ColorIndexMask = Data->ColorIndexMask;
	DestDataType.TextureCoordinatesSRV = Data->TextureCoordinatesSRV;
	DestDataType.LightMapCoordinateIndex = Data->LightMapCoordinateIndex;
	DestDataType.NumTexCoords = Data->NumTexCoords;
	DestDataType.LODLightmapDataIndex = Data->LODLightmapDataIndex;

	PassthroughVertexFactory->SetData(DestDataType);
}


/*-----------------------------------------------------------------------------
TGPUSkinVertexFactory
-----------------------------------------------------------------------------*/

TGlobalResource<FBoneBufferPool> FGPUBaseSkinVertexFactory::BoneBufferPool;

template <GPUSkinBoneInfluenceType BoneInfluenceType>
bool TGPUSkinVertexFactory<BoneInfluenceType>::ShouldCompilePermutation(const FVertexFactoryShaderPermutationParameters& Parameters)
{
	bool bUnlimitedBoneInfluences = (BoneInfluenceType == UnlimitedBoneInfluence && GCVarUnlimitedBoneInfluences);
	return ShouldWeCompileGPUSkinVFShaders(Parameters.Platform, Parameters.MaterialParameters.FeatureLevel) &&
		  (((Parameters.MaterialParameters.bIsUsedWithSkeletalMesh || Parameters.MaterialParameters.bIsUsedWithMorphTargets) && (BoneInfluenceType != UnlimitedBoneInfluence || bUnlimitedBoneInfluences)) 
			  || Parameters.MaterialParameters.bIsSpecialEngineMaterial);
}

template <GPUSkinBoneInfluenceType BoneInfluenceType>
void TGPUSkinVertexFactory<BoneInfluenceType>::ModifyCompilationEnvironment(const FVertexFactoryShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment )
{
	FVertexFactory::ModifyCompilationEnvironment(Parameters, OutEnvironment);

	const FStaticFeatureLevel MaxSupportedFeatureLevel = GetMaxSupportedFeatureLevel(Parameters.Platform);
	// TODO: support GPUScene on mobile
	const bool bUseGPUScene = UseGPUScene(Parameters.Platform, MaxSupportedFeatureLevel) && (MaxSupportedFeatureLevel > ERHIFeatureLevel::ES3_1);
	const bool bSupportsPrimitiveIdStream = Parameters.VertexFactoryType->SupportsPrimitiveIdStream();
	{
		const bool bLimit2BoneInfluences = (CVarGPUSkinLimit2BoneInfluences.GetValueOnAnyThread() != 0);
		OutEnvironment.SetDefine(TEXT("GPUSKIN_LIMIT_2BONE_INFLUENCES"), (bLimit2BoneInfluences ? 1 : 0));
	}

	OutEnvironment.SetDefine(TEXT("GPUSKIN_USE_BONES_SRV_BUFFER"), SupportsBonesBufferSRV(Parameters.Platform) ? 1 : 0);
	OutEnvironment.SetDefine(TEXT("GPUSKIN_UNLIMITED_BONE_INFLUENCE"), BoneInfluenceType == UnlimitedBoneInfluence ? 1 : 0);

	OutEnvironment.SetDefine(TEXT("GPU_SKINNED_MESH_FACTORY"), 1);

	OutEnvironment.SetDefine(TEXT("VF_SUPPORTS_PRIMITIVE_SCENE_DATA"), bSupportsPrimitiveIdStream && bUseGPUScene);

	// Mobile doesn't support motion blur, don't use previous frame morph delta for mobile.
	const bool bIsMobile = IsMobilePlatform(Parameters.Platform);
	OutEnvironment.SetDefine(TEXT("GPUSKIN_MORPH_USE_PREVIOUS"), !bIsMobile);

	// Whether the material supports morph targets
	OutEnvironment.SetDefine(TEXT("GPUSKIN_MORPH_BLEND"), Parameters.MaterialParameters.bIsUsedWithMorphTargets || Parameters.MaterialParameters.bIsSpecialEngineMaterial);
}

/**
 * TGPUSkinVertexFactory does not support manual vertex fetch yet so worst case element set is returned to make sure the PSO can be compiled
 */
template <GPUSkinBoneInfluenceType BoneInfluenceType>
void TGPUSkinVertexFactory<BoneInfluenceType>::GetPSOPrecacheVertexFetchElements(EVertexInputStreamType VertexInputStreamType, FVertexDeclarationElementList& Elements)
{
	check(VertexInputStreamType == EVertexInputStreamType::Default);

	// Position
	Elements.Add(FVertexElement(0, 0, VET_Float3, 0, 0, false));

	// Normals
	Elements.Add(FVertexElement(1, 0, VET_PackedNormal, 1, 0, false));
	Elements.Add(FVertexElement(2, 0, VET_PackedNormal, 2, 0, false));
	
	// Bone data
	uint32 BaseStreamIndex = 3;
	if (BoneInfluenceType == UnlimitedBoneInfluence)
	{
		// Blend offset count
		Elements.Add(FVertexElement(BaseStreamIndex++, 0, VET_UInt, 3, 0, false));
	}
	else
	{
		// Blend indices
		Elements.Add(FVertexElement(BaseStreamIndex++, 0, VET_UByte4, 3, 0, false));
		Elements.Add(FVertexElement(BaseStreamIndex++, 0, VET_UByte4, 14, 0, false));

		// Blend weights
		Elements.Add(FVertexElement(BaseStreamIndex++, 0, VET_UByte4N, 4, 0, false));
		Elements.Add(FVertexElement(BaseStreamIndex++, 0, VET_UByte4N, 15, 0, false));
	}

	// Texcoords
	Elements.Add(FVertexElement(BaseStreamIndex++, 0, VET_Half4, 5, 0, false));
	Elements.Add(FVertexElement(BaseStreamIndex++, 0, VET_Half4, 6, 0, false));

	// Color
	Elements.Add(FVertexElement(BaseStreamIndex++, 0, VET_Color, 13, 0, false));

	// Attribute ID
	Elements.Add(FVertexElement(BaseStreamIndex++, 0, VET_UInt, 16, 0, true));

	// Morph blend data
	Elements.Add(FVertexElement(Elements.Num(), 0, VET_Float3, 9, 0, false));
	Elements.Add(FVertexElement(Elements.Num(), 0, VET_Float3, 10, 0, false));
}

/**
* Add the vertex declaration elements for the streams.
* @param InData - Type with stream components.
* @param OutElements - Vertex declaration list to modify.
*/
template <GPUSkinBoneInfluenceType BoneInfluenceType>
void TGPUSkinVertexFactory<BoneInfluenceType>::AddVertexElements(FVertexDeclarationElementList& OutElements)
{
	check(Data.IsValid());

	// Position
	OutElements.Add(AccessStreamComponent(Data->PositionComponent, 0));

	// Tangent basis vector
	OutElements.Add(AccessStreamComponent(Data->TangentBasisComponents[0], 1));
	OutElements.Add(AccessStreamComponent(Data->TangentBasisComponents[1], 2));

	// Texture coordinates
	if (Data->TextureCoordinates.Num())
	{
		const uint8 BaseTexCoordAttribute = 5;
		for (int32 CoordinateIndex = 0; CoordinateIndex < Data->TextureCoordinates.Num(); ++CoordinateIndex)
		{
			OutElements.Add(AccessStreamComponent(
				Data->TextureCoordinates[CoordinateIndex],
				BaseTexCoordAttribute + CoordinateIndex
			));
		}

		for (int32 CoordinateIndex = Data->TextureCoordinates.Num(); CoordinateIndex < MAX_TEXCOORDS; ++CoordinateIndex)
		{
			OutElements.Add(AccessStreamComponent(
				Data->TextureCoordinates[Data->TextureCoordinates.Num() - 1],
				BaseTexCoordAttribute + CoordinateIndex
			));
		}
	}

	if (Data->ColorComponentsSRV == nullptr)
	{
		Data->ColorComponentsSRV = GNullColorVertexBuffer.VertexBufferSRV;
		Data->ColorIndexMask = 0;
	}

	// Vertex color - account for the possibility that the mesh has no vertex colors
	if (Data->ColorComponent.VertexBuffer)
	{
		OutElements.Add(AccessStreamComponent(Data->ColorComponent, 13));
	}
	else
	{
		// If the mesh has no color component, set the null color buffer on a new stream with a stride of 0.
		// This wastes 4 bytes of memory per vertex, but prevents having to compile out twice the number of vertex factories.
		FVertexStreamComponent NullColorComponent(&GNullColorVertexBuffer, 0, 0, VET_Color, EVertexStreamUsage::ManualFetch);
		OutElements.Add(AccessStreamComponent(NullColorComponent, 13));
	}

	if (BoneInfluenceType == UnlimitedBoneInfluence)
	{
		// Blend offset count
		OutElements.Add(AccessStreamComponent(Data->BlendOffsetCount, 3));
	}
	else
	{
		// Bone indices
		OutElements.Add(AccessStreamComponent(Data->BoneIndices, 3));

		// Bone weights
		OutElements.Add(AccessStreamComponent(Data->BoneWeights, 4));

		// Extra bone indices & weights
		if (GetNumBoneInfluences() > MAX_INFLUENCES_PER_STREAM)
		{
			OutElements.Add(AccessStreamComponent(Data->ExtraBoneIndices, 14));
			OutElements.Add(AccessStreamComponent(Data->ExtraBoneWeights, 15));
		}
		else
		{
			OutElements.Add(AccessStreamComponent(Data->BoneIndices, 14));
			OutElements.Add(AccessStreamComponent(Data->BoneWeights, 15));
		}
	}

	// Primitive Id
	AddPrimitiveIdStreamElement(EVertexInputStreamType::Default, OutElements, 16, 0xff);

	// If the mesh is not a morph target, bind null component to morph delta stream.
	FVertexStreamComponent NullComponent(&GNullVertexBuffer, 0, 0, VET_Float3);
	FVertexElement DeltaPositionElement = AccessStreamComponent(Data->bMorphTarget ? Data->DeltaPositionComponent : NullComponent, 9);
	// Cache delta stream index (position & tangentZ share the same stream)
	MorphDeltaStreamIndex = DeltaPositionElement.StreamIndex;
	OutElements.Add(DeltaPositionElement);
	OutElements.Add(FVertexFactory::AccessStreamComponent(Data->bMorphTarget ? Data->DeltaTangentZComponent : NullComponent, 10));
}

/**
* Creates declarations for each of the vertex stream components and
* initializes the device resource
*/
template <GPUSkinBoneInfluenceType BoneInfluenceType>
void TGPUSkinVertexFactory<BoneInfluenceType>::InitRHI()
{
	// list of declaration items
	FVertexDeclarationElementList Elements;
	AddVertexElements(Elements);	

	// create the actual device decls
	InitDeclaration(Elements);
}

template <GPUSkinBoneInfluenceType BoneInfluenceType>
void TGPUSkinVertexFactory<BoneInfluenceType>::InitDynamicRHI()
{
	FVertexFactory::InitDynamicRHI();
	//ShaderData.UpdateBoneData(GetFeatureLevel());
}

template <GPUSkinBoneInfluenceType BoneInfluenceType>
void TGPUSkinVertexFactory<BoneInfluenceType>::ReleaseDynamicRHI()
{
	FVertexFactory::ReleaseDynamicRHI();
	ShaderData.ReleaseBoneData();
}

template <GPUSkinBoneInfluenceType BoneInfluenceType>
void TGPUSkinVertexFactory<BoneInfluenceType>::UpdateMorphVertexStream(const FMorphVertexBuffer* MorphVertexBuffer)
{
	if (MorphVertexBuffer && this->Streams.IsValidIndex(MorphDeltaStreamIndex))
	{
		this->Streams[MorphDeltaStreamIndex].VertexBuffer = MorphVertexBuffer;
	}
}

template <GPUSkinBoneInfluenceType BoneInfluenceType>
const FMorphVertexBuffer* TGPUSkinVertexFactory<BoneInfluenceType>::GetMorphVertexBuffer(bool bPrevious, uint32 FrameNumber) const
{
	check(Data.IsValid());
	return Data->MorphVertexBufferPool ? &Data->MorphVertexBufferPool->GetMorphVertexBufferForReading(bPrevious, FrameNumber) : nullptr;
}

/*-----------------------------------------------------------------------------
TGPUSkinAPEXClothVertexFactory
-----------------------------------------------------------------------------*/

template <GPUSkinBoneInfluenceType BoneInfluenceType>
void TGPUSkinAPEXClothVertexFactory<BoneInfluenceType>::ReleaseDynamicRHI()
{
	Super::ReleaseDynamicRHI();
	ClothShaderData.ReleaseClothSimulData();

	// Release the RHIResource reference held in FGPUSkinAPEXClothDataType
	if (ClothDataPtr)
	{
		ClothDataPtr->ClothBuffer.SafeRelease();
	}
}

/*-----------------------------------------------------------------------------
TGPUSkinVertexFactoryShaderParameters
-----------------------------------------------------------------------------*/

/** Shader parameters for use with TGPUSkinVertexFactory */
class FGPUSkinVertexFactoryShaderParameters : public FVertexFactoryShaderParameters
{
	DECLARE_TYPE_LAYOUT(FGPUSkinVertexFactoryShaderParameters, NonVirtual);
public:
	/**
	* Bind shader constants by name
	* @param	ParameterMap - mapping of named shader constants to indices
	*/
	void Bind(const FShaderParameterMap& ParameterMap)
	{
		PerBoneMotionBlur.Bind(ParameterMap,TEXT("PerBoneMotionBlur"));
		BoneMatrices.Bind(ParameterMap,TEXT("BoneMatrices"));
		PreviousBoneMatrices.Bind(ParameterMap,TEXT("PreviousBoneMatrices"));
		InputWeightIndexSize.Bind(ParameterMap, TEXT("InputWeightIndexSize"));
		InputWeightStream.Bind(ParameterMap, TEXT("InputWeightStream"));
		NumBoneInfluencesParam.Bind(ParameterMap, TEXT("NumBoneInfluencesParam"));
		IsMorphTarget.Bind(ParameterMap, TEXT("bIsMorphTarget"));
		PreviousMorphBufferParameter.Bind(ParameterMap, TEXT("PreviousMorphBuffer"));
	}

	void GetElementShaderBindings(
		const FSceneInterface* Scene,
		const FSceneView* View,
		const FMeshMaterialShader* Shader,
		const EVertexInputStreamType InputStreamType,
		ERHIFeatureLevel::Type FeatureLevel,
		const FVertexFactory* VertexFactory,
		const FMeshBatchElement& BatchElement,
		class FMeshDrawSingleShaderBindings& ShaderBindings,
		FVertexInputStreamArray& VertexStreams) const
	{
		const FGPUBaseSkinVertexFactory::FShaderDataType& ShaderData = ((const FGPUBaseSkinVertexFactory*)VertexFactory)->GetShaderData();

		bool bLocalPerBoneMotionBlur = false;

		if (SupportsBonesBufferSRV(FeatureLevel))
		{
			if (BoneMatrices.IsBound())
			{
				FRHIShaderResourceView* CurrentData = ShaderData.GetBoneBufferForReading(false).VertexBufferSRV;
				ShaderBindings.Add(BoneMatrices, CurrentData);
			}

			if (PreviousBoneMatrices.IsBound())
			{
				// todo: Maybe a check for PreviousData!=CurrentData would save some performance (when objects don't have velocty yet) but removing the bool also might save performance
				bLocalPerBoneMotionBlur = true;

				// Bone data is updated whenever animation triggers a dynamic update, animation can skip frames hence the frequency is not necessary every frame.
				// So check if bone data is updated this frame, if not then the previous frame data is stale and not suitable for motion blur.
				bool bBoneDataUpdatedThisFrame = View->Family->FrameNumber == ShaderData.UpdatedFrameNumber;
				// If world is paused, use current frame bone matrices, so velocity is canceled and skeletal mesh isn't blurred from motion.
				bool bPrevious = !View->Family->bWorldIsPaused && bBoneDataUpdatedThisFrame;
				FRHIShaderResourceView* PreviousData = ShaderData.GetBoneBufferForReading(bPrevious).VertexBufferSRV;
				ShaderBindings.Add(PreviousBoneMatrices, PreviousData);
			}
		}
		else
		{
			ShaderBindings.Add(Shader->GetUniformBufferParameter<FBoneMatricesUniformShaderParameters>(), ShaderData.GetUniformBuffer());
		}

		ShaderBindings.Add(PerBoneMotionBlur, (uint32)(bLocalPerBoneMotionBlur ? 1 : 0));

		ShaderBindings.Add(InputWeightIndexSize, ShaderData.InputWeightIndexSize);
		if (InputWeightStream.IsBound())
		{
			FRHIShaderResourceView* CurrentData = ShaderData.InputWeightStream;
			ShaderBindings.Add(InputWeightStream, CurrentData);
		}

		if (NumBoneInfluencesParam.IsBound())
		{
			uint32 NumInfluences = ((const FGPUBaseSkinVertexFactory*)VertexFactory)->GetNumBoneInfluences();
			ShaderBindings.Add(NumBoneInfluencesParam, NumInfluences);
		}

		ShaderBindings.Add(IsMorphTarget, (uint32)(((const FGPUBaseSkinVertexFactory*)VertexFactory)->IsMorphTarget() ? 1 : 0));

		// Mobile doesn't support motion blur, don't use previous frame morph delta for mobile.
		const EShaderPlatform ShaderPlatform = GetFeatureLevelShaderPlatform(FeatureLevel);
		const bool bIsMobile = IsMobilePlatform(ShaderPlatform);
		if (!bIsMobile)
		{
			const FMorphVertexBuffer* MorphVertexBuffer = nullptr;
			const auto* GPUSkinVertexFactory = (const FGPUBaseSkinVertexFactory*)VertexFactory;
			MorphVertexBuffer = GPUSkinVertexFactory->GetMorphVertexBuffer(!View->Family->bWorldIsPaused, View->Family->FrameNumber);
			ShaderBindings.Add(PreviousMorphBufferParameter, MorphVertexBuffer ? MorphVertexBuffer->GetSRV() : GNullVertexBuffer.VertexBufferSRV.GetReference());
		}
	}

private:
	LAYOUT_FIELD(FShaderParameter, PerBoneMotionBlur)
	LAYOUT_FIELD(FShaderResourceParameter, BoneMatrices)
	LAYOUT_FIELD(FShaderResourceParameter, PreviousBoneMatrices)
	LAYOUT_FIELD(FShaderParameter, InputWeightIndexSize);
	LAYOUT_FIELD(FShaderResourceParameter, InputWeightStream);
	LAYOUT_FIELD(FShaderParameter, NumBoneInfluencesParam);
	LAYOUT_FIELD(FShaderParameter, IsMorphTarget);
	LAYOUT_FIELD(FShaderResourceParameter, PreviousMorphBufferParameter);
};

IMPLEMENT_TYPE_LAYOUT(FGPUSkinVertexFactoryShaderParameters);

IMPLEMENT_GPUSKINNING_VERTEX_FACTORY_PARAMETER_TYPE(TGPUSkinVertexFactory, SF_Vertex, FGPUSkinVertexFactoryShaderParameters);

/** bind gpu skin vertex factory to its shader file and its shader parameters */
IMPLEMENT_GPUSKINNING_VERTEX_FACTORY_TYPE(TGPUSkinVertexFactory, "/Engine/Private/GpuSkinVertexFactory.ush",
	  EVertexFactoryFlags::UsedWithMaterials 
	| EVertexFactoryFlags::SupportsDynamicLighting
	| EVertexFactoryFlags::SupportsPSOPrecaching
);


/*-----------------------------------------------------------------------------
FGPUSkinPassthroughVertexFactory
-----------------------------------------------------------------------------*/
FGPUSkinPassthroughVertexFactory::FGPUSkinPassthroughVertexFactory(ERHIFeatureLevel::Type InFeatureLevel)
	: FLocalVertexFactory(InFeatureLevel, "FGPUSkinPassthroughVertexFactory")
{
	bGPUSkinPassThrough = true;
}

FGPUSkinPassthroughVertexFactory::~FGPUSkinPassthroughVertexFactory()
{
}

void FGPUSkinPassthroughVertexFactory::ReleaseRHI()
{
	FLocalVertexFactory::ReleaseRHI();

	// When adding anything else to this function be aware of the bypassing code in InternalUpdateVertexDeclaration.
	PositionRDG = nullptr;
	PrevPositionRDG = nullptr;
	TangentRDG = nullptr;
	ColorRDG = nullptr;
	PositionVBAlias.ReleaseRHI();
	TangentVBAlias.ReleaseRHI();
	ColorVBAlias.ReleaseRHI();
}

void FGPUSkinPassthroughVertexFactory::InternalUpdateVertexDeclaration(FGPUBaseSkinVertexFactory const* SourceVertexFactory)
{
	if (PositionVBAlias.VertexBufferRHI != nullptr)
	{
		Data.PositionComponent.VertexBuffer = &PositionVBAlias;
		Data.PositionComponent.Offset = 0;
		Data.PositionComponent.VertexStreamUsage = EVertexStreamUsage::Overridden;
		Data.PositionComponent.Stride = 3 * sizeof(float);
	}
	else
	{
		Data.PositionComponent = SourceVertexFactory->GetPositionStreamComponent();
	}

	if (TangentVBAlias.VertexBufferRHI != nullptr)
	{
		Data.TangentBasisComponents[0].VertexBuffer = &TangentVBAlias;
		Data.TangentBasisComponents[0].Offset = 0;
		Data.TangentBasisComponents[0].Type = VET_Short4N;
		Data.TangentBasisComponents[0].Stride = 16;
		Data.TangentBasisComponents[0].VertexStreamUsage = EVertexStreamUsage::Overridden | EVertexStreamUsage::ManualFetch;

		Data.TangentBasisComponents[1].VertexBuffer = &TangentVBAlias;
		Data.TangentBasisComponents[1].Offset = 8;
		Data.TangentBasisComponents[1].Type = VET_Short4N;
		Data.TangentBasisComponents[1].Stride = 16;
		Data.TangentBasisComponents[1].VertexStreamUsage = EVertexStreamUsage::Overridden | EVertexStreamUsage::ManualFetch;
	}
	else
	{
		Data.TangentBasisComponents[0] = SourceVertexFactory->GetTangentStreamComponent(0);
		Data.TangentBasisComponents[1] = SourceVertexFactory->GetTangentStreamComponent(1);
	}

	if (ColorVBAlias.VertexBufferRHI)
	{
		Data.ColorComponent.VertexBuffer = &ColorVBAlias;
		Data.ColorComponent.Offset = 0;
		Data.ColorComponent.Type = VET_Color;
		Data.ColorComponent.Stride = 4;
		Data.ColorComponent.VertexStreamUsage = EVertexStreamUsage::Overridden | EVertexStreamUsage::ManualFetch;
		
		Data.ColorIndexMask = ~0u;
	}

	Data.PositionComponentSRV = PositionSRVAlias ? PositionSRVAlias : SourceVertexFactory->GetPositionsSRV().GetReference();
	Data.PreSkinPositionComponentSRV = SourceVertexFactory->GetPositionsSRV().GetReference();
	Data.TangentsSRV = TangentSRVAlias ? TangentSRVAlias : SourceVertexFactory->GetTangentsSRV().GetReference();
	Data.ColorComponentsSRV = ColorSRVAlias ? ColorSRVAlias : SourceVertexFactory->GetColorComponentsSRV().GetReference();

	// hack to allow us to release the alias pointers properly in ReleaseRHI.
	// To be cleaned up in UE-68826
	FLocalVertexFactory::ReleaseRHI();
	FLocalVertexFactory::ReleaseDynamicRHI();
	FLocalVertexFactory::InitDynamicRHI();
	FLocalVertexFactory::InitRHI();

	// Find added streams
	PositionStreamIndex = -1;
	TangentStreamIndex = -1;

	for (int32 Index = 0; Index < Streams.Num(); ++Index)
	{
		if (Streams[Index].VertexBuffer->VertexBufferRHI.GetReference() == Data.PositionComponent.VertexBuffer->VertexBufferRHI.GetReference())
		{
			PositionStreamIndex = Index;
		}
		if (TangentVBAlias.VertexBufferRHI != nullptr)
		{
			if (Streams[Index].VertexBuffer->VertexBufferRHI.GetReference() == Data.TangentBasisComponents[0].VertexBuffer->VertexBufferRHI.GetReference())
			{
				TangentStreamIndex = Index;
			}
		}
	}
	checkf(PositionStreamIndex != -1, TEXT("Unable to find stream for RWBuffer Vertex buffer!"));
}

void FGPUSkinPassthroughVertexFactory::InternalUpdateVertexDeclaration(
	FGPUBaseSkinVertexFactory const* SourceVertexFactory, 
	struct FRWBuffer* PositionRWBuffer, 
	struct FRWBuffer* TangentRWBuffer)
{
	PositionRDG = nullptr;
	PrevPositionRDG = nullptr;
	TangentRDG = nullptr;
	ColorRDG = nullptr;

	PositionVBAlias.VertexBufferRHI = PositionRWBuffer ? PositionRWBuffer->Buffer : nullptr;
	TangentVBAlias.VertexBufferRHI = TangentRWBuffer ? TangentRWBuffer->Buffer : nullptr;
	ColorVBAlias.VertexBufferRHI = nullptr;

	PositionSRVAlias = PositionRWBuffer ? PositionRWBuffer->SRV : nullptr;
	PrevPositionSRVAlias = nullptr;
	TangentSRVAlias = TangentRWBuffer ? TangentRWBuffer->SRV : nullptr;
	ColorSRVAlias = nullptr;

	InternalUpdateVertexDeclaration(SourceVertexFactory);
}

void FGPUSkinPassthroughVertexFactory::InternalUpdateVertexDeclaration(
	EOverrideFlags OverrideFlags,
	FGPUBaseSkinVertexFactory const* SourceVertexFactory, 
	TRefCountPtr<FRDGPooledBuffer> const& PositionBuffer, 
	TRefCountPtr<FRDGPooledBuffer> const& TangentBuffer,
	TRefCountPtr<FRDGPooledBuffer> const& ColorBuffer)
{
	if (EnumHasAnyFlags(OverrideFlags, EOverrideFlags::Position))
	{
		PrevPositionRDG = PositionBuffer.IsValid() ? PositionRDG : nullptr;
		PositionRDG = PositionBuffer;
	}
	if (EnumHasAnyFlags(OverrideFlags, EOverrideFlags::Tangent))
	{
		TangentRDG = TangentBuffer;
	}
	if (EnumHasAnyFlags(OverrideFlags, EOverrideFlags::Color))
	{
		ColorRDG = ColorBuffer;
	}

	PositionVBAlias.VertexBufferRHI = PositionRDG.IsValid() ? PositionRDG->GetRHI() : nullptr;
	TangentVBAlias.VertexBufferRHI = TangentRDG.IsValid() ? TangentRDG->GetRHI() : nullptr;
	ColorVBAlias.VertexBufferRHI = ColorRDG.IsValid() ? ColorRDG->GetRHI() : nullptr;

	PositionSRVAlias = PositionRDG.IsValid() ? PositionRDG->GetOrCreateSRV(FRHIBufferSRVCreateInfo(PF_R32_FLOAT)) : nullptr;
	PrevPositionSRVAlias = PrevPositionRDG.IsValid() ? PrevPositionRDG->GetOrCreateSRV(FRHIBufferSRVCreateInfo(PF_R32_FLOAT)) : nullptr;
	const EPixelFormat TangentsFormat = IsOpenGLPlatform(GMaxRHIShaderPlatform) ? PF_R16G16B16A16_SINT : PF_R16G16B16A16_SNORM;
	TangentSRVAlias = TangentRDG.IsValid() ? TangentRDG->GetOrCreateSRV(FRHIBufferSRVCreateInfo(TangentsFormat)) : nullptr;
	ColorSRVAlias = ColorRDG.IsValid() ? ColorRDG->GetOrCreateSRV(FRHIBufferSRVCreateInfo(PF_R8G8B8A8)) : nullptr;

	InternalUpdateVertexDeclaration(SourceVertexFactory);
}


/*-----------------------------------------------------------------------------
	FGPUBaseSkinAPEXClothVertexFactory
-----------------------------------------------------------------------------*/
bool FGPUBaseSkinAPEXClothVertexFactory::IsClothEnabled(EShaderPlatform Platform)
{
	static FShaderPlatformCachedIniValue<bool> MobileEnableClothIniValue(TEXT("r.Mobile.EnableCloth"));
	const bool bEnableClothOnMobile = (MobileEnableClothIniValue.Get(Platform) != 0);
	const bool bIsMobile = IsMobilePlatform(Platform);
	return !bIsMobile || bEnableClothOnMobile;
}


/*-----------------------------------------------------------------------------
	TGPUSkinAPEXClothVertexFactoryShaderParameters
-----------------------------------------------------------------------------*/
/** Shader parameters for use with TGPUSkinAPEXClothVertexFactory */
class TGPUSkinAPEXClothVertexFactoryShaderParameters : public FGPUSkinVertexFactoryShaderParameters
{
	DECLARE_TYPE_LAYOUT(TGPUSkinAPEXClothVertexFactoryShaderParameters, NonVirtual);
public:

	/**
	* Bind shader constants by name
	* @param	ParameterMap - mapping of named shader constants to indices
	*/
	void Bind(const FShaderParameterMap& ParameterMap)
	{
		FGPUSkinVertexFactoryShaderParameters::Bind(ParameterMap);
		ClothSimulVertsPositionsNormalsParameter.Bind(ParameterMap,TEXT("ClothSimulVertsPositionsNormals"));
		PreviousClothSimulVertsPositionsNormalsParameter.Bind(ParameterMap,TEXT("PreviousClothSimulVertsPositionsNormals"));
		ClothToLocalParameter.Bind(ParameterMap, TEXT("ClothToLocal"));
		PreviousClothToLocalParameter.Bind(ParameterMap, TEXT("PreviousClothToLocal"));
		ClothBlendWeightParameter.Bind(ParameterMap, TEXT("ClothBlendWeight"));
		GPUSkinApexClothParameter.Bind(ParameterMap, TEXT("GPUSkinApexCloth"));
		GPUSkinApexClothStartIndexOffsetParameter.Bind(ParameterMap, TEXT("GPUSkinApexClothStartIndexOffset"));
		ClothNumInfluencesPerVertexParameter.Bind(ParameterMap, TEXT("ClothNumInfluencesPerVertex"));
	}
	
	void GetElementShaderBindings(
		const FSceneInterface* Scene,
		const FSceneView* View,
		const FMeshMaterialShader* Shader,
		const EVertexInputStreamType InputStreamType,
		ERHIFeatureLevel::Type FeatureLevel,
		const FVertexFactory* VertexFactory,
		const FMeshBatchElement& BatchElement,
		class FMeshDrawSingleShaderBindings& ShaderBindings,
		FVertexInputStreamArray& VertexStreams) const
	{
		// Call regular GPU skinning shader parameters
		FGPUSkinVertexFactoryShaderParameters::GetElementShaderBindings(Scene, View, Shader, InputStreamType, FeatureLevel, VertexFactory, BatchElement, ShaderBindings, VertexStreams);
		FGPUBaseSkinVertexFactory const* GPUSkinVertexFactory = (const FGPUBaseSkinVertexFactory*)VertexFactory;
		FGPUBaseSkinAPEXClothVertexFactory const* ClothVertexFactory = GPUSkinVertexFactory->GetClothVertexFactory();
		check(ClothVertexFactory != nullptr);

		const FGPUBaseSkinAPEXClothVertexFactory::ClothShaderType& ClothShaderData = ClothVertexFactory->GetClothShaderData();

		ShaderBindings.Add(Shader->GetUniformBufferParameter<FAPEXClothUniformShaderParameters>(), ClothShaderData.GetClothUniformBuffer());

		uint32 FrameNumber = View->Family->FrameNumber;

		ShaderBindings.Add(ClothSimulVertsPositionsNormalsParameter, ClothShaderData.GetClothBufferForReading(false, FrameNumber).VertexBufferSRV);
		ShaderBindings.Add(ClothToLocalParameter, ClothShaderData.GetClothToLocalForReading(false, FrameNumber));
		ShaderBindings.Add(ClothBlendWeightParameter,ClothShaderData.ClothBlendWeight);
		ShaderBindings.Add(ClothNumInfluencesPerVertexParameter, ClothShaderData.NumInfluencesPerVertex);

		// Mobile doesn't support motion blur, no need to feed the previous frame cloth data
		const EShaderPlatform ShaderPlatform = GetFeatureLevelShaderPlatform(FeatureLevel);
		const bool bIsMobile = IsMobilePlatform(ShaderPlatform);
		if (!bIsMobile)
		{
			ShaderBindings.Add(PreviousClothSimulVertsPositionsNormalsParameter, ClothShaderData.GetClothBufferForReading(true, FrameNumber).VertexBufferSRV);
			ShaderBindings.Add(PreviousClothToLocalParameter, ClothShaderData.GetClothToLocalForReading(true, FrameNumber));
		}

		ShaderBindings.Add(GPUSkinApexClothParameter, ClothVertexFactory->GetClothBuffer());
		int32 ClothIndexOffset = ClothVertexFactory->GetClothIndexOffset(BatchElement.MinVertexIndex);
		FIntPoint GPUSkinApexClothStartIndexOffset(BatchElement.MinVertexIndex, ClothIndexOffset);
		ShaderBindings.Add(GPUSkinApexClothStartIndexOffsetParameter, GPUSkinApexClothStartIndexOffset);
	}

protected:
	
	LAYOUT_FIELD(FShaderResourceParameter, ClothSimulVertsPositionsNormalsParameter);
	LAYOUT_FIELD(FShaderResourceParameter, PreviousClothSimulVertsPositionsNormalsParameter);
	LAYOUT_FIELD(FShaderParameter, ClothToLocalParameter);
	LAYOUT_FIELD(FShaderParameter, PreviousClothToLocalParameter);
	LAYOUT_FIELD(FShaderParameter, ClothBlendWeightParameter);
	LAYOUT_FIELD(FShaderResourceParameter, GPUSkinApexClothParameter);
	LAYOUT_FIELD(FShaderParameter, GPUSkinApexClothStartIndexOffsetParameter);
	LAYOUT_FIELD(FShaderParameter, ClothNumInfluencesPerVertexParameter);
};

IMPLEMENT_TYPE_LAYOUT(TGPUSkinAPEXClothVertexFactoryShaderParameters);

/*-----------------------------------------------------------------------------
	TGPUSkinAPEXClothVertexFactory::ClothShaderType
-----------------------------------------------------------------------------*/

bool FGPUBaseSkinAPEXClothVertexFactory::ClothShaderType::UpdateClothSimulData(FRHICommandListImmediate& RHICmdList, const TArray<FVector3f>& InSimulPositions,
	const TArray<FVector3f>& InSimulNormals, uint32 FrameNumberToPrepare, ERHIFeatureLevel::Type FeatureLevel)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FGPUBaseSkinAPEXClothVertexFactory_UpdateClothSimulData);

	uint32 NumSimulVerts = InSimulPositions.Num();

	check(IsInRenderingThread());
	
	FVertexBufferAndSRV* CurrentClothBuffer = &GetClothBufferForWriting(FrameNumberToPrepare);

	NumSimulVerts = FMath::Min(NumSimulVerts, (uint32)MAX_APEXCLOTH_VERTICES_FOR_VB);

	uint32 VectorArraySize = NumSimulVerts * sizeof(float) * 6;
	uint32 PooledArraySize = ClothSimulDataBufferPool.PooledSizeForCreationArguments(VectorArraySize);
	if(!IsValidRef(*CurrentClothBuffer) || PooledArraySize != CurrentClothBuffer->VertexBufferRHI->GetSize())
	{
		if(IsValidRef(*CurrentClothBuffer))
		{
			ClothSimulDataBufferPool.ReleasePooledResource(*CurrentClothBuffer);
		}
		*CurrentClothBuffer = ClothSimulDataBufferPool.CreatePooledResource(VectorArraySize);
		check(IsValidRef(*CurrentClothBuffer));
	}

	if(NumSimulVerts)
	{
		if (DeferSkeletalLockAndFillToRHIThread())
		{
			FRHIBuffer* VertexBuffer = CurrentClothBuffer->VertexBufferRHI;
			RHICmdList.EnqueueLambda([VertexBuffer, VectorArraySize, &InSimulPositions, &InSimulNormals](FRHICommandListImmediate& InRHICmdList)
			{
				QUICK_SCOPE_CYCLE_COUNTER(STAT_FRHICommandUpdateBoneBuffer_Execute);
				float* RESTRICT Data = (float* RESTRICT)InRHICmdList.LockBuffer(VertexBuffer, 0, VectorArraySize, RLM_WriteOnly);
				uint32 LambdaNumSimulVerts = InSimulPositions.Num();
				check(LambdaNumSimulVerts > 0 && LambdaNumSimulVerts <= MAX_APEXCLOTH_VERTICES_FOR_VB);
				float* RESTRICT Pos = (float* RESTRICT) &InSimulPositions[0].X;
				float* RESTRICT Normal = (float* RESTRICT) &InSimulNormals[0].X;
				for (uint32 Index = 0; Index < LambdaNumSimulVerts; Index++)
				{
					FPlatformMisc::Prefetch(Pos + PLATFORM_CACHE_LINE_SIZE);
					FPlatformMisc::Prefetch(Normal + PLATFORM_CACHE_LINE_SIZE);

					FMemory::Memcpy(Data, Pos, sizeof(float) * 3);
					FMemory::Memcpy(Data + 3, Normal, sizeof(float) * 3);
					Data += 6;
					Pos += 3;
					Normal += 3;
				}
				InRHICmdList.UnlockBuffer(VertexBuffer);
			});

			RHICmdList.RHIThreadFence(true);

			return true;
		}
		float* RESTRICT Data = (float* RESTRICT)RHILockBuffer(CurrentClothBuffer->VertexBufferRHI, 0, VectorArraySize, RLM_WriteOnly);
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_FGPUBaseSkinAPEXClothVertexFactory_UpdateClothSimulData_CopyData);
			float* RESTRICT Pos = (float* RESTRICT) &InSimulPositions[0].X;
			float* RESTRICT Normal = (float* RESTRICT) &InSimulNormals[0].X;
			for (uint32 Index = 0; Index < NumSimulVerts; Index++)
			{
				FPlatformMisc::Prefetch(Pos + PLATFORM_CACHE_LINE_SIZE);
				FPlatformMisc::Prefetch(Normal + PLATFORM_CACHE_LINE_SIZE);

				FMemory::Memcpy(Data, Pos, sizeof(float) * 3);
				FMemory::Memcpy(Data + 3, Normal, sizeof(float) * 3);
				Data += 6;
				Pos += 3;
				Normal += 3;
			}
		}
		RHIUnlockBuffer(CurrentClothBuffer->VertexBufferRHI);
	}
	
	return false;
}

/*-----------------------------------------------------------------------------
	TGPUSkinAPEXClothVertexFactory
-----------------------------------------------------------------------------*/
TGlobalResource<FClothBufferPool> FGPUBaseSkinAPEXClothVertexFactory::ClothSimulDataBufferPool;

/**
* Modify compile environment to enable the apex clothing path
* @param OutEnvironment - shader compile environment to modify
*/
template <GPUSkinBoneInfluenceType BoneInfluenceType>
void TGPUSkinAPEXClothVertexFactory<BoneInfluenceType>::ModifyCompilationEnvironment( const FVertexFactoryShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment )
{
	Super::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("GPUSKIN_APEX_CLOTH"),TEXT("1"));
	
	// Mobile doesn't support motion blur, don't use previous frame data.
	const bool bIsMobile = IsMobilePlatform(Parameters.Platform);
	OutEnvironment.SetDefine(TEXT("GPUSKIN_APEX_CLOTH_PREVIOUS"), !bIsMobile);
}

template <GPUSkinBoneInfluenceType BoneInfluenceType>
bool TGPUSkinAPEXClothVertexFactory<BoneInfluenceType>::ShouldCompilePermutation(const FVertexFactoryShaderPermutationParameters& Parameters)
{
	return IsClothEnabled(Parameters.Platform)
		&& (Parameters.MaterialParameters.bIsUsedWithAPEXCloth || Parameters.MaterialParameters.bIsSpecialEngineMaterial)
		&& Super::ShouldCompilePermutation(Parameters);
}

template <GPUSkinBoneInfluenceType BoneInfluenceType>
void TGPUSkinAPEXClothVertexFactory<BoneInfluenceType>::SetData(const FGPUSkinDataType* InData)
{
	const FGPUSkinAPEXClothDataType* InClothData = (const FGPUSkinAPEXClothDataType*)(InData);
	check(InClothData);

	if (!this->Data)
	{
		ClothDataPtr = new FGPUSkinAPEXClothDataType();
		this->Data = TUniquePtr<FGPUSkinDataType>(ClothDataPtr);
	}

	*ClothDataPtr = *InClothData;
	FGPUBaseSkinVertexFactory::UpdateRHI();
}

/**
* Creates declarations for each of the vertex stream components and
* initializes the device resource
*/
template <GPUSkinBoneInfluenceType BoneInfluenceType>
void TGPUSkinAPEXClothVertexFactory<BoneInfluenceType>::InitRHI()
{
	// list of declaration items
	FVertexDeclarationElementList Elements;	
	Super::AddVertexElements(Elements);

	// create the actual device decls
	FVertexFactory::InitDeclaration(Elements);
}

IMPLEMENT_GPUSKINNING_VERTEX_FACTORY_PARAMETER_TYPE(TGPUSkinAPEXClothVertexFactory, SF_Vertex, TGPUSkinAPEXClothVertexFactoryShaderParameters);

/** bind cloth gpu skin vertex factory to its shader file and its shader parameters */
IMPLEMENT_GPUSKINNING_VERTEX_FACTORY_TYPE(TGPUSkinAPEXClothVertexFactory, "/Engine/Private/GpuSkinVertexFactory.ush",
	  EVertexFactoryFlags::UsedWithMaterials
	| EVertexFactoryFlags::SupportsDynamicLighting
	| EVertexFactoryFlags::SupportsPSOPrecaching
);

#undef IMPLEMENT_GPUSKINNING_VERTEX_FACTORY_PARAMETER_TYPE
#undef IMPLEMENT_GPUSKINNING_VERTEX_FACTORY_TYPE
