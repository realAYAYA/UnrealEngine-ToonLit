// Copyright Epic Games, Inc. All Rights Reserved.

#include "GroomAsset.h"

#include "Async/Async.h"
#include "EngineUtils.h"
#include "GroomAssetImportData.h"
#include "GroomBuilder.h"
#include "HairCardsBuilder.h"
#include "GroomImportOptions.h"
#include "GroomSettings.h"
#include "RenderingThread.h"
#include "Engine/AssetUserData.h"
#include "HairStrandsVertexFactory.h"
#include "HAL/LowLevelMemTracker.h"
#include "Misc/App.h"
#include "Misc/Paths.h"
#include "Serialization/LargeMemoryReader.h"
#include "Serialization/LargeMemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "UObject/AnimObjectVersion.h"
#include "UObject/PhysicsObjectVersion.h"
#include "UObject/ReleaseObjectVersion.h"
#include "UObject/UE5MainStreamObjectVersion.h"
#include "NiagaraSystem.h"
#include "GroomComponent.h"
#include "Math/Box.h"
#include "Engine/StaticMesh.h"
#include "Logging/LogMacros.h"
#include "HairStrandsCore.h"
#include "UObject/ObjectSaveContext.h"
#include "GroomBindingAsset.h"
#include "GroomDeformerBuilder.h"
#include "HairCardsVertexFactory.h"
#include "PSOPrecache.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GroomAsset)


#if WITH_EDITOR
#include "Interfaces/ITargetPlatform.h"
#include "PlatformInfo.h"
#endif

#if WITH_EDITORONLY_DATA
#include "DerivedDataCacheInterface.h"
#include "EditorFramework/AssetImportData.h"
#endif

LLM_DEFINE_TAG(Groom);

#define LOCTEXT_NAMESPACE "GroomAsset"

static int32 GHairStrandsLoadAsset = 1;
static FAutoConsoleVariableRef CVarHairStrandsLoadAsset(TEXT("r.HairStrands.LoadAsset"), GHairStrandsLoadAsset, TEXT("Allow groom asset to be loaded"));

// Editor async groom load can be useful in a workflow that consists mostly of loading grooms from a hot DDC
static int32 GEnableGroomAsyncLoad = 0;
static FAutoConsoleVariableRef CVarGroomAsyncLoad(TEXT("r.HairStrands.AsyncLoad"), GEnableGroomAsyncLoad, TEXT("Allow groom asset to be loaded asynchronously in the editor"));

static TAutoConsoleVariable<int32> GHairStrandsWarningLogVerbosity(
	TEXT("r.HairStrands.Log"),
	-1,
	TEXT("Enable warning log report for groom related asset (0: no logging, 1: error only, 2: error & warning only, other: all logs). By default all logging are enabled (-1). Value needs to be set at startup time."));

static int32 GHairStrandsDDCLogEnable = 0;
static FAutoConsoleVariableRef CVarHairStrandsDDCLogEnable(TEXT("r.HairStrands.DDCLog"), GHairStrandsDDCLogEnable, TEXT("Enable DDC logging for groom assets and groom binding assets"));

bool IsHairStrandsDDCLogEnable()
{
	return GHairStrandsDDCLogEnable > 0;
}

static int32 GHairMaxSimulatedLOD = -1;
static FAutoConsoleVariableRef CVarHairMaxSimulatedLOD(TEXT("r.HairStrands.MaxSimulatedLOD"), GHairMaxSimulatedLOD, TEXT("Maximum hair LOD to be simulated"));
static bool IsHairLODSimulationEnabled(const int32 LODIndex) { return (LODIndex >= 0 && (GHairMaxSimulatedLOD < 0 || (GHairMaxSimulatedLOD >= 0 && LODIndex <= GHairMaxSimulatedLOD))); }

/////////////////////////////////////////////////////////////////////////////////////////
// Asset dump function

void DumpLoadedGroomData(IConsoleVariable* InCVarPakTesterEnabled);
void DumpLoadedGroomAssets(IConsoleVariable* InCVarPakTesterEnabled);
void DumpLoadedGroomComponent(IConsoleVariable* InCVarPakTesterEnabled);
void DumpLoadedGroomBindingAssets(IConsoleVariable* InCVarPakTesterEnabled);

int32 GHairStrandsDump_GroomAsset = 0;
static FAutoConsoleVariableRef CVarHairStrandsDump_GroomAsset(
	TEXT("r.HairStrands.Dump.GroomAsset"),
	GHairStrandsDump_GroomAsset,
	TEXT("Dump information of all the loaded groom assets."),
	FConsoleVariableDelegate::CreateStatic(DumpLoadedGroomAssets));

int32 GHairStrandsDump_GroomBindingAsset = 0;
static FAutoConsoleVariableRef CVarHairStrandsDump_GroomBindingAsset(
	TEXT("r.HairStrands.Dump.GroomBindingAsset"),
	GHairStrandsDump_GroomBindingAsset,
	TEXT("Dump information of all the loaded groom binding assets."),
	FConsoleVariableDelegate::CreateStatic(DumpLoadedGroomBindingAssets));

int32 GHairStrandsDump_All = 0;
static FAutoConsoleVariableRef CVarHairStrandsDump_All(
	TEXT("r.HairStrands.Dump"),
	GHairStrandsDump_All,
	TEXT("Dump all the loaded groom assets, groom binding assets, and instanciated groom components."),
	FConsoleVariableDelegate::CreateStatic(DumpLoadedGroomData));

uint32 FHairGroupData::FStrands::GetDataSize() const
{
	uint32 Total = 0;
	Total += FBaseWithInterpolation::GetDataSize();
	uint32 CTotal = 0;
	uint32 BTotal = 0;
	CTotal += ClusterCullingBulkData.LODVisibility.GetAllocatedSize();
	CTotal += ClusterCullingBulkData.CPULODScreenSize.GetAllocatedSize();
	CTotal += ClusterCullingBulkData.ClusterLODInfos.IsBulkDataLoaded()		? ClusterCullingBulkData.ClusterLODInfos.GetBulkDataSize() : 0;
	BTotal += ClusterCullingBulkData.VertexToClusterIds.IsBulkDataLoaded()	? ClusterCullingBulkData.VertexToClusterIds.GetBulkDataSize() : 0;
	BTotal += ClusterCullingBulkData.ClusterVertexIds.IsBulkDataLoaded()	? ClusterCullingBulkData.ClusterVertexIds.GetBulkDataSize() : 0;
	CTotal += ClusterCullingBulkData.PackedClusterInfos.IsBulkDataLoaded()	? ClusterCullingBulkData.PackedClusterInfos.GetBulkDataSize() : 0;

	return BTotal + Total + CTotal;
}

void DumpLoadedGroomAssets(IConsoleVariable* InCVarPakTesterEnabled)
{
	const bool bDetails = true;
	const float ToMb = 1.f / 1000000.f;

	uint32 Total_Asset = 0;
	uint32 Total_Group = 0;

	uint32 Total_CPUMemorySize_Guides = 0;
	uint32 Total_CPUMemorySize_Strands= 0;
	uint32 Total_CPUMemorySize_Cards  = 0;
	uint32 Total_CPUMemorySize_Meshes = 0;
	uint32 Total_GPUMemorySize_Guides = 0;
	uint32 Total_GPUMemorySize_Strands= 0;
	uint32 Total_GPUMemorySize_Cards  = 0;
	uint32 Total_GPUMemorySize_Meshes = 0;

	UE_LOG(LogHairStrands, Log, TEXT("[Groom] ##### UGroomAssets #####"));
	UE_LOG(LogHairStrands, Log, TEXT("----------------------------------------------------------------------------------------------------------------------------------------------------------------"));
	UE_LOG(LogHairStrands, Log, TEXT("--  No.  - LOD -    CPU Total (     Guides|    Strands|      Cards|     Meshes) -    GPU Total (     Guides|    Strands|      Cards|     Meshes) - Asset Name "));
	UE_LOG(LogHairStrands, Log, TEXT("----------------------------------------------------------------------------------------------------------------------------------------------------------------"));
	FString OutputData;
	for (TObjectIterator<UGroomAsset> AssetIt; AssetIt; ++AssetIt)
	{
		if (AssetIt)
		{
			const uint32 GroupCount = AssetIt->HairGroupsData.Num();
			for (uint32 GroupIt = 0; GroupIt < GroupCount; ++GroupIt)
			{
				const uint32 CPUMemorySize_Guides  = AssetIt->HairGroupsData[GroupIt].Guides.GetDataSize();
				const uint32 CPUMemorySize_Strands = AssetIt->HairGroupsData[GroupIt].Strands.GetDataSize();
				const uint32 CPUMemorySize_Cards   = AssetIt->HairGroupsData[GroupIt].Cards.GetDataSize();
				const uint32 CPUMemorySize_Meshes  = AssetIt->HairGroupsData[GroupIt].Meshes.GetDataSize();

				const uint32 GPUMemorySize_Guides  = AssetIt->HairGroupsData[GroupIt].Guides.GetResourcesSize();
				const uint32 GPUMemorySize_Strands = AssetIt->HairGroupsData[GroupIt].Strands.GetResourcesSize();
				const uint32 GPUMemorySize_Cards   = AssetIt->HairGroupsData[GroupIt].Cards.GetResourcesSize();
				const uint32 GPUMemorySize_Meshes  = AssetIt->HairGroupsData[GroupIt].Meshes.GetResourcesSize();

				uint32 CPUMemorySize = 0;
				uint32 GPUMemorySize = 0;
				CPUMemorySize += CPUMemorySize_Guides;
				CPUMemorySize += CPUMemorySize_Strands;
				CPUMemorySize += CPUMemorySize_Cards;
				CPUMemorySize += CPUMemorySize_Meshes;

				GPUMemorySize += GPUMemorySize_Guides;
				GPUMemorySize += GPUMemorySize_Strands;
				GPUMemorySize += GPUMemorySize_Cards;
				GPUMemorySize += GPUMemorySize_Meshes;

				Total_CPUMemorySize_Guides += CPUMemorySize_Guides;
				Total_CPUMemorySize_Strands+= CPUMemorySize_Strands;
				Total_CPUMemorySize_Cards  += CPUMemorySize_Cards;
				Total_CPUMemorySize_Meshes += CPUMemorySize_Meshes;
				Total_GPUMemorySize_Guides += GPUMemorySize_Guides;
				Total_GPUMemorySize_Strands+= GPUMemorySize_Strands;
				Total_GPUMemorySize_Cards  += GPUMemorySize_Cards;
				Total_GPUMemorySize_Meshes += GPUMemorySize_Meshes;

				const uint32 LODCount = AssetIt->HairGroupsLOD[GroupIt].LODs.Num();
				if (bDetails)
				{
//					UE_LOG(LogHairStrands, Log, TEXT("--  No.  - LOD -    CPU Total (     Guides|    Strands|      Cards|     Meshes) -    GPU Total (     Guides|    Strands|      Cards|     Meshes) - Asset Name "));
//					UE_LOG(LogHairStrands, Log, TEXT("-- 00/00 -  00 -  0000.0Mb (0000.0Mb |0000.0Mb|0000.0Mb |0000.0Mb) -  0000.0Mb (0000.0Mb|0000.0Mb|0000.0Mb|0000.0Mb) - AssetName"));
					UE_LOG(LogHairStrands, Log, TEXT("-- %2d/%2d -  %2d -  %9.3fMb (%9.3fMb|%9.3fMb|%9.3fMb|%9.3fMb) -  %9.3fMb (%9.3fMb|%9.3fMb|%9.3fMb|%9.3fMb) - %s"), 
						GroupIt,
						GroupCount,

						LODCount, 

						CPUMemorySize * ToMb,
						CPUMemorySize_Guides * ToMb,
						CPUMemorySize_Strands* ToMb,
						CPUMemorySize_Cards  * ToMb,
						CPUMemorySize_Meshes * ToMb,

						GPUMemorySize* ToMb,
						GPUMemorySize_Guides * ToMb,
						GPUMemorySize_Strands* ToMb,
						GPUMemorySize_Cards  * ToMb,
						GPUMemorySize_Meshes * ToMb,
						GroupIt == 0 ? *AssetIt->GetPathName() : TEXT("."));
				}
			}

			Total_Asset++;
			Total_Group += GroupCount;
		}
	}

	uint32 Total_CPUMemorySize = 0;
	Total_CPUMemorySize += Total_CPUMemorySize_Guides;
	Total_CPUMemorySize += Total_CPUMemorySize_Strands;
	Total_CPUMemorySize += Total_CPUMemorySize_Cards;
	Total_CPUMemorySize += Total_CPUMemorySize_Meshes;

	uint32 Total_GPUMemorySize = 0;
	Total_GPUMemorySize += Total_GPUMemorySize_Guides;
	Total_GPUMemorySize += Total_GPUMemorySize_Strands;
	Total_GPUMemorySize += Total_GPUMemorySize_Cards;
	Total_GPUMemorySize += Total_GPUMemorySize_Meshes;

	UE_LOG(LogHairStrands, Log, TEXT("----------------------------------------------------------------------------------------------------------------------------------------------------------------"));
	UE_LOG(LogHairStrands, Log, TEXT("-- A:%3d|G:%3d -  %9.3fMb (%9.3fMb|%9.3fMb|%9.3fMb|%9.3fMb) -  %9.3fMb (%9.3fMb|%9.3fMb|%9.3fMb|%9.3fMb)"),
		Total_Asset,
		Total_Group,

		Total_CPUMemorySize * ToMb,
		Total_CPUMemorySize_Guides * ToMb,
		Total_CPUMemorySize_Strands * ToMb,
		Total_CPUMemorySize_Cards * ToMb,
		Total_CPUMemorySize_Meshes * ToMb,

		Total_GPUMemorySize * ToMb,
		Total_GPUMemorySize_Guides * ToMb,
		Total_GPUMemorySize_Strands * ToMb,
		Total_GPUMemorySize_Cards * ToMb,
		Total_GPUMemorySize_Meshes * ToMb);
}

void DumpLoadedGroomBindingAssets(IConsoleVariable* InCVarPakTesterEnabled)
{
	const bool bDetails = true;
	const float ToMb = 1.f / 1000000.f;

	uint32 Total_Asset = 0;
	uint32 Total_Group = 0;

	uint32 Total_CPUMemorySize_Guides = 0;
	uint32 Total_CPUMemorySize_Strands= 0;
	uint32 Total_CPUMemorySize_Cards  = 0;
	
	uint32 Total_GPUMemorySize_Guides = 0;
	uint32 Total_GPUMemorySize_Strands= 0;
	uint32 Total_GPUMemorySize_Cards  = 0;
	

	UE_LOG(LogHairStrands, Log, TEXT("[Groom] ##### UGroomBindingAssets #####"));
	UE_LOG(LogHairStrands, Log, TEXT("----------------------------------------------------------------------------------------------------------------------------------------------------------------"));
	UE_LOG(LogHairStrands, Log, TEXT("--  No.  - LOD -    CPU Total (     Guides|    Strands|      Cards) -    GPU Total (     Guides|    Strands|      Cards) - Asset Name "));
	UE_LOG(LogHairStrands, Log, TEXT("----------------------------------------------------------------------------------------------------------------------------------------------------------------"));
	FString OutputData;
	for (TObjectIterator<UGroomBindingAsset> AssetIt; AssetIt; ++AssetIt)
	{
		if (AssetIt)
		{			
			const uint32 GroupCount = AssetIt->HairGroupBulkDatas.Num();
			for (uint32 GroupIt = 0; GroupIt < GroupCount; ++GroupIt)
			{
				const uint32 CPUMemorySize_Guides  = AssetIt->HairGroupBulkDatas[GroupIt].SimRootBulkData.GetDataSize();
				const uint32 CPUMemorySize_Strands = AssetIt->HairGroupBulkDatas[GroupIt].RenRootBulkData.GetDataSize();

				uint32 GPUMemorySize_Guides = 0;
				if (const FHairStrandsRestRootResource* RootResource = AssetIt->HairGroupResources[GroupIt].SimRootResources)
				{
					GPUMemorySize_Guides += RootResource->GetResourcesSize();
				}
				uint32 GPUMemorySize_Strands = 0;
				if (const FHairStrandsRestRootResource* RootResource = AssetIt->HairGroupResources[GroupIt].RenRootResources)
				{
					GPUMemorySize_Strands += RootResource->GetResourcesSize();
				}

				uint32 CPUMemorySize_Cards = 0;
				uint32 GPUMemorySize_Cards = 0;
				const uint32 CardCount = AssetIt->HairGroupBulkDatas[GroupIt].CardsRootBulkData.Num();
				for (uint32 CardIt = 0; CardIt < CardCount; ++CardIt)
				{
					CPUMemorySize_Cards += AssetIt->HairGroupBulkDatas[GroupIt].CardsRootBulkData[CardIt].GetDataSize();
					if (const FHairStrandsRestRootResource* RootResource = AssetIt->HairGroupResources[GroupIt].CardsRootResources[CardIt])
					{
						GPUMemorySize_Cards += RootResource->GetResourcesSize();
					}
				}

				const uint32 CPUMemorySize = CPUMemorySize_Guides + CPUMemorySize_Strands + CPUMemorySize_Cards;
				const uint32 GPUMemorySize = GPUMemorySize_Guides + GPUMemorySize_Strands + GPUMemorySize_Cards;

				Total_CPUMemorySize_Guides += CPUMemorySize_Guides;
				Total_CPUMemorySize_Strands+= CPUMemorySize_Strands;
				Total_CPUMemorySize_Cards  += CPUMemorySize_Cards;
				
				Total_GPUMemorySize_Guides += GPUMemorySize_Guides;
				Total_GPUMemorySize_Strands+= GPUMemorySize_Strands;
				Total_GPUMemorySize_Cards  += GPUMemorySize_Cards;

				const uint32 SkelLODCount = AssetIt->HairGroupBulkDatas[GroupIt].RenRootBulkData.MeshProjectionLODs.Num();
				if (bDetails)
				{
//					UE_LOG(LogHairStrands, Log, TEXT("--  No.  - LOD -    CPU Total (     Guides|    Strands|      Cards) -    GPU Total (     Guides|    Strands|      Cards) - Asset Name "));
//					UE_LOG(LogHairStrands, Log, TEXT("-- 00/00 -  00 -  0000.0Mb (0000.0Mb |0000.0Mb|0000.0Mb) -  0000.0Mb (0000.0Mb|0000.0Mb|0000.0Mb) - AssetName"));
					UE_LOG(LogHairStrands, Log, TEXT("-- %2d/%2d -  %2d -  %9.3fMb (%9.3fMb|%9.3fMb|%9.3fMb) -  %9.3fMb (%9.3fMb|%9.3fMb|%9.3fMb) - %s"), 
						GroupIt,
						GroupCount,

						SkelLODCount,

						CPUMemorySize * ToMb,
						CPUMemorySize_Guides * ToMb,
						CPUMemorySize_Strands* ToMb,
						CPUMemorySize_Cards  * ToMb,

						GPUMemorySize* ToMb,
						GPUMemorySize_Guides * ToMb,
						GPUMemorySize_Strands* ToMb,
						GPUMemorySize_Cards  * ToMb,
						GroupIt == 0 ? *AssetIt->GetPathName() : TEXT("."));
				}
			}

			Total_Asset++;
			Total_Group += GroupCount;
		}
	}

	uint32 Total_CPUMemorySize = 0;
	Total_CPUMemorySize += Total_CPUMemorySize_Guides;
	Total_CPUMemorySize += Total_CPUMemorySize_Strands;
	Total_CPUMemorySize += Total_CPUMemorySize_Cards;

	uint32 Total_GPUMemorySize = 0;
	Total_GPUMemorySize += Total_GPUMemorySize_Guides;
	Total_GPUMemorySize += Total_GPUMemorySize_Strands;
	Total_GPUMemorySize += Total_GPUMemorySize_Cards;

	UE_LOG(LogHairStrands, Log, TEXT("----------------------------------------------------------------------------------------------------------------------------------------------------------------"));
	UE_LOG(LogHairStrands, Log, TEXT("-- A:%3d|G:%3d -  %9.3fMb (%9.3fMb|%9.3fMb|%9.3fMb) -  %9.3fMb (%9.3fMb|%9.3fMb|%9.3fMb)"),
		Total_Asset,
		Total_Group,

		Total_CPUMemorySize * ToMb,
		Total_CPUMemorySize_Guides * ToMb,
		Total_CPUMemorySize_Strands * ToMb,
		Total_CPUMemorySize_Cards * ToMb,

		Total_GPUMemorySize * ToMb,
		Total_GPUMemorySize_Guides * ToMb,
		Total_GPUMemorySize_Strands * ToMb,
		Total_GPUMemorySize_Cards * ToMb);
}

