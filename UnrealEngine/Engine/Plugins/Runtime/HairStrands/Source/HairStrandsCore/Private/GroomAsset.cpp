// Copyright Epic Games, Inc. All Rights Reserved.

#include "GroomAsset.h"

#include "Async/Async.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
#include "GroomAssetImportData.h"
#include "GroomBuilder.h"
#include "HairCardsBuilder.h"
#include "GroomImportOptions.h"
#include "GroomSettings.h"
#include "Materials/MaterialInterface.h"
#include "RenderingThread.h"
#include "Engine/AssetUserData.h"
#include "HairStrandsVertexFactory.h"
#include "HAL/LowLevelMemTracker.h"
#include "Misc/App.h"
#include "Misc/Paths.h"
#include "RHIStaticStates.h"
#include "Serialization/LargeMemoryReader.h"
#include "Serialization/LargeMemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "UObject/AnimObjectVersion.h"
#include "UObject/Package.h"
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
#include "UObject/UObjectIterator.h"
#include "RenderGraphBuilder.h"
#include "UObject/DevObjectVersion.h"

#if WITH_EDITORONLY_DATA
#include "DerivedDataCache.h"
#include "DerivedDataRequestOwner.h"
#endif

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

void UpdateHairStrandsVerbosity(IConsoleVariable* InCVarVerbosity);
static TAutoConsoleVariable<int32> GHairStrandsWarningLogVerbosity(
	TEXT("r.HairStrands.Log"),
	-1,
	TEXT("Enable warning log report for groom related asset (0: no logging, 1: error only, 2: error & warning only, other: all logs). By default all logging are enabled (-1). Value needs to be set at startup time."),
	FConsoleVariableDelegate::CreateStatic(UpdateHairStrandsVerbosity));

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

uint32 FHairGroupPlatformData::FStrands::GetDataSize() const
{
	uint32 Total = 0;
	Total += FBaseWithInterpolation::GetDataSize();
	uint32 CTotal = 0;
	uint32 BTotal = 0;
	BTotal += ClusterBulkData.Data.CurveToClusterIds.IsBulkDataLoaded() ? ClusterBulkData.Data.CurveToClusterIds.GetBulkDataSize() : 0;
	CTotal += ClusterBulkData.Data.PackedClusterInfos.IsBulkDataLoaded()? ClusterBulkData.Data.PackedClusterInfos.GetBulkDataSize() : 0;
	CTotal += ClusterBulkData.Data.PointLODs.IsBulkDataLoaded()	? ClusterBulkData.Data.PointLODs.GetBulkDataSize() : 0;

	return BTotal + Total + CTotal;
}

FGroomAssetMemoryStats FGroomAssetMemoryStats::Get(const FHairGroupPlatformData& In)
{
	FGroomAssetMemoryStats Out;
	Out.CPU.Guides  = In.Guides.GetDataSize();
	Out.CPU.Strands = In.Strands.GetDataSize();
	Out.CPU.Cards   = In.Cards.GetDataSize();
	Out.CPU.Meshes  = In.Meshes.GetDataSize();

	Out.GPU.Guides  = In.Guides.GetResourcesSize();
	Out.GPU.Strands = In.Strands.GetResourcesSize();
	Out.GPU.Cards   = In.Cards.GetResourcesSize();
	Out.GPU.Meshes  = In.Meshes.GetResourcesSize();

	
	// Strands only
	if (In.Strands.RestResource) 			Out.Memory.Rest 			= In.Strands.RestResource->GetResourcesSize();
	if (In.Strands.InterpolationResource) 	Out.Memory.Interpolation 	= In.Strands.InterpolationResource->GetResourcesSize();
	if (In.Strands.ClusterResource)			Out.Memory.Cluster 			= In.Strands.ClusterResource->GetResourcesSize();
#if RHI_RAYTRACING
	if (In.Strands.RaytracingResource) 		Out.Memory.Raytracing 		= In.Strands.RaytracingResource->GetResourcesSize();
#endif
	if (In.Strands.RestResource) 			Out.Curves.Rest 			= In.Strands.RestResource->MaxAvailableCurveCount;
	if (In.Strands.InterpolationResource) 	Out.Curves.Interpolation 	= In.Strands.InterpolationResource->MaxAvailableCurveCount;
	if (In.Strands.ClusterResource)			Out.Curves.Cluster 			= In.Strands.ClusterResource->MaxAvailableCurveCount;
#if RHI_RAYTRACING
	if (In.Strands.RaytracingResource)		Out.Curves.Raytracing 		= In.Strands.RaytracingResource->MaxAvailableCurveCount;
#endif
	return Out;
}

void FGroomAssetMemoryStats::Accumulate(const FGroomAssetMemoryStats& In)
{
	CPU.Guides += In.CPU.Guides ;
	CPU.Strands+= In.CPU.Strands;
	CPU.Cards  += In.CPU.Cards  ;
	CPU.Meshes += In.CPU.Meshes ;

	GPU.Guides += In.GPU.Guides ;
	GPU.Strands+= In.GPU.Strands;
	GPU.Cards  += In.GPU.Cards  ;
	GPU.Meshes += In.GPU.Meshes ;
}
uint32 FGroomAssetMemoryStats::GetTotalCPUSize() const
{
	uint32 Out = 0;
	Out += GPU.Guides ;
	Out += GPU.Strands;
	Out += GPU.Cards  ;
	Out += GPU.Meshes ;
	return Out;
}

uint32 FGroomAssetMemoryStats::GetTotalGPUSize() const
{
	uint32 Out = 0;
	Out += CPU.Guides ;
	Out += CPU.Strands;
	Out += CPU.Cards  ;
	Out += CPU.Meshes ;
	return Out;
}

void DumpLoadedGroomAssets(IConsoleVariable* InCVarPakTesterEnabled)
{
	const bool bDetails = true;
	const float ToMb = 1.f / 1000000.f;

	uint32 Total_Asset = 0;
	uint32 Total_Group = 0;

	FGroomAssetMemoryStats Total;

	UE_LOG(LogHairStrands, Log, TEXT("[Groom] ##### UGroomAssets #####"));
	UE_LOG(LogHairStrands, Log, TEXT("----------------------------------------------------------------------------------------------------------------------------------------------------------------"));
	UE_LOG(LogHairStrands, Log, TEXT("--  No.  - LOD -    CPU Total (     Guides|    Strands|      Cards|     Meshes) -    GPU Total (     Guides|    Strands|      Cards|     Meshes) - Asset Name "));
	UE_LOG(LogHairStrands, Log, TEXT("----------------------------------------------------------------------------------------------------------------------------------------------------------------"));
	FString OutputData;
	for (TObjectIterator<UGroomAsset> AssetIt; AssetIt; ++AssetIt)
	{
		if (AssetIt)
		{
			const uint32 GroupCount = AssetIt->GetHairGroupsPlatformData().Num();
			for (uint32 GroupIt = 0; GroupIt < GroupCount; ++GroupIt)
			{
				const FGroomAssetMemoryStats Group = FGroomAssetMemoryStats::Get(AssetIt->GetHairGroupsPlatformData()[GroupIt]);
				Total.Accumulate(Group);
				const uint32 LODCount = AssetIt->GetHairGroupsLOD()[GroupIt].LODs.Num();
				if (bDetails)
				{
//					UE_LOG(LogHairStrands, Log, TEXT("--  No.  - LOD -    CPU Total (     Guides|    Strands|      Cards|     Meshes) -    GPU Total (     Guides|    Strands|      Cards|     Meshes) - Asset Name "));
//					UE_LOG(LogHairStrands, Log, TEXT("-- 00/00 -  00 -  0000.0Mb (0000.0Mb |0000.0Mb|0000.0Mb |0000.0Mb) -  0000.0Mb (0000.0Mb|0000.0Mb|0000.0Mb|0000.0Mb) - AssetName"));
					UE_LOG(LogHairStrands, Log, TEXT("-- %2d/%2d -  %2d -  %9.3fMb (%9.3fMb|%9.3fMb|%9.3fMb|%9.3fMb) -  %9.3fMb (%9.3fMb|%9.3fMb|%9.3fMb|%9.3fMb) - %s"), 
						GroupIt,
						GroupCount,
						LODCount, 
						Group.GetTotalCPUSize() * ToMb,
						Group.CPU.Guides * ToMb,
						Group.CPU.Strands* ToMb,
						Group.CPU.Cards  * ToMb,
						Group.CPU.Meshes * ToMb,
                        Group.GetTotalGPUSize()* ToMb,
						Group.GPU.Guides * ToMb,
						Group.GPU.Strands* ToMb,
						Group.GPU.Cards  * ToMb,
						Group.GPU.Meshes * ToMb,
						GroupIt == 0 ? *AssetIt->GetPathName() : TEXT("."));
				}
			}

			Total_Asset++;
			Total_Group += GroupCount;
		}
	}

	UE_LOG(LogHairStrands, Log, TEXT("----------------------------------------------------------------------------------------------------------------------------------------------------------------"));
	UE_LOG(LogHairStrands, Log, TEXT("-- A:%3d|G:%3d -  %9.3fMb (%9.3fMb|%9.3fMb|%9.3fMb|%9.3fMb) -  %9.3fMb (%9.3fMb|%9.3fMb|%9.3fMb|%9.3fMb)"),
		Total_Asset,
		Total_Group,
		Total.GetTotalCPUSize() * ToMb,
		Total.CPU.Guides * ToMb,
		Total.CPU.Strands * ToMb,
		Total.CPU.Cards * ToMb,
		Total.CPU.Meshes * ToMb,
        Total.GetTotalGPUSize() * ToMb,
		Total.GPU.Guides * ToMb,
		Total.GPU.Strands * ToMb,
		Total.GPU.Cards * ToMb,
		Total.GPU.Meshes * ToMb);
}

FGroomBindingAssetMemoryStats FGroomBindingAssetMemoryStats::Get(const UGroomBindingAsset::FHairGroupPlatformData& InCPU, const UGroomBindingAsset::FHairGroupResource& InGPU)
{
	FGroomBindingAssetMemoryStats Out;
	Out.CPU.Guides  = InCPU.SimRootBulkData.GetDataSize();
	Out.CPU.Strands = InCPU.RenRootBulkData.GetDataSize();

	if (const FHairStrandsRestRootResource* R = InGPU.SimRootResources)
	{
		Out.GPU.Guides += R->GetResourcesSize();
	}
	if (const FHairStrandsRestRootResource* R = InGPU.RenRootResources)
	{
		Out.GPU.Strands += R->GetResourcesSize();
	}

	const uint32 CardCount = InCPU.CardsRootBulkData.Num();
	for (uint32 CardIt = 0; CardIt < CardCount; ++CardIt)
	{
		Out.CPU.Cards += InCPU.CardsRootBulkData[CardIt].GetDataSize();
		if (const FHairStrandsRestRootResource* R = InGPU.CardsRootResources[CardIt])
		{
			Out.GPU.Cards += R->GetResourcesSize();
		}
	}

	return Out;
}

void FGroomBindingAssetMemoryStats::Accumulate(const FGroomBindingAssetMemoryStats& In)
{
	CPU.Guides += In.CPU.Guides ;
	CPU.Strands+= In.CPU.Strands;
	CPU.Cards  += In.CPU.Cards  ;

	GPU.Guides += In.GPU.Guides ;
	GPU.Strands+= In.GPU.Strands;
	GPU.Cards  += In.GPU.Cards  ;
}
uint32 FGroomBindingAssetMemoryStats::GetTotalCPUSize() const
{
	uint32 Out = 0;
	Out += GPU.Guides ;
	Out += GPU.Strands;
	Out += GPU.Cards  ;
	return Out;
}

uint32 FGroomBindingAssetMemoryStats::GetTotalGPUSize() const
{
	uint32 Out = 0;
	Out += CPU.Guides ;
	Out += CPU.Strands;
	Out += CPU.Cards  ;
	return Out;
}

void DumpLoadedGroomBindingAssets(IConsoleVariable* InCVarPakTesterEnabled)
{
	const bool bDetails = true;
	const float ToMb = 1.f / 1000000.f;

	uint32 Total_Asset = 0;
	uint32 Total_Group = 0;

	FGroomBindingAssetMemoryStats Total;
	
	UE_LOG(LogHairStrands, Log, TEXT("[Groom] ##### UGroomBindingAssets #####"));
	UE_LOG(LogHairStrands, Log, TEXT("----------------------------------------------------------------------------------------------------------------------------------------------------------------"));
	UE_LOG(LogHairStrands, Log, TEXT("--  No.  - LOD -    CPU Total (     Guides|    Strands|      Cards) -    GPU Total (     Guides|    Strands|      Cards) - Asset Name "));
	UE_LOG(LogHairStrands, Log, TEXT("----------------------------------------------------------------------------------------------------------------------------------------------------------------"));
	FString OutputData;
	for (TObjectIterator<UGroomBindingAsset> AssetIt; AssetIt; ++AssetIt)
	{
		if (AssetIt)
		{			
			const uint32 GroupCount = AssetIt->GetHairGroupsPlatformData().Num();
			for (uint32 GroupIt = 0; GroupIt < GroupCount; ++GroupIt)
			{
				const FGroomBindingAssetMemoryStats Group = FGroomBindingAssetMemoryStats::Get(AssetIt->GetHairGroupsPlatformData()[GroupIt], AssetIt->GetHairGroupResources()[GroupIt]);
				Total.Accumulate(Group);

				const uint32 SkelLODCount = AssetIt->GetHairGroupsPlatformData()[GroupIt].RenRootBulkData.GetLODCount();
				if (bDetails)
				{
//					UE_LOG(LogHairStrands, Log, TEXT("--  No.  - LOD -    CPU Total (     Guides|    Strands|      Cards) -    GPU Total (     Guides|    Strands|      Cards) - Asset Name "));
//					UE_LOG(LogHairStrands, Log, TEXT("-- 00/00 -  00 -  0000.0Mb (0000.0Mb |0000.0Mb|0000.0Mb) -  0000.0Mb (0000.0Mb|0000.0Mb|0000.0Mb) - AssetName"));
					UE_LOG(LogHairStrands, Log, TEXT("-- %2d/%2d -  %2d -  %9.3fMb (%9.3fMb|%9.3fMb|%9.3fMb) -  %9.3fMb (%9.3fMb|%9.3fMb|%9.3fMb) - %s"), 
						GroupIt,
						GroupCount,
						SkelLODCount,
                        Group.GetTotalCPUSize() * ToMb,
						Group.CPU.Guides * ToMb,
						Group.CPU.Strands* ToMb,
						Group.CPU.Cards  * ToMb,
                        Group.GetTotalGPUSize()* ToMb,
						Group.GPU.Guides * ToMb,
						Group.GPU.Strands* ToMb,
						Group.GPU.Cards  * ToMb,
						GroupIt == 0 ? *AssetIt->GetPathName() : TEXT("."));
				}
			}

			Total_Asset++;
			Total_Group += GroupCount;
		}
	}
	UE_LOG(LogHairStrands, Log, TEXT("----------------------------------------------------------------------------------------------------------------------------------------------------------------"));
	UE_LOG(LogHairStrands, Log, TEXT("-- A:%3d|G:%3d -  %9.3fMb (%9.3fMb|%9.3fMb|%9.3fMb) -  %9.3fMb (%9.3fMb|%9.3fMb|%9.3fMb)"),
		Total_Asset,
		Total_Group,
		Total.GetTotalCPUSize() * ToMb,
		Total.CPU.Guides * ToMb,
		Total.CPU.Strands * ToMb,
		Total.CPU.Cards * ToMb,
		Total.GetTotalGPUSize() * ToMb,
		Total.GPU.Guides * ToMb,
		Total.GPU.Strands * ToMb,
		Total.GPU.Cards * ToMb);
}

