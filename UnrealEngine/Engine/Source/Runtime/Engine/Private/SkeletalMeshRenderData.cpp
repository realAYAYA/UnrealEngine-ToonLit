// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rendering/SkeletalMeshRenderData.h"
#include "Engine/SkinnedAsset.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkinnedAssetAsyncCompileUtils.h"
#include "Engine/SkinnedAssetCommon.h"
#include "EngineLogs.h"
#include "UObject/Package.h"
#include "Rendering/RenderCommandPipes.h"

#if WITH_EDITOR
#include "ProfilingDebugging/CookStats.h"
#include "DerivedDataCacheInterface.h"
#include "IMeshBuilderModule.h"
#include "RenderingThread.h"
#include "Serialization/LargeMemoryReader.h"
#include "Serialization/LargeMemoryWriter.h"

#if ENABLE_COOK_STATS
namespace SkeletalMeshCookStats
{
	static FCookStats::FDDCResourceUsageStats UsageStats;
	static FCookStatsManager::FAutoRegisterCallback RegisterCookStats([](FCookStatsManager::AddStatFuncRef AddStat)
	{
		UsageStats.LogStats(AddStat, TEXT("SkeletalMesh.Usage"), TEXT(""));
	});
}
#endif

extern int32 GStripSkeletalMeshLodsDuringCooking;

#endif // WITH_EDITOR

int32 GSkeletalMeshKeepMobileMinLODSettingOnDesktop = 0;
static FAutoConsoleVariableRef CVarSkeletalMeshKeepMobileMinLODSettingOnDesktop(
	TEXT("r.SkeletalMesh.KeepMobileMinLODSettingOnDesktop"),
	GSkeletalMeshKeepMobileMinLODSettingOnDesktop,
	TEXT("If non-zero, mobile setting for MinLOD will be stored in the cooked data for desktop platforms"));

#if WITH_EDITOR

/** 
 * Utility functions for storing and accessing data that exceeds the usual signed 32bit limits 
 * for data length.
 * We achieve this by splitting the data into multiple chunks that the DDC can handle along with 
 * a header chunk. Then when the data is requested we can load each chunk and reconstruct the 
 * original data.
 */
namespace DDCUtils64Bit
{
	struct FDDCChunkingHeader
	{
		/** Overall size of the data when reconstructed. */
		int64 TotalSize;
		/** The number of chunks that the data was split into. */
		int32 NumChunks;
	};

	/** The same as calling GetDerivedDataCacheRef().GetSynchronous(...) but with a TArray64 as the output parameter. */
	bool GetSynchronous(const FString& DerivedDataKey, USkinnedAsset* Owner, TArray64<uint8>& OutDerivedData)
	{
		TStringBuilder<512> OwnerPathName;
		Owner->GetPathName(nullptr, OwnerPathName);

		TArray<uint8> Data32Bit;
		if (GetDerivedDataCacheRef().GetSynchronous(*DerivedDataKey, Data32Bit, OwnerPathName))
		{
			// Note that currently the MoveTemp does nothing and the data is being copied although 
			// at some point this might be optimized and the TArray64 will just assume ownership of
			// the TArrays allocation instead.
			OutDerivedData = MoveTemp(Data32Bit);
			return true;
		}
		else
		{
			TStringBuilder<512> HeaderKey;
			HeaderKey << DerivedDataKey << TEXT("Header");

			TArray<uint8> HeaderData;
			HeaderData.Reserve(sizeof(FDDCChunkingHeader));

			// Early out if we cannot find the header or that it is the wrong size (in which case we cannot cast it)
			if (!GetDerivedDataCacheRef().GetSynchronous(HeaderKey.ToString(), HeaderData, Owner->GetPathName()) || HeaderData.Num() != sizeof(FDDCChunkingHeader))
			{
				return false;
			}

			FDDCChunkingHeader* Header = (FDDCChunkingHeader*)HeaderData.GetData();
			OutDerivedData.Reserve(Header->TotalSize);

			for (int32 ChunkIndex = 0; ChunkIndex < Header->NumChunks; ChunkIndex++)
			{
				TStringBuilder<512> ChunkKey;
				ChunkKey << DerivedDataKey << TEXT("Chunk") << ChunkIndex;

				TArray<uint8> ChunkData;
				if (!GetDerivedDataCacheRef().GetSynchronous(ChunkKey.ToString(), ChunkData, OwnerPathName))
				{
					OutDerivedData.Empty(); // Get rid of any partial results we might have
					return false;
				}

				OutDerivedData.Append(ChunkData);
			}

			return true;
		}
	}