void DumpLoadedGroomData(IConsoleVariable* InCVarPakTesterEnabled)
{
	DumpLoadedGroomAssets(InCVarPakTesterEnabled);
	DumpLoadedGroomBindingAssets(InCVarPakTesterEnabled);
	DumpLoadedGroomComponent(InCVarPakTesterEnabled);
}

/////////////////////////////////////////////////////////////////////////////////////////

void UpdateHairStrandsLogVerbosity()
{
	const int32 Verbosity = GHairStrandsWarningLogVerbosity->GetInt();
	switch (Verbosity)
	{
	case 0:  UE_SET_LOG_VERBOSITY(LogHairStrands, NoLogging); break;
	case 1:  UE_SET_LOG_VERBOSITY(LogHairStrands, Error); break;
	case 2:  UE_SET_LOG_VERBOSITY(LogHairStrands, Warning); break;
	default: UE_SET_LOG_VERBOSITY(LogHairStrands, Log); break;
	};
}

/////////////////////////////////////////////////////////////////////////////////////////

template<typename ResourceType>
static void InitAtlasTexture(ResourceType* InResource, UTexture2D* InTexture, EHairAtlasTextureType InType)
{
	if (InTexture == nullptr || InResource == nullptr)
		return;

	InTexture->ConditionalPostLoad();

	ENQUEUE_RENDER_COMMAND(HairStrandsCardsTextureCommand)(
	[InResource, InTexture, InType](FRHICommandListImmediate& RHICmdList)
	{
		FSamplerStateRHIRef DefaultSampler = TStaticSamplerState<SF_AnisotropicLinear, AM_Clamp, AM_Clamp, AM_Clamp, 0, 0>::GetRHI();
		switch (InType)
		{
		case EHairAtlasTextureType::Depth:
		{
			InResource->DepthTexture = InTexture->TextureReference.TextureReferenceRHI;
			InResource->DepthSampler = DefaultSampler;
		} break;
		case EHairAtlasTextureType::Tangent:
		{
			InResource->TangentTexture = InTexture->TextureReference.TextureReferenceRHI;
			InResource->TangentSampler = DefaultSampler;
		} break;
		case EHairAtlasTextureType::Attribute:
		{
			InResource->AttributeTexture = InTexture->TextureReference.TextureReferenceRHI;
			InResource->AttributeSampler = DefaultSampler;
		} break;
		case EHairAtlasTextureType::Coverage:
		{
			InResource->CoverageTexture = InTexture->TextureReference.TextureReferenceRHI;
			InResource->CoverageSampler = DefaultSampler;
		} break;
		case EHairAtlasTextureType::AuxilaryData:
		{
			InResource->AuxilaryDataTexture = InTexture->TextureReference.TextureReferenceRHI;
			InResource->AuxilaryDataSampler = DefaultSampler;
		} break;
		case EHairAtlasTextureType::Material:
		{
			InResource->MaterialTexture = InTexture->TextureReference.TextureReferenceRHI;
			InResource->MaterialSampler = DefaultSampler;
		} break;
		}
	});
}

template<typename T>
static const T* GetSourceDescription(const TArray<T>& InHairGroups, uint32 GroupIndex, uint32 LODIndex, int32& SourceIndex)
{
	SourceIndex = 0;
	for (const T& SourceDesc : InHairGroups)
	{
		if (SourceDesc.GroupIndex == GroupIndex && SourceDesc.LODIndex == LODIndex)
		{
			return &SourceDesc;
		}
		++SourceIndex;
	}
	SourceIndex = -1;
	return nullptr;
}

template<typename T>
static T* GetSourceDescription(TArray<T>& InHairGroups, uint32 GroupIndex, uint32 LODIndex, int32& SourceIndex)
{
	SourceIndex = 0;
	for (T& SourceDesc : InHairGroups)
	{
		if (SourceDesc.GroupIndex == GroupIndex && SourceDesc.LODIndex == LODIndex)
		{
			return &SourceDesc;
		}
		++SourceIndex;
	}
	SourceIndex = -1;
	return nullptr;
}

bool IsHairStrandsAssetLoadingEnable()
{
	return GHairStrandsLoadAsset > 0;
}

// Build: 
// * Guides bulk data
// * Strands bulk data
// * Interpolation bulk data
// * Clusters (bulk) data
//
// Guides, strands, and interpolation data are all transient
static bool BuildHairGroup(
	const int32 GroupIndex,
	const bool bNeedInterpolationData,
	const FHairDescriptionGroups& HairDescriptionGroups, 
	const TArray<FHairGroupsInterpolation>& HairGroupsInterpolation,
	const TArray<FHairGroupsLOD>& HairGroupsLOD,
	TArray<FHairGroupInfoWithVisibility>& OutHairGroupsInfo,
	TArray<FHairGroupData>& OutHairGroupsData)
{
	const bool bIsValid = HairDescriptionGroups.IsValid();
	if (bIsValid)
	{
		const FHairDescriptionGroup& HairGroup = HairDescriptionGroups.HairGroups[GroupIndex];
		check(GroupIndex <= HairDescriptionGroups.HairGroups.Num());
		check(GroupIndex == HairGroup.Info.GroupID);

		FHairStrandsDatas StrandsData;
		FHairStrandsDatas GuidesData;
		FGroomBuilder::BuildData(HairGroup, HairGroupsInterpolation[GroupIndex], OutHairGroupsInfo[GroupIndex], StrandsData, GuidesData);

		FGroomBuilder::BuildBulkData(HairGroup.Info, GuidesData, OutHairGroupsData[GroupIndex].Guides.BulkData);
		FGroomBuilder::BuildBulkData(HairGroup.Info, StrandsData, OutHairGroupsData[GroupIndex].Strands.BulkData);

		// If there is no simulation or no global interpolation on that group there is no need for builder the interpolation data
		if (bNeedInterpolationData)
		{
			FHairStrandsInterpolationDatas InterpolationData;
			FGroomBuilder::BuildInterplationData(HairGroup.Info, StrandsData, GuidesData, HairGroupsInterpolation[GroupIndex].InterpolationSettings, InterpolationData);
			FGroomBuilder::BuildInterplationBulkData(GuidesData, InterpolationData, OutHairGroupsData[GroupIndex].Strands.InterpolationBulkData);
		}
		else
		{
			OutHairGroupsData[GroupIndex].Strands.InterpolationBulkData.Reset();
		}

		FGroomBuilder::BuildClusterBulkData(StrandsData, HairDescriptionGroups.Bounds.SphereRadius, HairGroupsLOD[GroupIndex], OutHairGroupsData[GroupIndex].Strands.ClusterCullingBulkData);
	}
	return bIsValid;
}

// Build: 
// * Clusters (bulk) data
//
// Guides and strands data are all transient
static bool BuildHairGroupCluster(
	const int32 GroupIndex,
	const FHairDescriptionGroups& HairDescriptionGroups,
	const TArray<FHairGroupsInterpolation>& HairGroupsInterpolation,
	const TArray<FHairGroupsLOD>& HairGroupsLOD,
	TArray<FHairGroupInfoWithVisibility>& OutHairGroupsInfo,
	TArray<FHairGroupData>& OutHairGroupsData)
{
	const bool bIsValid = HairDescriptionGroups.IsValid();
	if (bIsValid)
	{
		const FHairDescriptionGroup& HairGroup = HairDescriptionGroups.HairGroups[GroupIndex];
		check(GroupIndex <= HairDescriptionGroups.HairGroups.Num());
		check(GroupIndex == HairGroup.Info.GroupID);

		FHairStrandsDatas StrandsData;
		FHairStrandsDatas GuidesData;
		FGroomBuilder::BuildData(HairGroup, HairGroupsInterpolation[GroupIndex], OutHairGroupsInfo[GroupIndex], StrandsData, GuidesData);
		FGroomBuilder::BuildClusterBulkData(StrandsData, HairDescriptionGroups.Bounds.SphereRadius, HairGroupsLOD[GroupIndex], OutHairGroupsData[GroupIndex].Strands.ClusterCullingBulkData);
	}
	return bIsValid;
}

/////////////////////////////////////////////////////////////////////////////////////////

uint8 UGroomAsset::GenerateClassStripFlags(FArchive& Ar)
{
#if WITH_EDITOR
	const bool bIsCook = Ar.IsCooking();
	const ITargetPlatform* CookTarget = Ar.CookingTarget();

	bool bIsStrandsSupportedOnTargetPlatform = true;
	bool bIsLODStripped = false;
	bool bIsStrandsStripped = false;
	bool bIsCardsStripped = false;
	bool bIsMeshesStripped = false;
	if (bIsCook)
	{
		// Determine if strands are supported on the target cook platform
		TArray<FName> ShaderFormats;
		CookTarget->GetAllTargetedShaderFormats(ShaderFormats);
		for (int32 FormatIndex = 0; FormatIndex < ShaderFormats.Num(); ++FormatIndex)
		{
			const EShaderPlatform ShaderPlatform = ShaderFormatToLegacyShaderPlatform(ShaderFormats[FormatIndex]);
			bIsStrandsSupportedOnTargetPlatform &= IsHairStrandsSupported(EHairStrandsShaderType::Strands, ShaderPlatform);
		}

		// Determine the platform min LOD if the stripping hasn't been disabled
		const bool bDisableMinLODStrip = DisableBelowMinLodStripping.GetValueForPlatform(*CookTarget->IniPlatformName());
		int32 PlatformMinLOD = 0;
		if (!bDisableMinLODStrip)
		{
			PlatformMinLOD = MinLOD.GetValueForPlatform(*CookTarget->IniPlatformName());
		}

		bIsLODStripped = (PlatformMinLOD > 0) && !bDisableMinLODStrip;
		if (bIsLODStripped)
		{
			for (const FHairGroupsLOD& GroupsLOD : HairGroupsLOD)
			{
				for (int32 LODIndex = 0; LODIndex < PlatformMinLOD && LODIndex < GroupsLOD.LODs.Num(); ++LODIndex)
				{
					const FHairLODSettings& LODSetting = GroupsLOD.LODs[LODIndex];
					switch (LODSetting.GeometryType)
					{
					case EGroomGeometryType::Strands:
						bIsStrandsStripped = true;
						break;
					case EGroomGeometryType::Cards:
						bIsCardsStripped = true;
						break;
					case EGroomGeometryType::Meshes:
						bIsMeshesStripped = true;
						break;
					}
				}
			}
		}
	}

	uint8 ClassDataStripFlags = 0;
	ClassDataStripFlags |= HasImportedStrandsData() ? CDSF_ImportedStrands : 0;

	// Strands are cooked out if the platform doesn't support them even if they were not marked for stripping
	ClassDataStripFlags |= !bIsStrandsSupportedOnTargetPlatform || bIsStrandsStripped ? CDSF_StrandsStripped : 0;
	ClassDataStripFlags |= bIsLODStripped ? CDSF_MinLodData : 0;
	ClassDataStripFlags |= bIsCardsStripped ? CDSF_CardsStripped : 0;
	ClassDataStripFlags |= bIsMeshesStripped ? CDSF_MeshesStripped : 0;

	return ClassDataStripFlags;
#else
	return 0;
#endif
}

void UGroomAsset::ApplyStripFlags(uint8 StripFlags, const ITargetPlatform* CookTarget)
{
#if WITH_EDITOR
	if (!CookTarget)
	{
		return;
	}

	const bool bDisableMinLodStrip = DisableBelowMinLodStripping.GetValueForPlatform(*CookTarget->IniPlatformName()); 
	int32 PlatformMinLOD = 0;
	if (!bDisableMinLodStrip)
	{
		PlatformMinLOD = MinLOD.GetValueForPlatform(*CookTarget->IniPlatformName());
	}

	const bool bIsStrandsStrippedForCook = !!(StripFlags & CDSF_StrandsStripped);

	// Set the CookedOut flags as appropriate and compute the value for EffectiveLODBias
	for (int32 GroupIndex = 0; GroupIndex < HairGroupsData.Num(); ++GroupIndex)
	{
		FHairGroupData& GroupData = HairGroupsData[GroupIndex];
		GroupData.bIsCookedOut = bIsStrandsStrippedForCook;

		// Determine the max LOD for strands since they are cooked out if the platform doesn't support them
		// They are cooked out as a whole
		int32 StrandsMaxLOD = -1;
		if (bIsStrandsStrippedForCook)
		{
			const FHairGroupsLOD& GroupsLOD = HairGroupsLOD[GroupIndex];
			for (int32 LODIndex = 0; LODIndex < GroupsLOD.LODs.Num(); ++LODIndex)
			{
				const FHairLODSettings& LODSetting = GroupsLOD.LODs[LODIndex];
				if (LODSetting.GeometryType == EGroomGeometryType::Strands)
				{
					StrandsMaxLOD = LODIndex;
				}
			}
		}
		
		// The MinLOD for stripping this group, taking into account the possibility of strands being cooked out
		const int32 MinStrippedLOD = FMath::Max(StrandsMaxLOD + 1, PlatformMinLOD);
		EffectiveLODBias[GroupIndex] = MinStrippedLOD;

		for (int32 Index = 0; Index < GroupData.Cards.LODs.Num(); ++Index)
		{
			GroupData.Cards.LODs[Index].bIsCookedOut = !!(StripFlags & CDSF_CardsStripped) && (Index < MinStrippedLOD);
		}

		for (int32 Index = 0; Index < GroupData.Meshes.LODs.Num(); ++Index)
		{
			GroupData.Meshes.LODs[Index].bIsCookedOut = !!(StripFlags & CDSF_MeshesStripped) && (Index < MinStrippedLOD);
		}
	}
#endif
}

#if WITH_EDITORONLY_DATA
bool UGroomAsset::HasImportedStrandsData() const
{
	bool bHasImportedStrandsData = false;
	for (const FString& Key : StrandsDerivedDataKey)
	{
		bHasImportedStrandsData |= !Key.IsEmpty();
	}
	return bHasImportedStrandsData;
}
#endif

// Serialization for *array* of hair elements
static void InternalSerialize(FArchive& Ar, UObject* Owner, TArray<FHairGroupData::FCards::FLOD>& CardLODData);
static void InternalSerialize(FArchive& Ar, UObject* Owner, TArray<FHairGroupData::FMeshes::FLOD>& MeshLODData);
static void InternalSerialize(FArchive& Ar, UObject* Owner, TArray<FHairGroupData>& GroupData);

// Serialization for hair elements
static void InternalSerialize(FArchive& Ar, UObject* Owner, FHairGroupData::FCards::FLOD& CardLODData);
static void InternalSerialize(FArchive& Ar, UObject* Owner, FHairGroupData::FMeshes::FLOD& MeshLODData);
static void InternalSerialize(FArchive& Ar, UObject* Owner, FHairGroupData& GroupData);

void UGroomAsset::Serialize(FArchive& Ar)
{
	uint8 ClassDataStripFlags = GenerateClassStripFlags(Ar);
	ApplyStripFlags(ClassDataStripFlags, Ar.CookingTarget());

	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FPhysicsObjectVersion::GUID);
	Ar.UsingCustomVersion(FReleaseObjectVersion::GUID);			// Needed to support MeshDescription AttributesSet serialization
	Ar.UsingCustomVersion(FUE5MainStreamObjectVersion::GUID);	// Needed to support MeshDescription AttributesSet serialization
	Ar.UsingCustomVersion(FAnimObjectVersion::GUID);    // Needed to support Cards and Cluster culling serialization

	if (Ar.CustomVer(FPhysicsObjectVersion::GUID) >= FPhysicsObjectVersion::GroomWithDescription)
	{
		FStripDataFlags StripFlags(Ar, ClassDataStripFlags);

		bool bHasImportedStrands = true; // true because prior to this version, we assume that a groom is created from imported strands
		if (Ar.CustomVer(FAnimObjectVersion::GUID) >= FAnimObjectVersion::GroomLODStripping)
		{
			// StripFlags keeps the flags only when cooking, but the class flags are needed when not cooking
			Ar << ClassDataStripFlags;
			bHasImportedStrands = ClassDataStripFlags & CDSF_ImportedStrands;
		}

		if (StripFlags.IsEditorDataStripped() || !bHasImportedStrands || (Ar.IsSaving() && !CanRebuildFromDescription()))
		{
			// When cooking data or serializing old format to new format,
			// serialize the computed groom data
			InternalSerialize(Ar, this, HairGroupsData);
		}
#if WITH_EDITORONLY_DATA
		else
		{
			// When serializing data for editor, serialize the HairDescription as bulk data
			// The computed groom data is fetched from the Derived Data Cache
			if (!HairDescriptionBulkData[EHairDescriptionType::Source])
			{
				// When loading, bulk data can be null so instantiate a new one to serialize into
				HairDescriptionBulkData[EHairDescriptionType::Source] = MakeUnique<FHairDescriptionBulkData>();
			}

			// We don' serialize the hair description if it is a transactional object (i.e., for undo/redo)
			if (!Ar.IsTransacting())
			{
				HairDescriptionBulkData[EHairDescriptionType::Source]->Serialize(Ar, this);
			}
		}
#endif // WITH_EDITORONLY_DATA
	}
	else
	{
		// Old format serialized the computed groom data directly, but are no longer supported
		UE_LOG(LogHairStrands, Error, TEXT("[Groom] The groom asset %s is too old. Please reimported the groom from its original source file."), *GetName());
	}
}

UGroomAsset::UGroomAsset(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsInitialized = false;

	MinLOD.Default = 0;
	DisableBelowMinLodStripping.Default = false;

	UpdateHairStrandsLogVerbosity();
}

bool UGroomAsset::HasGeometryType(uint32 GroupIndex, EGroomGeometryType Type) const
{
	check(GroupIndex < uint32(HairGroupsLOD.Num()));
	const uint32 LODCount = HairGroupsLOD[GroupIndex].LODs.Num();
	for (uint32 LODIt=0;LODIt<LODCount;++LODIt)
	{
		if (HairGroupsLOD[GroupIndex].LODs[LODIt].GeometryType == Type)
			return true;
	}
	return false;
}

bool UGroomAsset::HasGeometryType(EGroomGeometryType Type) const
{
	const uint32 GroupCount = HairGroupsLOD.Num();
	for (uint32 GroupIt = 0; GroupIt < GroupCount; ++GroupIt)
	{
		if (HasGeometryType(GroupIt, Type))
			return true;
	}
	return false;
}

TArray<FHairVertexFactoryTypesPerMaterialData> UGroomAsset::CollectVertexFactoryTypesPerMaterialData(EShaderPlatform ShaderPlatform)
{
	TArray<FHairVertexFactoryTypesPerMaterialData> VFsPerMaterials;

	const auto AddVFPerMaterial = [&VFsPerMaterials](int32 InMaterialIndex, EHairGeometryType InHairGeometryType, const FVertexFactoryType* InVFType)
	{
		if (InMaterialIndex < 0)
		{
			return;
		}

		FHairVertexFactoryTypesPerMaterialData* VFsPerMaterial = VFsPerMaterials.FindByPredicate([InMaterialIndex, InHairGeometryType](const FHairVertexFactoryTypesPerMaterialData& Other) { return (Other.MaterialIndex == InMaterialIndex && Other.HairGeometryType == InHairGeometryType); });
		if (VFsPerMaterial == nullptr)
		{
			VFsPerMaterial = &VFsPerMaterials.AddDefaulted_GetRef();
			VFsPerMaterial->MaterialIndex = InMaterialIndex;
			VFsPerMaterial->HairGeometryType = InHairGeometryType;
		}
		VFsPerMaterial->VertexFactoryTypes.AddUnique(InVFType);
	};

	const int32 GroupCount = GetNumHairGroups();
	for (int32 GroupIt = 0; GroupIt < GroupCount; GroupIt++)
	{
		const FHairGroupData& InGroupData = HairGroupsData[GroupIt];

		// Strands
		{
			const int32 MaterialIndex = GetMaterialIndex(HairGroupsRendering[GroupIt].MaterialSlotName);
			AddVFPerMaterial(MaterialIndex, EHairGeometryType::Strands, &FHairStrandsVertexFactory::StaticType);
		}

		// Cards
		if (IsHairStrandsEnabled(EHairStrandsShaderType::Cards, ShaderPlatform))
		{
			uint32 CardsLODIndex = 0;
			for (const FHairGroupData::FCards::FLOD& LOD : InGroupData.Cards.LODs)
			{
				if (LOD.IsValid())
				{
					// Material
					int32 MaterialIndex = INDEX_NONE;
					for (const FHairGroupsCardsSourceDescription& Desc : HairGroupsCards)
					{
						if (Desc.GroupIndex == GroupIt && Desc.LODIndex == CardsLODIndex)
						{
							MaterialIndex = GetMaterialIndex(Desc.MaterialSlotName);
							break;
						}
					}
					AddVFPerMaterial(MaterialIndex, EHairGeometryType::Cards, &FHairCardsVertexFactory::StaticType);
				}
				++CardsLODIndex;
			}
		}

		// Meshes
		{
			uint32 MeshesLODIndex = 0;
			for (const FHairGroupData::FMeshes::FLOD& LOD : InGroupData.Meshes.LODs)
			{
				if (LOD.IsValid())
				{
					// Material
					int32 MaterialIndex = INDEX_NONE;
					for (const FHairGroupsMeshesSourceDescription& Desc : HairGroupsMeshes)
					{
						if (Desc.GroupIndex == GroupIt && Desc.LODIndex == MeshesLODIndex)
						{
							MaterialIndex = GetMaterialIndex(Desc.MaterialSlotName);
							break;
						}
					}
					AddVFPerMaterial(MaterialIndex, EHairGeometryType::Meshes, &FHairCardsVertexFactory::StaticType);
				}
				++MeshesLODIndex;
			}
		}
	}

	return VFsPerMaterials;
}