void DumpLoadedGroomData(IConsoleVariable* InCVarPakTesterEnabled)
{
	DumpLoadedGroomAssets(InCVarPakTesterEnabled);
	DumpLoadedGroomBindingAssets(InCVarPakTesterEnabled);
	DumpLoadedGroomComponent(InCVarPakTesterEnabled);
}

/////////////////////////////////////////////////////////////////////////////////////////

void UpdateHairStrandsVerbosity(IConsoleVariable* InCVarVerbosity)
{
	const int32 Verbosity = InCVarVerbosity->GetInt();
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
	const TArray<FHairGroupsInterpolation>& InHairGroupsInterpolation,
	const TArray<FHairGroupsLOD>& InHairGroupsLOD,
	TArray<FHairGroupInfoWithVisibility>& OutHairGroupsInfo,
	TArray<FHairGroupPlatformData>& OutHairGroupsData)
{
	const bool bIsValid = HairDescriptionGroups.IsValid();
	if (bIsValid)
	{
		const FHairDescriptionGroup& HairGroup = HairDescriptionGroups.HairGroups[GroupIndex];
		check(GroupIndex <= HairDescriptionGroups.HairGroups.Num());
		check(GroupIndex == HairGroup.Info.GroupID);

		FHairStrandsDatas StrandsData;
		FHairStrandsDatas GuidesData;
		FGroomBuilder::BuildData(HairGroup, InHairGroupsInterpolation[GroupIndex], OutHairGroupsInfo[GroupIndex], StrandsData, GuidesData);

		FGroomBuilder::BuildBulkData(HairGroup.Info, GuidesData, OutHairGroupsData[GroupIndex].Guides.BulkData);
		FGroomBuilder::BuildBulkData(HairGroup.Info, StrandsData, OutHairGroupsData[GroupIndex].Strands.BulkData);

		// If there is no simulation or no global interpolation on that group there is no need for builder the interpolation data
		if (bNeedInterpolationData)
		{
			FHairStrandsInterpolationDatas InterpolationData;
			FGroomBuilder::BuildInterplationData(HairGroup.Info, StrandsData, GuidesData, InHairGroupsInterpolation[GroupIndex].InterpolationSettings, InterpolationData);
			FGroomBuilder::BuildInterplationBulkData(GuidesData, InterpolationData, OutHairGroupsData[GroupIndex].Strands.InterpolationBulkData);
		}
		else
		{
			OutHairGroupsData[GroupIndex].Strands.InterpolationBulkData.Reset();
		}

		FGroomBuilder::BuildClusterBulkData(StrandsData, HairDescriptionGroups.Bounds.SphereRadius, InHairGroupsLOD[GroupIndex], OutHairGroupsData[GroupIndex].Strands.ClusterBulkData);
	}
	return bIsValid;
}