	/** The same as calling GetDerivedDataCacheRef().Put(...) but with a TArrayView64 as the input data. */
	void Put(const FString& DerivedDataKey, USkinnedAsset* Owner, TArrayView64<const uint8> DerivedData)
	{
		TStringBuilder<512> OwnerPathName;
		Owner->GetPathName(nullptr, OwnerPathName);

		// We don't use the full 32 bit range as internally the DDC might append info to the end of 
		// the chunk, so we reserve 4kb for this, which is more than enough space to be safe.

		const int64 ChunkSize = MAX_int32 - (4 * 1024);
		if (DerivedData.Num() <= ChunkSize)
		{
			TArrayView<const uint8> DerivedData32Bit(DerivedData.GetData(), (int32)DerivedData.Num());
			GetDerivedDataCacheRef().Put(*DerivedDataKey, DerivedData32Bit, OwnerPathName);
		}
		else
		{
			const int32 NumChunks = (int32)FMath::DivideAndRoundUp(DerivedData.Num(), ChunkSize);

			FDDCChunkingHeader Header{ DerivedData.Num(), NumChunks };

			{
				TStringBuilder<512> HeaderKey;
				HeaderKey << DerivedDataKey << TEXT("Header");

				TArrayView<const uint8> HeaderView((uint8*)&Header, sizeof(FDDCChunkingHeader));

				GetDerivedDataCacheRef().Put(HeaderKey.ToString(), HeaderView, OwnerPathName);
			}

			for (int32 ChunkIndex = 0; ChunkIndex < NumChunks; ++ChunkIndex)
			{
				const int64 ChunkStart = ChunkIndex * ChunkSize;
				const uint64 BytesInChunk = FMath::Min(ChunkSize, DerivedData.Num() - ChunkStart);

				TArrayView<const uint8> ChunkData(DerivedData.GetData() + ChunkStart, (int32)BytesInChunk);

				TStringBuilder<512> ChunkKey;
				ChunkKey << DerivedDataKey << TEXT("Chunk") << ChunkIndex;
				GetDerivedDataCacheRef().Put(ChunkKey.ToString(), ChunkData, OwnerPathName);
			}
		}
	}
} //namespace DDCUtils64Bit

/** This code verify that the data is all in sync index buffer versus sections data. It is active only in debug build*/
void VerifyAllLodSkeletalMeshModelIntegrity(USkinnedAsset* Owner)
{
	if (!Owner || !Owner->GetImportedModel())
	{
		return;
	}

	FSkeletalMeshModel* SkelMeshModel = Owner->GetImportedModel();
	for (int32 LODIndex = 0; LODIndex < SkelMeshModel->LODModels.Num(); LODIndex++)
	{
		FSkeletalMeshLODModel* LODModel = &(SkelMeshModel->LODModels[LODIndex]);
		int32 SectionsVerticeNum = 0;
		int32 SectionsTriangleNum = 0;
		for (const FSkelMeshSection& Section : LODModel->Sections)
		{
			SectionsVerticeNum += Section.GetNumVertices();
			SectionsTriangleNum += Section.NumTriangles;
			int32 LastSectionIndexBuffer = Section.BaseIndex + (Section.NumTriangles * 3);
			if (Section.NumTriangles > 0)
			{
				//Remove 1 if we have at least one triangle
				LastSectionIndexBuffer--;
			}

			if (LODModel->IndexBuffer.IsValidIndex(LastSectionIndexBuffer))
			{
				uint32 FirstSectionIndexBufferValue = LODModel->IndexBuffer[Section.BaseIndex];
				uint32 LastSectionIndexBufferValue = LODModel->IndexBuffer[LastSectionIndexBuffer];
				if (FirstSectionIndexBufferValue < Section.BaseVertexIndex || LastSectionIndexBufferValue >= Section.BaseVertexIndex + Section.GetNumVertices())
				{
					UE_ASSET_LOG(LogSkeletalMesh, Error, Owner, TEXT("The source model is corrupted! Section triangle refer to a vertex not in the section. LOD %d"), LODIndex);
				}
			}
			else
			{
				UE_ASSET_LOG(LogSkeletalMesh, Error, Owner, TEXT("The source model is corrupted! Section index buffer is invalid. LOD %d"), LODIndex);
			}
		}

		if (LODModel->NumVertices != SectionsVerticeNum)
		{
			UE_ASSET_LOG(LogSkeletalMesh, Error, Owner, TEXT("The source model is corrupted! Total sections vertice count is different from source model vertice count. LOD %d"), LODIndex);
		}
		if ((LODModel->IndexBuffer.Num() / 3) != SectionsTriangleNum)
		{
			UE_ASSET_LOG(LogSkeletalMesh, Error, Owner, TEXT("The source model is corrupted! Total sections triangle count is different from source model triangle count (index count divide by 3). LOD %d"), LODIndex);
		}
	}
}