enum EGroomAssetChangeType
{
	GroomChangeType_Interpolation = 1,
	GroomChangeType_Cards = 2,
	GroomChangeType_Meshes = 4,
	GroomChangeType_LOD = 8
};

inline void InternalUpdateResource(FRenderResource* Resource)
{
	if (Resource)
	{
		BeginUpdateResourceRHI(Resource);
	}
}

template<typename T>
inline void InternalReleaseResource(T*& Resource)
{
	if (Resource)
	{
		T* InResource = Resource;
		ENQUEUE_RENDER_COMMAND(ReleaseHairResourceCommand)(
			[InResource](FRHICommandList& RHICmdList)
			{
				InResource->ReleaseResource();
				delete InResource;
			});
		Resource = nullptr;
	}
}

#if WITH_EDITOR
void UGroomAsset::UpdateResource()
{
	LLM_SCOPE_BYTAG(Groom);

	uint32 AllChangeType = 0;
	for (uint32 GroupIndex = 0, GroupCount = GetNumHairGroups(); GroupIndex < GroupCount; ++GroupIndex)
	{
		uint32 ChangeType = (CachedHairGroupsLOD[GroupIndex] == HairGroupsLOD[GroupIndex] ? 0 : GroomChangeType_LOD);
		AllChangeType = AllChangeType | ChangeType;

		check(GroupIndex < uint32(GetNumHairGroups()));
		FHairGroupData& GroupData = HairGroupsData[GroupIndex];
		if (GroupData.Guides.RestResource)
		{
			InternalUpdateResource(GroupData.Guides.RestResource);
		}

		if (IsHairStrandsEnabled(EHairStrandsShaderType::Strands))
		{
			InternalUpdateResource(GroupData.Strands.RestResource);
			if (GroupData.Strands.InterpolationResource)
			{
				InternalUpdateResource(GroupData.Strands.InterpolationResource);
			}

			if ((ChangeType & GroomChangeType_LOD) && GroupData.Strands.HasValidData() && CanRebuildFromDescription())
			{
				const FHairDescriptionGroups& LocalHairDescriptionGroups = GetHairDescriptionGroups();

				// Force rebuilding the LOD data as the LOD settings has changed
				BuildHairGroupCluster(
					GroupIndex,
					LocalHairDescriptionGroups,
					HairGroupsInterpolation,
					HairGroupsLOD,
					HairGroupsInfo,
					HairGroupsData);

				GroupData.Strands.ClusterCullingResource = new FHairStrandsClusterCullingResource(GroupData.Strands.ClusterCullingBulkData, FHairResourceName(GetFName(), GroupIndex));
			}
			else
			{
				InternalUpdateResource(GroupData.Strands.ClusterCullingResource);
			}

			#if RHI_RAYTRACING
			if (IsHairRayTracingEnabled() && GroupData.Strands.RaytracingResource)
			{
				// Release raytracing resources, so that it will be recreate by the component.
				InternalReleaseResource(GroupData.Strands.RaytracingResource);
			}
			#endif
		}
	}

	if (BuildCardsGeometry())  { AllChangeType |= GroomChangeType_Cards;  }
	if (BuildMeshesGeometry()) { AllChangeType |= GroomChangeType_Meshes; }

	if (AllChangeType & (GroomChangeType_LOD | GroomChangeType_Cards | GroomChangeType_Meshes))
	{
		OnGroomAssetResourcesChanged.Broadcast();
	}

	UpdateHairGroupsInfo();
	UpdateCachedSettings();
}
#endif // #if WITH_EDITOR

////////////////////////////////////////////////////////////////////////////////////////////////////
// Initialize resources

void UGroomAsset::InitResources()
{
	LLM_SCOPE_BYTAG(Groom);

	InitGuideResources();
	InitStrandsResources();
	InitCardsResources();
	InitMeshesResources();

	bIsInitialized = true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Release resources

void UGroomAsset::ReleaseResource()
{
	bIsInitialized = false;
	for (uint32 GroupIndex = 0, GroupCount = GetNumHairGroups(); GroupIndex < GroupCount; ++GroupIndex)
	{
		ReleaseGuidesResource(GroupIndex);
		ReleaseStrandsResource(GroupIndex);
		ReleaseCardsResource(GroupIndex);
		ReleaseMeshesResource(GroupIndex);
	}
}

void UGroomAsset::ReleaseGuidesResource(uint32 GroupIndex)
{
	if (GroupIndex >= uint32(HairGroupsData.Num())) return;
	FHairGroupData& GroupData = HairGroupsData[GroupIndex];
	if (GroupData.Guides.IsValid())
	{
		InternalReleaseResource(GroupData.Guides.RestResource);
	}
}

void UGroomAsset::ReleaseStrandsResource(uint32 GroupIndex)
{
	if (GroupIndex >= uint32(HairGroupsData.Num())) return;
	FHairGroupData& GroupData = HairGroupsData[GroupIndex];
	if (GroupData.Strands.IsValid())
	{
		InternalReleaseResource(GroupData.Strands.RestResource);
		InternalReleaseResource(GroupData.Strands.ClusterCullingResource);
		InternalReleaseResource(GroupData.Strands.InterpolationResource);
		#if RHI_RAYTRACING
		InternalReleaseResource(GroupData.Strands.RaytracingResource);
		#endif
	}
}

void UGroomAsset::ReleaseCardsResource(uint32 GroupIndex)
{
	if (GroupIndex >= uint32(HairGroupsData.Num())) return;
	FHairGroupData& GroupData = HairGroupsData[GroupIndex];
	for (FHairGroupData::FCards::FLOD& LOD : GroupData.Cards.LODs)
	{
		InternalReleaseResource(LOD.RestResource);
		InternalReleaseResource(LOD.InterpolationResource);
		InternalReleaseResource(LOD.Guides.RestResource);
		InternalReleaseResource(LOD.Guides.InterpolationResource);
		#if RHI_RAYTRACING
		InternalReleaseResource(LOD.RaytracingResource);
		#endif
	}
}

void UGroomAsset::ReleaseMeshesResource(uint32 GroupIndex)
{
	if (GroupIndex >= uint32(HairGroupsData.Num())) return;
	FHairGroupData& GroupData = HairGroupsData[GroupIndex];
	for (FHairGroupData::FMeshes::FLOD& LOD : GroupData.Meshes.LODs)
	{
		InternalReleaseResource(LOD.RestResource);
		#if RHI_RAYTRACING
		InternalReleaseResource(LOD.RaytracingResource);
		#endif
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// 

void UGroomAsset::UpdateHairGroupsInfo()
{
	const uint32 GroupCount = GetNumHairGroups();
	const bool bForceReset = HairGroupsInfo.Num() != GroupCount;
	HairGroupsInfo.SetNum(GroupCount);

	uint32 GroupIndex = 0;
	for (FHairGroupData& Data: HairGroupsData)
	{
		FHairGroupInfoWithVisibility& Info = HairGroupsInfo[GroupIndex];
		Info.GroupID = GroupIndex;
		Info.NumCurves = Data.Strands.BulkData.GetNumCurves();
		Info.NumGuides = Data.Guides.BulkData.GetNumCurves();
		Info.NumCurveVertices = Data.Strands.BulkData.GetNumPoints();
		Info.NumGuideVertices = Data.Guides.BulkData.GetNumPoints();
		Info.MaxCurveLength = Data.Strands.BulkData.GetMaxLength();
		if (bForceReset)
		{
			Info.bIsVisible = true;
		}
		++GroupIndex;
	}
}

template<typename T>
bool ConvertMaterial(TArray<T>& Groups, TArray<FHairGroupsMaterial>& HairGroupsMaterials)
{
	bool bNeedSaving = false;
	for (T& Group : Groups)
	{
		if (Group.Material)
		{
			FName MaterialSlotName = Group.Material->GetFName();
			bool bFound = false;
			for (FHairGroupsMaterial& Material : HairGroupsMaterials)
			{
				if (Material.SlotName == MaterialSlotName)
				{
					bFound = true;
					Group.MaterialSlotName = MaterialSlotName;
					break;
				}
			}

			if (!bFound)
			{
				FHairGroupsMaterial& MaterialEntry = HairGroupsMaterials.AddDefaulted_GetRef();
				MaterialEntry.Material = Group.Material;
				MaterialEntry.SlotName = MaterialSlotName;
				Group.MaterialSlotName = MaterialSlotName;
			}
			Group.Material = nullptr;
			bNeedSaving = true;
		}
	}

	return bNeedSaving;
}

void UGroomAsset::PostLoad()
{
	LLM_SCOPE_BYTAG(Groom);

	Super::PostLoad();

	// Legacy asset are missing rendering or interpolation settings
#if WITH_EDITORONLY_DATA
	const bool bIsLegacyAsset = HairGroupsInterpolation.Num() == 0;
	if (bIsLegacyAsset)
	{
		if (HairDescriptionBulkData[HairDescriptionType])
		{
			const FHairDescriptionGroups& LocalHairDescriptionGroups = GetHairDescriptionGroups();

			const uint32 GroupCount = LocalHairDescriptionGroups.HairGroups.Num();
			SetNumGroup(GroupCount);
		}
		else
		{
			// Old format serialized the computed groom data directly, but are no longer supported
			UE_LOG(LogHairStrands, Error, TEXT("[Groom] The groom asset %s is too old. Please reimported the groom from its original source file."), *GetName());
			SetNumGroup(0, false);
		}
	}
#endif

#if WITH_EDITORONLY_DATA
	bool bSucceed = true;
	{
		// Interpolation settings are used for building the interpolation data, and per se defined the number of groups
		const uint32 GroupCount = HairGroupsInterpolation.Num();
		if (uint32(GetNumHairGroups()) != GroupCount)
		{
			SetNumGroup(GroupCount);
		}

		StrandsDerivedDataKey.SetNum(GroupCount);
		CardsDerivedDataKey.SetNum(GroupCount);
		MeshesDerivedDataKey.SetNum(GroupCount);

		// The async load is allowed only if all the groom derived data is already cached since parts of the build path need to run in the game thread
		bool bAsyncLoadEnabled = (GEnableGroomAsyncLoad > 0) && IsFullyCached();

		// Some contexts like cooking and commandlets are not conducive to async load
		bAsyncLoadEnabled = bAsyncLoadEnabled && !GIsCookerLoadingPackage && !IsRunningCommandlet() && !FApp::IsUnattended();
		if (bAsyncLoadEnabled)
		{
			GroomAssetStrongPtr = TStrongObjectPtr<UGroomAsset>(this); // keeps itself alive while completing the async load
			Async(EAsyncExecution::LargeThreadPool,
				[this]()
				{
					UE_LOG(LogHairStrands, Log, TEXT("[Groom] %s is fully cached. Loading it asynchronously."), *GetName());

					if (CacheDerivedDatas() && !bRetryLoadFromGameThread)
					{
						// Post-load completion code that must be executed in the game thread
						Async(EAsyncExecution::TaskGraphMainThread,
							[this]()
							{
								GroomAssetStrongPtr.Reset();

								InitResources();

								// This will update the GroomComponents that are using groom that was async loaded
								{
									FGroomComponentRecreateRenderStateContext RecreateContext(this);
								}

								OnGroomAsyncLoadFinished.Broadcast();
							});
					}
					else
					{
						UE_LOG(LogHairStrands, Log, TEXT("[Groom] %s failed to load asynchronously. Trying synchronous load."), *GetName());

						// Load might have failed because if failed to fetch the data from the DDC
						// Retry a sync load from the game thread
						Async(EAsyncExecution::TaskGraphMainThread,
							[this]()
							{
								GroomAssetStrongPtr.Reset();

								CacheDerivedDatas();

								OnGroomAsyncLoadFinished.Broadcast();
							});
					}
				});

			return;
		}
		else
		{
			bSucceed = UGroomAsset::CacheDerivedDatas();
		}
	}
#else
	bool bSucceed = false;
#endif

	check(GetNumHairGroups() > 0);

	// Build hair strands if needed
#if WITH_EDITORONLY_DATA
	if (!bSucceed && IsHairStrandsEnabled(EHairStrandsShaderType::Strands) && CanRebuildFromDescription())
	{
		const FHairDescriptionGroups& LocalHairDescriptionGroups = GetHairDescriptionGroups();

		const int32 GroupCount = HairGroupsInterpolation.Num();
		check(LocalHairDescriptionGroups.HairGroups.Num() == GroupCount);
		for (int32 GroupIndex = 0; GroupIndex < GroupCount; ++GroupIndex)
		{
			const bool bNeedInterpoldationData = NeedsInterpolationData(GroupIndex);
			const bool bNeedToBuildData = 
				(HairGroupsData[GroupIndex].Guides.BulkData.GetNumCurves() == 0 ||
				 (bNeedInterpoldationData && HairGroupsData[GroupIndex].Strands.InterpolationBulkData.GetPointCount() == 0)) &&
				 HairGroupsData[GroupIndex].Strands.BulkData.GetNumCurves() > 0; // Empty groom has no data to build
			if (bNeedToBuildData)
			{
				BuildHairGroup(
					GroupIndex,
					bNeedInterpoldationData,
					LocalHairDescriptionGroups,
					HairGroupsInterpolation,
					HairGroupsLOD,
					HairGroupsInfo,
					HairGroupsData);
			}
		}
	}
#endif

	// Convert all material data to new format
	{
		// Strands
		bool bNeedSaving = false;
		bNeedSaving = bNeedSaving || ConvertMaterial(HairGroupsRendering,	HairGroupsMaterials);
		bNeedSaving = bNeedSaving || ConvertMaterial(HairGroupsCards,		HairGroupsMaterials);
		bNeedSaving = bNeedSaving || ConvertMaterial(HairGroupsMeshes,		HairGroupsMaterials);

		if (bNeedSaving)
		{
			MarkPackageDirty();
		}
	}

	if (!IsTemplate() && IsHairStrandsAssetLoadingEnable())
	{
		InitResources();
	}

	check(AreGroupsValid());

	UpdateHairGroupsInfo();
#if WITH_EDITORONLY_DATA
	UpdateCachedSettings();
#endif // #if WITH_EDITORONLY_DATA

	if (IsResourcePSOPrecachingEnabled())
	{
		ERHIFeatureLevel::Type FeatureLevel = GetWorld() ? GetWorld()->FeatureLevel.GetValue() : GMaxRHIFeatureLevel;
		EShaderPlatform ShaderPlatform = GetFeatureLevelShaderPlatform(FeatureLevel);

		TArray<FHairVertexFactoryTypesPerMaterialData> VFsPerMaterials = CollectVertexFactoryTypesPerMaterialData(ShaderPlatform);

		FPSOPrecacheParams PrecachePSOParams;
		for (FHairVertexFactoryTypesPerMaterialData& VFsPerMaterial : VFsPerMaterials)
		{
			UMaterialInterface* MaterialInterface = HairGroupsMaterials[VFsPerMaterial.MaterialIndex].Material;
			if (MaterialInterface)
			{
				MaterialInterface->PrecachePSOs(VFsPerMaterial.VertexFactoryTypes, PrecachePSOParams);
			}
		}
	}
}

void UGroomAsset::PreSave(const class ITargetPlatform* TargetPlatform)
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS;
	Super::PreSave(TargetPlatform);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS;
}

void UGroomAsset::PreSave(FObjectPreSaveContext ObjectSaveContext)
{
#if WITH_EDITORONLY_DATA
	check(AreGroupsValid());

	const uint32 GroupCount = GetNumHairGroups();
	uint32 ChangeType = 0;

	for (uint32 GroupIt = 0; GroupIt < GroupCount; ++GroupIt)
	{
		if (!(CachedHairGroupsInterpolation[GroupIt] == HairGroupsInterpolation[GroupIt]))
		{
			ChangeType = ChangeType | GroomChangeType_Interpolation;
			break;
		}
	}

	for (uint32 GroupIt = 0; GroupIt < GroupCount; ++GroupIt)
	{
		if (!(CachedHairGroupsLOD[GroupIt] == HairGroupsLOD[GroupIt]))
		{
			ChangeType = ChangeType | GroomChangeType_LOD;
			break;
		}
	}

	if ((ChangeType & GroomChangeType_Interpolation) || (ChangeType & GroomChangeType_LOD))
	{
		if (CanRebuildFromDescription())
		{
			const FHairDescriptionGroups &LocalHairDescriptionGroups = GetHairDescriptionGroups();

			FGroomComponentRecreateRenderStateContext RecreateRenderContext(this);
			for (uint32 GroupIt = 0; GroupIt < GroupCount; ++GroupIt)
			{
				const bool bHasInterpolationChanged = !(CachedHairGroupsInterpolation[GroupIt] == HairGroupsInterpolation[GroupIt]);
				const bool bHasLODChanged = !(CachedHairGroupsLOD[GroupIt] == HairGroupsLOD[GroupIt]);
				const bool bNeedInterpoldationData = NeedsInterpolationData(GroupIt);

				if (bHasInterpolationChanged)
				{
					BuildHairGroup(
						GroupIt,
						bNeedInterpoldationData,
						LocalHairDescriptionGroups,
						HairGroupsInterpolation,
						HairGroupsLOD,
						HairGroupsInfo,
						HairGroupsData);
				}
				else if (bHasLODChanged)
				{
					BuildHairGroupCluster(
						GroupIt,
						LocalHairDescriptionGroups,
						HairGroupsInterpolation,
						HairGroupsLOD,
						HairGroupsInfo,
						HairGroupsData);
				}
			}
		}
		InitResources();
	}

	UpdateHairGroupsInfo();
	UpdateCachedSettings();
#endif
	Super::PreSave(ObjectSaveContext);
}

void UGroomAsset::BeginDestroy()
{
	ReleaseResource();
	Super::BeginDestroy();
}

#if WITH_EDITOR

static bool IsCardsTextureResources(const FName PropertyName)
{
	return
		   PropertyName == GET_MEMBER_NAME_CHECKED(FHairGroupCardsTextures, DepthTexture)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(FHairGroupCardsTextures, CoverageTexture)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(FHairGroupCardsTextures, TangentTexture)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(FHairGroupCardsTextures, AttributeTexture)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(FHairGroupCardsTextures, AuxilaryDataTexture)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(FHairGroupCardsTextures, MaterialTexture);
}
static void InitCardsTextureResources(UGroomAsset* GroomAsset);

static bool IsCardsProceduralAttributes(const FName PropertyName)
{	
	return
		   PropertyName == GET_MEMBER_NAME_CHECKED(FHairCardsClusterSettings, ClusterDecimation)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(FHairCardsClusterSettings, Type)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(FHairCardsClusterSettings, bUseGuide)

		|| PropertyName == GET_MEMBER_NAME_CHECKED(FHairCardsTextureSettings, AtlasMaxResolution)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(FHairCardsTextureSettings, PixelPerCentimeters)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(FHairCardsTextureSettings, LengthTextureCount)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(FHairCardsTextureSettings, DensityTextureCount)

		|| PropertyName == GET_MEMBER_NAME_CHECKED(FHairCardsGeometrySettings, GenerationType)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(FHairCardsGeometrySettings, CardsCount)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(FHairCardsGeometrySettings, ClusterType)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(FHairCardsGeometrySettings, MinSegmentLength)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(FHairCardsGeometrySettings, AngularThreshold)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(FHairCardsGeometrySettings, MinCardsLength)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(FHairCardsGeometrySettings, MaxCardsLength);
}

static bool IsStrandsInterpolationAttributes(const FName PropertyName)
{
	return
		   PropertyName == GET_MEMBER_NAME_CHECKED(FHairDecimationSettings, CurveDecimation)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(FHairDecimationSettings, VertexDecimation)

		|| PropertyName == GET_MEMBER_NAME_CHECKED(FHairDeformationSettings, NumCurves)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(FHairDeformationSettings, NumPoints)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(FHairDeformationSettings, bEnableRigging)

		// Add dependency on simulation and per LOD-simulation/global-interoplation to strip-out interoplation data if there are not needed
		|| PropertyName == GET_MEMBER_NAME_CHECKED(UGroomAsset, EnableGlobalInterpolation)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(FHairSolverSettings, EnableSimulation)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(FHairLODSettings, Simulation)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(FHairLODSettings, GlobalInterpolation)

		|| PropertyName == GET_MEMBER_NAME_CHECKED(FHairInterpolationSettings, bOverrideGuides)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(FHairInterpolationSettings, HairToGuideDensity)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(FHairInterpolationSettings, InterpolationQuality)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(FHairInterpolationSettings, InterpolationDistance)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(FHairInterpolationSettings, bRandomizeGuide)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(FHairInterpolationSettings, bUseUniqueGuide);
}

void UGroomAsset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	FProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	const FName PropertyName = PropertyThatChanged ? PropertyThatChanged->GetFName() : NAME_None;
	const bool bGeometryTypeChanged = PropertyName == GET_MEMBER_NAME_CHECKED(FHairLODSettings, GeometryType);
	if (bGeometryTypeChanged)
	{
		// If the asset didn't have any mesh or cards, we need to create/allocate the cards/mesh groups
		const uint32 GroupCount = HairGroupsData.Num();
		if (IsHairStrandsEnabled(EHairStrandsShaderType::Cards) && HasGeometryType(EGroomGeometryType::Cards) && HairGroupsCards.Num() == 0)
		{
			HairGroupsCards.Init(FHairGroupsCardsSourceDescription(), GroupCount);
			FHairGroupsCardsSourceDescription Dirty;
			Dirty.ProceduralSettings.ClusterSettings.ClusterDecimation = 0;
			Dirty.SourceType = HasImportedStrandsData() ? EHairCardsSourceType::Procedural : EHairCardsSourceType::Imported;
			CachedHairGroupsCards.Init(Dirty, GroupCount);
		}

		if (IsHairStrandsEnabled(EHairStrandsShaderType::Meshes) && HasGeometryType(EGroomGeometryType::Meshes) && HairGroupsMeshes.Num() == 0)
		{
			HairGroupsMeshes.Init(FHairGroupsMeshesSourceDescription(), GroupCount);
			FHairGroupsMeshesSourceDescription Dirty;
			Dirty.ImportedMesh = nullptr;
			CachedHairGroupsMeshes.Init(Dirty, GroupCount);
		}
	}

	// Rebuild the groom asset if some decimation attribute has changed
	const bool bStrandsInterpolationChanged = IsStrandsInterpolationAttributes(PropertyName);
	if (bStrandsInterpolationChanged)
	{
		CacheDerivedDatas();
	}
	
	const bool bCardsArrayChanged = PropertyName == GET_MEMBER_NAME_CHECKED(UGroomAsset, HairGroupsCards);
	if (bCardsArrayChanged && PropertyChangedEvent.ChangeType == EPropertyChangeType::ArrayAdd)
	{
		HairGroupsCards.Last().SourceType = HasImportedStrandsData() ? EHairCardsSourceType::Procedural : EHairCardsSourceType::Imported;
	}

	SavePendingProceduralAssets();

	const bool bHairStrandsRaytracingRadiusChanged = PropertyName == GET_MEMBER_NAME_CHECKED(FHairShadowSettings, HairRaytracingRadiusScale);
	
	// By pass update for all procedural cards parameters, as we don't want them to invalidate the cards data. 
	// Cards should be refresh only under user action
	// By pass update if bStrandsInterpolationChanged has the resources have already been recreated
	const bool bCardsToolUpdate = IsCardsProceduralAttributes(PropertyName);
	if (!bCardsToolUpdate && !bStrandsInterpolationChanged)
	{
		FGroomComponentRecreateRenderStateContext Context(this);
		UpdateResource();
	}

	// Special path for reloading the cards texture if needed as texture are not part of the cards DDC key, so the build is not retrigger 
	// when a user change the cards texture asset from the texture panel
	const bool bCardsResourcesUpdate = IsCardsTextureResources(PropertyName);
	if (bCardsResourcesUpdate)
	{
		FGroomComponentRecreateRenderStateContext Context(this);
		InitCardsTextureResources(this);
	}

	const bool bCardMaterialChanged = PropertyName == GET_MEMBER_NAME_CHECKED(FHairGroupsCardsSourceDescription, Material);
	const bool bMeshMaterialChanged = PropertyName == GET_MEMBER_NAME_CHECKED(FHairGroupsMeshesSourceDescription, Material);

	if (bStrandsInterpolationChanged || bGeometryTypeChanged || bCardMaterialChanged || bMeshMaterialChanged)
	{
		// Delegate used for notifying groom data & groom resoures invalidation
		OnGroomAssetResourcesChanged.Broadcast();
	}
	else if (!bCardsToolUpdate)
	{
		// Delegate used for notifying groom data invalidation
		OnGroomAssetChanged.Broadcast();
	}
}

void UGroomAsset::MarkMaterialsHasChanged()
{
	OnGroomAssetResourcesChanged.Broadcast();
}
#endif // WITH_EDITOR

#if WITH_EDITORONLY_DATA
void UGroomAsset::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	if (AssetImportData)
	{
		OutTags.Add(FAssetRegistryTag(SourceFileTagName(), AssetImportData->GetSourceData().ToJson(), FAssetRegistryTag::TT_Hidden));
	}

	Super::GetAssetRegistryTags(OutTags);
}