// Build: 
// * Clusters (bulk) data
//
// Guides and strands data are all transient
static bool BuildHairGroupCluster(
	const int32 GroupIndex,
	const FHairDescriptionGroups& InHairDescriptionGroups,
	const TArray<FHairGroupsInterpolation>& InHairGroupsInterpolation,
	const TArray<FHairGroupsLOD>& InHairGroupsLOD,
	TArray<FHairGroupInfoWithVisibility>& OutHairGroupsInfo,
	TArray<FHairGroupPlatformData>& OutHairGroupsData)
{
	const bool bIsValid = InHairDescriptionGroups.IsValid();
	if (bIsValid)
	{
		const FHairDescriptionGroup& HairGroup = InHairDescriptionGroups.HairGroups[GroupIndex];
		check(GroupIndex <= InHairDescriptionGroups.HairGroups.Num());
		check(GroupIndex == HairGroup.Info.GroupID);

		FHairStrandsDatas StrandsData;
		FHairStrandsDatas GuidesData;
		FGroomBuilder::BuildData(HairGroup, InHairGroupsInterpolation[GroupIndex], OutHairGroupsInfo[GroupIndex], StrandsData, GuidesData);
		FGroomBuilder::BuildClusterBulkData(StrandsData, InHairDescriptionGroups.Bounds.SphereRadius, InHairGroupsLOD[GroupIndex], OutHairGroupsData[GroupIndex].Strands.ClusterBulkData);
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
		const bool bDisableMinLODStrip = GetDisableBelowMinLodStripping().GetValueForPlatform(*CookTarget->IniPlatformName());
		int32 PlatformMinLOD = 0;
		if (!bDisableMinLODStrip)
		{
			PlatformMinLOD = GetMinLOD().GetValueForPlatform(*CookTarget->IniPlatformName());
		}

		bIsLODStripped = (PlatformMinLOD > 0) && !bDisableMinLODStrip;
		if (bIsLODStripped)
		{
			for (const FHairGroupsLOD& GroupsLOD : GetHairGroupsLOD())
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

	const bool bDisableMinLodStrip = GetDisableBelowMinLodStripping().GetValueForPlatform(*CookTarget->IniPlatformName()); 
	int32 PlatformMinLOD = 0;
	if (!bDisableMinLodStrip)
	{
		PlatformMinLOD = GetMinLOD().GetValueForPlatform(*CookTarget->IniPlatformName());
	}

	const bool bIsStrandsStrippedForCook = !!(StripFlags & CDSF_StrandsStripped);

	// Set the CookedOut flags as appropriate and compute the value for EffectiveLODBias
	for (int32 GroupIndex = 0; GroupIndex < GetHairGroupsPlatformData().Num(); ++GroupIndex)
	{
		FHairGroupPlatformData& GroupData = GetHairGroupsPlatformData()[GroupIndex];
		GroupData.Strands.bIsCookedOut = bIsStrandsStrippedForCook;

		// Determine the max LOD for strands since they are cooked out if the platform doesn't support them
		// They are cooked out as a whole
		int32 StrandsMaxLOD = -1;
		if (bIsStrandsStrippedForCook)
		{
			const FHairGroupsLOD& GroupsLOD = GetHairGroupsLOD()[GroupIndex];
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
		GetEffectiveLODBias()[GroupIndex] = MinStrippedLOD;

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
static void InternalSerializeCards(FArchive& Ar, UObject* Owner, TArray<FHairGroupPlatformData::FCards::FLOD>& CardLODData);
static void InternalSerializeMeshes(FArchive& Ar, UObject* Owner, TArray<FHairGroupPlatformData::FMeshes::FLOD>& MeshLODData);
static void InternalSerializePlatformDatas(FArchive& Ar, UObject* Owner, TArray<FHairGroupPlatformData>& GroupData);

// Serialization for hair elements
static void InternalSerializeCard(FArchive& Ar, UObject* Owner, FHairGroupPlatformData::FCards::FLOD& CardLODData);
static void InternalSerializeMesh(FArchive& Ar, UObject* Owner, FHairGroupPlatformData::FMeshes::FLOD& MeshLODData);
static void InternalSerializeGuide(FArchive& Ar, UObject* Owner, FHairGroupPlatformData::FGuides& GuideData);
static void InternalSerializeStrand(FArchive& Ar, UObject* Owner, FHairGroupPlatformData::FStrands& StrandData, bool bHeader, bool bData, bool* bOutHasDataInCache=nullptr);
static void InternalSerializePlatformData(FArchive& Ar, UObject* Owner, FHairGroupPlatformData& GroupData);

// This dummy serialization function is only intended to support *loading* of legacy content 
static void DummySerizalizationForLegacyPurpose(FArchive& Ar)
{
	// Only support loading of legacy content 
	check(Ar.IsLoading());
	Ar.UsingCustomVersion(FReleaseObjectVersion::GUID);
	Ar.UsingCustomVersion(FUE5ReleaseStreamObjectVersion::GUID);

	uint32 GroupCount = 0;					Ar << GroupCount;
	for (uint32 GroupIt = 0; GroupIt < GroupCount; ++GroupIt)
	{
		// Strands
		{
			uint32 CurveCount = 0;			Ar << CurveCount;
			uint32 PointCount = 0;			Ar << PointCount;
			float MaxLength = 0;			Ar << MaxLength;
			float MaxRadius = 0;			Ar << MaxRadius;
			FBox BoundingBox;				Ar << BoundingBox;
			uint32 Flags = 0;				Ar << Flags;
			uint32 AttributeOffsets[7] = { 0 };
			for (uint8 It = 0; It < 7; ++It)
			{
				Ar << AttributeOffsets[It];
			}
		}

		// Guides
		{
			uint32 CurveCount = 0;			Ar << CurveCount;
			uint32 PointCount = 0;			Ar << PointCount;
			float MaxLength = 0;			Ar << MaxLength;
			float MaxRadius = 0;			Ar << MaxRadius;
			FBox BoundingBox;				Ar << BoundingBox;
			uint32 Flags = 0;				Ar << Flags;
			uint32 AttributeOffsets[7] = { 0 };
			for (uint8 It = 0; It < 7; ++It)
			{
				Ar << AttributeOffsets[It];
			}
		}

		// Interpolation
		{
			uint32 Flags = 0;				Ar << Flags;
			uint32 PointCount = 0;			Ar << PointCount;
			uint32 SimPointCount = 0;		Ar << SimPointCount;
		}

		// Cluster
		{
			uint32 ClusterCount = 0;		Ar << ClusterCount;
			uint32 ClusterLODCount = 0;		Ar << ClusterLODCount;
			uint32 VertexCount = 0;			Ar << VertexCount;
			uint32 VertexLODCount = 0;		Ar << VertexLODCount;
			TArray<bool> LODVisibility;		Ar << LODVisibility;
			TArray<float>CPULODScreenSize;	Ar << CPULODScreenSize;
		}

		// Misc
		{
			bool bIsCooked; 				Ar << bIsCooked;
		}
	}
}

void UGroomAsset::Serialize(FArchive& Ar)
{
	uint8 ClassDataStripFlags = GenerateClassStripFlags(Ar);
	ApplyStripFlags(ClassDataStripFlags, Ar.CookingTarget());

	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FPhysicsObjectVersion::GUID);
	Ar.UsingCustomVersion(FReleaseObjectVersion::GUID);			// Needed to support MeshDescription AttributesSet serialization
	Ar.UsingCustomVersion(FUE5MainStreamObjectVersion::GUID);	// Needed to support MeshDescription AttributesSet serialization
	Ar.UsingCustomVersion(FAnimObjectVersion::GUID);    // Needed to support Cards and Cluster culling serialization

	// For older assets, force width override to preserve prior to fixing the imported groom's width.
	if (Ar.CustomVer(FUE5MainStreamObjectVersion::GUID) < FUE5MainStreamObjectVersion::GroomAssetWidthOverride && Ar.IsLoading())
	{
		for (FHairGroupsRendering& Group : GetHairGroupsRendering())
		{
			Group.GeometrySettings.HairWidth_Override = true;
		}
	}

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


		if (StripFlags.IsEditorDataStripped())
		{
			// When cooking data or loading cooked data
			InternalSerializePlatformDatas(Ar, this, GetHairGroupsPlatformData());
		}
		else if (Ar.CustomVer(FUE5MainStreamObjectVersion::GUID) < FUE5MainStreamObjectVersion::GroomAssetRemoveInAssetSerialization && Ar.IsLoading() && !bHasImportedStrands)
		{
			// Legacy support for groom asset not having strands data
			DummySerizalizationForLegacyPurpose(Ar);
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

	SetMinLOD(0);
	SetDisableBelowMinLodStripping(false);
}

bool UGroomAsset::HasGeometryType(uint32 GroupIndex, EGroomGeometryType Type) const
{
	check(GroupIndex < uint32(GetHairGroupsLOD().Num()));
	const uint32 LODCount = GetHairGroupsLOD()[GroupIndex].LODs.Num();
	for (uint32 LODIt=0;LODIt<LODCount;++LODIt)
	{
		if (GetHairGroupsLOD()[GroupIndex].LODs[LODIt].GeometryType == Type)
			return true;
	}
	return false;
}

bool UGroomAsset::HasGeometryType(EGroomGeometryType Type) const
{
	const uint32 GroupCount = GetHairGroupsLOD().Num();
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
		VFsPerMaterial->VertexFactoryDataList.AddUnique(FPSOPrecacheVertexFactoryData(InVFType));
	};

	const int32 GroupCount = GetNumHairGroups();
	for (int32 GroupIt = 0; GroupIt < GroupCount; GroupIt++)
	{
		const FHairGroupPlatformData& InGroupData = GetHairGroupsPlatformData()[GroupIt];

		// Strands
		{
			const int32 MaterialIndex = GetMaterialIndex(GetHairGroupsRendering()[GroupIt].MaterialSlotName);
			AddVFPerMaterial(MaterialIndex, EHairGeometryType::Strands, &FHairStrandsVertexFactory::StaticType);
		}

		// Cards
		if (IsHairStrandsEnabled(EHairStrandsShaderType::Cards, ShaderPlatform))
		{
			uint32 CardsLODIndex = 0;
			for (const FHairGroupPlatformData::FCards::FLOD& LOD : InGroupData.Cards.LODs)
			{
				if (LOD.IsValid())
				{
					// Material
					int32 MaterialIndex = INDEX_NONE;
					for (const FHairGroupsCardsSourceDescription& Desc : GetHairGroupsCards())
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
			for (const FHairGroupPlatformData::FMeshes::FLOD& LOD : InGroupData.Meshes.LODs)
			{
				if (LOD.IsValid())
				{
					// Material
					int32 MaterialIndex = INDEX_NONE;
					for (const FHairGroupsMeshesSourceDescription& Desc : GetHairGroupsMeshes())
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
		uint32 ChangeType = (CachedHairGroupsLOD[GroupIndex] == GetHairGroupsLOD()[GroupIndex] ? 0 : GroomChangeType_LOD);
		AllChangeType = AllChangeType | ChangeType;

		check(GroupIndex < uint32(GetNumHairGroups()));
		FHairGroupPlatformData& GroupData = GetHairGroupsPlatformData()[GroupIndex];
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
					GetHairGroupsInterpolation(),
					GetHairGroupsLOD(),
					GetHairGroupsInfo(),
					GetHairGroupsPlatformData());

				GroupData.Strands.ClusterResource = new FHairStrandsClusterResource(GroupData.Strands.ClusterBulkData, FHairResourceName(GetFName(), GroupIndex), GetAssetPathName());
			}
			else
			{
				InternalUpdateResource(GroupData.Strands.ClusterResource);
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

	if (BuildCardsData())  { AllChangeType |= GroomChangeType_Cards;  }
	if (BuildMeshesData()) { AllChangeType |= GroomChangeType_Meshes; }

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
	if (GroupIndex >= uint32(GetHairGroupsPlatformData().Num())) return;
	FHairGroupPlatformData& GroupData = GetHairGroupsPlatformData()[GroupIndex];
	if (GroupData.Guides.IsValid())
	{
		InternalReleaseResource(GroupData.Guides.RestResource);
	}
}

void UGroomAsset::ReleaseStrandsResource(uint32 GroupIndex)
{
	if (GroupIndex >= uint32(GetHairGroupsPlatformData().Num())) return;
	FHairGroupPlatformData& GroupData = GetHairGroupsPlatformData()[GroupIndex];
	if (GroupData.Strands.IsValid())
	{
		InternalReleaseResource(GroupData.Strands.RestResource);
		InternalReleaseResource(GroupData.Strands.ClusterResource);
		InternalReleaseResource(GroupData.Strands.InterpolationResource);
		#if RHI_RAYTRACING
		InternalReleaseResource(GroupData.Strands.RaytracingResource);
		#endif
	}
}

void UGroomAsset::ReleaseCardsResource(uint32 GroupIndex)
{
	if (GroupIndex >= uint32(GetHairGroupsPlatformData().Num())) return;
	FHairGroupPlatformData& GroupData = GetHairGroupsPlatformData()[GroupIndex];
	for (FHairGroupPlatformData::FCards::FLOD& LOD : GroupData.Cards.LODs)
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
	if (GroupIndex >= uint32(GetHairGroupsPlatformData().Num())) return;
	FHairGroupPlatformData& GroupData = GetHairGroupsPlatformData()[GroupIndex];
	for (FHairGroupPlatformData::FMeshes::FLOD& LOD : GroupData.Meshes.LODs)
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
	const bool bForceReset = GetHairGroupsInfo().Num() != GroupCount;
	GetHairGroupsInfo().SetNum(GroupCount);

	uint32 GroupIndex = 0;
	for (FHairGroupPlatformData& Data: GetHairGroupsPlatformData())
	{
		FHairGroupInfoWithVisibility& Info = GetHairGroupsInfo()[GroupIndex];
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
bool ConvertMaterial(TArray<T>& Groups, TArray<FHairGroupsMaterial>& InHairGroupsMaterials)
{
	bool bNeedSaving = false;
	for (T& Group : Groups)
	{
		if (Group.Material)
		{
			FName MaterialSlotName = Group.Material->GetFName();
			bool bFound = false;
			for (FHairGroupsMaterial& Material : InHairGroupsMaterials)
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
				FHairGroupsMaterial& MaterialEntry = InHairGroupsMaterials.AddDefaulted_GetRef();
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
	const bool bIsLegacyAsset = GetHairGroupsInterpolation().Num() == 0;
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
	// Convert old rigging/guide override
	for (FHairGroupsInterpolation& Group : GetHairGroupsInterpolation())
	{
		const bool bNeedLegacyConversion = Group.InterpolationSettings.bOverrideGuides_DEPRECATED || Group.RiggingSettings.bEnableRigging_DEPRECATED;
		if (bNeedLegacyConversion)
		{				
			if (Group.RiggingSettings.bEnableRigging_DEPRECATED && Group.InterpolationSettings.bOverrideGuides_DEPRECATED)
			{
				Group.InterpolationSettings.GuideType = EGroomGuideType::Rigged;
			}
			else if (Group.InterpolationSettings.bOverrideGuides_DEPRECATED)
			{
				Group.InterpolationSettings.GuideType = EGroomGuideType::Generated;
			}

			Group.InterpolationSettings.RiggedGuideNumCurves = Group.RiggingSettings.NumCurves_DEPRECATED;
			Group.InterpolationSettings.RiggedGuideNumPoints = Group.RiggingSettings.NumPoints_DEPRECATED;

			// Reset deprecated values, so that if the asset is saved, the legacy conversion will no longer occur
			Group.InterpolationSettings.bOverrideGuides_DEPRECATED = false;
			Group.RiggingSettings.bEnableRigging_DEPRECATED = false;
			Group.RiggingSettings.NumCurves_DEPRECATED = 0;
			Group.RiggingSettings.NumPoints_DEPRECATED = 0;
		}
	}

#if WITH_EDITORONLY_DATA
	bool bSucceed = true;
	{
		// Interpolation settings are used for building the interpolation data, and per se defined the number of groups
		const uint32 GroupCount = GetHairGroupsInterpolation().Num();
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

		const int32 GroupCount = GetHairGroupsInterpolation().Num();
		check(LocalHairDescriptionGroups.HairGroups.Num() == GroupCount);
		for (int32 GroupIndex = 0; GroupIndex < GroupCount; ++GroupIndex)
		{
			const bool bNeedInterpoldationData = NeedsInterpolationData(GroupIndex);
			const bool bNeedToBuildData = 
				(GetHairGroupsPlatformData()[GroupIndex].Guides.BulkData.GetNumCurves() == 0 ||
				 (bNeedInterpoldationData && GetHairGroupsPlatformData()[GroupIndex].Strands.InterpolationBulkData.GetPointCount() == 0)) &&
				 GetHairGroupsPlatformData()[GroupIndex].Strands.BulkData.GetNumCurves() > 0; // Empty groom has no data to build
			if (bNeedToBuildData)
			{
				BuildHairGroup(
					GroupIndex,
					bNeedInterpoldationData,
					LocalHairDescriptionGroups,
					GetHairGroupsInterpolation(),
					GetHairGroupsLOD(),
					GetHairGroupsInfo(),
					GetHairGroupsPlatformData());
			}
		}
	}
#endif

	// Convert all material data to new format
	{
		// Strands
		bool bNeedSaving = false;
		bNeedSaving = bNeedSaving || ConvertMaterial(GetHairGroupsRendering(),	GetHairGroupsMaterials());
		bNeedSaving = bNeedSaving || ConvertMaterial(GetHairGroupsCards(),		GetHairGroupsMaterials());
		bNeedSaving = bNeedSaving || ConvertMaterial(GetHairGroupsMeshes(),		GetHairGroupsMaterials());

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
		ERHIFeatureLevel::Type FeatureLevel = GetWorld() ? GetWorld()->GetFeatureLevel() : GMaxRHIFeatureLevel;
		EShaderPlatform ShaderPlatform = GetFeatureLevelShaderPlatform(FeatureLevel);

		TArray<FHairVertexFactoryTypesPerMaterialData> VFsPerMaterials = CollectVertexFactoryTypesPerMaterialData(ShaderPlatform);

		TArray<FMaterialPSOPrecacheRequestID> RequestIDs;
		FPSOPrecacheParams PrecachePSOParams;
		for (FHairVertexFactoryTypesPerMaterialData& VFsPerMaterial : VFsPerMaterials)
		{
			UMaterialInterface* MaterialInterface = GetHairGroupsMaterials()[VFsPerMaterial.MaterialIndex].Material;
			if (MaterialInterface)
			{
				MaterialInterface->PrecachePSOs(VFsPerMaterial.VertexFactoryDataList, PrecachePSOParams, EPSOPrecachePriority::Medium, RequestIDs);
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
		if (!(CachedHairGroupsInterpolation[GroupIt] == GetHairGroupsInterpolation()[GroupIt]))
		{
			ChangeType = ChangeType | GroomChangeType_Interpolation;
			break;
		}
	}

	for (uint32 GroupIt = 0; GroupIt < GroupCount; ++GroupIt)
	{
		if (!(CachedHairGroupsLOD[GroupIt] == GetHairGroupsLOD()[GroupIt]))
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
				const bool bHasInterpolationChanged = !(CachedHairGroupsInterpolation[GroupIt] == GetHairGroupsInterpolation()[GroupIt]);
				const bool bHasLODChanged = !(CachedHairGroupsLOD[GroupIt] == GetHairGroupsLOD()[GroupIt]);
				const bool bNeedInterpoldationData = NeedsInterpolationData(GroupIt);

				if (bHasInterpolationChanged)
				{
					BuildHairGroup(
						GroupIt,
						bNeedInterpoldationData,
						LocalHairDescriptionGroups,
						GetHairGroupsInterpolation(),
						GetHairGroupsLOD(),
						GetHairGroupsInfo(),
						GetHairGroupsPlatformData());
				}
				else if (bHasLODChanged)
				{
					BuildHairGroupCluster(
						GroupIt,
						LocalHairDescriptionGroups,
						GetHairGroupsInterpolation(),
						GetHairGroupsLOD(),
						GetHairGroupsInfo(),
						GetHairGroupsPlatformData());
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


		// Add dependency on simulation and per LOD-simulation/global-interpolation to strip-out interpolation data if there are not needed
		|| PropertyName == UGroomAsset::GetEnableGlobalInterpolationMemberName()
		|| PropertyName == UGroomAsset::GetEnableSimulationCacheMemberName()
		|| PropertyName == GET_MEMBER_NAME_CHECKED(FHairSolverSettings, EnableSimulation)

		// Decimation control needs to force a rebuild
		|| PropertyName == GET_MEMBER_NAME_CHECKED(FHairLODSettings, CurveDecimation)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(FHairLODSettings, VertexDecimation)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(FHairLODSettings, AngularThreshold)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(FHairLODSettings, ScreenSize)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(FHairLODSettings, ThicknessScale)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(FHairLODSettings, bVisible)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(FHairLODSettings, Simulation)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(FHairLODSettings, GlobalInterpolation)

		|| PropertyName == GET_MEMBER_NAME_CHECKED(FHairInterpolationSettings, GuideType)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(FHairInterpolationSettings, HairToGuideDensity)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(FHairInterpolationSettings, RiggedGuideNumCurves)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(FHairInterpolationSettings, RiggedGuideNumPoints)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(FHairInterpolationSettings, InterpolationQuality)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(FHairInterpolationSettings, InterpolationDistance)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(FHairInterpolationSettings, bRandomizeGuide)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(FHairInterpolationSettings, bUseUniqueGuide);
}

static bool IsStrandsLODAttributes(const FName PropertyName)
{
	return
		// LOD count needs for cluster rebuilding
		PropertyName == UGroomAsset::GetHairGroupsLODMemberName()

		// LOD groups needs for cluster rebuilding
		|| PropertyName == GET_MEMBER_NAME_CHECKED(FHairGroupsLOD, LODs)

		// LOD settings needs for cluster rebuilding
		|| PropertyName == GET_MEMBER_NAME_CHECKED(FHairLODSettings, CurveDecimation)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(FHairLODSettings, VertexDecimation)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(FHairLODSettings, AngularThreshold)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(FHairLODSettings, ScreenSize)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(FHairLODSettings, ThicknessScale)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(FHairLODSettings, bVisible)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(FHairLODSettings, GeometryType)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(FHairLODSettings, BindingType)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(FHairLODSettings, Simulation)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(FHairLODSettings, GlobalInterpolation);
}

static bool IsRiggingSettings(const FName PropertyName)
{
	return
		PropertyName == GET_MEMBER_NAME_CHECKED(FHairInterpolationSettings, GuideType) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(FHairInterpolationSettings, RiggedGuideNumCurves) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(FHairInterpolationSettings, RiggedGuideNumPoints);
}

static bool IsCardsAttributes(const FName PropertyName)
{
	return PropertyName == UGroomAsset::GetHairGroupsCardsMemberName();
}

static bool IsMeshesAttributes(const FName PropertyName)
{
	return PropertyName == UGroomAsset::GetHairGroupsMeshesMemberName();
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
		const uint32 GroupCount = GetHairGroupsPlatformData().Num();
		if (IsHairStrandsEnabled(EHairStrandsShaderType::Cards) && HasGeometryType(EGroomGeometryType::Cards) && GetHairGroupsCards().Num() == 0)
		{
			GetHairGroupsCards().Init(FHairGroupsCardsSourceDescription(), GroupCount);
			FHairGroupsCardsSourceDescription Dirty;
			Dirty.ProceduralSettings.ClusterSettings.ClusterDecimation = 0;
			Dirty.SourceType = HasImportedStrandsData() ? EHairCardsSourceType::Procedural : EHairCardsSourceType::Imported;
			CachedHairGroupsCards.Init(Dirty, GroupCount);
		}

		if (IsHairStrandsEnabled(EHairStrandsShaderType::Meshes) && HasGeometryType(EGroomGeometryType::Meshes) && GetHairGroupsMeshes().Num() == 0)
		{
			GetHairGroupsMeshes().Init(FHairGroupsMeshesSourceDescription(), GroupCount);
			FHairGroupsMeshesSourceDescription Dirty;
			Dirty.ImportedMesh = nullptr;
			CachedHairGroupsMeshes.Init(Dirty, GroupCount);
		}
	}

	// Rebuild the groom cached data if interpolation or LODs have changed
	const bool bNeedRebuildDerivedData = 
		IsStrandsInterpolationAttributes(PropertyName) || 
		IsStrandsLODAttributes(PropertyName) || 
		IsCardsAttributes(PropertyName) || 
		IsMeshesAttributes(PropertyName);
	if (bNeedRebuildDerivedData)
	{
		CacheDerivedDatas();
	}
	
	const bool bCardsArrayChanged = PropertyName == UGroomAsset::GetHairGroupsCardsMemberName();
	if (bCardsArrayChanged && PropertyChangedEvent.ChangeType == EPropertyChangeType::ArrayAdd)
	{
		GetHairGroupsCards().Last().SourceType = HasImportedStrandsData() ? EHairCardsSourceType::Procedural : EHairCardsSourceType::Imported;
	}

	SavePendingProceduralAssets();

	const bool bHairStrandsRaytracingRadiusChanged = PropertyName == GET_MEMBER_NAME_CHECKED(FHairShadowSettings, HairRaytracingRadiusScale);
	
	// By pass update for all procedural cards parameters, as we don't want them to invalidate the cards data. 
	// Cards should be refresh only under user action
	// By pass update if bStrandsInterpolationChanged has the resources have already been recreated
	const bool bCardsToolUpdate = IsCardsProceduralAttributes(PropertyName);
	if (!bCardsToolUpdate && !bNeedRebuildDerivedData)
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

	// If rigging options changed, recreate skeletal mesh asset
	const bool bRiggingChanged = IsRiggingSettings(PropertyName);
	if (bRiggingChanged)
	{
		bool bUpdateRiggedMesh = false;
		for (const FHairGroupsInterpolation& Interpolation : GetHairGroupsInterpolation())
		{
			bUpdateRiggedMesh |= Interpolation.InterpolationSettings.GuideType == EGroomGuideType::Rigged;
		}
		if (bUpdateRiggedMesh)
		{
			SetRiggedSkeletalMesh(FGroomDeformerBuilder::CreateSkeletalMesh(this));
		}
	}

	const bool bCardMaterialChanged = PropertyName == GET_MEMBER_NAME_CHECKED(FHairGroupsCardsSourceDescription, Material);
	const bool bMeshMaterialChanged = PropertyName == GET_MEMBER_NAME_CHECKED(FHairGroupsMeshesSourceDescription, Material);

	if (bNeedRebuildDerivedData || bGeometryTypeChanged || bCardMaterialChanged || bMeshMaterialChanged)
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
	return GetHairGroupsPlatformData().Num();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Array serialize
void InternalSerializeMeshes(FArchive& Ar, UObject* Owner, TArray<FHairGroupPlatformData::FMeshes::FLOD>& LODs)
{
	uint32 Count = LODs.Num();
	Ar << Count;
	if (Ar.IsLoading())
	{
		LODs.SetNum(Count);
	}
	for (uint32 MeshIt = 0; MeshIt < Count; ++MeshIt)
	{
		InternalSerializeMesh(Ar, Owner, LODs[MeshIt]);
	}
}

void InternalSerializeCards(FArchive& Ar, UObject* Owner, TArray<FHairGroupPlatformData::FCards::FLOD>& LODs)
{
	uint32 Count = LODs.Num();
	Ar << Count;
	if (Ar.IsLoading())
	{
		LODs.SetNum(Count);
	}
	for (uint32 MeshIt = 0; MeshIt < Count; ++MeshIt)
	{
		InternalSerializeCard(Ar, Owner, LODs[MeshIt]);
	}
}

void InternalSerializePlatformDatas(FArchive& Ar, UObject* Owner, TArray<FHairGroupPlatformData>& GroupDatas)
{
	uint32 GroupCount = GroupDatas.Num();
	Ar << GroupCount;
	if (Ar.IsLoading())
	{
		GroupDatas.SetNum(GroupCount);
	}
	for (uint32 GroupIt = 0; GroupIt < GroupCount; ++GroupIt)
	{
		InternalSerializePlatformData(Ar, Owner, GroupDatas[GroupIt]);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Element serialize
static void InternalSerializeCard(FArchive& Ar, UObject* Owner, FHairGroupPlatformData::FCards::FLOD& CardLODData)
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

		FHairGroupPlatformData::FBaseWithInterpolation NoGuideData;
		NoGuideData.BulkData.Serialize(Ar, Owner);
		NoGuideData.InterpolationBulkData.Serialize(Ar, Owner);
	}
}

static void InternalSerializeMesh(FArchive& Ar, UObject* Owner, FHairGroupPlatformData::FMeshes::FLOD& MeshLODData)
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

static void InternalSerializeGuide(FArchive& Ar, UObject* Owner, FHairGroupPlatformData::FGuides& GuideData)
{
	GuideData.BulkData.Serialize(Ar, Owner);
}

static void InternalSerializeStrand(FArchive& Ar, UObject* Owner, FHairGroupPlatformData::FStrands& StrandData, bool bHeader, bool bData, bool* bOutHasDataInCache)
{
	Ar.UsingCustomVersion(FAnimObjectVersion::GUID);

	// When cooking data, force loading of *all* bulk data prior to saving them
	// Note: bFillBulkdata is true for filling in the bulkdata container prior to serialization. This also forces the resources loading 
	// from the 'start' (i.e., without offset)
	const bool bFillInBulkDataForCooking = Ar.IsCooking() && Ar.IsSaving();
	if (bFillInBulkDataForCooking)
	{
		{ FHairStreamingRequest R; R.Request(HAIR_MAX_NUM_CURVE_PER_GROUP, HAIR_MAX_NUM_POINT_PER_GROUP, -1/*LODIndex*/, StrandData.BulkData,               true /*bWait*/, true /*bFillBulkdata*/, true /*bWarmCache*/, Owner->GetFName()); }
		{ FHairStreamingRequest R; R.Request(HAIR_MAX_NUM_CURVE_PER_GROUP, HAIR_MAX_NUM_POINT_PER_GROUP, -1/*LODIndex*/, StrandData.InterpolationBulkData,  true /*bWait*/, true /*bFillBulkdata*/, true /*bWarmCache*/, Owner->GetFName()); }
		{ FHairStreamingRequest R; R.Request(HAIR_MAX_NUM_CURVE_PER_GROUP, HAIR_MAX_NUM_POINT_PER_GROUP, -1/*LODIndex*/, StrandData.ClusterBulkData,        true /*bWait*/, true /*bFillBulkdata*/, true /*bWarmCache*/, Owner->GetFName()); }
	}

	if (!Ar.IsCooking() || !StrandData.bIsCookedOut)
	{
		if (bHeader)	{ StrandData.BulkData.SerializeHeader(Ar, Owner); }
		if (bData)		{ StrandData.BulkData.SerializeData(Ar, Owner); }
		if (bHeader)	{ StrandData.InterpolationBulkData.SerializeHeader(Ar, Owner); }
		if (bData)		{ StrandData.InterpolationBulkData.SerializeData(Ar, Owner); }
		if (Ar.CustomVer(FAnimObjectVersion::GUID) >= FAnimObjectVersion::SerializeHairClusterCullingData)
		{
			if (bHeader){ StrandData.ClusterBulkData.SerializeHeader(Ar, Owner); }
			if (bData)	{ StrandData.ClusterBulkData.SerializeData(Ar, Owner); }
		}

		#if WITH_EDITORONLY_DATA
		// Pre-warm DDC cache
		const bool bPreWarmCache = Ar.IsLoading() && bHeader && !bData;
		if (bPreWarmCache)
		{
			bool bHasDataInCache = true;
			{ FHairStreamingRequest R; bHasDataInCache &= R.WarmCache(HAIR_MAX_NUM_CURVE_PER_GROUP, HAIR_MAX_NUM_POINT_PER_GROUP, -1/*LODIndex*/, StrandData.BulkData); }
			{ FHairStreamingRequest R; bHasDataInCache &= R.WarmCache(HAIR_MAX_NUM_CURVE_PER_GROUP, HAIR_MAX_NUM_POINT_PER_GROUP, -1/*LODIndex*/, StrandData.InterpolationBulkData); }
			{ FHairStreamingRequest R; bHasDataInCache &= R.WarmCache(HAIR_MAX_NUM_CURVE_PER_GROUP, HAIR_MAX_NUM_POINT_PER_GROUP, -1/*LODIndex*/, StrandData.ClusterBulkData); }

			if (bOutHasDataInCache) { *bOutHasDataInCache = bHasDataInCache; }
		}
		#endif
	}
	else
	{
		// Fall back to no data, but still serialize guide data as they an be used for cards simulation
		// Theoritically, we should have something to detect if we are going to use or not guide (for 
		// simulation or RBF deformation) on the target platform
		FHairGroupPlatformData::FStrands NoStrandsData;
		if (bHeader) 	{ NoStrandsData.BulkData.SerializeHeader(Ar, Owner); }
		if (bData)		{ NoStrandsData.BulkData.SerializeData(Ar, Owner); }
		if (bHeader) 	{ NoStrandsData.InterpolationBulkData.SerializeHeader(Ar, Owner); }
		if (bData) 		{ NoStrandsData.InterpolationBulkData.SerializeData(Ar, Owner); }
		if (Ar.CustomVer(FAnimObjectVersion::GUID) >= FAnimObjectVersion::SerializeHairClusterCullingData)
		{
			if (bHeader){ NoStrandsData.ClusterBulkData.SerializeHeader(Ar, Owner); }
			if (bData)	{ NoStrandsData.ClusterBulkData.SerializeData(Ar, Owner); }
		}
	}
}

static void InternalSerializePlatformData(FArchive& Ar, UObject* Owner, FHairGroupPlatformData& GroupData)
{
	Ar.UsingCustomVersion(FAnimObjectVersion::GUID);

	InternalSerializeGuide(Ar, Owner, GroupData.Guides);
	InternalSerializeStrand(Ar, Owner, GroupData.Strands, true, true);

	if (Ar.CustomVer(FAnimObjectVersion::GUID) >= FAnimObjectVersion::SerializeGroomCardsAndMeshes)
	{
		bool bIsCooked = Ar.IsCooking();
		Ar << bIsCooked;

		if (bIsCooked)
		{
			InternalSerializeCards(Ar, Owner, GroupData.Cards.LODs);
			InternalSerializeMeshes(Ar, Owner, GroupData.Meshes.LODs);
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
	const uint32 GroupCount = GetHairGroupsInterpolation().Num();
	return
		GroupCount > 0 &&
		GetHairGroupsPlatformData().Num() == GroupCount &&
		GetHairGroupsPhysics().Num() == GroupCount &&
		GetHairGroupsRendering().Num() == GroupCount &&
		GetHairGroupsLOD().Num() == GroupCount;
}

void UGroomAsset::SetNumGroup(uint32 InGroupCount, bool bResetGroupData, bool bResetOtherData)
{
	ReleaseResource();
	if (bResetGroupData)
	{
		GetHairGroupsPlatformData().Reset();
	}

	// In order to preserve the existing asset settings, the settings are only reset if the group count has changed.
	if (InGroupCount != GetHairGroupsPlatformData().Num())
	{
		GetHairGroupsPlatformData().Init(FHairGroupPlatformData(), InGroupCount);
	}

	if (InGroupCount != GetHairGroupsPhysics().Num())
	{
		if (bResetOtherData)
		{
			GetHairGroupsPhysics().Init(FHairGroupsPhysics(), InGroupCount);
		}
		else
		{
			GetHairGroupsPhysics().SetNum(InGroupCount);
		}
	}

	if (InGroupCount != GetHairGroupsRendering().Num())
	{
		if (bResetOtherData)
		{
			GetHairGroupsRendering().Init(FHairGroupsRendering(), InGroupCount);
		}
		else
		{
			GetHairGroupsRendering().SetNum(InGroupCount);
		}
	}

	if (InGroupCount != GetHairGroupsInterpolation().Num())
	{
		if (bResetOtherData)
		{
			GetHairGroupsInterpolation().Init(FHairGroupsInterpolation(), InGroupCount);
		}
		else
		{
			GetHairGroupsInterpolation().SetNum(InGroupCount);
		}
	}

	if (InGroupCount != GetHairGroupsLOD().Num())
	{
		if (bResetOtherData)
		{
			GetHairGroupsLOD().Init(FHairGroupsLOD(), InGroupCount);
		}
		else
		{
			GetHairGroupsLOD().SetNum(InGroupCount);
		}

		// Insure that each group has at least one LOD
		for (FHairGroupsLOD& GroupLOD : GetHairGroupsLOD())
		{
			if (GroupLOD.LODs.IsEmpty())
			{
				FHairLODSettings& S = GroupLOD.LODs.AddDefaulted_GetRef();
				S.ScreenSize = 1;
				S.CurveDecimation = 1;
			}
		}
	}

	if (InGroupCount != GetHairGroupsInfo().Num())
	{
		if (bResetOtherData)
		{
			GetHairGroupsInfo().Init(FHairGroupInfoWithVisibility(), InGroupCount);
		}
		else
		{
			GetHairGroupsInfo().SetNum(InGroupCount);
		}
	}

	// The following groups remain unchanged:
	// * HairGroupsCards
	// * GetHairGroupsMeshes()
	// * GetHairGroupsMaterials()

	GetEffectiveLODBias().SetNum(InGroupCount);

#if WITH_EDITORONLY_DATA
	StrandsDerivedDataKey.SetNum(InGroupCount);
	CardsDerivedDataKey.SetNum(InGroupCount);
	MeshesDerivedDataKey.SetNum(InGroupCount);
#endif
}

void UGroomAsset::SetStableRasterization(bool bEnable)
{
	for (FHairGroupsRendering& Group : GetHairGroupsRendering())
	{
		Group.AdvancedSettings.bUseStableRasterization = bEnable;
	}
}

void UGroomAsset::SetScatterSceneLighting(bool bEnable)
{
	for (FHairGroupsRendering& Group : GetHairGroupsRendering())
	{
		Group.AdvancedSettings.bScatterSceneLighting = bEnable;
	}
}

void UGroomAsset::SetHairWidth(float Width)
{
	for (FHairGroupsRendering& Group : GetHairGroupsRendering())
	{
		Group.GeometrySettings.HairWidth = Width;
	}
}

// If groom derived data needs to be rebuilt (new format, serialization
// differences, etc.) replace the version GUID below with a new one.
// In case of merge conflicts with DDC versions, you MUST generate a new GUID
// and set this new GUID as the version.
// DDC Guid needs to be bumped in:
// * Main    : Engine\Source\Runtime\Core\Private\UObject\DevObjectVersion.cpp
// * Release : Engine\Source\Runtime\Core\Private\UObject\UE5ReleaseStreamObjectVersion.cpp
// * ...

#if WITH_EDITORONLY_DATA

namespace GroomDerivedDataCacheUtils
{
	const FString& GetGroomDerivedDataVersion()
	{
		static FString CachedVersionString = FDevSystemGuids::GetSystemGuid(FDevSystemGuids::Get().GROOM_DERIVED_DATA_VERSION).ToString();
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

		if (Desc.GroupIndex >= GroomAsset->GetHairGroupsPlatformData().Num() || Desc.LODIndex >= GroomAsset->GetHairGroupsPlatformData()[Desc.GroupIndex].Cards.LODs.Num())
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

	FString BuildStrandsDerivedDataKeySuffix(uint32 GroupIndex, const FHairGroupsInterpolation& InterpolationSettings, const FHairGroupsLOD& LODSettings, bool bNeedInterpolationData, const FHairDescriptionBulkData* HairBulkDescription)
	{
		// Serialize the build settings into a temporary array
		// The archive is flagged as persistent so that machines of different endianness produce identical binary results.
		TArray<uint8> TempBytes;
		TempBytes.Reserve(64);
		FMemoryWriter Ar(TempBytes, /*bIsPersistent=*/ true);

		GroomDerivedDataCacheUtils::SerializeHairInterpolationSettingsForDDC(Ar, GroupIndex, const_cast<FHairGroupsInterpolation&>(InterpolationSettings), const_cast<FHairGroupsLOD&>(LODSettings), bNeedInterpolationData);

		FString KeySuffix;
		if (HairBulkDescription)
		{
			// Reserve twice the size of TempBytes because of ByteToHex below + 3 for "ID" and \0
			KeySuffix.Reserve(HairBulkDescription->GetIdString().Len() + TempBytes.Num() * 2 + 3);
			KeySuffix += TEXT("ID");
			KeySuffix += HairBulkDescription->GetIdString();
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
}

bool UGroomAsset::IsFullyCached()
{
	// Check if all the groom derived data for strands, cards and meshes are already stored in the DDC
	TArray<FString> CacheKeys;
	const uint32 GroupCount = GetHairGroupsInterpolation().Num();
	for (uint32 GroupIndex = 0; GroupIndex < GroupCount; ++GroupIndex)
	{
		const FString StrandsDDCKey = UGroomAsset::GetDerivedDataKeyForStrands(GroupIndex);
		CacheKeys.Add(StrandsDDCKey);

		// Some cards and meshes LOD settings may not produce any derived data so those must be excluded
		bool bHasCardsLODs = false;
		bool bHasMeshesLODs = false;
		const uint32 LODCount = GetHairGroupsLOD()[GroupIndex].LODs.Num();
		for (uint32 LODIt = 0; LODIt < LODCount; ++LODIt)
		{
			int32 SourceIt = 0;
			// GetSourceDescription will cross-check the LOD settings with the cards/meshes settings to see if they would produce any data
			if (const FHairGroupsCardsSourceDescription* Desc = GetSourceDescription(GetHairGroupsCards(), GroupIndex, LODIt, SourceIt))
			{
				Desc->HasMeshChanged(); // this query will trigger a load of the mesh dependency, which has to be done in the game thread
				bHasCardsLODs = true;
			}
			if (const FHairGroupsMeshesSourceDescription* Desc = GetSourceDescription(GetHairGroupsMeshes(), GroupIndex, LODIt, SourceIt))
			{
				Desc->HasMeshChanged(); // this query will trigger a load of the mesh dependency, which has to be done in the game thread
				bHasMeshesLODs = true;
			}
		}

		if (bHasCardsLODs)
		{
			const FString CardsKeySuffix = GroomDerivedDataCacheUtils::BuildCardsDerivedDataKeySuffix(GroupIndex, GetHairGroupsLOD()[GroupIndex].LODs, GetHairGroupsCards());
			const FString CardsDDCKey = StrandsDDCKey + CardsKeySuffix;
			CacheKeys.Add(CardsDDCKey);
		}

		if (bHasMeshesLODs)
		{
			const FString MeshesKeySuffix = GroomDerivedDataCacheUtils::BuildMeshesDerivedDataKeySuffix(GroupIndex, GetHairGroupsLOD()[GroupIndex].LODs, GetHairGroupsMeshes());
			const FString MeshesDDCKey = GroomDerivedDataCacheUtils::BuildGroomDerivedDataKey(MeshesKeySuffix);
			CacheKeys.Add(MeshesDDCKey);
		}
	}

	return GetDerivedDataCacheRef().AllCachedDataProbablyExists(CacheKeys);
}

FString UGroomAsset::GetDerivedDataKey(bool bUseStrandCachedKey)
{
	FString OutKey;
	for (uint32 GroupIt = 0, GroupCount = GetNumHairGroups(); GroupIt < GroupCount; ++GroupIt)
	{		
		// Optionnaly use the cached strands key. This is to workaround an issue with legacy HairDescription which uses FEditorBulkData, 
		// and whose StringId is altered when loaded (only for legacy hair description converted to FEditorBulkData). This StringId modification
		// changes the DDC key of a groom asset depending if has been loaded or not, causing DDC miss
		const bool bComputeStrandsKey = !bUseStrandCachedKey || StrandsDerivedDataKey[GroupIt].IsEmpty();
		const FString StrandsKey = bComputeStrandsKey ? GetDerivedDataKeyForStrands(GroupIt) : StrandsDerivedDataKey[GroupIt];

		// Compute group key
		OutKey += StrandsKey;
		OutKey += GetDerivedDataKeyForCards(GroupIt, StrandsKey);
		OutKey += GetDerivedDataKeyForMeshes(GroupIt);
	}
	return OutKey;
}

FString UGroomAsset::GetDerivedDataKeyForCards(uint32 GroupIndex, const FString& StrandsKey)
{
	const FString KeySuffix = GroomDerivedDataCacheUtils::BuildCardsDerivedDataKeySuffix(GroupIndex, GetHairGroupsLOD()[GroupIndex].LODs, GetHairGroupsCards());
	const FString DerivedDataKey = StrandsKey + KeySuffix;
	return DerivedDataKey;
}

FString UGroomAsset::GetDerivedDataKeyForMeshes(uint32 GroupIndex)
{
	const FString KeySuffix = GroomDerivedDataCacheUtils::BuildMeshesDerivedDataKeySuffix(GroupIndex, GetHairGroupsLOD()[GroupIndex].LODs, GetHairGroupsMeshes());
	const FString DerivedDataKey = GroomDerivedDataCacheUtils::BuildGroomDerivedDataKey(KeySuffix);
	return DerivedDataKey;
}

FString UGroomAsset::GetDerivedDataKeyForStrands(uint32 GroupIndex)
{
	const FHairGroupsInterpolation& InterpolationSettings = GetHairGroupsInterpolation()[GroupIndex];
	const FHairGroupsLOD& LODSettings = GetHairGroupsLOD()[GroupIndex];

	// If simulation or global interpolation is enabled, then interpolation data are required. Otherwise they can be skipped
	const bool bNeedInterpolationData = NeedsInterpolationData(GroupIndex);
	const FString KeySuffix = GroomDerivedDataCacheUtils::BuildStrandsDerivedDataKeySuffix(GroupIndex, InterpolationSettings, LODSettings, bNeedInterpolationData, HairDescriptionBulkData[HairDescriptionType].Get());
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

	const uint32 GroupCount = GetHairGroupsInterpolation().Num();
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

	const uint32 GroupCount = GetHairGroupsInterpolation().Num();
	check(GroupIndex < GroupCount);
	if (GroupIndex >= GroupCount)
	{
		return false;
	}

	FString DerivedDataKey;
	bool bSuccess = false;

	// Cache each geometric representation separately
	bSuccess |= CacheStrandsData(GroupIndex, DerivedDataKey);
	bSuccess |= CacheCardsData(GroupIndex, DerivedDataKey);
	bSuccess |= CacheMeshesData(GroupIndex);

	return bSuccess;
}

bool UGroomAsset::CacheStrandsData(uint32 GroupIndex, FString& OutDerivedDataKey)
{
	if (!HairDescriptionBulkData[HairDescriptionType])
	{
		// Groom having only cards/meshes (i.e., not containing imported strands) don't have hair description
		bool bHasStrands = false;
		for (const FHairLODSettings& LOD : GetHairGroupsLOD()[GroupIndex].LODs)
		{
			if (LOD.GeometryType == EGroomGeometryType::Strands)
			{
				bHasStrands = true;
				break;
			}
		}

		if (bHasStrands)
		{
			UE_LOG(LogHairStrands, Error, TEXT("[Groom] The groom asset %s is too old. Please reimported the groom from its original source file."), *GetName());
		}
		return false;
	}

	using namespace UE::DerivedData;

	const FString DerivedDataKey= UGroomAsset::GetDerivedDataKeyForStrands(GroupIndex);
	const FCacheKey HeaderKey = ConvertLegacyCacheKey(DerivedDataKey + FString(TEXT("_Header")));
	const FSharedString Name = MakeStringView(GetPathName());
	FSharedBuffer Data;
	{
		FRequestOwner Owner(EPriority::Blocking);
		GetCache().GetValue({ {Name, HeaderKey} }, Owner, [&Data](FCacheGetValueResponse&& Response)
		{
			Data = Response.Value.GetData().Decompress();
		});
		Owner.Wait();
	}

	// Populate key/name for streaming data request
	auto FillDrivedDataKey = [&DerivedDataKey, &Name](FHairGroupPlatformData& In)
	{
		In.Strands.BulkData.DerivedDataKey = DerivedDataKey + FString(TEXT("_RestData"));
		In.Strands.InterpolationBulkData.DerivedDataKey = DerivedDataKey + FString(TEXT("_InterpolationData"));
		In.Strands.ClusterBulkData.DerivedDataKey = DerivedDataKey + FString(TEXT("_ClusterData"));
	};

	FHairGroupPlatformData& PlatformData = GetHairGroupsPlatformData()[GroupIndex];

	bool bSuccess = false;
	if (Data)
	{
		UE_CLOG(IsHairStrandsDDCLogEnable(), LogHairStrands, Log, TEXT("[Groom/DDC] Strands - Found (Groom:%s Group6:%d)."), *GetName(), GroupIndex);

		// Reset hair group data to ensure previously loaded bulk data are cleared/cleaned prior to load new data.
		PlatformData = FHairGroupPlatformData();
		FillDrivedDataKey(PlatformData);

		FMemoryReaderView Ar(Data, /*bIsPersistent*/ true);

		InternalSerializeGuide(Ar, this, PlatformData.Guides);
		InternalSerializeStrand(Ar, this, PlatformData.Strands, true/*Header*/, false/*Data*/, &bSuccess);
	}
	if (!bSuccess)
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

		// Build groom data with the new build settings
		bSuccess = BuildHairGroup(
			GroupIndex,
			NeedsInterpolationData(GroupIndex),
			LocalHairDescriptionGroups,
			GetHairGroupsInterpolation(),
			GetHairGroupsLOD(),
			GetHairGroupsInfo(),
			GetHairGroupsPlatformData());
		
		if (bSuccess)
		{
			FillDrivedDataKey(PlatformData);

			// Header
			{
				TArray<uint8> WriteData;
				FMemoryWriter Ar(WriteData, /*bIsPersistent*/ true);
				InternalSerializeGuide(Ar, this, PlatformData.Guides);
				InternalSerializeStrand(Ar, this, PlatformData.Strands, true/*Header*/, false/*Data*/);

				FRequestOwner AsyncOwner(EPriority::Normal);
				GetCache().PutValue({ {Name, HeaderKey, FValue::Compress(MakeSharedBufferFromArray(MoveTemp(WriteData)))} }, AsyncOwner);
				AsyncOwner.KeepAlive();
			}

			// Data (Rest)
			{
				TArray<FCachePutValueRequest> Out;
				PlatformData.Strands.BulkData.Write_DDC(this, Out);

				FRequestOwner AsyncOwner(EPriority::Normal);
				GetCache().PutValue(Out, AsyncOwner);
				AsyncOwner.KeepAlive();
			}

			// Data (Interpolation)
			if (PlatformData.Strands.InterpolationBulkData.GetResourceCount() > 0)
			{
				TArray<FCachePutValueRequest> Out;
				PlatformData.Strands.InterpolationBulkData.Write_DDC(this, Out);

				FRequestOwner AsyncOwner(EPriority::Normal);
				GetCache().PutValue(Out, AsyncOwner);
				AsyncOwner.KeepAlive();
			}

			// Data (cluster Culling)
			{
				TArray<FCachePutValueRequest> Out;
				PlatformData.Strands.ClusterBulkData.Write_DDC(this, Out);

				FRequestOwner AsyncOwner(EPriority::Normal);
				GetCache().PutValue(Out, AsyncOwner);
				AsyncOwner.KeepAlive();
			}
		}
	}

	OutDerivedDataKey = DerivedDataKey;
	StrandsDerivedDataKey[GroupIndex] = DerivedDataKey;

	return bSuccess;
}

bool UGroomAsset::HasValidMeshesData(uint32 GroupIndex) const
{
	const FHairGroupPlatformData& HairGroupData = GetHairGroupsPlatformData()[GroupIndex];
	const FHairGroupsLOD& GroupsLOD = GetHairGroupsLOD()[GroupIndex];
	for (int32 LODIt = 0; LODIt < GroupsLOD.LODs.Num(); ++LODIt)
	{
		if (GroupsLOD.LODs[LODIt].GeometryType == EGroomGeometryType::Meshes)
		{
			int32 SourceIt = 0;
			if (const FHairGroupsMeshesSourceDescription* Desc = GetSourceDescription(GetHairGroupsMeshes(), GroupIndex, LODIt, SourceIt))
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
	const FHairGroupPlatformData& HairGroupData = GetHairGroupsPlatformData()[GroupIndex];
	const FHairGroupsLOD& GroupsLOD = GetHairGroupsLOD()[GroupIndex];
	for (int32 LODIt = 0; LODIt < GroupsLOD.LODs.Num(); ++LODIt)
	{
		// Note: We don't condition/filter resource allocation based on GeometryType == EGroomGeometryType::Cards, because 
		// geometry type can be switched at runtime (between cards/strands)
		{
			int32 SourceIt = 0;
			if (const FHairGroupsCardsSourceDescription* Desc = GetSourceDescription(GetHairGroupsCards(), GroupIndex, LODIt, SourceIt))
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
bool UGroomAsset::CacheCardsData(uint32 GroupIndex, const FString& StrandsKey)
{
	const bool bHasValidCardsData = HasValidCardsData(GroupIndex);
	if (!bHasValidCardsData)
	{
		const bool bNeedReset = !CardsDerivedDataKey[GroupIndex].IsEmpty();
		CardsDerivedDataKey[GroupIndex].Empty();
		return bNeedReset;
	}

	const FString KeySuffix = GroomDerivedDataCacheUtils::BuildCardsDerivedDataKeySuffix(GroupIndex, GetHairGroupsLOD()[GroupIndex].LODs, GetHairGroupsCards());
	const FString DerivedDataKey = StrandsKey + KeySuffix;

	FHairGroupPlatformData& HairGroupData = GetHairGroupsPlatformData()[GroupIndex];

	// We should never query the DDC for something that we won't be able to build. So check that first.
	// The goal is to avoid paying the price of a guaranteed failure on high-latency DDC backends every run.
	bool bDataCanBeBuilt = false;

	const FHairGroupsLOD& GroupsLOD = GetHairGroupsLOD()[GroupIndex];
	for (int32 LODIt = 0; LODIt < GroupsLOD.LODs.Num(); ++LODIt)
	{
		int32 SourceIt = 0;
		if (FHairGroupsCardsSourceDescription* Desc = GetSourceDescription(GetHairGroupsCards(), GroupIndex, LODIt, SourceIt))
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

	using namespace UE::DerivedData;
	const FCacheKey Key = ConvertLegacyCacheKey(DerivedDataKey);
	const FSharedString Name = MakeStringView(GetPathName());
	FSharedBuffer Data;
	{
		FRequestOwner Owner(EPriority::Blocking);
		GetCache().GetValue({ {Name, Key} }, Owner, [&Data](FCacheGetValueResponse&& Response)
		{
			Data = Response.Value.GetData().Decompress();
		});
		Owner.Wait();
	}

	if (Data)
	{
		UE_CLOG(IsHairStrandsDDCLogEnable(), LogHairStrands, Log, TEXT("[Groom/DDC] Cards - Found (Groom:%s Group:%d)."), *GetName(), GroupIndex);
		
		FMemoryReaderView Ar(Data, /*bIsPersistent*/ true);
		InternalSerializeCards(Ar, this, HairGroupData.Cards.LODs);
	}
	else
	{
		UE_CLOG(IsHairStrandsDDCLogEnable(), LogHairStrands, Log, TEXT("[Groom/DDC] Cards - Not found (Groom:%s Group:%d)."), *GetName(), GroupIndex);

		if (!BuildCardsData(GroupIndex))
		{
			CardsDerivedDataKey[GroupIndex].Empty();
			return false;
		}

		TArray<uint8> WriteData;
		FMemoryWriter Ar(WriteData, /*bIsPersistent*/ true);
		InternalSerializeCards(Ar, this, HairGroupData.Cards.LODs);
		FRequestOwner AsyncOwner(EPriority::Normal);
		GetCache().PutValue({ {Name, Key, FValue::Compress(MakeSharedBufferFromArray(MoveTemp(WriteData)))} }, AsyncOwner);
		AsyncOwner.KeepAlive();
	}

	// Handle the case where the cards data is already cached in the DDC
	// Need to populate the strands data with it
	FHairGroupPlatformData& GroupData = GetHairGroupsPlatformData()[GroupIndex];
	if (StrandsDerivedDataKey[GroupIndex].IsEmpty() && GroupData.Strands.BulkData.GetNumPoints() == 0)
	{
		for (int32 LODIt = 0; LODIt < GroupData.Cards.LODs.Num(); ++LODIt)
		{
			int32 SourceIt = 0;
			if (FHairGroupsCardsSourceDescription* Desc = GetSourceDescription(GetHairGroupsCards(), GroupIndex, LODIt, SourceIt))
			{
				FHairGroupPlatformData::FCards::FLOD& LOD = GroupData.Cards.LODs[LODIt];
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
		if (FHairGroupsCardsSourceDescription* Desc = GetSourceDescription(GetHairGroupsCards(), GroupIndex, LODIt, SourceIt))
		{
			Desc->UpdateMeshKey();
		}
	}

	CardsDerivedDataKey[GroupIndex] = DerivedDataKey;

	return true;
}

// Return true if the meshes data have changed
bool UGroomAsset::CacheMeshesData(uint32 GroupIndex)
{
	const bool bHasValidMeshesData = UGroomAsset::HasValidMeshesData(GroupIndex);
	if (!bHasValidMeshesData)
	{
		const bool bNeedReset = !MeshesDerivedDataKey[GroupIndex].IsEmpty();
		MeshesDerivedDataKey[GroupIndex].Empty();
		return bNeedReset;
	}

	const FString KeySuffix = GroomDerivedDataCacheUtils::BuildMeshesDerivedDataKeySuffix(GroupIndex, GetHairGroupsLOD()[GroupIndex].LODs, GetHairGroupsMeshes());
	const FString DerivedDataKey = GroomDerivedDataCacheUtils::BuildGroomDerivedDataKey(KeySuffix);

	FHairGroupPlatformData& HairGroupData = GetHairGroupsPlatformData()[GroupIndex];

	// We should never query the DDC for something that we won't be able to build. So check that first.
	// The goal is to avoid paying the price of a guaranteed failure on high-latency DDC backends every run.
	bool bDataCanBeBuilt = false;

	const FHairGroupsLOD& GroupsLOD = GetHairGroupsLOD()[GroupIndex];
	for (int32 LODIt = 0; LODIt < GroupsLOD.LODs.Num(); ++LODIt)
	{
		int32 SourceIt = 0;
		if (FHairGroupsMeshesSourceDescription* Desc = GetSourceDescription(GetHairGroupsMeshes(), GroupIndex, LODIt, SourceIt))
		{
			if (GroupsLOD.LODs[LODIt].GeometryType == EGroomGeometryType::Meshes)
			{
				bDataCanBeBuilt |= Desc->ImportedMesh != nullptr;
			}
		}
	}

	using namespace UE::DerivedData;
	const FCacheKey Key = ConvertLegacyCacheKey(DerivedDataKey);
	const FSharedString Name = MakeStringView(GetPathName());
	FSharedBuffer Data;
	{
		FRequestOwner Owner(EPriority::Blocking);
		GetCache().GetValue({ {Name, Key} }, Owner, [&Data](FCacheGetValueResponse&& Response)
		{
			Data = Response.Value.GetData().Decompress();
		});
		Owner.Wait();
	}

	TArray<uint8> DerivedData;
	if (bDataCanBeBuilt && GetDerivedDataCacheRef().GetSynchronous(*DerivedDataKey, DerivedData, GetPathName()))
	{
		UE_CLOG(IsHairStrandsDDCLogEnable(), LogHairStrands, Log, TEXT("[Groom/DDC] Meshes - Found (Groom:%s Group:%d)."), *GetName(), GroupIndex);

		FMemoryReaderView Ar(Data, /*bIsPersistent*/ true);
		InternalSerializeMeshes(Ar, this, HairGroupData.Meshes.LODs);
	}
	else
	{
		UE_CLOG(IsHairStrandsDDCLogEnable(), LogHairStrands, Log, TEXT("[Groom/DDC] Meshes - Not found (Groom:%s Group:%d)."), *GetName(), GroupIndex);

		if (!BuildMeshesData(GroupIndex))
		{
			MeshesDerivedDataKey[GroupIndex].Empty();
			return false;
		}

		TArray<uint8> WriteData;
		FMemoryWriter Ar(WriteData, /*bIsPersistent*/ true);
		InternalSerializeMeshes(Ar, this, HairGroupData.Meshes.LODs);
		FRequestOwner AsyncOwner(EPriority::Normal);
		GetCache().PutValue({ {Name, Key, FValue::Compress(MakeSharedBufferFromArray(MoveTemp(WriteData)))} }, AsyncOwner);
		AsyncOwner.KeepAlive();
	}

	MeshesDerivedDataKey[GroupIndex] = DerivedDataKey;

	return true;
}

inline FString GetLODName(const UGroomAsset* Asset, uint32 LODIndex)
{
	return Asset->GetOutermost()->GetName() + FString::Printf(TEXT("_LOD%d"), LODIndex);
}

bool UGroomAsset::BuildCardsData(uint32 GroupIndex)
{
	LLM_SCOPE_BYTAG(Groom);

	if (!IsHairStrandsEnabled(EHairStrandsShaderType::Cards) || GetHairGroupsCards().Num() == 0 || !CanRebuildFromDescription())
	{
		return false;
	}

	bool bHasChanged = GetHairGroupsCards().Num() != CachedHairGroupsCards.Num();

	check(GroupIndex < uint32(GetNumHairGroups()));
	FHairGroupPlatformData& GroupData = GetHairGroupsPlatformData()[GroupIndex];

	// The settings might have been previously cached without the data having been built
	const uint32 LODCount = GetHairGroupsLOD()[GroupIndex].LODs.Num();
	TArray<bool> bIsAlreadyBuilt;
	bIsAlreadyBuilt.SetNum(LODCount);
	for (uint32 LODIt = 0; LODIt < LODCount; ++LODIt)
	{
		int32 SourceIt = 0;
		if (const FHairGroupsCardsSourceDescription* Desc = GetSourceDescription(GetHairGroupsCards(), GroupIndex, LODIt, SourceIt))
		{
			bIsAlreadyBuilt[LODIt] = GroupData.Strands.HasValidData() && Desc->CardsInfo.NumCardVertices > 0 && !Desc->HasMeshChanged();
			bHasChanged |= !bIsAlreadyBuilt[LODIt];
		}
	}

	if (!bHasChanged)
	{
		for (uint32 SourceIt = 0, SourceCount = GetHairGroupsCards().Num(); SourceIt < SourceCount; ++SourceIt)
		{
			const bool bEquals = CachedHairGroupsCards[SourceIt] == GetHairGroupsCards()[SourceIt];
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
					FHairGroupCardsTextures& Textures = GetHairGroupsCards()[SourceIt].Textures;
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
	const FHairGroupsLOD& GroupsLOD = GetHairGroupsLOD()[GroupIndex];
	for (uint32 LODIt = 0; LODIt < LODCount; ++LODIt)
	{
		int32 SourceIt = 0;
		if (FHairGroupsCardsSourceDescription* Desc = GetSourceDescription(GetHairGroupsCards(), GroupIndex, LODIt, SourceIt))
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

			FHairGroupPlatformData::FCards::FLOD& LOD = GroupData.Cards.LODs[LODIt];
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
				const FName OwnerName = GetAssetPathName(LODIt);

				LOD.RestResource = new FHairCardsRestResource(LOD.BulkData, ResourceName, OwnerName);
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
				LOD.InterpolationResource = new FHairCardsInterpolationResource(LOD.InterpolationBulkData, ResourceName, OwnerName);

				// Create own interpolation settings for cards.
				// Force closest guides as this is the most relevant matching metric for cards, due to their coarse geometry
				FHairInterpolationSettings CardsInterpolationSettings = GetHairGroupsInterpolation()[GroupIndex].InterpolationSettings;
				CardsInterpolationSettings.GuideType = EGroomGuideType::Imported;
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
				FGroomBuilder::BuildData(LocalHairDescriptionGroups.HairGroups[GroupIndex], GetHairGroupsInterpolation()[GroupIndex], DummyInfo, RenStrandsData, SimGuidesData);

				// 3. Compute interpolation data & bulk data between the sim guides and the cards guides
				FHairStrandsInterpolationDatas LODGuideInterpolationData;
				FGroomBuilder::BuildInterplationData(DummyGroupInfo, LODGuidesData, SimGuidesData, CardsInterpolationSettings, LODGuideInterpolationData);
				FGroomBuilder::BuildInterplationBulkData(SimGuidesData, LODGuideInterpolationData, LOD.Guides.InterpolationBulkData);

				if (bCopyRenderData)
				{
					GroupData.Strands.BulkData = LOD.Guides.BulkData;
					GroupData.Guides.BulkData  = LOD.Guides.BulkData;
				}

				LOD.Guides.RestResource = new FHairStrandsRestResource(LOD.Guides.BulkData, EHairStrandsResourcesType::Cards, ResourceName, OwnerName);

				LOD.Guides.InterpolationResource = new FHairStrandsInterpolationResource(LOD.Guides.InterpolationBulkData, ResourceName, OwnerName);

				// Update card stats to display
				Desc->CardsInfo.NumCardVertices = LOD.BulkData.GetNumVertices();
				Desc->CardsInfo.NumCards = LOD.Guides.BulkData.GetNumCurves();// LOD.Data.Cards.IndexOffsets.Num();
			}
		}
	}

	return bDataBuilt;
}

bool UGroomAsset::BuildCardsData()
{
	bool bDataChanged = false;
	for (uint32 GroupIndex = 0, GroupCount = GetNumHairGroups(); GroupIndex < GroupCount; ++GroupIndex)
	{
		bDataChanged |= CacheCardsData(GroupIndex, StrandsDerivedDataKey[GroupIndex]);
	}

	if (bDataChanged)
	{
		InitGuideResources();

		// When building cards in an empty groom, the strands resources need to be initialized once
		FHairGroupPlatformData& GroupData = GetHairGroupsPlatformData()[0];
		if (!GroupData.Strands.ClusterResource)
		{
			InitStrandsResources();
		}

		InitCardsResources();
	}

	return bDataChanged;
}

bool UGroomAsset::BuildMeshesData()
{
	bool bDataChanged = false;
	for (uint32 GroupIndex = 0, GroupCount = GetNumHairGroups(); GroupIndex < GroupCount; ++GroupIndex)
	{
		bDataChanged |= CacheMeshesData(GroupIndex);
	}

	if (bDataChanged)
	{
		InitMeshesResources();
	}

	return bDataChanged;
}

bool UGroomAsset::BuildMeshesData(uint32 GroupIndex)
{
	if (!IsHairStrandsEnabled(EHairStrandsShaderType::Meshes) || GetHairGroupsMeshes().Num() == 0)
	{
		return false;
	}

	bool bHasChanged = GetHairGroupsMeshes().Num() != CachedHairGroupsMeshes.Num();

	FHairGroupPlatformData& GroupData = GetHairGroupsPlatformData()[GroupIndex];

	// The settings might have been previously cached without the data having been built
	const uint32 LODCount = GetHairGroupsLOD()[GroupIndex].LODs.Num();
	TArray<bool> bIsAlreadyBuilt;
	bIsAlreadyBuilt.SetNum(LODCount);
	for (uint32 LODIt = 0; LODIt < LODCount; ++LODIt)
	{
		int32 SourceIt = 0;
		if (const FHairGroupsMeshesSourceDescription* Desc = GetSourceDescription(GetHairGroupsMeshes(), GroupIndex, LODIt, SourceIt))
		{
			bIsAlreadyBuilt[LODIt] = GroupData.Meshes.LODs.IsValidIndex(LODIt) && GroupData.Meshes.LODs[LODIt].BulkData.GetNumVertices() > 0 && !Desc->HasMeshChanged();
			bHasChanged |= !bIsAlreadyBuilt[LODIt];
		}
	}

	if (!bHasChanged)
	{
		for (uint32 SourceIt = 0, SourceCount = GetHairGroupsMeshes().Num(); SourceIt < SourceCount; ++SourceIt)
		{
			const bool bEquals = CachedHairGroupsMeshes[SourceIt] == GetHairGroupsMeshes()[SourceIt];
			if (!bEquals)
			{
				bHasChanged = true;
				break;
			}
		}
	}

	if (!bHasChanged)
	{
		for (uint32 SourceIt = 0, SourceCount = GetHairGroupsMeshes().Num(); SourceIt < SourceCount; ++SourceIt)
		{
			const bool bEquals = CachedHairGroupsMeshes[SourceIt] == GetHairGroupsMeshes()[SourceIt];
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
					FHairGroupCardsTextures& Textures = GetHairGroupsMeshes()[SourceIt].Textures;
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
	const FHairGroupsLOD& GroupsLOD = GetHairGroupsLOD()[GroupIndex];
	for (uint32 LODIt = 0; LODIt < LODCount; ++LODIt)
	{
		int32 SourceIt = 0;
		if (const FHairGroupsMeshesSourceDescription* Desc = GetSourceDescription(GetHairGroupsMeshes(), GroupIndex, LODIt, SourceIt))
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

			FHairGroupPlatformData::FMeshes::FLOD& LOD = GroupData.Meshes.LODs[LODIt];

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
					GroupData.Strands.BulkData.Header.BoundingBox,
					LOD.BulkData);
			}

			bDataBuilt |= true;
		}
	}

	// Update the imported mesh DDC keys
	for (int32 LODIt = 0; LODIt < GroupData.Meshes.LODs.Num(); ++LODIt)
	{
		int32 SourceIt = 0;
		if (FHairGroupsMeshesSourceDescription* Desc = GetSourceDescription(GetHairGroupsMeshes(), GroupIndex, LODIt, SourceIt))
		{
			Desc->UpdateMeshKey();
		}
	}

	return bDataBuilt;
}

void UGroomAsset::UpdateCachedSettings()
{
#if WITH_EDITORONLY_DATA
	CachedHairGroupsRendering		= GetHairGroupsRendering();
	CachedHairGroupsPhysics			= GetHairGroupsPhysics();
	CachedHairGroupsInterpolation	= GetHairGroupsInterpolation();
	CachedHairGroupsLOD				= GetHairGroupsLOD();
	CachedHairGroupsCards			= GetHairGroupsCards();
	CachedHairGroupsMeshes			= GetHairGroupsMeshes();
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
		FHairGroupPlatformData& GroupData = GetHairGroupsPlatformData()[GroupIndex];
		if (GroupData.Guides.HasValidData())
		{
			if (GroupData.Guides.RestResource == nullptr)
			{
				GroupData.Guides.RestResource = new FHairStrandsRestResource(GroupData.Guides.BulkData, EHairStrandsResourcesType::Guides, FHairResourceName(GetFName(), GroupIndex), GetAssetPathName());
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
		FHairGroupPlatformData& GroupData = GetHairGroupsPlatformData()[GroupIndex];
		check(GroupData.Guides.IsValid());
		if (GroupData.Strands.InterpolationResource == nullptr)
		{
			GroupData.Strands.InterpolationResource = new FHairStrandsInterpolationResource(GroupData.Strands.InterpolationBulkData, FHairResourceName(GetFName(), GroupIndex), GetAssetPathName());
		}
		return GroupData.Strands.InterpolationResource;
	}
	return nullptr;
}

#if RHI_RAYTRACING
FHairStrandsRaytracingResource* UGroomAsset::AllocateCardsRaytracingResources(uint32 GroupIndex, uint32 LODIndex)
{
	if (IsRayTracingAllowed() && GroupIndex < uint32(GetNumHairGroups()) && LODIndex < uint32(GetHairGroupsLOD()[GroupIndex].LODs.Num()))
	{
		FHairGroupPlatformData& GroupData = GetHairGroupsPlatformData()[GroupIndex];
		FHairGroupPlatformData::FCards::FLOD& LOD = GroupData.Cards.LODs[LODIndex];
		check(LOD.BulkData.IsValid());

		if (LOD.RaytracingResource == nullptr)
		{
			LOD.RaytracingResource = new FHairStrandsRaytracingResource(LOD.BulkData, FHairResourceName(GetFName(), GroupIndex, LODIndex), GetAssetPathName(LODIndex));
		}
		return LOD.RaytracingResource;
	}
	return nullptr;
}

FHairStrandsRaytracingResource* UGroomAsset::AllocateMeshesRaytracingResources(uint32 GroupIndex, uint32 LODIndex)
{
	if (IsRayTracingAllowed() && GroupIndex < uint32(GetNumHairGroups()) && LODIndex < uint32(GetHairGroupsLOD()[GroupIndex].LODs.Num()))
	{
		FHairGroupPlatformData& GroupData = GetHairGroupsPlatformData()[GroupIndex];
		FHairGroupPlatformData::FMeshes::FLOD& LOD = GroupData.Meshes.LODs[LODIndex];
		check(LOD.BulkData.IsValid());

		if (LOD.RaytracingResource == nullptr)
		{
			LOD.RaytracingResource = new FHairStrandsRaytracingResource(LOD.BulkData, FHairResourceName(GetFName(), GroupIndex, LODIndex), GetAssetPathName(LODIndex));
		}
		return LOD.RaytracingResource;
	}
	return nullptr;
}

FHairStrandsRaytracingResource* UGroomAsset::AllocateStrandsRaytracingResources(uint32 GroupIndex)
{
	if (IsRayTracingAllowed() && GroupIndex < uint32(GetNumHairGroups()))
	{
		FHairGroupPlatformData& GroupData = GetHairGroupsPlatformData()[GroupIndex];
		check(GroupData.Strands.HasValidData());

		if (GroupData.Strands.RaytracingResource == nullptr)
		{
			GroupData.Strands.RaytracingResource = new FHairStrandsRaytracingResource(GroupData.Strands.BulkData, FHairResourceName(GetFName(), GroupIndex), GetAssetPathName());
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

	const FName OwnerName = GetAssetPathName();

	for (uint32 GroupIndex = 0, GroupCount = GetNumHairGroups(); GroupIndex < GroupCount; ++GroupIndex)
	{
		FHairGroupPlatformData& GroupData = GetHairGroupsPlatformData()[GroupIndex];

		if (GroupData.Strands.HasValidData())
		{
			FHairResourceName ResourceName(GetFName(), GroupIndex);
			GroupData.Strands.RestResource = new FHairStrandsRestResource(GroupData.Strands.BulkData, EHairStrandsResourcesType::Strands, ResourceName, OwnerName);

			if (GroupData.Strands.ClusterBulkData.IsValid())
			{
				GroupData.Strands.ClusterResource = new FHairStrandsClusterResource(GroupData.Strands.ClusterBulkData, ResourceName, OwnerName);
			}

			// Interpolation are lazy allocated, as these resources are only used when RBF/simulation is enabled by a groom component
			// AllocateInterpolationResources(GroupIndex);
		}
	}
}

static void InitCardsTextureResources(UGroomAsset* GroomAsset)
{
	if (!GroomAsset || !IsHairStrandsEnabled(EHairStrandsShaderType::Cards) || GroomAsset->GetHairGroupsCards().Num() == 0)
	{
		return;
	}

	for (uint32 GroupIndex = 0, GroupCount = GroomAsset->GetNumHairGroups(); GroupIndex < GroupCount; ++GroupIndex)
	{
		FHairGroupPlatformData& GroupData = GroomAsset->GetHairGroupsPlatformData()[GroupIndex];

		const uint32 LODCount = GroomAsset->GetHairGroupsLOD()[GroupIndex].LODs.Num();
		GroupData.Cards.LODs.SetNum(LODCount);

		for (uint32 LODIt = 0; LODIt < LODCount; ++LODIt)
		{
			FHairGroupPlatformData::FCards::FLOD& LOD = GroupData.Cards.LODs[LODIt];

			int32 SourceIt = 0;
			const FHairGroupsCardsSourceDescription* Desc = GetSourceDescription(GroomAsset->GetHairGroupsCards(), GroupIndex, LODIt, SourceIt);
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

	if (!IsHairStrandsEnabled(EHairStrandsShaderType::Cards) || GetHairGroupsCards().Num() == 0)
	{
		return;
	}

	for (uint32 GroupIndex = 0, GroupCount = GetNumHairGroups(); GroupIndex < GroupCount; ++GroupIndex)
	{
		FHairGroupPlatformData& GroupData = GetHairGroupsPlatformData()[GroupIndex];

		const uint32 LODCount = GetHairGroupsLOD()[GroupIndex].LODs.Num();
		GroupData.Cards.LODs.SetNum(LODCount);

		for (uint32 LODIt = 0; LODIt < LODCount; ++LODIt)
		{
			FHairGroupPlatformData::FCards::FLOD& LOD = GroupData.Cards.LODs[LODIt];

			int32 SourceIt = 0;
			const FHairGroupsCardsSourceDescription* Desc = GetSourceDescription(GetHairGroupsCards(), GroupIndex, LODIt, SourceIt);
			if (!Desc)
			{
				continue;
			}

			if (LOD.RestResource == nullptr &&			// don't initialize again if they were previously initialized during the BuildCardsGeometry
				LOD.HasValidData())
			{
				FHairResourceName ResourceName(GetFName(), GroupIndex, LODIt);
				const FName OwnerName = GetAssetPathName(LODIt);

				LOD.RestResource = new FHairCardsRestResource(LOD.BulkData, ResourceName, OwnerName);
				BeginInitResource(LOD.RestResource); // Immediate allocation, as needed for the vertex factory, input stream building

				LOD.InterpolationResource = new FHairCardsInterpolationResource(LOD.InterpolationBulkData, ResourceName, OwnerName);

				LOD.Guides.RestResource = new FHairStrandsRestResource(LOD.Guides.BulkData, EHairStrandsResourcesType::Cards, ResourceName, OwnerName);

				LOD.Guides.InterpolationResource = new FHairStrandsInterpolationResource(LOD.Guides.InterpolationBulkData, ResourceName, OwnerName);

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
				Desc->CardsInfo.NumCards = LOD.Guides.BulkData.GetNumCurves();// LOD.Data.Cards.GetNumCards();
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
		FHairGroupPlatformData& GroupData = GetHairGroupsPlatformData()[GroupIndex];

		const uint32 LODCount = GroupData.Meshes.LODs.Num();
		for (uint32 LODIt = 0; LODIt < LODCount; ++LODIt)
		{
			int32 SourceIt = 0;
			const FHairGroupsMeshesSourceDescription* Desc = GetSourceDescription(GetHairGroupsMeshes(), GroupIndex, LODIt, SourceIt);
			if (!Desc)
			{
				continue;
			}
			FHairGroupPlatformData::FMeshes::FLOD& LOD = GroupData.Meshes.LODs[LODIt];
			InternalReleaseResource(LOD.RestResource);

			if (LOD.HasValidData())
			{
				LOD.RestResource = new FHairMeshesRestResource(LOD.BulkData, FHairResourceName(GetFName(), GroupIndex, LODIt), GetAssetPathName(LODIt));
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
	if (GroupIndex < 0 || GroupIndex >= GetHairGroupsLOD().Num())
	{
		return EGroomGeometryType::Strands;
	}

	if (LODIndex < 0 || LODIndex >= GetHairGroupsLOD()[GroupIndex].LODs.Num())
	{
		return EGroomGeometryType::Strands;
	}

	return GetHairGroupsLOD()[GroupIndex].LODs[LODIndex].GeometryType;
}

EGroomBindingType UGroomAsset::GetBindingType(int32 GroupIndex, int32 LODIndex) const
{
	if (GroupIndex < 0 || GroupIndex >= GetHairGroupsLOD().Num() || !IsHairStrandsBindingEnable())
	{
		return EGroomBindingType::Rigid; // Fallback to rigid by default
	}

	if (LODIndex < 0 || LODIndex >= GetHairGroupsLOD()[GroupIndex].LODs.Num())
	{
		return EGroomBindingType::Rigid; // Fallback to rigid by default
	}

	return GetHairGroupsLOD()[GroupIndex].LODs[LODIndex].BindingType;
}

bool UGroomAsset::IsVisible(int32 GroupIndex, int32 LODIndex) const
{
	if (GroupIndex < 0 || GroupIndex >= GetHairGroupsLOD().Num())
	{
		return false;
	}

	if (LODIndex < 0 || LODIndex >= GetHairGroupsLOD()[GroupIndex].LODs.Num())
	{
		return false;
	}

	return GetHairGroupsLOD()[GroupIndex].LODs[LODIndex].bVisible;
}

bool UGroomAsset::IsDeformationEnable(int32 GroupIndex) const
{
	return 
		GetHairGroupsInterpolation().IsValidIndex(GroupIndex) && 
		GetHairGroupsInterpolation()[GroupIndex].InterpolationSettings.GuideType == EGroomGuideType::Rigged;
}

bool UGroomAsset::IsSimulationEnable(int32 GroupIndex, int32 LODIndex) const 
{
	if (GroupIndex < 0 || GroupIndex >= GetHairGroupsLOD().Num() || !IsHairStrandsSimulationEnable())
	{
		return false;
	}

	if (LODIndex >= GetHairGroupsLOD()[GroupIndex].LODs.Num())
	{
		return false;
	}

	check(GetHairGroupsPhysics().Num() == GetHairGroupsLOD().Num());

	// If the LOD index is not forced, then its value could be -1. In this case we return the 'global' asset value
	if (LODIndex < 0)
	{
		return GetHairGroupsPhysics()[GroupIndex].SolverSettings.EnableSimulation;
	}

	if (!IsHairLODSimulationEnabled(LODIndex))
	{
		return false;
	}

	return GetHairGroupsLOD()[GroupIndex].LODs[LODIndex].bVisible && (
		GetHairGroupsLOD()[GroupIndex].LODs[LODIndex].Simulation == EGroomOverrideType::Enable ||
		(GetHairGroupsLOD()[GroupIndex].LODs[LODIndex].Simulation == EGroomOverrideType::Auto && GetHairGroupsPhysics()[GroupIndex].SolverSettings.EnableSimulation));
}

bool UGroomAsset::IsGlobalInterpolationEnable(int32 GroupIndex, int32 LODIndex) const
{
	if (GroupIndex < 0 || GroupIndex >= GetHairGroupsLOD().Num())
	{
		return false;
	}

	if (LODIndex < 0 || LODIndex >= GetHairGroupsLOD()[GroupIndex].LODs.Num())
	{
		return false;
	}

	// If the LOD index is not forced, then its value could be -1. In this case we return the 'global' asset value
	if (LODIndex < 0)
	{
		return GetEnableGlobalInterpolation();
	}

	return
		GetHairGroupsLOD()[GroupIndex].LODs[LODIndex].GlobalInterpolation == EGroomOverrideType::Enable ||
		(GetHairGroupsLOD()[GroupIndex].LODs[LODIndex].GlobalInterpolation == EGroomOverrideType::Auto && GetEnableGlobalInterpolation());
}

bool UGroomAsset::IsSimulationEnable() const
{
	for (int32 GroupIndex = 0; GroupIndex < GetHairGroupsPlatformData().Num(); ++GroupIndex)
	{
		for (int32 LODIt = 0; LODIt < GetHairGroupsLOD().Num(); ++LODIt)
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
	for (int32 LODIt = 0; LODIt < GetHairGroupsLOD().Num(); ++LODIt)
	{
		if (IsSimulationEnable(GroupIndex, LODIt) || IsGlobalInterpolationEnable(GroupIndex, LODIt) || GetEnableSimulationCache())
		{
			return true;
		}
	}

	return IsDeformationEnable(GroupIndex);
}

bool UGroomAsset::NeedsInterpolationData() const
{
	for (int32 GroupIndex = 0; GroupIndex < GetHairGroupsPlatformData().Num(); ++GroupIndex)
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
	for (const FHairGroupsLOD& S : GetHairGroupsLOD())
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

	const int32 GroupCount = GetHairGroupsLOD().Num();
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
			GetHairGroupsLOD()[GroupIt].LODs.RemoveAt(LODIt);
		}

		--LODsTOKeepIndex;
	}

	// Rebuild the LOD data
	if (bRebuildResources && CanRebuildFromDescription())
	{
		const FHairDescriptionGroups& LocalHairDescriptionGroups = GetHairDescriptionGroups();
		for (int32 GroupIt = 0; GroupIt < GroupCount; ++GroupIt)
		{
			FHairGroupPlatformData& GroupData = GetHairGroupsPlatformData()[GroupIt];
			InternalReleaseResource(GroupData.Strands.ClusterResource);

			BuildHairGroupCluster(
				GroupIt,
				LocalHairDescriptionGroups,
				GetHairGroupsInterpolation(),
				GetHairGroupsLOD(),
				GetHairGroupsInfo(),
				GetHairGroupsPlatformData());
		}
	}
}
#endif // WITH_EDITORONLY_DATA

bool UGroomAsset::HasDebugData() const
{
	for (uint32 GroupIndex = 0, GroupCount = GetNumHairGroups(); GroupIndex < GroupCount; ++GroupIndex)
	{
		if (!GetHairGroupsPlatformData()[GroupIndex].Debug.Data.IsValid())
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
		FGroomBuilder::BuildData(LocalHairDescriptionGroups.HairGroups[GroupIndex], GetHairGroupsInterpolation()[GroupIndex], DummyInfo, StrandsData, GuidesData);

		FHairGroupPlatformData& GroupData = GetHairGroupsPlatformData()[GroupIndex];
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
	const int32 SlotCount = GetHairGroupsMaterials().Num();
	for (int32 SlotIt = 0; SlotIt < SlotCount; ++SlotIt)
	{
		if (GetHairGroupsMaterials()[SlotIt].SlotName == MaterialSlotName)
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
	for (const FHairGroupsMaterial& Material : GetHairGroupsMaterials())
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
	if (MaterialIndex < 0 || MaterialIndex >= GetHairGroupsMaterials().Num())
		return false;

	const FName MaterialSlotName = GetHairGroupsMaterials()[MaterialIndex].SlotName;
	return 
		InternalIsMaterialUsed(GetHairGroupsRendering(), MaterialSlotName) ||
		InternalIsMaterialUsed(GetHairGroupsCards(), MaterialSlotName) ||
		InternalIsMaterialUsed(GetHairGroupsMeshes(), MaterialSlotName);
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

	if (DescIndex >= uint32(GetHairGroupsCards().Num()))
		return;

	FHairGroupsCardsSourceDescription* Desc = &GetHairGroupsCards()[DescIndex];

	const int32 GroupIndex = Desc->GroupIndex;
	const int32 LODIndex = Desc->LODIndex;
	if (GroupIndex >= GetHairGroupsPlatformData().Num())
		return;

	// 1. Convert old parameters (ClusterDecimation, bUseCards) to new parameters (GenerationType & CardsCount)
	{
		FHairCardsClusterSettings& ClusterSettings = Desc->ProceduralSettings.ClusterSettings;
		FHairCardsGeometrySettings& GeometrySettings = Desc->ProceduralSettings.GeometrySettings;
		const bool bNeedConversion = ClusterSettings.ClusterDecimation > 0;
		if (bNeedConversion)
		{
			const int32 MaxCardCount = GetHairGroupsPlatformData()[GroupIndex].Strands.BulkData.GetNumCurves();

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
		FGroomBuilder::BuildData(LocalHairDescriptionGroups.HairGroups[GroupIndex], GetHairGroupsInterpolation()[GroupIndex], DummyInfo, StrandsData, GuidesData);

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

	const FName OwnerName = GetAssetPathName(LODIndex);
	// 3. Create resources and enqueue texture generation (GPU, kicked by the render thread) 
	Q.Resources = new FHairCardsRestResource(Q.BulkData, FHairResourceName(GetFName(), GroupIndex, LODIndex), OwnerName);
	BeginInitResource(Q.Resources); // Immediate allocation, as needed for the vertex factory, input stream building
	Q.ProceduralResources = new FHairCardsProceduralResource(Q.ProceduralData.RenderData, Q.ProceduralData.Atlas.Resolution, Q.ProceduralData.Voxels, OwnerName);
	BeginInitResource(Q.ProceduralResources); // Immediate allocation, as needed for the vertex factory, input stream building

	FHairCardsBuilder::BuildTextureAtlas(&Q.ProceduralData, Q.Resources, Q.ProceduralResources, Q.Textures);

	// 4. Save output asset (geometry, and enqueue texture saving)
	{
		// Create a static meshes with the vertex data
		if (GetHairGroupsCards()[DescIndex].ProceduralMesh == nullptr)
		{
			const FString PackageName = GetOutermost()->GetName();
			const FString SuffixName = FText::Format(LOCTEXT("CardsStatisMesh", "_CardsMesh_Group{0}_LOD{1}"), FText::AsNumber(GroupIndex), FText::AsNumber(LODIndex)).ToString();
			GetHairGroupsCards()[DescIndex].ProceduralMesh = FHairStrandsCore::CreateStaticMesh(PackageName, SuffixName);
		}

		// Convert procedural cards data to cards data prior to export
		FHairCardsDatas CardData;
		{
			CardData.Cards = Q.ProceduralData.Cards;
		}

		FHairCardsBuilder::ExportGeometry(CardData, GetHairGroupsCards()[DescIndex].ProceduralMesh);
		FHairStrandsCore::SaveAsset(GetHairGroupsCards()[DescIndex].ProceduralMesh);
		GetHairGroupsCards()[DescIndex].ProceduralMeshKey = GroomDerivedDataCacheUtils::BuildCardsDerivedDataKeySuffix(this, *Desc);
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
		FGroomBuilder::BuildData(HairGroup, GetHairGroupsInterpolation()[GroupIndex], DummyInfo, OutStrandsData, OutGuidesData);
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
	if (FHairGroupsCardsSourceDescription* Desc = GetSourceDescription(GetHairGroupsCards(), GroupIndex, LODIndex, SourceIt))
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
	CumulativeResourceSize.AddDedicatedSystemMemoryBytes(GetHairGroupsRendering().GetAllocatedSize());
	CumulativeResourceSize.AddDedicatedSystemMemoryBytes(GetHairGroupsPhysics().GetAllocatedSize());
	CumulativeResourceSize.AddDedicatedSystemMemoryBytes(GetHairGroupsInterpolation().GetAllocatedSize());
	CumulativeResourceSize.AddDedicatedSystemMemoryBytes(GetHairGroupsLOD().GetAllocatedSize());
	CumulativeResourceSize.AddDedicatedSystemMemoryBytes(GetHairGroupsCards().GetAllocatedSize());
	CumulativeResourceSize.AddDedicatedSystemMemoryBytes(GetHairGroupsMeshes().GetAllocatedSize());
	CumulativeResourceSize.AddDedicatedSystemMemoryBytes(GetHairGroupsMaterials().GetAllocatedSize());
	CumulativeResourceSize.AddDedicatedSystemMemoryBytes(GetHairGroupsPlatformData().GetAllocatedSize());
	
	for (const FHairGroupPlatformData & GroupData : GetHairGroupsPlatformData())
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

FName UGroomAsset::GetAssetPathName(int32 LODIndex)
{
#if RHI_ENABLE_RESOURCE_INFO
	if (LODIndex > -1)
	{
		return FName(FString::Printf(TEXT("%s [LOD%d]"), *GetPathName(), LODIndex));
	}
	else
	{
		return FName(GetPathName());
	}
#else
	return NAME_None;
#endif
}

#undef LOCTEXT_NAMESPACE

#define GROOMASSET_DEFINE_MEMBER_NAME(Name) \
	FName UGroomAsset::Get##Name##MemberName()\
	{\
		PRAGMA_DISABLE_DEPRECATION_WARNINGS\
		return FName(TEXT(#Name));\
		PRAGMA_ENABLE_DEPRECATION_WARNINGS\
	}

#define GROOMASSET_DEFINE_GET(Type, Name, Access, Const0, Const1) \
	Const0 Type Access UGroomAsset::Get##Name() Const1\
	{\
		PRAGMA_DISABLE_DEPRECATION_WARNINGS\
		return Name;\
		PRAGMA_ENABLE_DEPRECATION_WARNINGS\
	}

#define GROOMASSET_DEFINE_SET(Type, Name, Access, Const) \
	void UGroomAsset::Set##Name(Const Type Access In##Name)\
	{\
		PRAGMA_DISABLE_DEPRECATION_WARNINGS\
		Name = In##Name;\
		PRAGMA_ENABLE_DEPRECATION_WARNINGS\
	}

#define DEFINE_GROOM_MEMBER_ACCESSOR_BP(Type, Name)\
	GROOMASSET_DEFINE_MEMBER_NAME(Name)\
	UFUNCTION(BlueprintGetter)\
	GROOMASSET_DEFINE_GET(Type, Name, , , const)\
	UFUNCTION(BlueprintSetter)\
	GROOMASSET_DEFINE_SET(Type, Name, , )

#define DEFINE_GROOM_MEMBER_ACCESSOR_BP_ARRAY(Type, Name)\
	GROOMASSET_DEFINE_MEMBER_NAME(Name)\
	UFUNCTION(BlueprintGetter)\
	GROOMASSET_DEFINE_GET(Type, Name, &, , )\
	UFUNCTION(BlueprintSetter)\
	GROOMASSET_DEFINE_SET(Type, Name, &, const)\
	GROOMASSET_DEFINE_GET(Type, Name, &, const, const)

#define DEFINE_GROOM_MEMBER_ACCESSOR(Type, Name)\
	GROOMASSET_DEFINE_MEMBER_NAME(Name)\
	GROOMASSET_DEFINE_GET(Type, Name, , , const)\
	GROOMASSET_DEFINE_SET(Type, Name, , )

#define DEFINE_GROOM_MEMBER_ACCESSOR_ARRAY(Type, Name)\
	GROOMASSET_DEFINE_MEMBER_NAME(Name)\
	GROOMASSET_DEFINE_GET(Type, Name, &, , )\
	GROOMASSET_DEFINE_SET(Type, Name, &, const)\
	GROOMASSET_DEFINE_GET(Type, Name, &, const, const)

// Define most of the groom member accessor
DEFINE_GROOM_MEMBER_ACCESSOR_BP_ARRAY(TArray<FHairGroupsRendering>, HairGroupsRendering);
DEFINE_GROOM_MEMBER_ACCESSOR_BP_ARRAY(TArray<FHairGroupsPhysics>, HairGroupsPhysics);
DEFINE_GROOM_MEMBER_ACCESSOR_BP_ARRAY(TArray<FHairGroupsInterpolation>, HairGroupsInterpolation);
DEFINE_GROOM_MEMBER_ACCESSOR_BP_ARRAY(TArray<FHairGroupsLOD>, HairGroupsLOD);
DEFINE_GROOM_MEMBER_ACCESSOR_BP_ARRAY(TArray<FHairGroupsCardsSourceDescription>, HairGroupsCards);
DEFINE_GROOM_MEMBER_ACCESSOR_BP_ARRAY(TArray<FHairGroupsMeshesSourceDescription>, HairGroupsMeshes);
DEFINE_GROOM_MEMBER_ACCESSOR_BP_ARRAY(TArray<FHairGroupsMaterial>, HairGroupsMaterials);
DEFINE_GROOM_MEMBER_ACCESSOR_BP_ARRAY(TArray<int32>, DeformedGroupSections);

DEFINE_GROOM_MEMBER_ACCESSOR_BP(bool, EnableGlobalInterpolation);
DEFINE_GROOM_MEMBER_ACCESSOR_BP(bool, EnableSimulationCache);
DEFINE_GROOM_MEMBER_ACCESSOR_BP(EGroomInterpolationType, HairInterpolationType);
DEFINE_GROOM_MEMBER_ACCESSOR_BP(USkeletalMesh*, RiggedSkeletalMesh);

DEFINE_GROOM_MEMBER_ACCESSOR(FPerPlatformInt, MinLOD);
DEFINE_GROOM_MEMBER_ACCESSOR(FPerPlatformBool, DisableBelowMinLodStripping);
DEFINE_GROOM_MEMBER_ACCESSOR_ARRAY(TArray<float>, EffectiveLODBias);
DEFINE_GROOM_MEMBER_ACCESSOR_ARRAY(TArray<FHairGroupPlatformData>, HairGroupsPlatformData);
DEFINE_GROOM_MEMBER_ACCESSOR_ARRAY(TArray<FHairGroupInfoWithVisibility>, HairGroupsInfo);