FString FSkeletalMeshRenderData::GetDerivedDataKey(const ITargetPlatform* TargetPlatform, USkinnedAsset* Owner)
{
	return Owner->BuildDerivedDataKey(TargetPlatform);
}

void FSkeletalMeshRenderData::Cache(const ITargetPlatform* TargetPlatform, USkinnedAsset* Owner, FSkinnedAssetCompilationContext* ContextPtr)
{
	check(Owner);
	// Disable ContextPtr check because only USkeletalMesh supports it.
	//check(ContextPtr);

	check(LODRenderData.Num() == 0); // Should only be called on new, empty RenderData
	check(TargetPlatform);

	auto SerializeLodModelDdcData = [&Owner](FSkeletalMeshLODModel* LODModel, FArchive& Ar)
	{
		//Make sure we add everything FSkeletalMeshLODModel got modified by the skeletalmesh builder
		Ar << LODModel->Sections;
		Ar << LODModel->NumVertices;
		Ar << LODModel->NumTexCoords;
		Ar << LODModel->IndexBuffer;
		Ar << LODModel->ActiveBoneIndices;
		Ar << LODModel->RequiredBones;
		Ar << LODModel->MeshToImportVertexMap;
		Ar << LODModel->MaxImportVertex;
		TArray<uint32>& RawPointIndices = LODModel->GetRawPointIndices();
		Ar << RawPointIndices;
	};

	{
		COOK_STAT(auto Timer = SkeletalMeshCookStats::UsageStats.TimeSyncWork());
		int32 T0 = FPlatformTime::Cycles();

		//When we import a skeletalmesh, in some cases the asset is not yet built, and the usersectiondata and the inline cache are not set
		//until the initial build. This is due to the section count which is establish by the initial build of the import data. The section count
		//is part of the key because users can change section settings(see UserSectionData). So when we do a initial build we do not compute yet
		//the key and force the build code path, the key will be compute after the build and the DDC data will be store with the computed key.
		const bool bAllowDdcFetch = Owner->IsInitialBuildDone();
		if (bAllowDdcFetch)
		{
			DerivedDataKey = Owner->BuildDerivedDataKey(TargetPlatform);
		}
		
		//If we have an initial build, the ddc key will be computed only after the build. Some structure are missing until we first build the asset to get the drived data key
		
		TArray64<uint8> DerivedData;
		if(bAllowDdcFetch && DDCUtils64Bit::GetSynchronous(DerivedDataKey, Owner, DerivedData))
		{
			COOK_STAT(Timer.AddHit(DerivedData.Num()));

			FLargeMemoryReader Ar(DerivedData.GetData(), DerivedData.Num(), ELargeMemoryReaderFlags::Persistent);

			//Helper structure to change the morph targets
			TUniquePtr<FFinishBuildMorphTargetData> FinishBuildMorphTargetData;

			FSkeletalMeshModel* SkelMeshModel = Owner->GetImportedModel();
			check(SkelMeshModel);

			//Get the morph target data, we put it in the compilation context to apply them in the game thread before the InitResources
			if (Owner->GetMorphTargets().Num() > 0)
			{
				FinishBuildMorphTargetData = Owner->GetMorphTargets()[0]->CreateFinishBuildMorphTargetData();
			}
			else
			{
				// Create and initialize the FinishBuildInternalData, use the class default object to call the virtual function
				FinishBuildMorphTargetData = UMorphTarget::StaticClass()->GetDefaultObject<UMorphTarget>()->CreateFinishBuildMorphTargetData();
			}
			check(FinishBuildMorphTargetData);
			FinishBuildMorphTargetData->LoadFromMemoryArchive(Ar);

			//Serialize the LODModel sections since they are dependent on the reduction
			for (int32 LODIndex = 0; LODIndex < SkelMeshModel->LODModels.Num(); LODIndex++)
			{
				FSkeletalMeshLODModel* LODModel = &(SkelMeshModel->LODModels[LODIndex]);
				SerializeLodModelDdcData(LODModel, Ar);
				LODModel->SyncronizeUserSectionsDataArray();
			}

			Serialize(Ar, Owner);
			for (int32 LODIndex = 0; LODIndex < LODRenderData.Num(); ++LODIndex)
			{
				FSkeletalMeshLODRenderData& LODData = LODRenderData[LODIndex];
				if (LODData.bStreamedDataInlined)
				{
					break;
				}
				constexpr uint8 DummyStripFlags = 0;
				const bool bForceKeepCPUResources = FSkeletalMeshLODRenderData::ShouldForceKeepCPUResources();
				const bool bNeedsCPUAccess = FSkeletalMeshLODRenderData::ShouldKeepCPUResources(Owner, LODIndex, bForceKeepCPUResources);
				LODData.SerializeStreamedData(Ar, Owner, LODIndex, DummyStripFlags, bNeedsCPUAccess, bForceKeepCPUResources);
			}

			//Apply the morphtargets change if any
			if (FinishBuildMorphTargetData.IsValid())
			{
				// Morph target is only supported on USkeletalMesh
				if (USkeletalMesh* SkMesh = Cast<USkeletalMesh>(Owner))
				{
					FinishBuildMorphTargetData->ApplyEditorData(SkMesh, ContextPtr ? ContextPtr->bIsSerializeSaving : false);
				}
			}

			int32 T1 = FPlatformTime::Cycles();
			UE_LOG(LogSkeletalMesh, Verbose, TEXT("Skeletal Mesh found in DDC [%fms] %s"), FPlatformTime::ToMilliseconds(T1 - T0), *Owner->GetPathName());
		}
		else
		{
			UE_LOG(LogSkeletalMesh, Log, TEXT("Building Skeletal Mesh %s..."),*Owner->GetName());

			// Allocate empty entries for each LOD level in source mesh
			FSkeletalMeshModel* SkelMeshModel = Owner->GetImportedModel();
			check(SkelMeshModel);

			for (int32 LODIndex = 0; LODIndex < SkelMeshModel->LODModels.Num(); LODIndex++)
			{
				Owner->BuildLODModel(TargetPlatform, LODIndex);

				const FSkeletalMeshLODModel* LODModel = &(SkelMeshModel->LODModels[LODIndex]);
				const FSkeletalMeshLODInfo* LODInfo = Owner->GetLODInfo(LODIndex);
				check(LODInfo);

				FSkeletalMeshLODRenderData* LODData = new FSkeletalMeshLODRenderData();
				LODRenderData.Add(LODData);
				
				//Get the UVs and tangents precision build settings flag specific for this LOD index
				ESkeletalMeshVertexFlags VertexBufferBuildFlags = Owner->GetVertexBufferFlags();
				{
					const bool bUseFullPrecisionUVs = LODInfo->BuildSettings.bUseFullPrecisionUVs;
					const bool bUseHighPrecisionTangentBasis = LODInfo->BuildSettings.bUseHighPrecisionTangentBasis;
					const bool bUseBackwardsCompatibleF16TruncUVs = LODInfo->BuildSettings.bUseBackwardsCompatibleF16TruncUVs;
					const bool bUseHighPrecisionWeights = LODInfo->BuildSettings.bUseHighPrecisionSkinWeights;
					if (bUseFullPrecisionUVs)
					{
						VertexBufferBuildFlags |= ESkeletalMeshVertexFlags::UseFullPrecisionUVs;
					}
					if (bUseHighPrecisionTangentBasis)
					{
						VertexBufferBuildFlags |= ESkeletalMeshVertexFlags::UseHighPrecisionTangentBasis;
					}
					if (bUseBackwardsCompatibleF16TruncUVs)
					{
						VertexBufferBuildFlags |= ESkeletalMeshVertexFlags::UseBackwardsCompatibleF16TruncUVs;
					}
					if (bUseHighPrecisionWeights)
					{
						VertexBufferBuildFlags |= ESkeletalMeshVertexFlags::UseHighPrecisionWeights;
					}
				}
				FSkeletalMeshLODRenderData::FBuildSettings BuildSettings;
				BuildSettings.BuildFlags = VertexBufferBuildFlags;
				BuildSettings.bBuildHalfEdgeBuffers = LODInfo->bBuildHalfEdgeBuffers;
				
				LODData->BuildFromLODModel(LODModel, LODInfo->VertexAttributes, BuildSettings);
			}

			FLargeMemoryWriter Ar(0, /*bIsPersistent=*/ true);

			int32 MorphTargetNumber = Owner->GetMorphTargets().Num();
			Ar << MorphTargetNumber;
			for (int32 MorphTargetIndex = 0; MorphTargetIndex < MorphTargetNumber; ++MorphTargetIndex)
			{
				Owner->GetMorphTargets()[MorphTargetIndex]->SerializeMemoryArchive(Ar);
			}
			//No need to serialize the morph target mapping since we will rebuild the mapping when loading a ddc

			//Serialize the LODModel sections since they are dependent on the reduction
			for (int32 LODIndex = 0; LODIndex < SkelMeshModel->LODModels.Num(); LODIndex++)
			{
				FSkeletalMeshLODModel* LODModel = &(SkelMeshModel->LODModels[LODIndex]);
				SerializeLodModelDdcData(LODModel, Ar);
			}

			IMeshBuilderModule& MeshBuilderModule = IMeshBuilderModule::GetForPlatform(TargetPlatform);
			MeshBuilderModule.PostBuildSkeletalMesh(this, Owner);

			//Serialize the render data
			Serialize(Ar, Owner);
			for (int32 LODIndex = 0; LODIndex < LODRenderData.Num(); ++LODIndex)
			{
				FSkeletalMeshLODRenderData& LODData = LODRenderData[LODIndex];
				if (LODData.bStreamedDataInlined)
				{
					break;
				}
				const uint8 LODStripFlags = FSkeletalMeshLODRenderData::GenerateClassStripFlags(Ar, Owner, LODIndex);
				const bool bForceKeepCPUResources = FSkeletalMeshLODRenderData::ShouldForceKeepCPUResources();
				const bool bNeedsCPUAccess = FSkeletalMeshLODRenderData::ShouldKeepCPUResources(Owner, LODIndex, bForceKeepCPUResources);
				LODData.SerializeStreamedData(Ar, Owner, LODIndex, LODStripFlags, bNeedsCPUAccess, bForceKeepCPUResources);
			}

			//Recompute the derived data key in case there was some data correction during the build process, this make sure the DDC key is always representing the correct build result.
			//There should never be correction of the data during the build, the data has to be corrected in the post load before calling this function.
			FString BuiltDerivedDataKey = Owner->BuildDerivedDataKey(TargetPlatform);
			//Only compare keys if the ddc fetch was allowed
			if (bAllowDdcFetch)
			{
				if (BuiltDerivedDataKey != DerivedDataKey)
				{
					//If we are in this case we should resave the asset so the source data will be the same and we can use this DDC. Reduction can change the number of sections and the user section data is in the DDC key.
					//So if we change the reduction algorithm, its possible we fall in this situation.
					//We save the real data key which force the asset to always rebuild when the editor is loading it until the user save it
					UE_LOG(LogSkeletalMesh, Log, TEXT("Skeletal mesh [%s]: The derived data key is different after the build. Save the asset to avoid rebuilding it everytime the editor load it."), *Owner->GetPathName());
				}
			}
			else
			{
				//After the initial build we set the key to the built one
				DerivedDataKey = BuiltDerivedDataKey;
			}

			//Store the data using the built key to avoid DDC corruption
			TArrayView64<const uint8> ArView(Ar.GetData(), Ar.TotalSize());
			DDCUtils64Bit::Put(BuiltDerivedDataKey, Owner, ArView);

			int32 T1 = FPlatformTime::Cycles();
			UE_LOG(LogSkeletalMesh, Log, TEXT("Built Skeletal Mesh [%.2fs] %s"), FPlatformTime::ToMilliseconds(T1 - T0) / 1000.0f, *Owner->GetPathName());
			COOK_STAT(Timer.AddMiss(DerivedData.Num()));
		}
	}
	VerifyAllLodSkeletalMeshModelIntegrity(Owner);
}