void UGroomAsset::PostInitProperties()
{
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		AssetImportData = NewObject<UAssetImportData>(this, TEXT("AssetImportData"));
	}

	Super::PostInitProperties();
}
#endif

int32 UGroomAsset::GetNumHairGroups() const
{
	return HairGroupsData.Num();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Array serialize
void InternalSerialize(FArchive& Ar, UObject* Owner, TArray<FHairGroupData::FMeshes::FLOD>& LODs)
{
	uint32 Count = LODs.Num();
	Ar << Count;
	if (Ar.IsLoading())
	{
		LODs.SetNum(Count);
	}
	for (uint32 MeshIt = 0; MeshIt < Count; ++MeshIt)
	{
		InternalSerialize(Ar, Owner, LODs[MeshIt]);
	}
}

void InternalSerialize(FArchive& Ar, UObject* Owner, TArray<FHairGroupData::FCards::FLOD>& LODs)
{
	uint32 Count = LODs.Num();
	Ar << Count;
	if (Ar.IsLoading())
	{
		LODs.SetNum(Count);
	}
	for (uint32 MeshIt = 0; MeshIt < Count; ++MeshIt)
	{
		InternalSerialize(Ar, Owner, LODs[MeshIt]);
	}
}

void InternalSerialize(FArchive& Ar, UObject* Owner, TArray<FHairGroupData>& GroupDatas)
{
	uint32 GroupCount = GroupDatas.Num();
	Ar << GroupCount;
	if (Ar.IsLoading())
	{
		GroupDatas.SetNum(GroupCount);
	}
	for (uint32 GroupIt = 0; GroupIt < GroupCount; ++GroupIt)
	{
		InternalSerialize(Ar, Owner, GroupDatas[GroupIt]);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Element serialize
static void InternalSerialize(FArchive& Ar, UObject* Owner, FHairGroupData::FCards::FLOD& CardLODData)
{
	if (!Ar.IsCooking() || !CardLODData.bIsCookedOut)
	{
		CardLODData.BulkData.Serialize(Ar);
		CardLODData.InterpolationBulkData.Serialize(Ar);

		CardLODData.Guides.BulkData.Serialize(Ar, Owner);
		CardLODData.Guides.InterpolationBulkData.Serialize(Ar, Owner);
	}
	else
	{
		// LOD has been marked to be cooked out so serialize empty data
		FHairCardsBulkData NoCardsBulkData;
		NoCardsBulkData.Serialize(Ar);

		FHairCardsInterpolationBulkData NoInterpolationBulkData;
		NoInterpolationBulkData.Serialize(Ar);

		FHairGroupData::FBaseWithInterpolation NoGuideData;
		NoGuideData.BulkData.Serialize(Ar, Owner);
		NoGuideData.InterpolationBulkData.Serialize(Ar, Owner);
	}
}

static void InternalSerialize(FArchive& Ar, UObject* Owner, FHairGroupData::FMeshes::FLOD& MeshLODData)
{
	if (!Ar.IsCooking() || !MeshLODData.bIsCookedOut)
	{
		MeshLODData.BulkData.Serialize(Ar);
	}
	else
	{
		// LOD has been marked to be cooked out so serialize empty data
		FHairMeshesBulkData NoMeshesBulkData;
		NoMeshesBulkData.Serialize(Ar);
	}
}

static void InternalSerialize(FArchive& Ar, UObject* Owner, FHairGroupData& GroupData)
{
	Ar.UsingCustomVersion(FAnimObjectVersion::GUID);

	FHairGroupData NoStrandsData;
	if (!Ar.IsCooking() || !GroupData.bIsCookedOut)
	{
		GroupData.Strands.BulkData.Serialize(Ar, Owner);
		GroupData.Guides.BulkData.Serialize(Ar, Owner);
		GroupData.Strands.InterpolationBulkData.Serialize(Ar, Owner);

	}
	else
	{
		// Fall back to no data, but still serialize guide data as they an be used for cards simulation
		// Theoritically, we should have something to detect if we are going to use or not guide (for 
		// simulation or RBF deformation) on the target platform
		NoStrandsData.Strands.BulkData.Serialize(Ar, Owner);
		GroupData.Guides.BulkData.Serialize(Ar, Owner);
		NoStrandsData.Strands.InterpolationBulkData.Serialize(Ar, Owner);
	}

	if (Ar.CustomVer(FAnimObjectVersion::GUID) >= FAnimObjectVersion::SerializeHairClusterCullingData)
	{
		if (!Ar.IsCooking() || !GroupData.bIsCookedOut)
		{
			GroupData.Strands.ClusterCullingBulkData.Serialize(Ar, Owner);
		}
		else
		{
			NoStrandsData.Strands.ClusterCullingBulkData.Serialize(Ar, Owner);
		}

		if (Ar.CustomVer(FAnimObjectVersion::GUID) >= FAnimObjectVersion::SerializeGroomCardsAndMeshes)
		{
			bool bIsCooked = Ar.IsCooking();
			Ar << bIsCooked;

			if (bIsCooked)
			{
				InternalSerialize(Ar, Owner, GroupData.Cards.LODs);
				InternalSerialize(Ar, Owner, GroupData.Meshes.LODs);
			}
		}
	}
}

void UGroomAsset::AddAssetUserData(UAssetUserData* InUserData)
{
	if (InUserData != nullptr)
	{
		UAssetUserData* ExistingData = GetAssetUserDataOfClass(InUserData->GetClass());
		if (ExistingData != nullptr)
		{
			AssetUserData.Remove(ExistingData);
		}
		AssetUserData.Add(InUserData);
	}
}

UAssetUserData* UGroomAsset::GetAssetUserDataOfClass(TSubclassOf<UAssetUserData> InUserDataClass)
{
	for (int32 DataIdx = 0; DataIdx < AssetUserData.Num(); DataIdx++)
	{
		UAssetUserData* Datum = AssetUserData[DataIdx];
		if (Datum != nullptr && Datum->IsA(InUserDataClass))
		{
			return Datum;
		}
	}
	return NULL;
}

void UGroomAsset::RemoveUserDataOfClass(TSubclassOf<UAssetUserData> InUserDataClass)
{
	for (int32 DataIdx = 0; DataIdx < AssetUserData.Num(); DataIdx++)
	{
		UAssetUserData* Datum = AssetUserData[DataIdx];
		if (Datum != nullptr && Datum->IsA(InUserDataClass))
		{
			AssetUserData.RemoveAt(DataIdx);
			return;
		}
	}
}

const TArray<UAssetUserData*>* UGroomAsset::GetAssetUserDataArray() const
{
	return &ToRawPtrTArrayUnsafe(AssetUserData);
}

bool UGroomAsset::CanRebuildFromDescription() const
{
#if WITH_EDITORONLY_DATA
	return HairDescriptionBulkData[HairDescriptionType].IsValid() && !HairDescriptionBulkData[HairDescriptionType]->IsEmpty();
#else
	return false;
#endif
}

bool UGroomAsset::AreGroupsValid() const
{
	const uint32 GroupCount = HairGroupsInterpolation.Num();
	return
		GroupCount > 0 &&
		HairGroupsData.Num() == GroupCount &&
		HairGroupsPhysics.Num() == GroupCount &&
		HairGroupsRendering.Num() == GroupCount &&
		HairGroupsLOD.Num() == GroupCount;
}

void UGroomAsset::SetNumGroup(uint32 InGroupCount, bool bResetGroupData, bool bResetOtherData)
{
	ReleaseResource();
	if (bResetGroupData)
	{
		HairGroupsData.Reset();
	}

	// In order to preserve the existing asset settings, the settings are only reset if the group count has changed.
	if (InGroupCount != HairGroupsData.Num())
	{
		HairGroupsData.Init(FHairGroupData(), InGroupCount);
	}

	if (InGroupCount != HairGroupsPhysics.Num())
	{
		if (bResetOtherData)
		{
			HairGroupsPhysics.Init(FHairGroupsPhysics(), InGroupCount);
		}
		else
		{
			HairGroupsPhysics.SetNum(InGroupCount);
		}
	}

	if (InGroupCount != HairGroupsRendering.Num())
	{
		if (bResetOtherData)
		{
			HairGroupsRendering.Init(FHairGroupsRendering(), InGroupCount);
		}
		else
		{
			HairGroupsRendering.SetNum(InGroupCount);
		}
	}

	if (InGroupCount != HairGroupsInterpolation.Num())
	{
		if (bResetOtherData)
		{
			HairGroupsInterpolation.Init(FHairGroupsInterpolation(), InGroupCount);
		}
		else
		{
			HairGroupsInterpolation.SetNum(InGroupCount);
		}
	}

	if (InGroupCount != HairGroupsLOD.Num())
	{
		if (bResetOtherData)
		{
			HairGroupsLOD.Init(FHairGroupsLOD(), InGroupCount);
		}
		else
		{
			HairGroupsLOD.SetNum(InGroupCount);
		}

		// Insure that each group has at least one LOD
		for (FHairGroupsLOD& GroupLOD : HairGroupsLOD)
		{
			if (GroupLOD.LODs.IsEmpty())
			{
				FHairLODSettings& S = GroupLOD.LODs.AddDefaulted_GetRef();
				S.ScreenSize = 1;
				S.CurveDecimation = 1;
			}
		}
	}

	if (InGroupCount != HairGroupsInfo.Num())
	{
		if (bResetOtherData)
		{
			HairGroupsInfo.Init(FHairGroupInfoWithVisibility(), InGroupCount);
		}
		else
		{
			HairGroupsInfo.SetNum(InGroupCount);
		}
	}

	// The following groups remain unchanged:
	// * HairGroupsCards
	// * HairGroupsMeshes
	// * HairGroupsMaterials

	EffectiveLODBias.SetNum(InGroupCount);

#if WITH_EDITORONLY_DATA
	StrandsDerivedDataKey.SetNum(InGroupCount);
	CardsDerivedDataKey.SetNum(InGroupCount);
	MeshesDerivedDataKey.SetNum(InGroupCount);
#endif
}

void UGroomAsset::SetStableRasterization(bool bEnable)
{
	for (FHairGroupsRendering& Group : HairGroupsRendering)
	{
		Group.AdvancedSettings.bUseStableRasterization = bEnable;
	}
}

void UGroomAsset::SetScatterSceneLighting(bool bEnable)
{
	for (FHairGroupsRendering& Group : HairGroupsRendering)
	{
		Group.AdvancedSettings.bScatterSceneLighting = bEnable;
	}
}

void UGroomAsset::SetHairWidth(float Width)
{
	for (FHairGroupsRendering& Group : HairGroupsRendering)
	{
		Group.GeometrySettings.HairWidth = Width;
	}
}

// If groom derived data needs to be rebuilt (new format, serialization
// differences, etc.) replace the version GUID below with a new one.
// In case of merge conflicts with DDC versions, you MUST generate a new GUID
// and set this new GUID as the version.
#define GROOM_DERIVED_DATA_VERSION TEXT("13E1F608CE4F4343816324E3AC11C04B")

#if WITH_EDITORONLY_DATA

namespace GroomDerivedDataCacheUtils
{
	const FString& GetGroomDerivedDataVersion()
	{
		static FString CachedVersionString(GROOM_DERIVED_DATA_VERSION);
		return CachedVersionString;
	}

	FString BuildGroomDerivedDataKey(const FString& KeySuffix)
	{
		return FDerivedDataCacheInterface::BuildCacheKey(*(TEXT("GROOM_V") + FGroomBuilder::GetVersion() + TEXT("_")), *GetGroomDerivedDataVersion(), *KeySuffix);
	}

	void SerializeHairInterpolationSettingsForDDC(FArchive& Ar, uint32 GroupIndex, FHairGroupsInterpolation& InterpolationSettings, FHairGroupsLOD& LODSettings, bool bRequireInterpolationData)
	{
		// Note: this serializer is only used to build the groom DDC key, no versioning is required
		Ar << GroupIndex;
		Ar << bRequireInterpolationData;

		InterpolationSettings.BuildDDCKey(Ar);
		LODSettings.BuildDDCKey(Ar);
	}

	// Geneate DDC key for an given hair group and a given LOD
	FString BuildCardsDerivedDataKeySuffix(const UGroomAsset* GroomAsset, FHairGroupsCardsSourceDescription& Desc)
	{
		if (Desc.GroupIndex < 0 || Desc.LODIndex < 0)
		{
			return FString();
		}

		if (Desc.GroupIndex >= GroomAsset->HairGroupsData.Num() || Desc.LODIndex >= GroomAsset->HairGroupsData[Desc.GroupIndex].Cards.LODs.Num())
		{
			return FString();
		}

		// Serialize the FHairGroupsCardsSourceDescription into a temporary array
		// The archive is flagged as persistent so that machines of different endianness produce identical binary results.
		TArray<uint8> TempBytes;
		TempBytes.Reserve(512);
		FMemoryWriter Ar(TempBytes, true);

		Ar << Desc.GroupIndex;
		Ar << Desc.LODIndex;
		if (Desc.SourceType == EHairCardsSourceType::Imported)
		{
			if (Desc.ImportedMesh)
			{
				FString Key = Desc.GetMeshKey();
				Ar << Key;
			}
		}
		else if (Desc.SourceType == EHairCardsSourceType::Procedural)
		{
			if (Desc.ProceduralMesh)
			{
				FString Key = Desc.GetMeshKey();
				Ar << Key;
			}
			Desc.ProceduralSettings.BuildDDCKey(Ar);

			if (Desc.GenerationSettings)
			{
				Desc.GenerationSettings->BuildDDCKey(Ar);
			}
		}

		FSHAHash Hash;
		FSHA1::HashBuffer(TempBytes.GetData(), TempBytes.Num(), Hash.Hash);

		static FString CardPrefixString(TEXT("CARDS_V") + FHairCardsBuilder::GetVersion() + TEXT("_"));
		return CardPrefixString + Hash.ToString();
	}

	// Geneate DDC key for an entire hair group (i.e. with all its LODs) for cards data
	FString BuildCardsDerivedDataKeySuffix(uint32 GroupIndex, const TArray<FHairLODSettings>& LODs, TArray<FHairGroupsCardsSourceDescription>& SourceDescriptions)
	{
		// Serialize the FHairGroupsCardsSourceDescription into a temporary array
		// The archive is flagged as persistent so that machines of different endianness produce identical binary results.
		TArray<uint8> TempBytes;
		TempBytes.Reserve(512);
		FMemoryWriter Ar(TempBytes, true);

		Ar << GroupIndex;
		for (int32 Index = 0; Index < SourceDescriptions.Num(); ++Index)
		{
			FHairGroupsCardsSourceDescription& Desc = SourceDescriptions[Index];
			if (Desc.GroupIndex != GroupIndex || Desc.LODIndex < 0)
			{
				continue;
			}

			// Also need to cross-check the LOD settings; there might not be a LOD settings that matches the LODIndex
			// in which case, no cards data is actually built for the LODIndex
			if (!LODs.IsValidIndex(Desc.LODIndex))
			{
				continue;
			}

			Ar << Desc.LODIndex;
			if (Desc.SourceType == EHairCardsSourceType::Imported)
			{
				if (Desc.ImportedMesh)
				{
					FString Key = Desc.GetMeshKey();
					Ar << Key;
				}
			}
			else if (Desc.SourceType == EHairCardsSourceType::Procedural)
			{
				if (Desc.ProceduralMesh)
				{
					FString Key = Desc.GetMeshKey();
					Ar << Key;
				}
				Desc.ProceduralSettings.BuildDDCKey(Ar);
			}
			// Material is not included as it doesn't affect the data building
		}

		FSHAHash Hash;
		FSHA1::HashBuffer(TempBytes.GetData(), TempBytes.Num(), Hash.Hash);

		static FString CardPrefixString(TEXT("CARDS_V") + FHairCardsBuilder::GetVersion() + TEXT("_"));
		return CardPrefixString + Hash.ToString();
	}

	FString BuildMeshesDerivedDataKeySuffix(uint32 GroupIndex, const TArray<FHairLODSettings>& LODs, TArray<FHairGroupsMeshesSourceDescription>& SourceDescriptions)
	{
		// Serialize the FHairGroupsMeshesSourceDescription into a temporary array
		// The archive is flagged as persistent so that machines of different endianness produce identical binary results.
		TArray<uint8> TempBytes;
		TempBytes.Reserve(512);
		FMemoryWriter Ar(TempBytes, true);

		Ar << GroupIndex;
		for (int32 Index = 0; Index < SourceDescriptions.Num(); ++Index)
		{
			FHairGroupsMeshesSourceDescription& Desc = SourceDescriptions[Index];
			if (Desc.GroupIndex != GroupIndex || Desc.LODIndex < 0)
			{
				continue;
			}

			// Also need to cross-check the LOD settings; there might not be a LOD settings that matches the LODIndex
			// in which case, no mesh data is actually built for the LODIndex
			if (!LODs.IsValidIndex(Desc.LODIndex) || LODs[Desc.LODIndex].GeometryType != EGroomGeometryType::Meshes)
			{
				continue;
			}

			Ar << Desc.LODIndex;
			if (Desc.ImportedMesh)
			{
				FString Key = Desc.GetMeshKey();
				Ar << Key;
			}
			// Material is not included as it doesn't affect the data building
		}

		FSHAHash Hash;
		FSHA1::HashBuffer(TempBytes.GetData(), TempBytes.Num(), Hash.Hash);

		static FString MeshPrefixString(TEXT("MESHES_V") + FHairMeshesBuilder::GetVersion() + TEXT("_"));
		return MeshPrefixString + Hash.ToString();
	}
}

bool UGroomAsset::IsFullyCached()
{
	// Check if all the groom derived data for strands, cards and meshes are already stored in the DDC
	TArray<FString> CacheKeys;
	const uint32 GroupCount = HairGroupsInterpolation.Num();
	for (uint32 GroupIndex = 0; GroupIndex < GroupCount; ++GroupIndex)
	{
		const FString StrandsDDCKey = UGroomAsset::GetDerivedDataKeyForStrands(GroupIndex);
		CacheKeys.Add(StrandsDDCKey);

		// Some cards and meshes LOD settings may not produce any derived data so those must be excluded
		bool bHasCardsLODs = false;
		bool bHasMeshesLODs = false;
		const uint32 LODCount = HairGroupsLOD[GroupIndex].LODs.Num();
		for (uint32 LODIt = 0; LODIt < LODCount; ++LODIt)
		{
			int32 SourceIt = 0;
			// GetSourceDescription will cross-check the LOD settings with the cards/meshes settings to see if they would produce any data
			if (const FHairGroupsCardsSourceDescription* Desc = GetSourceDescription(HairGroupsCards, GroupIndex, LODIt, SourceIt))
			{
				Desc->HasMeshChanged(); // this query will trigger a load of the mesh dependency, which has to be done in the game thread
				bHasCardsLODs = true;
			}
			if (const FHairGroupsMeshesSourceDescription* Desc = GetSourceDescription(HairGroupsMeshes, GroupIndex, LODIt, SourceIt))
			{
				Desc->HasMeshChanged(); // this query will trigger a load of the mesh dependency, which has to be done in the game thread
				bHasMeshesLODs = true;
			}
		}

		if (bHasCardsLODs)
		{
			const FString CardsKeySuffix = GroomDerivedDataCacheUtils::BuildCardsDerivedDataKeySuffix(GroupIndex, HairGroupsLOD[GroupIndex].LODs, HairGroupsCards);
			const FString CardsDDCKey = StrandsDDCKey + CardsKeySuffix;
			CacheKeys.Add(CardsDDCKey);
		}

		if (bHasMeshesLODs)
		{
			const FString MeshesKeySuffix = GroomDerivedDataCacheUtils::BuildMeshesDerivedDataKeySuffix(GroupIndex, HairGroupsLOD[GroupIndex].LODs, HairGroupsMeshes);
			const FString MeshesDDCKey = GroomDerivedDataCacheUtils::BuildGroomDerivedDataKey(MeshesKeySuffix);
			CacheKeys.Add(MeshesDDCKey);
		}
	}

	return GetDerivedDataCacheRef().AllCachedDataProbablyExists(CacheKeys);
}

FString UGroomAsset::BuildDerivedDataKeySuffix(uint32 GroupIndex, const FHairGroupsInterpolation& InterpolationSettings, const FHairGroupsLOD& LODSettings) const
{
	// Serialize the build settings into a temporary array
	// The archive is flagged as persistent so that machines of different endianness produce identical binary results.
	TArray<uint8> TempBytes;
	TempBytes.Reserve(64);
	FMemoryWriter Ar(TempBytes, /*bIsPersistent=*/ true);
	
	// If simulation or global interpolation is enabled, then interoplation data are required. Otherwise they can be skipped
	const bool bNeedInterpolationData = NeedsInterpolationData(GroupIndex);

	GroomDerivedDataCacheUtils::SerializeHairInterpolationSettingsForDDC(Ar, GroupIndex, const_cast<FHairGroupsInterpolation&>(InterpolationSettings), const_cast<FHairGroupsLOD&>(LODSettings), bNeedInterpolationData);

	FString KeySuffix;
	if (HairDescriptionBulkData[HairDescriptionType])
	{
		// Reserve twice the size of TempBytes because of ByteToHex below + 3 for "ID" and \0
		KeySuffix.Reserve(HairDescriptionBulkData[HairDescriptionType]->GetIdString().Len() + TempBytes.Num() * 2 + 3);
		KeySuffix += TEXT("ID");
		KeySuffix += HairDescriptionBulkData[HairDescriptionType]->GetIdString();
	}
	else
	{
		KeySuffix.Reserve(TempBytes.Num() * 2 + 1);
	}

	// Now convert the raw bytes to a string
	const uint8* SettingsAsBytes = TempBytes.GetData();
	for (int32 ByteIndex = 0; ByteIndex < TempBytes.Num(); ++ByteIndex)
	{
		ByteToHex(SettingsAsBytes[ByteIndex], KeySuffix);
	}

	return KeySuffix;
}

FString UGroomAsset::GetDerivedDataKey()
{
	FString Key;
	const uint32 GroupCount = GetNumHairGroups();
	for (uint32 GroupIt = 0; GroupIt < GroupCount; ++GroupIt)
	{
		FString StrandsKey = GetDerivedDataKeyForStrands(GroupIt);
		Key += StrandsKey;
		Key += GetDerivedDataKeyForCards(GroupIt, StrandsKey);
		Key += GetDerivedDataKeyForMeshes(GroupIt);
	}
	return Key;
}

FString UGroomAsset::GetDerivedDataKeyForCards(uint32 GroupIndex, const FString& StrandsKey)
{
	const FString KeySuffix = GroomDerivedDataCacheUtils::BuildCardsDerivedDataKeySuffix(GroupIndex, HairGroupsLOD[GroupIndex].LODs, HairGroupsCards);
	const FString DerivedDataKey = StrandsKey + KeySuffix;
	return DerivedDataKey;
}

FString UGroomAsset::GetDerivedDataKeyForStrands(uint32 GroupIndex)
{
	const FHairGroupsInterpolation& InterpolationSettings = HairGroupsInterpolation[GroupIndex];
	const FHairGroupsLOD& LODSettings = HairGroupsLOD[GroupIndex];

	const FString KeySuffix = BuildDerivedDataKeySuffix(GroupIndex, InterpolationSettings, LODSettings);
	const FString DerivedDataKey = GroomDerivedDataCacheUtils::BuildGroomDerivedDataKey(KeySuffix);

	return DerivedDataKey;
}

FString UGroomAsset::GetDerivedDataKeyForMeshes(uint32 GroupIndex)
{
	const FString KeySuffix = GroomDerivedDataCacheUtils::BuildMeshesDerivedDataKeySuffix(GroupIndex, HairGroupsLOD[GroupIndex].LODs, HairGroupsMeshes);
	const FString DerivedDataKey = GroomDerivedDataCacheUtils::BuildGroomDerivedDataKey(KeySuffix);
	return DerivedDataKey;
}


void UGroomAsset::CommitHairDescription(FHairDescription&& InHairDescription, EHairDescriptionType Type)
{
	HairDescriptionType = Type;

	CachedHairDescription[HairDescriptionType] = MakeUnique<FHairDescription>(InHairDescription);

	if (!HairDescriptionBulkData[HairDescriptionType])
	{
		HairDescriptionBulkData[HairDescriptionType] = MakeUnique<FHairDescriptionBulkData>();
	}

	// Update the cached hair description groups with the new hair description data
	CachedHairDescriptionGroups[HairDescriptionType] = MakeUnique<FHairDescriptionGroups>();
	FGroomBuilder::BuildHairDescriptionGroups(*CachedHairDescription[HairDescriptionType], *CachedHairDescriptionGroups[HairDescriptionType]);

	HairDescriptionBulkData[HairDescriptionType]->SaveHairDescription(*CachedHairDescription[HairDescriptionType]);
}

const FHairDescriptionGroups& UGroomAsset::GetHairDescriptionGroups()
{
	check(HairDescriptionBulkData[HairDescriptionType]);

	if (!CachedHairDescription[HairDescriptionType])
	{
		CachedHairDescription[HairDescriptionType] = MakeUnique<FHairDescription>();
		HairDescriptionBulkData[HairDescriptionType]->LoadHairDescription(*CachedHairDescription[HairDescriptionType]);
	}
	if (!CachedHairDescriptionGroups[HairDescriptionType])
	{
		CachedHairDescriptionGroups[HairDescriptionType] = MakeUnique<FHairDescriptionGroups>();
		FGroomBuilder::BuildHairDescriptionGroups(*CachedHairDescription[HairDescriptionType], *CachedHairDescriptionGroups[HairDescriptionType]);
	}

	return *CachedHairDescriptionGroups[HairDescriptionType];
}

FHairDescription UGroomAsset::GetHairDescription() const
{
	FHairDescription OutHairDescription;
	if (HairDescriptionBulkData[HairDescriptionType])
	{
		HairDescriptionBulkData[HairDescriptionType]->LoadHairDescription(OutHairDescription);
	}
	return MoveTemp(OutHairDescription);
}

bool UGroomAsset::CacheDerivedDatas()
{
	bRetryLoadFromGameThread = false;

	// Delete existing resources from GroomComponent and recreate them when the FGroomComponentRecreateRenderStateContext's destructor is called
	// These resources are recreated only when running from the game thread (non-async loading path)
	const bool bIsGameThread = IsInGameThread();
	FGroomComponentRecreateRenderStateContext RecreateContext(bIsGameThread ? this : nullptr);

	const uint32 GroupCount = HairGroupsInterpolation.Num();
	for (uint32 GroupIndex = 0; GroupIndex < GroupCount; ++GroupIndex)
	{
		bool bSucceed = CacheDerivedData(GroupIndex);
		if (!bSucceed)
			return false;
	}
	UpdateCachedSettings();
	UpdateHairGroupsInfo();

	if (bIsGameThread)
	{
		ReleaseResource();
		InitResources();
	}
	return true;
}

bool UGroomAsset::CacheDerivedData(uint32 GroupIndex)
{
	// Check if the asset correctly initialized prior building
	if (!AreGroupsValid())
	{
		return false;
	}

	const uint32 GroupCount = HairGroupsInterpolation.Num();
	check(GroupIndex < GroupCount);
	if (GroupIndex >= GroupCount)
	{
		return false;
	}

	FString DerivedDataKey;
	bool bSuccess = false;

	// Cache each geometric representation separately
	bSuccess |= CacheStrandsData(GroupIndex, DerivedDataKey);
	bSuccess |= CacheCardsGeometry(GroupIndex, DerivedDataKey);
	bSuccess |= CacheMeshesGeometry(GroupIndex);

	return bSuccess;
}

bool UGroomAsset::CacheStrandsData(uint32 GroupIndex, FString& OutDerivedDataKey)
{
	if (!HairDescriptionBulkData[HairDescriptionType])
	{
		UE_LOG(LogHairStrands, Error, TEXT("[Groom] The groom asset %s is too old. Please reimported the groom from its original source file."), *GetName());
		return false;
	}

	const FString DerivedDataKey = UGroomAsset::GetDerivedDataKeyForStrands(GroupIndex);

	bool bSuccess = true;
	TArray<uint8> DerivedData;
	if (GetDerivedDataCacheRef().GetSynchronous(*DerivedDataKey, DerivedData, GetPathName()))
	{
		UE_CLOG(IsHairStrandsDDCLogEnable(), LogHairStrands, Log, TEXT("[Groom/DDC] Strands - Found (Groom:%s Group6:%d)."), *GetName(), GroupIndex);

		FMemoryReader Ar(DerivedData, /*bIsPersistent=*/ true);

		int64 UncompressedSize = 0;
		Ar << UncompressedSize;

		uint8* DecompressionBuffer = reinterpret_cast<uint8*>(FMemory::Malloc(UncompressedSize));
		Ar.SerializeCompressed(DecompressionBuffer, UncompressedSize, NAME_Zlib);

		FHairGroupData& HairGroupData = HairGroupsData[GroupIndex];

		// Reset hair group data to ensure previously loaded bulk data are cleared/cleaned priori to load new data.
		HairGroupData = FHairGroupData();

		FLargeMemoryReader LargeMemReader(DecompressionBuffer, UncompressedSize, ELargeMemoryReaderFlags::Persistent | ELargeMemoryReaderFlags::TakeOwnership);
		InternalSerialize(LargeMemReader, this, HairGroupData);
	}
	else
	{
		if (!IsInGameThread())
		{
			// Strands build might actually be thread safe, but retry on the game thread to be safe
			bRetryLoadFromGameThread = true;
			return false;
		}

		UE_CLOG(IsHairStrandsDDCLogEnable(), LogHairStrands, Log, TEXT("[Groom/DDC] Strands - Not found (Groom:%s Group:%d)."), *GetName(), GroupIndex);

		const FHairDescriptionGroups& LocalHairDescriptionGroups = GetHairDescriptionGroups();

		if (!LocalHairDescriptionGroups.IsValid())
		{
			UE_LOG(LogHairStrands, Warning, TEXT("[Groom] The groom asset %s does not have valid hair groups."), *GetName());
			return false;
		}

		const bool bNeedInterpolationData = NeedsInterpolationData(GroupIndex);

		// Build groom data with the new build settings
		bSuccess = BuildHairGroup(
			GroupIndex,
			bNeedInterpolationData,
			LocalHairDescriptionGroups,
			HairGroupsInterpolation,
			HairGroupsLOD,
			HairGroupsInfo,
			HairGroupsData);
		
		if (bSuccess)
		{
			// Using a LargeMemoryWriter for serialization since the data can be bigger than 2 GB
			FLargeMemoryWriter LargeMemWriter(0, /*bIsPersistent=*/ true);
			InternalSerialize(LargeMemWriter, this, HairGroupsData[GroupIndex]);
			int64 UncompressedSize = LargeMemWriter.TotalSize();

			// Then the content of the LargeMemWriter is compressed into a MemoryWriter
			// Compression ratio can reach about 5:2 depending on the data
			// Since the DDC doesn't support data bigger than 2 GB
			// we can compute a size threshold to skip the caching when
			// the uncompressed size exceeds the threshold
			static constexpr const int64 SizeThreshold = (int64)MAX_int32 * 2.5;
			const bool bIsCacheable = UncompressedSize < SizeThreshold;
			if (bIsCacheable)
			{
				FMemoryWriter CompressedArchive(DerivedData, true);

				CompressedArchive << UncompressedSize; // needed for allocating decompression buffer
				CompressedArchive.SerializeCompressed(LargeMemWriter.GetData(), UncompressedSize, NAME_Zlib);

				GetDerivedDataCacheRef().Put(*DerivedDataKey, DerivedData, GetPathName());
			}
			else
			{
				UE_LOG(LogHairStrands, Error, TEXT("[Groom/DDC] Strands - The groom asset is too large to be cached into the Derived Data Cache. (Groom:%s, Group:%d, Size %d bytes, DDC Limit: SizeThreshold %d bytes)) ."), *GetName(), GroupIndex, UncompressedSize, SizeThreshold);
				return false;
			}
		}
	}

	OutDerivedDataKey = DerivedDataKey;
	StrandsDerivedDataKey[GroupIndex] = DerivedDataKey;

	return bSuccess;
}

bool UGroomAsset::HasValidMeshesData(uint32 GroupIndex) const
{
	const FHairGroupData& HairGroupData = HairGroupsData[GroupIndex];
	const FHairGroupsLOD& GroupsLOD = HairGroupsLOD[GroupIndex];
	for (int32 LODIt = 0; LODIt < GroupsLOD.LODs.Num(); ++LODIt)
	{
		if (GroupsLOD.LODs[LODIt].GeometryType == EGroomGeometryType::Meshes)
		{
			int32 SourceIt = 0;
			if (const FHairGroupsMeshesSourceDescription* Desc = GetSourceDescription(HairGroupsMeshes, GroupIndex, LODIt, SourceIt))
			{
				if (Desc->ImportedMesh)
				{
					return true;
				}
			}
		}
	}
	return false;
}

bool UGroomAsset::HasValidCardsData(uint32 GroupIndex) const
{
	const FHairGroupData& HairGroupData = HairGroupsData[GroupIndex];
	const FHairGroupsLOD& GroupsLOD = HairGroupsLOD[GroupIndex];
	for (int32 LODIt = 0; LODIt < GroupsLOD.LODs.Num(); ++LODIt)
	{
		// Note: We don't condition/filter resource allocation based on GeometryType == EGroomGeometryType::Cards, because 
		// geometry type can be switched at runtime (between cards/strands)
		{
			int32 SourceIt = 0;
			if (const FHairGroupsCardsSourceDescription* Desc = GetSourceDescription(HairGroupsCards, GroupIndex, LODIt, SourceIt))
			{
				if (Desc->GetMesh())
				{
					return true;
				}
			}
		}
	}
	return false;
}

// Return true if the meshes data have changed
bool UGroomAsset::CacheCardsGeometry(uint32 GroupIndex, const FString& StrandsKey)
{
	const bool bHasValidCardsData = HasValidCardsData(GroupIndex);
	if (!bHasValidCardsData)
	{
		const bool bNeedReset = !CardsDerivedDataKey[GroupIndex].IsEmpty();
		CardsDerivedDataKey[GroupIndex].Empty();
		return bNeedReset;
	}

	const FString KeySuffix = GroomDerivedDataCacheUtils::BuildCardsDerivedDataKeySuffix(GroupIndex, HairGroupsLOD[GroupIndex].LODs, HairGroupsCards);
	const FString DerivedDataKey = StrandsKey + KeySuffix;

	if (CardsDerivedDataKey[GroupIndex] == DerivedDataKey)
	{
		// Skip if no change
		return false;
	}

	FHairGroupData& HairGroupData = HairGroupsData[GroupIndex];

	// We should never query the DDC for something that we won't be able to build. So check that first.
	// The goal is to avoid paying the price of a guaranteed failure on high-latency DDC backends every run.
	bool bDataCanBeBuilt = false;

	const FHairGroupsLOD& GroupsLOD = HairGroupsLOD[GroupIndex];
	for (int32 LODIt = 0; LODIt < GroupsLOD.LODs.Num(); ++LODIt)
	{
		int32 SourceIt = 0;
		if (FHairGroupsCardsSourceDescription* Desc = GetSourceDescription(HairGroupsCards, GroupIndex, LODIt, SourceIt))
		{
			// Note: We don't condition/filter resource allocation based on GeometryType == EGroomGeometryType::Cards, because 
			// geometry type can be switched at runtime (between cards/strands)
			{
				UStaticMesh* CardsMesh = nullptr;
				if (Desc->SourceType == EHairCardsSourceType::Procedural)
				{
					CardsMesh = Desc->ProceduralMesh;
				}
				else if (Desc->SourceType == EHairCardsSourceType::Imported)
				{
					CardsMesh = Desc->ImportedMesh;
				}
				
				bDataCanBeBuilt |= CardsMesh != nullptr;
			}
		}
	}

	TArray<uint8> DerivedData;
	if (bDataCanBeBuilt && GetDerivedDataCacheRef().GetSynchronous(*DerivedDataKey, DerivedData, GetPathName()))
	{
		UE_CLOG(IsHairStrandsDDCLogEnable(), LogHairStrands, Log, TEXT("[Groom/DDC] Cards - Found (Groom:%s Group:%d)."), *GetName(), GroupIndex);
		
		FMemoryReader Ar(DerivedData, true);
		InternalSerialize(Ar, this, HairGroupData.Cards.LODs);
	}
	else
	{
		UE_CLOG(IsHairStrandsDDCLogEnable(), LogHairStrands, Log, TEXT("[Groom/DDC] Cards - Not found (Groom:%s Group:%d)."), *GetName(), GroupIndex);

		if (!BuildCardsGeometry(GroupIndex))
		{
			CardsDerivedDataKey[GroupIndex].Empty();
			return false;
		}

		FMemoryWriter Ar(DerivedData, true);
		InternalSerialize(Ar, this, HairGroupData.Cards.LODs);

		GetDerivedDataCacheRef().Put(*DerivedDataKey, DerivedData, GetPathName());
	}

	// Handle the case where the cards data is already cached in the DDC
	// Need to populate the strands data with it
	FHairGroupData& GroupData = HairGroupsData[GroupIndex];
	if (StrandsDerivedDataKey[GroupIndex].IsEmpty() && GroupData.Strands.BulkData.GetNumPoints() == 0)
	{
		for (int32 LODIt = 0; LODIt < GroupData.Cards.LODs.Num(); ++LODIt)
		{
			int32 SourceIt = 0;
			if (FHairGroupsCardsSourceDescription* Desc = GetSourceDescription(HairGroupsCards, GroupIndex, LODIt, SourceIt))
			{
				FHairGroupData::FCards::FLOD& LOD = GroupData.Cards.LODs[LODIt];
				GroupData.Strands.BulkData = LOD.Guides.BulkData;
				GroupData.Guides.BulkData = LOD.Guides.BulkData;
				break;
			}
		}
	}

	// Update the imported mesh DDC keys
	for (int32 LODIt = 0; LODIt < GroupData.Cards.LODs.Num(); ++LODIt)
	{
		int32 SourceIt = 0;
		if (FHairGroupsCardsSourceDescription* Desc = GetSourceDescription(HairGroupsCards, GroupIndex, LODIt, SourceIt))
		{
			Desc->UpdateMeshKey();
		}
	}

	CardsDerivedDataKey[GroupIndex] = DerivedDataKey;

	return true;
}

// Return true if the meshes data have changed
bool UGroomAsset::CacheMeshesGeometry(uint32 GroupIndex)
{
	const bool bHasValidMeshesData = UGroomAsset::HasValidMeshesData(GroupIndex);
	if (!bHasValidMeshesData)
	{
		const bool bNeedReset = !MeshesDerivedDataKey[GroupIndex].IsEmpty();
		MeshesDerivedDataKey[GroupIndex].Empty();
		return bNeedReset;
	}

	const FString KeySuffix = GroomDerivedDataCacheUtils::BuildMeshesDerivedDataKeySuffix(GroupIndex, HairGroupsLOD[GroupIndex].LODs, HairGroupsMeshes);
	const FString DerivedDataKey = GroomDerivedDataCacheUtils::BuildGroomDerivedDataKey(KeySuffix);

	if (MeshesDerivedDataKey[GroupIndex] == DerivedDataKey)
	{
		// Skip if no change
		return false;
	}

	FHairGroupData& HairGroupData = HairGroupsData[GroupIndex];

	// We should never query the DDC for something that we won't be able to build. So check that first.
	// The goal is to avoid paying the price of a guaranteed failure on high-latency DDC backends every run.
	bool bDataCanBeBuilt = false;

	const FHairGroupsLOD& GroupsLOD = HairGroupsLOD[GroupIndex];
	for (int32 LODIt = 0; LODIt < GroupsLOD.LODs.Num(); ++LODIt)
	{
		int32 SourceIt = 0;
		if (FHairGroupsMeshesSourceDescription* Desc = GetSourceDescription(HairGroupsMeshes, GroupIndex, LODIt, SourceIt))
		{
			if (GroupsLOD.LODs[LODIt].GeometryType == EGroomGeometryType::Meshes)
			{
				bDataCanBeBuilt |= Desc->ImportedMesh != nullptr;
			}
		}
	}

	TArray<uint8> DerivedData;
	if (bDataCanBeBuilt && GetDerivedDataCacheRef().GetSynchronous(*DerivedDataKey, DerivedData, GetPathName()))
	{
		UE_CLOG(IsHairStrandsDDCLogEnable(), LogHairStrands, Log, TEXT("[Groom/DDC] Meshes - Found (Groom:%s Group:%d)."), *GetName(), GroupIndex);

		FMemoryReader Ar(DerivedData, true);
		InternalSerialize(Ar, this, HairGroupData.Meshes.LODs);
	}
	else
	{
		UE_CLOG(IsHairStrandsDDCLogEnable(), LogHairStrands, Log, TEXT("[Groom/DDC] Meshes - Not found (Groom:%s Group:%d)."), *GetName(), GroupIndex);

		if (!BuildMeshesGeometry(GroupIndex))
		{
			MeshesDerivedDataKey[GroupIndex].Empty();
			return false;
		}

		FMemoryWriter Ar(DerivedData, true);
		InternalSerialize(Ar, this, HairGroupData.Meshes.LODs);

		GetDerivedDataCacheRef().Put(*DerivedDataKey, DerivedData, GetPathName());
	}

	MeshesDerivedDataKey[GroupIndex] = DerivedDataKey;

	return true;
}

inline FString GetLODName(const UGroomAsset* Asset, uint32 LODIndex)
{
	return Asset->GetOutermost()->GetName() + FString::Printf(TEXT("_LOD%d"), LODIndex);
}

bool UGroomAsset::BuildCardsGeometry(uint32 GroupIndex)
{
	LLM_SCOPE_BYTAG(Groom);

	if (!IsHairStrandsEnabled(EHairStrandsShaderType::Cards) || HairGroupsCards.Num() == 0 || !CanRebuildFromDescription())
	{
		return false;
	}

	bool bHasChanged = HairGroupsCards.Num() != CachedHairGroupsCards.Num();

	check(GroupIndex < uint32(GetNumHairGroups()));
	FHairGroupData& GroupData = HairGroupsData[GroupIndex];

	// The settings might have been previously cached without the data having been built
	const uint32 LODCount = HairGroupsLOD[GroupIndex].LODs.Num();
	TArray<bool> bIsAlreadyBuilt;
	bIsAlreadyBuilt.SetNum(LODCount);
	for (uint32 LODIt = 0; LODIt < LODCount; ++LODIt)
	{
		int32 SourceIt = 0;
		if (const FHairGroupsCardsSourceDescription* Desc = GetSourceDescription(HairGroupsCards, GroupIndex, LODIt, SourceIt))
		{
			bIsAlreadyBuilt[LODIt] = GroupData.Strands.HasValidData() && Desc->CardsInfo.NumCardVertices > 0 && !Desc->HasMeshChanged();
			bHasChanged |= !bIsAlreadyBuilt[LODIt];
		}
	}

	if (!bHasChanged)
	{
		for (uint32 SourceIt = 0, SourceCount = HairGroupsCards.Num(); SourceIt < SourceCount; ++SourceIt)
		{
			const bool bEquals = CachedHairGroupsCards[SourceIt] == HairGroupsCards[SourceIt];
			if (!bEquals)
			{
				bHasChanged = true;
			}
			else
			{
				// Reload/Update texture resources when the groom asset is saved, so that the textures 
				// content is up to date with what has been saved.
				if (IsInGameThread())
				{
					FHairGroupCardsTextures& Textures = HairGroupsCards[SourceIt].Textures;
					if (Textures.DepthTexture != nullptr)		Textures.DepthTexture->UpdateResource();
					if (Textures.TangentTexture != nullptr)		Textures.TangentTexture->UpdateResource();
					if (Textures.AttributeTexture != nullptr)	Textures.AttributeTexture->UpdateResource();
					if (Textures.CoverageTexture != nullptr)	Textures.CoverageTexture->UpdateResource();
					if (Textures.AuxilaryDataTexture != nullptr)Textures.AuxilaryDataTexture->UpdateResource();
					if (Textures.MaterialTexture != nullptr)	Textures.MaterialTexture->UpdateResource();
				}
			}
		}
	}

	if (!bHasChanged)
	{
		return false;
	}

	bool bDataBuilt = false;
	GroupData.Cards.LODs.SetNum(LODCount);
	const FHairGroupsLOD& GroupsLOD = HairGroupsLOD[GroupIndex];
	for (uint32 LODIt = 0; LODIt < LODCount; ++LODIt)
	{
		int32 SourceIt = 0;
		if (FHairGroupsCardsSourceDescription* Desc = GetSourceDescription(HairGroupsCards, GroupIndex, LODIt, SourceIt))
		{
			// #hair_todo: add finer culling granularity to rebuild only what is necessary
			const FHairGroupsCardsSourceDescription* CachedDesc = SourceIt < CachedHairGroupsCards.Num() ? &CachedHairGroupsCards[SourceIt] : nullptr;
			const bool bLODHasChanged = CachedDesc == nullptr || !(*CachedDesc == *Desc);
			if (!bLODHasChanged && (bIsAlreadyBuilt[LODIt] || GroupsLOD.LODs[LODIt].GeometryType != EGroomGeometryType::Cards)) // build only if it's Cards type
			{
				bDataBuilt |= bIsAlreadyBuilt[LODIt];
				continue;
			}

			if (!IsInGameThread())
			{
				// Build needs to execute from the game thread
				bRetryLoadFromGameThread = true;
				return false;
			}

			FHairGroupData::FCards::FLOD& LOD = GroupData.Cards.LODs[LODIt];
			LOD.BulkData.Reset();

			// 0. Release geometry resources
			InternalReleaseResource(LOD.RestResource);
			InternalReleaseResource(LOD.InterpolationResource);
			InternalReleaseResource(LOD.Guides.RestResource);
			InternalReleaseResource(LOD.Guides.InterpolationResource);

			// 1. Load geometry data, if any
			UStaticMesh* CardsMesh = nullptr;
			if (Desc->SourceType == EHairCardsSourceType::Procedural)
			{
				CardsMesh = Desc->ProceduralMesh;
			}
			else if (Desc->SourceType == EHairCardsSourceType::Imported)
			{
				CardsMesh = Desc->ImportedMesh;
			}

			bool bInitResources = false;
			FHairStrandsDatas LODGuidesData;
			if (CardsMesh != nullptr)
			{
				CardsMesh->ConditionalPostLoad();

				// * Create a transient FHairStrandsData in order to extract RootUV and transfer them to cards data
				// * Voxelize hair strands data (all group), to transfer group index from strands to cards
				FHairStrandsDatas TempHairStrandsData;
				FHairStrandsVoxelData TempHairStrandsVoxelData;
				{
					const FHairDescriptionGroups& LocalHairDescriptionGroups = GetHairDescriptionGroups();
					FHairGroupInfo DummyInfo;
					check(LocalHairDescriptionGroups.IsValid());
					TempHairStrandsData = LocalHairDescriptionGroups.HairGroups[GroupIndex].Strands;
					FGroomBuilder::BuildData(TempHairStrandsData);
					FGroomBuilder::VoxelizeGroupIndex(LocalHairDescriptionGroups, TempHairStrandsVoxelData);
				}
				
				bInitResources = FHairCardsBuilder::ImportGeometry(CardsMesh, TempHairStrandsData, TempHairStrandsVoxelData, LOD.BulkData, LODGuidesData, LOD.InterpolationBulkData);
				if (!bInitResources)
				{
					UE_LOG(LogHairStrands, Warning, TEXT("Failed to import cards from %s for Group %d LOD %d."), *CardsMesh->GetName(), GroupIndex, LODIt);
					LOD.BulkData.Reset();
				}
			}

			bDataBuilt |= bInitResources;

			// Clear the cards stats
			Desc->CardsInfo.NumCardVertices = 0;
			Desc->CardsInfo.NumCards = 0;

			// 2. Init geometry & texture resources, and generate interpolation data/resources
			if (bInitResources)
			{
				FHairResourceName ResourceName(GetFName(), GroupIndex, LODIt);

				LOD.RestResource = new FHairCardsRestResource(LOD.BulkData, ResourceName);
				BeginInitResource(LOD.RestResource); // Immediate allocation, as needed for the vertex factory, input stream building

				// 2.1 Load atlas textures
				InitAtlasTexture(LOD.RestResource, Desc->Textures.DepthTexture, EHairAtlasTextureType::Depth);
				InitAtlasTexture(LOD.RestResource, Desc->Textures.TangentTexture, EHairAtlasTextureType::Tangent);
				InitAtlasTexture(LOD.RestResource, Desc->Textures.AttributeTexture, EHairAtlasTextureType::Attribute);
				InitAtlasTexture(LOD.RestResource, Desc->Textures.CoverageTexture, EHairAtlasTextureType::Coverage);
				InitAtlasTexture(LOD.RestResource, Desc->Textures.AuxilaryDataTexture, EHairAtlasTextureType::AuxilaryData);
				InitAtlasTexture(LOD.RestResource, Desc->Textures.MaterialTexture, EHairAtlasTextureType::Material);
				LOD.RestResource->bInvertUV = Desc->SourceType == EHairCardsSourceType::Procedural;
				
				// 2.2 Load interoplatino resources
				LOD.InterpolationResource = new FHairCardsInterpolationResource(LOD.InterpolationBulkData, ResourceName);

				// Create own interpolation settings for cards.
				// Force closest guides as this is the most relevant matching metric for cards, due to their coarse geometry
				FHairInterpolationSettings CardsInterpolationSettings = HairGroupsInterpolation[GroupIndex].InterpolationSettings;
				CardsInterpolationSettings.bOverrideGuides = false;
				CardsInterpolationSettings.bUseUniqueGuide = true;
				CardsInterpolationSettings.bRandomizeGuide = false;
				CardsInterpolationSettings.InterpolationDistance = EHairInterpolationWeight::Parametric;
				CardsInterpolationSettings.InterpolationQuality = EHairInterpolationQuality::High;

				// There could be no strands data when importing cards into an empty groom so get them from the card guides
				bool bCopyRenderData = false;
				if (GroupData.Guides.BulkData.GetNumPoints() == 0)
				{
					// The RenderData is filled out by BuildData
					bCopyRenderData = true;
				}

				FHairGroupInfo DummyGroupInfo;

				// 1. Build data & bulk data for the cards guides
				FGroomBuilder::BuildData(LODGuidesData);
				FGroomBuilder::BuildBulkData(DummyGroupInfo, LODGuidesData, LOD.Guides.BulkData);

				// 2. (Re)Build data for the sim guides (since there are transient/no-cached)
				const FHairDescriptionGroups& LocalHairDescriptionGroups = GetHairDescriptionGroups();
				FHairGroupInfo DummyInfo;
				check(LocalHairDescriptionGroups.IsValid());
				FHairStrandsDatas SimGuidesData;
				FHairStrandsDatas RenStrandsData; // Dummy, not used
				FGroomBuilder::BuildData(LocalHairDescriptionGroups.HairGroups[GroupIndex], HairGroupsInterpolation[GroupIndex], DummyInfo, RenStrandsData, SimGuidesData);

				// 3. Compute interpolation data & bulk data between the sim guides and the cards guides
				FHairStrandsInterpolationDatas LODGuideInterpolationData;
				FGroomBuilder::BuildInterplationData(DummyGroupInfo, LODGuidesData, SimGuidesData, CardsInterpolationSettings, LODGuideInterpolationData);
				FGroomBuilder::BuildInterplationBulkData(SimGuidesData, LODGuideInterpolationData, LOD.Guides.InterpolationBulkData);

				if (bCopyRenderData)
				{
					GroupData.Strands.BulkData = LOD.Guides.BulkData;
					GroupData.Guides.BulkData  = LOD.Guides.BulkData;
				}

				LOD.Guides.RestResource = new FHairStrandsRestResource(LOD.Guides.BulkData, EHairStrandsResourcesType::Cards, ResourceName);

				LOD.Guides.InterpolationResource = new FHairStrandsInterpolationResource(LOD.Guides.InterpolationBulkData, ResourceName);

				// Update card stats to display
				Desc->CardsInfo.NumCardVertices = LOD.BulkData.GetNumVertices();
				Desc->CardsInfo.NumCards = LOD.Guides.BulkData.CurveCount;// LOD.Data.Cards.IndexOffsets.Num();
			}
		}
	}

	return bDataBuilt;
}

bool UGroomAsset::BuildCardsGeometry()
{
	bool bDataChanged = false;
	for (uint32 GroupIndex = 0, GroupCount = GetNumHairGroups(); GroupIndex < GroupCount; ++GroupIndex)
	{
		bDataChanged |= CacheCardsGeometry(GroupIndex, StrandsDerivedDataKey[GroupIndex]);
	}

	if (bDataChanged)
	{
		InitGuideResources();

		// When building cards in an empty groom, the strands resources need to be initialized once
		FHairGroupData& GroupData = HairGroupsData[0];
		if (!GroupData.Strands.ClusterCullingResource)
		{
			InitStrandsResources();
		}

		InitCardsResources();
	}

	return bDataChanged;
}

bool UGroomAsset::BuildMeshesGeometry()
{
	bool bDataChanged = false;
	for (uint32 GroupIndex = 0, GroupCount = GetNumHairGroups(); GroupIndex < GroupCount; ++GroupIndex)
	{
		bDataChanged |= CacheMeshesGeometry(GroupIndex);
	}

	if (bDataChanged)
	{
		InitMeshesResources();
	}

	return bDataChanged;
}

bool UGroomAsset::BuildMeshesGeometry(uint32 GroupIndex)
{
	if (!IsHairStrandsEnabled(EHairStrandsShaderType::Meshes) || HairGroupsMeshes.Num() == 0)
	{
		return false;
	}

	bool bHasChanged = HairGroupsMeshes.Num() != CachedHairGroupsMeshes.Num();

	FHairGroupData& GroupData = HairGroupsData[GroupIndex];

	// The settings might have been previously cached without the data having been built
	const uint32 LODCount = HairGroupsLOD[GroupIndex].LODs.Num();
	TArray<bool> bIsAlreadyBuilt;
	bIsAlreadyBuilt.SetNum(LODCount);
	for (uint32 LODIt = 0; LODIt < LODCount; ++LODIt)
	{
		int32 SourceIt = 0;
		if (const FHairGroupsMeshesSourceDescription* Desc = GetSourceDescription(HairGroupsMeshes, GroupIndex, LODIt, SourceIt))
		{
			bIsAlreadyBuilt[LODIt] = GroupData.Meshes.LODs.IsValidIndex(LODIt) && GroupData.Meshes.LODs[LODIt].BulkData.GetNumVertices() > 0 && !Desc->HasMeshChanged();
			bHasChanged |= !bIsAlreadyBuilt[LODIt];
		}
	}

	if (!bHasChanged)
	{
		for (uint32 SourceIt = 0, SourceCount = HairGroupsMeshes.Num(); SourceIt < SourceCount; ++SourceIt)
		{
			const bool bEquals = CachedHairGroupsMeshes[SourceIt] == HairGroupsMeshes[SourceIt];
			if (!bEquals)
			{
				bHasChanged = true;
				break;
			}
		}
	}

	if (!bHasChanged)
	{
		for (uint32 SourceIt = 0, SourceCount = HairGroupsMeshes.Num(); SourceIt < SourceCount; ++SourceIt)
		{
			const bool bEquals = CachedHairGroupsMeshes[SourceIt] == HairGroupsMeshes[SourceIt];
			if (!bEquals)
			{
				bHasChanged = true;
			}
			else
			{
				// Reload/Update texture resources when the groom asset is saved, so that the textures 
				// content is up to date with what has been saved.
				if (IsInGameThread())
				{
					FHairGroupCardsTextures& Textures = HairGroupsMeshes[SourceIt].Textures;
					if (Textures.DepthTexture != nullptr)		Textures.DepthTexture->UpdateResource();
					if (Textures.TangentTexture != nullptr)		Textures.TangentTexture->UpdateResource();
					if (Textures.AttributeTexture != nullptr)	Textures.AttributeTexture->UpdateResource();
					if (Textures.CoverageTexture != nullptr)	Textures.CoverageTexture->UpdateResource();
					if (Textures.AuxilaryDataTexture != nullptr)Textures.AuxilaryDataTexture->UpdateResource();
					if (Textures.MaterialTexture != nullptr)	Textures.MaterialTexture->UpdateResource();
				}
			}
		}
	}

	if (!bHasChanged)
	{
		return false;
	}

	bool bDataBuilt = false;
	GroupData.Meshes.LODs.SetNum(LODCount);
	const FHairGroupsLOD& GroupsLOD = HairGroupsLOD[GroupIndex];
	for (uint32 LODIt = 0; LODIt < LODCount; ++LODIt)
	{
		int32 SourceIt = 0;
		if (const FHairGroupsMeshesSourceDescription* Desc = GetSourceDescription(HairGroupsMeshes, GroupIndex, LODIt, SourceIt))
		{
			const FHairGroupsMeshesSourceDescription* CachedDesc = SourceIt < CachedHairGroupsMeshes.Num() ? &CachedHairGroupsMeshes[SourceIt] : nullptr;
			const bool bLODHasChanged = CachedDesc == nullptr || !(*CachedDesc == *Desc);
			if (!bLODHasChanged && (bIsAlreadyBuilt[LODIt] || GroupsLOD.LODs[LODIt].GeometryType != EGroomGeometryType::Meshes)) // build only if it's Meshes type
			{
				bDataBuilt |= bIsAlreadyBuilt[LODIt];
				continue;
			}

			if (!IsInGameThread())
			{
				// Build needs to execute from the game thread
				bRetryLoadFromGameThread = true;
				return false;
			}

			FHairGroupData::FMeshes::FLOD& LOD = GroupData.Meshes.LODs[LODIt];

			if (Desc->ImportedMesh)
			{
				Desc->ImportedMesh->ConditionalPostLoad();
				FHairMeshesBuilder::ImportGeometry(
					Desc->ImportedMesh,
					LOD.BulkData);
			}
			else
			{
				// Build a default box
				FHairMeshesBuilder::BuildGeometry(
					GroupData.Strands.BulkData.BoundingBox,
					LOD.BulkData);
			}

			bDataBuilt |= true;
		}
	}

	// Update the imported mesh DDC keys
	for (int32 LODIt = 0; LODIt < GroupData.Meshes.LODs.Num(); ++LODIt)
	{
		int32 SourceIt = 0;
		if (FHairGroupsMeshesSourceDescription* Desc = GetSourceDescription(HairGroupsMeshes, GroupIndex, LODIt, SourceIt))
		{
			Desc->UpdateMeshKey();
		}
	}

	return bDataBuilt;
}

void UGroomAsset::UpdateCachedSettings()
{
#if WITH_EDITORONLY_DATA
	CachedHairGroupsRendering		= HairGroupsRendering;
	CachedHairGroupsPhysics			= HairGroupsPhysics;
	CachedHairGroupsInterpolation	= HairGroupsInterpolation;
	CachedHairGroupsLOD				= HairGroupsLOD;
	CachedHairGroupsCards			= HairGroupsCards;
	CachedHairGroupsMeshes			= HairGroupsMeshes;
#endif
}
#endif // WITH_EDITORONLY_DATA


void UGroomAsset::InitGuideResources()
{
	// Guide resources are lazy allocated, as these resources are only used when RBF/simulation is enabled by a groom component
	// for (uint32 GroupIndex = 0, GroupCount = GetNumHairGroups(); GroupIndex < GroupCount; ++GroupIndex)
	//{
	//	AllocateGuidesResources(GroupIndex);
	//}
}

FHairStrandsRestResource* UGroomAsset::AllocateGuidesResources(uint32 GroupIndex)
{
	if (GroupIndex < uint32(GetNumHairGroups()))
	{
		FHairGroupData& GroupData = HairGroupsData[GroupIndex];
		if (GroupData.Guides.HasValidData())
		{
			if (GroupData.Guides.RestResource == nullptr)
			{
				GroupData.Guides.RestResource = new FHairStrandsRestResource(GroupData.Guides.BulkData, EHairStrandsResourcesType::Guides, FHairResourceName(GetFName(), GroupIndex));
			}
			return GroupData.Guides.RestResource;
		}
	}
	return nullptr;
}

FHairStrandsInterpolationResource* UGroomAsset::AllocateInterpolationResources(uint32 GroupIndex)
{
	if (GroupIndex < uint32(GetNumHairGroups()))
	{
		FHairGroupData& GroupData = HairGroupsData[GroupIndex];
		check(GroupData.Guides.IsValid());
		if (GroupData.Strands.InterpolationResource == nullptr)
		{
			GroupData.Strands.InterpolationResource = new FHairStrandsInterpolationResource(GroupData.Strands.InterpolationBulkData, FHairResourceName(GetFName(), GroupIndex));
		}
		return GroupData.Strands.InterpolationResource;
	}
	return nullptr;
}

#if RHI_RAYTRACING
FHairStrandsRaytracingResource* UGroomAsset::AllocateCardsRaytracingResources(uint32 GroupIndex, uint32 LODIndex)
{
	if (IsRayTracingEnabled() && GroupIndex < uint32(GetNumHairGroups()) && LODIndex < uint32(HairGroupsLOD[GroupIndex].LODs.Num()))
	{
		FHairGroupData& GroupData = HairGroupsData[GroupIndex];
		FHairGroupData::FCards::FLOD& LOD = GroupData.Cards.LODs[LODIndex];
		check(LOD.BulkData.IsValid());

		if (LOD.RaytracingResource == nullptr)
		{
			LOD.RaytracingResource = new FHairStrandsRaytracingResource(LOD.BulkData, FHairResourceName(GetFName(), GroupIndex, LODIndex));
		}
		return LOD.RaytracingResource;
	}
	return nullptr;
}

FHairStrandsRaytracingResource* UGroomAsset::AllocateMeshesRaytracingResources(uint32 GroupIndex, uint32 LODIndex)
{
	if (IsRayTracingEnabled() && GroupIndex < uint32(GetNumHairGroups()) && LODIndex < uint32(HairGroupsLOD[GroupIndex].LODs.Num()))
	{
		FHairGroupData& GroupData = HairGroupsData[GroupIndex];
		FHairGroupData::FMeshes::FLOD& LOD = GroupData.Meshes.LODs[LODIndex];
		check(LOD.BulkData.IsValid());

		if (LOD.RaytracingResource == nullptr)
		{
			LOD.RaytracingResource = new FHairStrandsRaytracingResource(LOD.BulkData, FHairResourceName(GetFName(), GroupIndex, LODIndex));
		}
		return LOD.RaytracingResource;
	}
	return nullptr;
}

FHairStrandsRaytracingResource* UGroomAsset::AllocateStrandsRaytracingResources(uint32 GroupIndex)
{
	if (IsRayTracingEnabled() && GroupIndex < uint32(GetNumHairGroups()))
	{
		FHairGroupData& GroupData = HairGroupsData[GroupIndex];
		check(GroupData.Strands.HasValidData());

		if (GroupData.Strands.RaytracingResource == nullptr)
		{
			GroupData.Strands.RaytracingResource = new FHairStrandsRaytracingResource(GroupData.Strands.BulkData, FHairResourceName(GetFName(), GroupIndex));
		}
		return GroupData.Strands.RaytracingResource;
	}
	return nullptr;
}
#endif // RHI_RAYTRACING

void UGroomAsset::InitStrandsResources()
{
	// Even though we shouldn't build the strands resources if the platforms does 
	// not support it, we can't test it as this is dependant on the EShaderPlatform 
	// enum, which is only available on the scene, and not available as the time the 
	// groom asset is loaded and initialized.
	// We should be testing: IsHairStrandsEnabled(EHairStrandsShaderType::Strands, Platform)
	//
	// To handle this we assume that thecooking as strip out the strands data
	if (!IsHairStrandsEnabled(EHairStrandsShaderType::Strands))
	{
		return;
	}

	for (uint32 GroupIndex = 0, GroupCount = GetNumHairGroups(); GroupIndex < GroupCount; ++GroupIndex)
	{
		FHairGroupData& GroupData = HairGroupsData[GroupIndex];

		if (GroupData.Strands.HasValidData())
		{
			FHairResourceName ResourceName(GetFName(), GroupIndex);

			GroupData.Strands.RestResource = new FHairStrandsRestResource(GroupData.Strands.BulkData, EHairStrandsResourcesType::Strands, ResourceName);

			if (GroupData.Strands.ClusterCullingBulkData.IsValid())
			{
				const uint32 HairGroupLODCount = HairGroupsLOD[GroupIndex].LODs.Num();
				bool bLastVisibility = true;
				// LOD visibility is not serialized into FHairStrandsClusterCullingBulkData as it does not affect the actual generated data. For consistency we 
				// patch the LOD visibility with the groom asset value, which might be different from what has been serialized
				for (uint32 LODIt = 0, LODCount = GroupData.Strands.ClusterCullingBulkData.LODVisibility.Num(); LODIt < LODCount; ++LODIt)
				{
					if (LODIt < HairGroupLODCount)
					{
						bLastVisibility = HairGroupsLOD[GroupIndex].LODs[LODIt].bVisible;
						GroupData.Strands.ClusterCullingBulkData.LODVisibility[LODIt] = bLastVisibility;
					}
					else
					{
						GroupData.Strands.ClusterCullingBulkData.LODVisibility[LODIt] = bLastVisibility;
					}
				}

				GroupData.Strands.ClusterCullingResource = new FHairStrandsClusterCullingResource(GroupData.Strands.ClusterCullingBulkData, ResourceName);
			}

			// Interpolation are lazy allocated, as these resources are only used when RBF/simulation is enabled by a groom component
			// AllocateInterpolationResources(GroupIndex);
		}
	}
}

static void InitCardsTextureResources(UGroomAsset* GroomAsset)
{
	if (!GroomAsset || !IsHairStrandsEnabled(EHairStrandsShaderType::Cards) || GroomAsset->HairGroupsCards.Num() == 0)
	{
		return;
	}

	for (uint32 GroupIndex = 0, GroupCount = GroomAsset->GetNumHairGroups(); GroupIndex < GroupCount; ++GroupIndex)
	{
		FHairGroupData& GroupData = GroomAsset->HairGroupsData[GroupIndex];

		const uint32 LODCount = GroomAsset->HairGroupsLOD[GroupIndex].LODs.Num();
		GroupData.Cards.LODs.SetNum(LODCount);

		for (uint32 LODIt = 0; LODIt < LODCount; ++LODIt)
		{
			FHairGroupData::FCards::FLOD& LOD = GroupData.Cards.LODs[LODIt];

			int32 SourceIt = 0;
			const FHairGroupsCardsSourceDescription* Desc = GetSourceDescription(GroomAsset->HairGroupsCards, GroupIndex, LODIt, SourceIt);
			if (!Desc)
			{
				continue;
			}

			if (LOD.HasValidData())
			{
				if (Desc)
				{
					InitAtlasTexture(LOD.RestResource, Desc->Textures.DepthTexture, EHairAtlasTextureType::Depth);
					InitAtlasTexture(LOD.RestResource, Desc->Textures.TangentTexture, EHairAtlasTextureType::Tangent);
					InitAtlasTexture(LOD.RestResource, Desc->Textures.AttributeTexture, EHairAtlasTextureType::Attribute);
					InitAtlasTexture(LOD.RestResource, Desc->Textures.CoverageTexture, EHairAtlasTextureType::Coverage);
					InitAtlasTexture(LOD.RestResource, Desc->Textures.AuxilaryDataTexture, EHairAtlasTextureType::AuxilaryData);
					InitAtlasTexture(LOD.RestResource, Desc->Textures.MaterialTexture, EHairAtlasTextureType::Material);
					if (LOD.RestResource)
					{
						LOD.RestResource->bInvertUV = Desc->SourceType == EHairCardsSourceType::Procedural; // Should fix procedural texture so that this does not happen
					}
				}
			}
		}
	}
}

void UGroomAsset::InitCardsResources()
{
	LLM_SCOPE_BYTAG(Groom);

	if (!IsHairStrandsEnabled(EHairStrandsShaderType::Cards) || HairGroupsCards.Num() == 0)
	{
		return;
	}

	for (uint32 GroupIndex = 0, GroupCount = GetNumHairGroups(); GroupIndex < GroupCount; ++GroupIndex)
	{
		FHairGroupData& GroupData = HairGroupsData[GroupIndex];

		const uint32 LODCount = HairGroupsLOD[GroupIndex].LODs.Num();
		GroupData.Cards.LODs.SetNum(LODCount);

		for (uint32 LODIt = 0; LODIt < LODCount; ++LODIt)
		{
			FHairGroupData::FCards::FLOD& LOD = GroupData.Cards.LODs[LODIt];

			int32 SourceIt = 0;
			const FHairGroupsCardsSourceDescription* Desc = GetSourceDescription(HairGroupsCards, GroupIndex, LODIt, SourceIt);
			if (!Desc)
			{
				continue;
			}

			if (LOD.RestResource == nullptr &&			// don't initialize again if they were previously initialized during the BuildCardsGeometry
				LOD.HasValidData())
			{
				FHairResourceName ResourceName(GetFName(), GroupIndex, LODIt);

				LOD.RestResource = new FHairCardsRestResource(LOD.BulkData, ResourceName);
				BeginInitResource(LOD.RestResource); // Immediate allocation, as needed for the vertex factory, input stream building

				LOD.InterpolationResource = new FHairCardsInterpolationResource(LOD.InterpolationBulkData, ResourceName);

				LOD.Guides.RestResource = new FHairStrandsRestResource(LOD.Guides.BulkData, EHairStrandsResourcesType::Cards, ResourceName);

				LOD.Guides.InterpolationResource = new FHairStrandsInterpolationResource(LOD.Guides.InterpolationBulkData, ResourceName);

				if (Desc)
				{
					InitAtlasTexture(LOD.RestResource, Desc->Textures.DepthTexture, EHairAtlasTextureType::Depth);
					InitAtlasTexture(LOD.RestResource, Desc->Textures.TangentTexture, EHairAtlasTextureType::Tangent);
					InitAtlasTexture(LOD.RestResource, Desc->Textures.AttributeTexture, EHairAtlasTextureType::Attribute);
					InitAtlasTexture(LOD.RestResource, Desc->Textures.CoverageTexture, EHairAtlasTextureType::Coverage);
					InitAtlasTexture(LOD.RestResource, Desc->Textures.AuxilaryDataTexture, EHairAtlasTextureType::AuxilaryData);
					InitAtlasTexture(LOD.RestResource, Desc->Textures.MaterialTexture, EHairAtlasTextureType::Material);
					LOD.RestResource->bInvertUV = Desc->SourceType == EHairCardsSourceType::Procedural; // Should fix procedural texture so that this does not happen
				}
			}

			if (Desc)
			{
				// Update card stats to display
				Desc->CardsInfo.NumCardVertices = LOD.BulkData.GetNumVertices();
				Desc->CardsInfo.NumCards = LOD.Guides.BulkData.CurveCount;// LOD.Data.Cards.GetNumCards();
			}
		}
	}
}

void UGroomAsset::InitMeshesResources()
{
	LLM_SCOPE_BYTAG(Groom);

	if (!IsHairStrandsEnabled(EHairStrandsShaderType::Meshes))
	{
		return;
	}

	for (uint32 GroupIndex = 0, GroupCount = GetNumHairGroups(); GroupIndex < GroupCount; ++GroupIndex)
	{
		FHairGroupData& GroupData = HairGroupsData[GroupIndex];

		const uint32 LODCount = GroupData.Meshes.LODs.Num();
		for (uint32 LODIt = 0; LODIt < LODCount; ++LODIt)
		{
			int32 SourceIt = 0;
			const FHairGroupsMeshesSourceDescription* Desc = GetSourceDescription(HairGroupsMeshes, GroupIndex, LODIt, SourceIt);
			if (!Desc)
			{
				continue;
			}
			FHairGroupData::FMeshes::FLOD& LOD = GroupData.Meshes.LODs[LODIt];
			InternalReleaseResource(LOD.RestResource);

			if (LOD.HasValidData())
			{
				LOD.RestResource = new FHairMeshesRestResource(LOD.BulkData, FHairResourceName(GetFName(), GroupIndex, LODIt));
				BeginInitResource(LOD.RestResource); // Immediate allocation, as needed for the vertex factory, input stream building


				if (Desc)
				{
					InitAtlasTexture(LOD.RestResource, Desc->Textures.DepthTexture, EHairAtlasTextureType::Depth);
					InitAtlasTexture(LOD.RestResource, Desc->Textures.TangentTexture, EHairAtlasTextureType::Tangent);
					InitAtlasTexture(LOD.RestResource, Desc->Textures.AttributeTexture, EHairAtlasTextureType::Attribute);
					InitAtlasTexture(LOD.RestResource, Desc->Textures.CoverageTexture, EHairAtlasTextureType::Coverage);
					InitAtlasTexture(LOD.RestResource, Desc->Textures.AuxilaryDataTexture, EHairAtlasTextureType::AuxilaryData);
					InitAtlasTexture(LOD.RestResource, Desc->Textures.MaterialTexture, EHairAtlasTextureType::Material);
				}
			}
		}
	}
}

EGroomGeometryType UGroomAsset::GetGeometryType(int32 GroupIndex, int32 LODIndex) const
{
	if (GroupIndex < 0 || GroupIndex >= HairGroupsLOD.Num())
	{
		return EGroomGeometryType::Strands;
	}

	if (LODIndex < 0 || LODIndex >= HairGroupsLOD[GroupIndex].LODs.Num())
	{
		return EGroomGeometryType::Strands;
	}

	return HairGroupsLOD[GroupIndex].LODs[LODIndex].GeometryType;
}

EGroomBindingType UGroomAsset::GetBindingType(int32 GroupIndex, int32 LODIndex) const
{
	if (GroupIndex < 0 || GroupIndex >= HairGroupsLOD.Num() || !IsHairStrandsBindingEnable())
	{
		return EGroomBindingType::Rigid; // Fallback to rigid by default
	}

	if (LODIndex < 0 || LODIndex >= HairGroupsLOD[GroupIndex].LODs.Num())
	{
		return EGroomBindingType::Rigid; // Fallback to rigid by default
	}

	return HairGroupsLOD[GroupIndex].LODs[LODIndex].BindingType;
}

bool UGroomAsset::IsVisible(int32 GroupIndex, int32 LODIndex) const
{
	if (GroupIndex < 0 || GroupIndex >= HairGroupsLOD.Num())
	{
		return false;
	}

	if (LODIndex < 0 || LODIndex >= HairGroupsLOD[GroupIndex].LODs.Num())
	{
		return false;
	}

	return HairGroupsLOD[GroupIndex].LODs[LODIndex].bVisible;
}

bool UGroomAsset::IsDeformationEnable(int32 GroupIndex) const
{
	return HairGroupsInterpolation.IsValidIndex(GroupIndex) && HairGroupsInterpolation[GroupIndex].RiggingSettings.bEnableRigging && HairGroupsInterpolation[GroupIndex].InterpolationSettings.bOverrideGuides;
}

bool UGroomAsset::IsSimulationEnable(int32 GroupIndex, int32 LODIndex) const 
{
	if (GroupIndex < 0 || GroupIndex >= HairGroupsLOD.Num() || !IsHairStrandsSimulationEnable())
	{
		return false;
	}

	if (LODIndex >= HairGroupsLOD[GroupIndex].LODs.Num())
	{
		return false;
	}

	check(HairGroupsPhysics.Num() == HairGroupsLOD.Num());

	// If the LOD index is not forced, then its value could be -1. In this case we return the 'global' asset value
	if (LODIndex < 0)
	{
		return HairGroupsPhysics[GroupIndex].SolverSettings.EnableSimulation;
	}

	if (!IsHairLODSimulationEnabled(LODIndex))
	{
		return false;
	}

	return HairGroupsLOD[GroupIndex].LODs[LODIndex].bVisible && (
		HairGroupsLOD[GroupIndex].LODs[LODIndex].Simulation == EGroomOverrideType::Enable ||
		(HairGroupsLOD[GroupIndex].LODs[LODIndex].Simulation == EGroomOverrideType::Auto && HairGroupsPhysics[GroupIndex].SolverSettings.EnableSimulation));
}

bool UGroomAsset::IsGlobalInterpolationEnable(int32 GroupIndex, int32 LODIndex) const
{
	if (GroupIndex < 0 || GroupIndex >= HairGroupsLOD.Num())
	{
		return false;
	}

	if (LODIndex < 0 || LODIndex >= HairGroupsLOD[GroupIndex].LODs.Num())
	{
		return false;
	}

	// If the LOD index is not forced, then its value could be -1. In this case we return the 'global' asset value
	if (LODIndex < 0)
	{
		return EnableGlobalInterpolation;
	}

	return
		HairGroupsLOD[GroupIndex].LODs[LODIndex].GlobalInterpolation == EGroomOverrideType::Enable ||
		(HairGroupsLOD[GroupIndex].LODs[LODIndex].GlobalInterpolation == EGroomOverrideType::Auto && EnableGlobalInterpolation);
}

bool UGroomAsset::IsSimulationEnable() const
{
	for (int32 GroupIndex = 0; GroupIndex < HairGroupsData.Num(); ++GroupIndex)
	{
		for (int32 LODIt = 0; LODIt < HairGroupsLOD.Num(); ++LODIt)
		{
			if (IsSimulationEnable(GroupIndex, LODIt))
			{
				return true;
			}
		}
	}

	return false;
}

bool UGroomAsset::NeedsInterpolationData(int32 GroupIndex) const
{
	for (int32 LODIt = 0; LODIt < HairGroupsLOD.Num(); ++LODIt)
	{
		if (IsSimulationEnable(GroupIndex, LODIt) || IsGlobalInterpolationEnable(GroupIndex, LODIt))
		{
			return true;
		}
	}

	return IsDeformationEnable(GroupIndex);
}

bool UGroomAsset::NeedsInterpolationData() const
{
	for (int32 GroupIndex = 0; GroupIndex < HairGroupsData.Num(); ++GroupIndex)
	{
		if (NeedsInterpolationData(GroupIndex))
		{
			return true;
		}
	}

	return false;
}

bool FHairDescriptionGroups::IsValid() const
{
	for (const FHairDescriptionGroup& HairGroup : HairGroups)
	{
		if (HairGroup.Info.NumCurves == 0)
		{
			return false;
		}
	}

	return 	HairGroups.Num() > 0;
}

int32 UGroomAsset::GetLODCount() const
{
	int32 MaxLODCount = -1;
	for (const FHairGroupsLOD& S : HairGroupsLOD)
	{
		MaxLODCount = FMath::Max(MaxLODCount, S.LODs.Num());
	}
	return MaxLODCount;
}

#if WITH_EDITORONLY_DATA
void UGroomAsset::StripLODs(const TArray<int32>& LODsToKeep, bool bRebuildResources)
{
	// Assume that the LOD are ordered from 0 ... Max
	// Export all LODs if the list is empty or has the same number of LODs
	if (LODsToKeep.Num() == GetLODCount() || LODsToKeep.Num() == 0)
	{
		return;
	}

	const int32 GroupCount = HairGroupsLOD.Num();
	int32 LODsTOKeepIndex = LODsToKeep.Num()-1;

	// Remove the LOD settings prior to rebuild the LOD data
	const int32 LODCount = GetLODCount();
	for (int32 LODIt = LODCount-1; LODIt > 0; --LODIt)
	{
		if (LODIt == LODsToKeep[LODsTOKeepIndex])
		{
			continue;
		}

		for (int32 GroupIt = 0; GroupIt < GroupCount; ++GroupIt)
		{
			HairGroupsLOD[GroupIt].LODs.RemoveAt(LODIt);
		}

		--LODsTOKeepIndex;
	}

	// Rebuild the LOD data
	if (bRebuildResources && CanRebuildFromDescription())
	{
		const FHairDescriptionGroups& LocalHairDescriptionGroups = GetHairDescriptionGroups();
		for (int32 GroupIt = 0; GroupIt < GroupCount; ++GroupIt)
		{
			FHairGroupData& GroupData = HairGroupsData[GroupIt];
			InternalReleaseResource(GroupData.Strands.ClusterCullingResource);

			BuildHairGroupCluster(
				GroupIt,
				LocalHairDescriptionGroups,
				HairGroupsInterpolation,
				HairGroupsLOD,
				HairGroupsInfo,
				HairGroupsData);
		}
	}
}
#endif // WITH_EDITORONLY_DATA

bool UGroomAsset::HasDebugData() const
{
	for (uint32 GroupIndex = 0, GroupCount = GetNumHairGroups(); GroupIndex < GroupCount; ++GroupIndex)
	{
		if (!HairGroupsData[GroupIndex].Debug.Data.IsValid())
		{
			return false;
		}
	}

	return true;
}

void UGroomAsset::CreateDebugData()
{
#if WITH_EDITORONLY_DATA
	if (!IsHairStrandsEnabled(EHairStrandsShaderType::Strands) || !CanRebuildFromDescription())
		return;

	for (uint32 GroupIndex = 0, GroupCount = GetNumHairGroups(); GroupIndex < GroupCount; ++GroupIndex)
	{
		// 2.1 Generate transient strands data
		const FHairDescriptionGroups& LocalHairDescriptionGroups = GetHairDescriptionGroups();
		FHairGroupInfo DummyInfo;
		FHairStrandsDatas StrandsData;
		FHairStrandsDatas GuidesData;
		FGroomBuilder::BuildData(LocalHairDescriptionGroups.HairGroups[GroupIndex], HairGroupsInterpolation[GroupIndex], DummyInfo, StrandsData, GuidesData);

		FHairGroupData& GroupData = HairGroupsData[GroupIndex];
		CreateHairStrandsDebugDatas(StrandsData, GroupData.Debug.Data);

		if (GroupData.Debug.Data.IsValid())
		{
			GroupData.Debug.Resource = new FHairStrandsDebugDatas::FResources();

			FHairStrandsDebugDatas* InData = &GroupData.Debug.Data;
			FHairStrandsDebugDatas::FResources* InResource = GroupData.Debug.Resource;
			ENQUEUE_RENDER_COMMAND(HairStrandsDebugResourceCommand)(
				[InData, InResource](FRHICommandListImmediate& RHICmdList)
				{
					FRDGBuilder GraphBuilder(RHICmdList);
					CreateHairStrandsDebugResources(GraphBuilder, InData, InResource);
					GraphBuilder.Execute();
				});
		}
	}
#endif
}

int32 UGroomAsset::GetMaterialIndex(FName MaterialSlotName) const
{
	const int32 SlotCount = HairGroupsMaterials.Num();
	for (int32 SlotIt = 0; SlotIt < SlotCount; ++SlotIt)
	{
		if (HairGroupsMaterials[SlotIt].SlotName == MaterialSlotName)
		{
			return SlotIt;
		}
	}

	return INDEX_NONE;
}

bool UGroomAsset::IsMaterialSlotNameValid(FName MaterialSlotName) const
{
	return GetMaterialIndex(MaterialSlotName) != INDEX_NONE;
}

TArray<FName> UGroomAsset::GetMaterialSlotNames() const
{
	TArray<FName> MaterialNames;
	for (const FHairGroupsMaterial& Material : HairGroupsMaterials)
	{
		MaterialNames.Add(Material.SlotName);
	}

	return MaterialNames;
}

template<typename T>
bool InternalIsMaterialUsed(const TArray<T>& Groups, const FName& MaterialSlotName)
{
	bool bNeedSaving = false;
	for (const T& Group : Groups)
	{
		if (Group.MaterialSlotName == MaterialSlotName)
		{
			return true;
		}
	}

	return false;
}

bool UGroomAsset::IsMaterialUsed(int32 MaterialIndex) const
{
	if (MaterialIndex < 0 || MaterialIndex >= HairGroupsMaterials.Num())
		return false;

	const FName MaterialSlotName = HairGroupsMaterials[MaterialIndex].SlotName;
	return 
		InternalIsMaterialUsed(HairGroupsRendering, MaterialSlotName) ||
		InternalIsMaterialUsed(HairGroupsCards, MaterialSlotName) ||
		InternalIsMaterialUsed(HairGroupsMeshes, MaterialSlotName);
}

#if WITH_EDITOR
struct FHairProceduralCardsQuery
{
	FHairCardsInterpolationBulkData InterpolationBulkData;
	FHairCardsProceduralDatas ProceduralData;
	FHairStrandsDatas GuideData;
	FHairCardsDatas Data;
	FHairCardsBulkData BulkData;

	UGroomAsset* Asset = nullptr;
	FHairCardsRestResource* Resources = nullptr;
	FHairCardsProceduralResource* ProceduralResources = nullptr;
	FHairGroupCardsTextures* Textures = nullptr;
};

// Hair_TODO: move this into the groom asset class
static TQueue<FHairProceduralCardsQuery*> HairCardsQueuries;

// hair_TODO: Rename into GenerateProceduralCards
// Generate geometry and textures for hair cards
void UGroomAsset::SaveProceduralCards(uint32 DescIndex)
{
	if (!IsHairStrandsEnabled(EHairStrandsShaderType::Cards) || !CanRebuildFromDescription())
	{
		return;
	}

	LLM_SCOPE_BYTAG(Groom);

	if (DescIndex >= uint32(HairGroupsCards.Num()))
		return;

	FHairGroupsCardsSourceDescription* Desc = &HairGroupsCards[DescIndex];

	const int32 GroupIndex = Desc->GroupIndex;
	const int32 LODIndex = Desc->LODIndex;
	if (GroupIndex >= HairGroupsData.Num())
		return;

	// 1. Convert old parameters (ClusterDecimation, bUseCards) to new parameters (GenerationType & CardsCount)
	{
		FHairCardsClusterSettings& ClusterSettings = Desc->ProceduralSettings.ClusterSettings;
		FHairCardsGeometrySettings& GeometrySettings = Desc->ProceduralSettings.GeometrySettings;
		const bool bNeedConversion = ClusterSettings.ClusterDecimation > 0;
		if (bNeedConversion)
		{
			const int32 MaxCardCount = HairGroupsData[GroupIndex].Strands.BulkData.GetNumCurves();

			GeometrySettings.GenerationType = ClusterSettings.bUseGuide ? EHairCardsGenerationType::UseGuides : EHairCardsGenerationType::CardsCount;
			GeometrySettings.CardsCount = FMath::Clamp(FMath::CeilToInt(ClusterSettings.ClusterDecimation * MaxCardCount), 1, MaxCardCount);
			GeometrySettings.ClusterType = ClusterSettings.Type;

			// Mark the asset as updated.
			ClusterSettings.ClusterDecimation = 0;
		}
	}

	// 2. Generate geometry (CPU)
	FHairProceduralCardsQuery* QP = new FHairProceduralCardsQuery();
	HairCardsQueuries.Enqueue(QP);

	FHairProceduralCardsQuery& Q = *QP;
	Q.Asset = this;
	{
		// 2.1 Generate transient strands data
		const FHairDescriptionGroups& LocalHairDescriptionGroups = GetHairDescriptionGroups();
		FHairGroupInfo DummyInfo;
		FHairStrandsDatas StrandsData;
		FHairStrandsDatas GuidesData;
		FGroomBuilder::BuildData(LocalHairDescriptionGroups.HairGroups[GroupIndex], HairGroupsInterpolation[GroupIndex], DummyInfo, StrandsData, GuidesData);

		// 2.2 Build cards geometry
		FHairCardsBuilder::BuildGeometry(
			GetLODName(this, Desc->LODIndex),
			StrandsData,
			GuidesData,
			Desc->ProceduralSettings,
			Q.ProceduralData,
			Q.BulkData,
			Q.GuideData,
			Q.InterpolationBulkData,
			Desc->Textures);
		Q.Textures = &Desc->Textures;
	}

	// 3. Create resources and enqueue texture generation (GPU, kicked by the render thread) 
	Q.Resources = new FHairCardsRestResource(Q.BulkData, FHairResourceName(GetFName(), GroupIndex, LODIndex));
	BeginInitResource(Q.Resources); // Immediate allocation, as needed for the vertex factory, input stream building
	Q.ProceduralResources = new FHairCardsProceduralResource(Q.ProceduralData.RenderData, Q.ProceduralData.Atlas.Resolution, Q.ProceduralData.Voxels);
	BeginInitResource(Q.ProceduralResources); // Immediate allocation, as needed for the vertex factory, input stream building

	FHairCardsBuilder::BuildTextureAtlas(&Q.ProceduralData, Q.Resources, Q.ProceduralResources, Q.Textures);

	// 4. Save output asset (geometry, and enqueue texture saving)
	{
		// Create a static meshes with the vertex data
		if (HairGroupsCards[DescIndex].ProceduralMesh == nullptr)
		{
			const FString PackageName = GetOutermost()->GetName();
			const FString SuffixName = FText::Format(LOCTEXT("CardsStatisMesh", "_CardsMesh_Group{0}_LOD{1}"), FText::AsNumber(GroupIndex), FText::AsNumber(LODIndex)).ToString();
			HairGroupsCards[DescIndex].ProceduralMesh = FHairStrandsCore::CreateStaticMesh(PackageName, SuffixName);
		}

		// Convert procedural cards data to cards data prior to export
		FHairCardsDatas CardData;
		{
			CardData.Cards = Q.ProceduralData.Cards;
		}

		FHairCardsBuilder::ExportGeometry(CardData, HairGroupsCards[DescIndex].ProceduralMesh);
		FHairStrandsCore::SaveAsset(HairGroupsCards[DescIndex].ProceduralMesh);
		HairGroupsCards[DescIndex].ProceduralMeshKey = GroomDerivedDataCacheUtils::BuildCardsDerivedDataKeySuffix(this, *Desc);
	}
}

// Save geometry and textures for hair cards
// Save out a static mesh based on generated cards
void UGroomAsset::SavePendingProceduralAssets()
{
	// Proceed procedural asset which needs to be saved
	if (!HairCardsQueuries.IsEmpty())
	{
		TQueue<FHairProceduralCardsQuery*> NotReady;
		FHairProceduralCardsQuery* Q = nullptr;
		while (HairCardsQueuries.Dequeue(Q))
		{
			if (Q)
			{
				if (Q->Asset == this && Q->Textures->bNeedToBeSaved)
				{
					if (Q->Textures->DepthTexture)			FHairStrandsCore::SaveAsset(Q->Textures->DepthTexture);
					if (Q->Textures->AttributeTexture)		FHairStrandsCore::SaveAsset(Q->Textures->AttributeTexture);
					if (Q->Textures->AuxilaryDataTexture)	FHairStrandsCore::SaveAsset(Q->Textures->AuxilaryDataTexture);
					if (Q->Textures->CoverageTexture)		FHairStrandsCore::SaveAsset(Q->Textures->CoverageTexture);
					if (Q->Textures->TangentTexture)		FHairStrandsCore::SaveAsset(Q->Textures->TangentTexture);
					if (Q->Textures->MaterialTexture)		FHairStrandsCore::SaveAsset(Q->Textures->MaterialTexture);
					Q->Textures->bNeedToBeSaved = false;

					InternalReleaseResource(Q->Resources);
					InternalReleaseResource(Q->ProceduralResources);
				}
				else
				{
					NotReady.Enqueue(Q);
				}
			}
		}
		while (NotReady.Dequeue(Q))
		{
			HairCardsQueuries.Enqueue(Q);
		}
	}
}
#endif // WITH_EDITOR

#if WITH_EDITORONLY_DATA
bool UGroomAsset::GetHairStrandsDatas(
	const int32 GroupIndex,
	FHairStrandsDatas& OutStrandsData,
	FHairStrandsDatas& OutGuidesData)
{
	if (!CanRebuildFromDescription())
	{
		return false;
	}
	const FHairDescriptionGroups& DescriptionGroups = GetHairDescriptionGroups();
	const bool bIsValid = DescriptionGroups.IsValid() && GroupIndex < DescriptionGroups.HairGroups.Num();
	if (bIsValid)
	{
		const FHairDescriptionGroup& HairGroup = DescriptionGroups.HairGroups[GroupIndex];
		check(GroupIndex == HairGroup.Info.GroupID);

		FHairGroupInfo DummyInfo;
		FGroomBuilder::BuildData(HairGroup, HairGroupsInterpolation[GroupIndex], DummyInfo, OutStrandsData, OutGuidesData);
	}
	return bIsValid;
}

bool UGroomAsset::GetHairCardsGuidesDatas(
	const int32 GroupIndex,
	const int32 LODIndex,
	FHairStrandsDatas& OutCardsGuidesData)
{
	FHairStrandsDatas StrandsData;
	FHairStrandsDatas GuidesData;
	const bool bIsValid = GetHairStrandsDatas(GroupIndex, StrandsData, GuidesData);
	if (!bIsValid)
	{
		return false;
	}

	int32 SourceIt = 0;
	if (FHairGroupsCardsSourceDescription* Desc = GetSourceDescription(HairGroupsCards, GroupIndex, LODIndex, SourceIt))
	{
		// 1. Load geometry data, if any
		UStaticMesh* CardsMesh = nullptr;
		if (Desc->SourceType == EHairCardsSourceType::Procedural)
		{
			CardsMesh = Desc->ProceduralMesh;
		}
		else if (Desc->SourceType == EHairCardsSourceType::Imported)
		{
			CardsMesh = Desc->ImportedMesh;
		}

		if (CardsMesh)
		{
			CardsMesh->ConditionalPostLoad();

			FHairStrandsVoxelData				DummyVoxelData;
			FHairCardsBulkData					DummyBulkData;
			FHairCardsInterpolationBulkData		DummyInterpolationBulkData;
			FHairStrandsDatas					DummyHairStrandsData;
			FHairCardsBuilder::ImportGeometry(CardsMesh, DummyHairStrandsData, DummyVoxelData, DummyBulkData, OutCardsGuidesData, DummyInterpolationBulkData);
			return true;
		}
	}
	return false;
}
#endif // WITH_EDITORONLY_DATA

void UGroomAsset::GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize)
{
	CumulativeResourceSize.AddDedicatedSystemMemoryBytes(HairGroupsRendering.GetAllocatedSize());
	CumulativeResourceSize.AddDedicatedSystemMemoryBytes(HairGroupsPhysics.GetAllocatedSize());
	CumulativeResourceSize.AddDedicatedSystemMemoryBytes(HairGroupsInterpolation.GetAllocatedSize());
	CumulativeResourceSize.AddDedicatedSystemMemoryBytes(HairGroupsLOD.GetAllocatedSize());
	CumulativeResourceSize.AddDedicatedSystemMemoryBytes(HairGroupsCards.GetAllocatedSize());
	CumulativeResourceSize.AddDedicatedSystemMemoryBytes(HairGroupsMeshes.GetAllocatedSize());
	CumulativeResourceSize.AddDedicatedSystemMemoryBytes(HairGroupsMaterials.GetAllocatedSize());
	CumulativeResourceSize.AddDedicatedSystemMemoryBytes(HairGroupsData.GetAllocatedSize());
	
	for (const FHairGroupData & GroupData : HairGroupsData)
	{		
		CumulativeResourceSize.AddDedicatedVideoMemoryBytes(GroupData.Guides.GetResourcesSize());
		CumulativeResourceSize.AddDedicatedVideoMemoryBytes(GroupData.Strands.GetResourcesSize());
		CumulativeResourceSize.AddDedicatedVideoMemoryBytes(GroupData.Cards.GetResourcesSize());
		CumulativeResourceSize.AddDedicatedVideoMemoryBytes(GroupData.Meshes.GetResourcesSize());

		CumulativeResourceSize.AddDedicatedSystemMemoryBytes(GroupData.Guides.GetDataSize());
		CumulativeResourceSize.AddDedicatedSystemMemoryBytes(GroupData.Strands.GetDataSize());
		CumulativeResourceSize.AddDedicatedSystemMemoryBytes(GroupData.Cards.GetDataSize());
		CumulativeResourceSize.AddDedicatedSystemMemoryBytes(GroupData.Meshes.GetDataSize());
	}
}

#undef LOCTEXT_NAMESPACE