void FSkeletalMeshRenderData::SyncUVChannelData(const TArray<FSkeletalMaterial>& ObjectData)
{
	TUniquePtr< TArray<FMeshUVChannelInfo> > UpdateData = MakeUnique< TArray<FMeshUVChannelInfo> >();
	UpdateData->Empty(ObjectData.Num());

	for (const FSkeletalMaterial& SkeletalMaterial : ObjectData)
	{
		UpdateData->Add(SkeletalMaterial.UVChannelData);
	}

	// SyncUVChannelData can be called from any thread during async skeletal mesh compilation. 
	// There is currently multiple race conditions in ENQUEUE_RENDER_COMMAND making it unsafe to be called from
	// any other thread than rendering or game because of the render thread suspension mecanism.
	// We sidestep the issue here by avoiding a call to ENQUEUE_RENDER_COMMAND if the resource has not been initialized and is still unknown
	// to the render thread.
	if (bInitialized)
	{
		ENQUEUE_RENDER_COMMAND(SyncUVChannelData)(UE::RenderCommandPipe::SkeletalMesh, [this, UpdateData = MoveTemp(UpdateData)]
		{
			Swap(UVChannelDataPerMaterial, *UpdateData.Get());
		});
	}
	else
	{
		Swap(UVChannelDataPerMaterial, *UpdateData.Get());
	}
}

#endif // WITH_EDITOR

FSkeletalMeshRenderData::FSkeletalMeshRenderData()
	: bReadyForStreaming(false)
	, NumInlinedLODs(0)
	, NumNonOptionalLODs(0)
	, CurrentFirstLODIdx(0)
	, PendingFirstLODIdx(0)
	, LODBiasModifier(0)
	, bSupportRayTracing(true)
	, bInitialized(false)
{}

FSkeletalMeshRenderData::~FSkeletalMeshRenderData()
{
	FSkeletalMeshLODRenderData** LODRenderDataArray = LODRenderData.GetData();
	for (int32 LODIndex = 0; LODIndex < LODRenderData.Num(); ++LODIndex)
	{
		LODRenderDataArray[LODIndex]->Release();
		// Prevent the array from calling the destructor to handle correctly the refcount.
		// For compatibility reason, LODRenderDataArray is using ptr directly instead of TRefCountPtr.
		LODRenderDataArray[LODIndex] = nullptr;
	}
	LODRenderData.Empty();
}

int32 FSkeletalMeshRenderData::GetNumNonStreamingLODs() const
{
	int LODCount = 0;
	for (int32 Idx = LODRenderData.Num() - 1; Idx >= 0; --Idx)
	{
		if (LODRenderData[Idx].bStreamedDataInlined)
		{
			++LODCount;
		}
		else
		{
			break;
		}
	}

	if (LODCount == 0 && LODRenderData.Num())
	{
		return 1;
	}
	else
	{
		return LODCount;
	}
}

int32 FSkeletalMeshRenderData::GetNumNonOptionalLODs() const
{
	int LODCount = 0;
	for (int32 Idx = LODRenderData.Num() - 1; Idx >= 0; --Idx)
	{
		// Make sure GetNumNonOptionalLODs() is bigger than GetNumNonStreamingLODs().
		if (LODRenderData[Idx].bStreamedDataInlined || !LODRenderData[Idx].bIsLODOptional)
		{
			++LODCount;
		}
		else
		{
			break;
		}
	}

	if (LODCount == 0 && LODRenderData.Num())
	{
		return 1;
	}
	else
	{
		return LODCount;
	}
}

void FSkeletalMeshRenderData::Serialize(FArchive& Ar, USkinnedAsset* Owner)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FSkeletalMeshRenderData::Serialize"), STAT_SkeletalMeshRenderData_Serialize, STATGROUP_LoadTime);

#if PLATFORM_DESKTOP

	if (Ar.IsFilterEditorOnly())
	{
		int32 MinMobileLODIdx = 0;
		bool bShouldSerialize = GSkeletalMeshKeepMobileMinLODSettingOnDesktop != 0;
#if WITH_EDITOR
		if (Ar.IsSaving())
		{
			if (Ar.CookingTarget()->GetPlatformInfo().PlatformGroupName == TEXT("Desktop")
				&& GStripSkeletalMeshLodsDuringCooking != 0
				&& GSkeletalMeshKeepMobileMinLODSettingOnDesktop != 0)
			{
				// Serialize 0 value when per quality level properties are used
				if (!Owner->IsMinLodQualityLevelEnable())
				{
					MinMobileLODIdx = Owner->GetMinLod().GetValueForPlatform(TEXT("Mobile")) - Owner->GetMinLod().GetValueForPlatform(TEXT("Desktop"));
					// Will be cast to uint8 when applying LOD bias. Also, make sure it's not < 0,
					// which can happen if the desktop min LOD is higher than the mobile setting
					MinMobileLODIdx = FMath::Clamp(MinMobileLODIdx, 0, 255); 
				}													
			}
			else
			{
				bShouldSerialize = false;
			}
		}
#endif

		if (bShouldSerialize)
		{
			Ar << MinMobileLODIdx;

			if (Ar.IsLoading() && GMaxRHIFeatureLevel == ERHIFeatureLevel::ES3_1)
			{
				LODBiasModifier = MinMobileLODIdx;
			}
		}
	}
#endif // PLATFORM_DESKTOP

	LODRenderData.Serialize(Ar, Owner);

#if WITH_EDITOR
	if (Ar.IsSaving())
	{
		NumInlinedLODs = GetNumNonStreamingLODs();
		NumNonOptionalLODs = GetNumNonOptionalLODs();
	}
#endif
	Ar << NumInlinedLODs << NumNonOptionalLODs;
#if WITH_EDITOR
	//Recompute on load because previously we were storing NumOptionalLODs, which is less convenient because it includes first LODs (and can be stripped by MinMip).
	if (Ar.IsLoading())
	{
		NumInlinedLODs = GetNumNonStreamingLODs();
		NumNonOptionalLODs = GetNumNonOptionalLODs();
	}
#endif
	ensure(LODRenderData.Num() >= NumInlinedLODs);
	
#if WITH_EDITORONLY_DATA
	const bool bUsingCookedEditorData = Owner->GetOutermost()->bIsCookedForEditor;
	if (bUsingCookedEditorData && Ar.IsLoading())
	{
		CurrentFirstLODIdx = Owner->GetMinLodIdx();
	}
	else
#endif
	{
		CurrentFirstLODIdx = LODRenderData.Num() - NumInlinedLODs;
	}

	PendingFirstLODIdx = CurrentFirstLODIdx;

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	bSupportRayTracing = Owner->GetSupportRayTracing();
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

void FSkeletalMeshRenderData::InitResources(bool bNeedsVertexColors, TArray<UMorphTarget*>& InMorphTargets, USkinnedAsset* Owner)
{
	if (!bInitialized)
	{
		// initialize resources for each lod
		for (int32 LODIndex = 0; LODIndex < LODRenderData.Num(); LODIndex++)
		{
			FSkeletalMeshLODRenderData& RenderData = LODRenderData[LODIndex];

			if(RenderData.GetNumVertices() > 0)
			{
				RenderData.InitResources(bNeedsVertexColors, LODIndex, InMorphTargets, Owner);
			}
		}

		ENQUEUE_RENDER_COMMAND(CmdSetSkeletalMeshReadyForStreaming)(UE::RenderCommandPipe::SkeletalMesh,
			[this, Owner]
		{
			bReadyForStreaming = true;
		});

		bInitialized = true;
	}
}

void FSkeletalMeshRenderData::ReleaseResources()
{
	if (bInitialized)
	{
		// release resources for each lod
		for (int32 LODIndex = 0; LODIndex < LODRenderData.Num(); LODIndex++)
		{
			LODRenderData[LODIndex].ReleaseResources();
		}
		bInitialized = false;
	}
}

uint32 FSkeletalMeshRenderData::GetNumBoneInfluences(int32 MinLODIndex) const
{
	uint32 NumBoneInfluences = 0;
	for (int32 LODIndex = MinLODIndex; LODIndex < LODRenderData.Num(); ++LODIndex)
	{
		const FSkeletalMeshLODRenderData& Data = LODRenderData[LODIndex];
		NumBoneInfluences = FMath::Max(NumBoneInfluences, Data.GetVertexBufferMaxBoneInfluences());
	}

	return NumBoneInfluences;
}

uint32 FSkeletalMeshRenderData::GetNumBoneInfluences() const
{
	return GetNumBoneInfluences(0);
}

bool FSkeletalMeshRenderData::RequiresCPUSkinning(ERHIFeatureLevel::Type FeatureLevel, int32 MinLODIndex) const
{
	const int32 MaxGPUSkinBones = FGPUBaseSkinVertexFactory::GetMaxGPUSkinBones();
	const int32 MaxBonesPerChunk = GetMaxBonesPerSection(MinLODIndex);
	// Do CPU skinning if we need too many bones per chunk
	return (MaxBonesPerChunk > MaxGPUSkinBones);
}

bool FSkeletalMeshRenderData::RequiresCPUSkinning(ERHIFeatureLevel::Type FeatureLevel) const
{
	return RequiresCPUSkinning(FeatureLevel, 0);
}

void FSkeletalMeshRenderData::GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize)
{
	for (int32 LODIndex = 0; LODIndex < LODRenderData.Num(); ++LODIndex)
	{
		const FSkeletalMeshLODRenderData& RenderData = LODRenderData[LODIndex];
		RenderData.GetResourceSizeEx(CumulativeResourceSize);
	}
}

SIZE_T FSkeletalMeshRenderData::GetCPUAccessMemoryOverhead() const
{
	SIZE_T Result = 0;
	for (int32 LODIndex = 0; LODIndex < LODRenderData.Num(); ++LODIndex)
	{
		const FSkeletalMeshLODRenderData& RenderData = LODRenderData[LODIndex];
		Result += RenderData.GetCPUAccessMemoryOverhead();
	}
	return Result;
}

int32 FSkeletalMeshRenderData::GetMaxBonesPerSection(int32 MinLODIdx) const
{
	int32 MaxBonesPerSection = 0;
	for (int32 LODIndex = MinLODIdx; LODIndex < LODRenderData.Num(); ++LODIndex)
	{
		const FSkeletalMeshLODRenderData& RenderData = LODRenderData[LODIndex];
		for (int32 SectionIndex = 0; SectionIndex < RenderData.RenderSections.Num(); ++SectionIndex)
		{
			MaxBonesPerSection = FMath::Max<int32>(MaxBonesPerSection, RenderData.RenderSections[SectionIndex].BoneMap.Num());
		}
	}
	return MaxBonesPerSection;
}

bool FSkeletalMeshRenderData::AnyRenderSectionCastsShadows(int32 MinLODIdx) const
{
	for (int32 LODIndex = MinLODIdx; LODIndex < LODRenderData.Num(); LODIndex++)
	{
		for (const FSkelMeshRenderSection& RenderSection : LODRenderData[LODIndex].RenderSections)
		{
			if (RenderSection.bCastShadow)
			{
				return true;
			}
		}
	}

	return false;
}

int32 FSkeletalMeshRenderData::GetMaxBonesPerSection() const
{
	return GetMaxBonesPerSection(0);
}

int32 FSkeletalMeshRenderData::GetFirstValidLODIdx(int32 MinIdx) const
{
	const int32 LODCount = LODRenderData.Num();
	if (LODCount == 0)
	{
		return INDEX_NONE;
	}

	int32 LODIndex = FMath::Clamp<int32>(MinIdx, 0, LODCount - 1);
	while (LODIndex < LODCount && !LODRenderData[LODIndex].GetNumVertices())
	{
		++LODIndex;
	}
	return (LODIndex < LODCount) ? LODIndex : INDEX_NONE;
}
