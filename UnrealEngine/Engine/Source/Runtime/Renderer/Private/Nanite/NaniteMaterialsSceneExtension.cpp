// Copyright Epic Games, Inc. All Rights Reserved.

#include "NaniteMaterialsSceneExtension.h"
#include "ScenePrivate.h"
#include "RenderUtils.h"

static TAutoConsoleVariable<int32> CVarNaniteMaterialDataBufferMinSizeBytes(
	TEXT("r.Nanite.MaterialBuffers.MaterialDataMinSizeBytes"),
	4 * 1024,
	TEXT("The smallest size (in bytes) of the Nanite material data buffer."),
	ECVF_ReadOnly | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarNanitePrimitiveMaterialDataBufferMinSizeBytes(
	TEXT("r.Nanite.MaterialBuffers.PrimitiveDataMinSizeBytes"),
	4 * 1024,
	TEXT("The smallest size (in bytes) of the Nanite per-primitive material data buffer."),
	ECVF_ReadOnly | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<bool> CVarNaniteMaterialBufferAsyncUpdates(
	TEXT("r.Nanite.MaterialBuffers.AsyncUpdates"),
	true,
	TEXT("When non-zero, Nanite material data buffer updates are updated asynchronously."),
	ECVF_RenderThreadSafe
);

static int32 GNaniteMaterialBufferForceFullUpload = 0;
static FAutoConsoleVariableRef CVarNaniteMaterialBufferForceFullUpload(
	TEXT("r.Nanite.MaterialBuffers.ForceFullUpload"),
	GNaniteMaterialBufferForceFullUpload,
	TEXT("0: Do not force a full upload.\n")
	TEXT("1: Force one full upload on the next update.\n")
	TEXT("2: Force a full upload every frame."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<bool> CVarNaniteMaterialBufferDefrag(
	TEXT("r.Nanite.MaterialBuffers.Defrag"),
	true,
	TEXT("Whether or not to allow defragmentation of the Nanite material data buffer."),
	ECVF_RenderThreadSafe
);

static int32 GNaniteMaterialBufferForceDefrag = 0;
static FAutoConsoleVariableRef CVarNaniteMaterialBufferDefragForce(
	TEXT("r.Nanite.MaterialBuffers.Defrag.Force"),
	GNaniteMaterialBufferForceDefrag,
	TEXT("0: Do not force a full defrag.\n")
	TEXT("1: Force one full defrag on the next update.\n")
	TEXT("2: Force a full defrag every frame."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarNaniteMaterialBufferDefragLowWaterMark(
	TEXT("r.Nanite.MaterialBuffers.Defrag.LowWaterMark"),
	0.375f,
	TEXT("Ratio of used to allocated memory at which to decide to defrag the Nanite material data buffer."),
	ECVF_RenderThreadSafe
);

BEGIN_SHADER_PARAMETER_STRUCT(FNaniteMaterialsParameters, RENDERER_API)
	SHADER_PARAMETER(uint32, PrimitiveMaterialElementStride)
	SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, PrimitiveMaterialData)
	SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, MaterialData)
END_SHADER_PARAMETER_STRUCT()

DECLARE_SCENE_UB_STRUCT(FNaniteMaterialsParameters, NaniteMaterials, RENDERER_API)

namespace Nanite
{

static void GetDefaultMaterialsParameters(FNaniteMaterialsParameters& OutParameters, FRDGBuilder& GraphBuilder)
{
	auto DefaultBuffer = GraphBuilder.CreateSRV(GSystemTextures.GetDefaultByteAddressBuffer(GraphBuilder, 4u));
	OutParameters.PrimitiveMaterialElementStride = 0;
	OutParameters.PrimitiveMaterialData = DefaultBuffer;
	OutParameters.MaterialData = DefaultBuffer;
}


IMPLEMENT_SCENE_EXTENSION(FMaterialsSceneExtension);

bool FMaterialsSceneExtension::ShouldCreateExtension(FScene& InScene)
{
	return DoesRuntimeSupportNanite(GetFeatureLevelShaderPlatform(InScene.GetFeatureLevel()), true, true);
}

void FMaterialsSceneExtension::InitExtension(FScene& InScene)
{
	Scene = &InScene;

	// Determine if we want to be initially enabled or disabled
	const bool bNaniteEnabled = UseNanite(GetFeatureLevelShaderPlatform(InScene.GetFeatureLevel()));
	SetEnabled(bNaniteEnabled);
}

ISceneExtensionUpdater* FMaterialsSceneExtension::CreateUpdater()
{
	return new FUpdater(*this);
}

ISceneExtensionRenderer* FMaterialsSceneExtension::CreateRenderer()
{
	// We only need to create renderers when we're enabled
	if (!IsEnabled())
	{
		return nullptr;
	}

	return new FRenderer(*this);
}

#if WITH_EDITOR

FRDGBufferRef FMaterialsSceneExtension::CreateHitProxyIDBuffer(FRDGBuilder& GraphBuilder) const
{
	TaskHandles[UpdateHitProxyIDsTask].Wait();

	FRDGBufferRef Buffer;
	if (HitProxyIDs.Num() > 0)
	{
		Buffer = GraphBuilder.CreateBuffer(
			FRDGBufferDesc::CreateByteAddressDesc(HitProxyIDs.Num() * sizeof(uint32)),
			TEXT("Nanite.HitProxyTableDataBuffer")
		);
		
		GraphBuilder.QueueBufferUpload(Buffer, MakeArrayView(HitProxyIDs.GetData(), HitProxyIDs.Num()));
	}
	else
	{
		Buffer = GSystemTextures.GetDefaultByteAddressBuffer(GraphBuilder, 4);
	}

	return Buffer;
}

#endif // WITH_EDITOR

void FMaterialsSceneExtension::SetEnabled(bool bEnabled)
{
	if (bEnabled != IsEnabled())
	{
		if (bEnabled)
		{
			MaterialBuffers = MakeUnique<FMaterialBuffers>();
		}
		else
		{
			MaterialBuffers = nullptr;
			MaterialBufferAllocator.Reset();
			PrimitiveData.Reset();
		#if WITH_EDITOR
			HitProxyIDAllocator.Reset();
			HitProxyIDs.Reset();
		#endif
		}
	}
}

void FMaterialsSceneExtension::FinishMaterialBufferUpload(
	FRDGBuilder& GraphBuilder,
	FNaniteMaterialsParameters* OutParams)
{
	if (!IsEnabled())
	{
		return;
	}

	FRDGBufferRef PrimitiveBuffer = nullptr;
	FRDGBufferRef MaterialBuffer = nullptr;

	const uint32 MinPrimitiveDataSize = (PrimitiveData.GetMaxIndex() + 1) * sizeof(FPackedPrimitiveData) / 4;
	const uint32 MinMaterialDataSize = MaterialBufferAllocator.GetMaxSize();

	if (MaterialUploader.IsValid())
	{
		// Sync on upload tasks
		UE::Tasks::Wait(
			MakeArrayView(
				{
					TaskHandles[UploadPrimitiveDataTask],
					TaskHandles[UploadMaterialDataTask]
				}
			)
		);
		PrimitiveBuffer = MaterialUploader->PrimitiveDataUploader.ResizeAndUploadTo(
			GraphBuilder,
			MaterialBuffers->PrimitiveDataBuffer,
			MinPrimitiveDataSize
		);
		MaterialBuffer = MaterialUploader->MaterialDataUploader.ResizeAndUploadTo(
			GraphBuilder,
			MaterialBuffers->MaterialDataBuffer,
			MinMaterialDataSize
		);
		MaterialUploader = nullptr;
	}
	else
	{
		PrimitiveBuffer = MaterialBuffers->PrimitiveDataBuffer.ResizeBufferIfNeeded(GraphBuilder, MinPrimitiveDataSize);
		MaterialBuffer = MaterialBuffers->MaterialDataBuffer.ResizeBufferIfNeeded(GraphBuilder, MinMaterialDataSize);
	}

	if (OutParams != nullptr)
	{
		OutParams->PrimitiveMaterialData = GraphBuilder.CreateSRV(PrimitiveBuffer);
		OutParams->MaterialData = GraphBuilder.CreateSRV(MaterialBuffer);
	}
}

bool FMaterialsSceneExtension::ProcessBufferDefragmentation()
{
	// Consolidate spans
	MaterialBufferAllocator.Consolidate();
#if WITH_EDITOR
	HitProxyIDAllocator.Consolidate();
#endif

	// Decide to defragment the buffer when the used size dips below a certain multiple of the max used size.
	// Since the buffer allocates in powers of two, we pick the mid point between 1/4 and 1/2 in hopes to prevent
	// thrashing when usage is close to a power of 2.
	//
	// NOTES:
	//	* We only currently use the state of the material buffer's fragmentation to decide to defrag both buffers
	//	* Rather than trying to minimize number of moves/uploads, we just realloc and re-upload everything. This
	//	  could be implemented in a more efficient manner if the current method proves expensive.

	const bool bAllowDefrag = CVarNaniteMaterialBufferDefrag.GetValueOnRenderThread();
	static const int32 MinMaterialBufferSizeDwords = CVarNaniteMaterialDataBufferMinSizeBytes.GetValueOnRenderThread() / 4;
	const float LowWaterMarkRatio = CVarNaniteMaterialBufferDefragLowWaterMark.GetValueOnRenderThread();
	const int32 EffectiveMaxSize = FMath::RoundUpToPowerOfTwo(MaterialBufferAllocator.GetMaxSize());
	const int32 LowWaterMark = uint32(EffectiveMaxSize * LowWaterMarkRatio);
	const int32 UsedSize = MaterialBufferAllocator.GetSparselyAllocatedSize();
	
	if (!bAllowDefrag)
	{
		return false;
	}

	// check to force a defrag
	const bool bForceDefrag = GNaniteMaterialBufferForceDefrag != 0;
	if (GNaniteMaterialBufferForceDefrag == 1)
	{
		GNaniteMaterialBufferForceDefrag = 0;
	}
	
	if (!bForceDefrag && (EffectiveMaxSize <= MinMaterialBufferSizeDwords || UsedSize > LowWaterMark))
	{
		// No need to defragment
		return false;
	}

	MaterialBufferAllocator.Reset();
#if WITH_EDITOR
	HitProxyIDAllocator.Reset();
	HitProxyIDs.Reset();
#endif

	for (auto& Data : PrimitiveData)
	{
		if (Data.MaterialBufferOffset != INDEX_NONE)
		{
			Data.MaterialBufferOffset = INDEX_NONE;
			Data.MaterialBufferSizeDwords = 0;
		}
	#if WITH_EDITOR
		Data.HitProxyBufferOffset = INDEX_NONE;
	#endif
	}

	return true;
}


FMaterialsSceneExtension::FMaterialBuffers::FMaterialBuffers() :
	PrimitiveDataBuffer(
		CVarNanitePrimitiveMaterialDataBufferMinSizeBytes.GetValueOnAnyThread() / 4,
		TEXT("Nanite.PrimitiveMaterialData")
	),
	MaterialDataBuffer(
		CVarNaniteMaterialDataBufferMinSizeBytes.GetValueOnAnyThread() / 4,
		TEXT("Nanite.MaterialData")
	)
{
}


FMaterialsSceneExtension::FUpdater::FUpdater(FMaterialsSceneExtension& InSceneData) :
	SceneData(&InSceneData),
	bEnableAsync(CVarNaniteMaterialBufferAsyncUpdates.GetValueOnRenderThread())
{
}

void FMaterialsSceneExtension::FUpdater::End()
{
	// Ensure these tasks finish before we fall out of scope.
	// NOTE: This should be unnecessary if the updater shares the graph builder's lifetime but we don't enforce that
	SceneData->SyncAllTasks();
}

void FMaterialsSceneExtension::FUpdater::PreSceneUpdate(FRDGBuilder& GraphBuilder, const FScenePreUpdateChangeSet& ChangeSet)
{
	// If there was a pending upload from a prior update (due to the buffer never being used), finish the upload now.
	// This keeps the upload entries from growing unbounded and prevents any undefined behavior caused by any
	// updates that overlap primitives.
	SceneData->FinishMaterialBufferUpload(GraphBuilder);

	// Update whether or not we are enabled based on in Nanite is enabled
	const bool bNaniteEnabled = UseNanite(GetFeatureLevelShaderPlatform(SceneData->Scene->GetFeatureLevel()));
	SceneData->SetEnabled(bNaniteEnabled);

	if (!SceneData->IsEnabled())
	{
		return;
	}

	SceneData->TaskHandles[FreeBufferSpaceTask] = GraphBuilder.AddSetupTask(
		[this, RemovedList=ChangeSet.RemovedPrimitiveIds]
		{
			// Remove and free material slot data for removed primitives
			// NOTE: Using the ID list instead of the primitive list since we're in an async task
			for (const auto& PersistentIndex : RemovedList)
			{
				if (SceneData->PrimitiveData.IsValidIndex(PersistentIndex.Index))
				{
					FMaterialsSceneExtension::FPrimitiveData& Data = SceneData->PrimitiveData[PersistentIndex.Index];
					if (Data.MaterialBufferOffset != INDEX_NONE)
					{
						SceneData->MaterialBufferAllocator.Free(Data.MaterialBufferOffset, Data.MaterialBufferSizeDwords);
					}
				#if WITH_EDITOR
					if (Data.HitProxyBufferOffset != INDEX_NONE)
					{
						SceneData->HitProxyIDAllocator.Free(Data.HitProxyBufferOffset, Data.NumMaterials);
					}
				#endif
		
					SceneData->PrimitiveData.RemoveAt(PersistentIndex.Index);
				}
			}

			// Check to force a full upload by CVar
			// NOTE: Doesn't currently discern which scene to affect
			bForceFullUpload = GNaniteMaterialBufferForceFullUpload != 0;
			if (GNaniteMaterialBufferForceFullUpload == 1)
			{
				GNaniteMaterialBufferForceFullUpload = 0;
			}

			bDefragging = SceneData->ProcessBufferDefragmentation();
			bForceFullUpload |= bDefragging;
		},
		UE::Tasks::ETaskPriority::Normal,
		bEnableAsync
	);
}

void FMaterialsSceneExtension::FUpdater::PostSceneUpdate(FRDGBuilder& GraphBuilder, const FScenePostUpdateChangeSet& ChangeSet)
{
	if (!SceneData->IsEnabled())
	{
		return;
	}

	// Cache the updated PrimitiveSceneInfos (this is safe as long as we only access it in updater funcs and RDG setup tasks)
	AddedList = ChangeSet.AddedPrimitiveSceneInfos;

	// Kick off a task to initialize added slots
	if (AddedList.Num() > 0)
	{
		SceneData->TaskHandles[InitPrimitiveDataTask] = GraphBuilder.AddSetupTask(
			[this]
			{
				for (auto PrimitiveSceneInfo : AddedList)
				{
					if (!PrimitiveSceneInfo->Proxy->IsNaniteMesh())
					{
						continue;
					}
					
					const int32 PersistentIndex = PrimitiveSceneInfo->GetPersistentIndex().Index;
					auto* NaniteProxy = static_cast<Nanite::FSceneProxyBase*>(PrimitiveSceneInfo->Proxy);
					
					FPrimitiveData NewData;
					NewData.PrimitiveSceneInfo = PrimitiveSceneInfo;
					NewData.NumMaterials = NaniteProxy->GetMaterialMaxIndex() + 1;
					NewData.bHasUVDensities = NaniteProxy->HasDynamicDisplacement();
					SceneData->PrimitiveData.EmplaceAt(PersistentIndex, NewData);
	
					if (!bForceFullUpload)
					{
						DirtyPrimitiveList.Add(PersistentIndex);
					}
				}
			},
			MakeArrayView({ SceneData->TaskHandles[FreeBufferSpaceTask] }),
			UE::Tasks::ETaskPriority::Normal,
			bEnableAsync
		);
	}

#if WITH_EDITOR
	auto AllocateAndStoreHitProxies = [this](FPrimitiveData& Data)
	{
		check(bForceFullUpload || Data.HitProxyBufferOffset == INDEX_NONE); // Sanity check

		auto NaniteProxy = static_cast<Nanite::FSceneProxyBase*>(Data.PrimitiveSceneInfo->Proxy);	
		auto& MaterialSections = NaniteProxy->GetMaterialSections();	
	
		// Check to allocate space in the hit proxy ID buffer
		const bool bNeedsMaterialHitProxies = Data.NumMaterials > 0 &&
			NaniteProxy->GetHitProxyMode() == Nanite::FSceneProxyBase::EHitProxyMode::MaterialSection;
	
		if (bNeedsMaterialHitProxies)
		{
			if (Data.HitProxyBufferOffset == INDEX_NONE)
			{
				Data.HitProxyBufferOffset = SceneData->HitProxyIDAllocator.Allocate(Data.NumMaterials);
			}
			SceneData->HitProxyIDs.SetNumUninitialized(SceneData->HitProxyIDAllocator.GetMaxSize());
	
			uint32 SectionIndex = 0;
			for (auto&& HitProxyID : NaniteProxy->GetHitProxyIds())
			{
				const int32 MaterialIndex = MaterialSections[SectionIndex].MaterialIndex;
				check(MaterialIndex < Data.NumMaterials);

				const uint32 Packed = HitProxyID.GetColor().ToPackedABGR();
				SceneData->HitProxyIDs[Data.HitProxyBufferOffset + MaterialIndex] = Packed;
				++SectionIndex;
			}
		}
	};

	// Launch a task to add the new primitives' material hit proxies to the buffer
	SceneData->TaskHandles[UpdateHitProxyIDsTask] = GraphBuilder.AddSetupTask(
		[this, AllocateAndStoreHitProxies]
		{
			if (bForceFullUpload)
			{
				for (auto& Data : SceneData->PrimitiveData)
				{
					AllocateAndStoreHitProxies(Data);
				}
			}
			else
			{
				for (auto PrimitiveSceneInfo : AddedList)
				{
					const int32 PersistentIndex = PrimitiveSceneInfo->GetPersistentIndex().Index;
					if (SceneData->PrimitiveData.IsValidIndex(PersistentIndex))
					{
						AllocateAndStoreHitProxies(SceneData->PrimitiveData[PersistentIndex]);
					}
				}
			}
		},
		MakeArrayView(
			{
				SceneData->TaskHandles[FreeBufferSpaceTask],
				SceneData->TaskHandles[InitPrimitiveDataTask]
			}
		),
		UE::Tasks::ETaskPriority::Normal,
		bEnableAsync
	);
#endif
}

void FMaterialsSceneExtension::FUpdater::PostCacheNaniteMaterialBins(
	FRDGBuilder& GraphBuilder,
	const TConstArrayView<FPrimitiveSceneInfo*>& SceneInfosWithStaticDrawListUpdate)
{
	if (!SceneData->IsEnabled())
	{
		return;
	}

	// Again, caching because we can assume the lifetime of this list lives as long as the graph builder
	MaterialUpdateList = SceneInfosWithStaticDrawListUpdate;

	// Gets the information needed from the primitive for material slot data and allocates the appropriate space in the buffer
	// for the primitive's material info
	auto AllocSpaceForPrimitive = [this](FPrimitiveData& Data)
	{
		auto* NaniteProxy = static_cast<Nanite::FSceneProxyBase*>(Data.PrimitiveSceneInfo->Proxy);

		// Update the mesh pass count/mask here, now that material bins have been cached
		Data.NumMeshPasses = 0;
		Data.MeshPassMask = 0;

		uint32 MeshPassBit = 1;
		for (const auto& PassMaterialSlots : Data.PrimitiveSceneInfo->NaniteMaterialSlots)
		{
			if (PassMaterialSlots.Num() > 0)
			{
				++Data.NumMeshPasses;
				Data.MeshPassMask |= MeshPassBit;
			}
			MeshPassBit <<= 1;
		}

		const uint32 MaterialSlotSizeDwords = sizeof(FNaniteMaterialSlot::FPacked) / 4u;
		const uint32 UVDensitySize = Data.bHasUVDensities ? 4u : 0u;
		const uint32 NeededSize = Data.NumMaterials * (Data.NumMeshPasses * MaterialSlotSizeDwords + UVDensitySize);
		if (NeededSize != Data.MaterialBufferSizeDwords)
		{
			if (Data.MaterialBufferSizeDwords > 0)
			{
				SceneData->MaterialBufferAllocator.Free(Data.MaterialBufferOffset, Data.MaterialBufferSizeDwords);
			}
			Data.MaterialBufferOffset = NeededSize > 0 ? SceneData->MaterialBufferAllocator.Allocate(NeededSize) : INDEX_NONE;
			Data.MaterialBufferSizeDwords = NeededSize;

			if (!bForceFullUpload)
			{
				DirtyPrimitiveList.Add(Data.PrimitiveSceneInfo->GetPersistentIndex().Index);
			}
		}
	};

	// Kick off the allocate task (synced just prior to primitive uploads)
	SceneData->TaskHandles[AllocMaterialBufferTask] = GraphBuilder.AddSetupTask(
		[this, AllocSpaceForPrimitive]
		{
			if (bDefragging)
			{
				for (auto& Data : SceneData->PrimitiveData)
				{
					AllocSpaceForPrimitive(Data);
				}
			}
			else
			{
				// Only check to reallocate space for primitives that have re-cached their material bins
				for (auto PrimitiveSceneInfo : MaterialUpdateList)
				{
					const int32 Index = PrimitiveSceneInfo->GetPersistentIndex().Index;
					if (SceneData->PrimitiveData.IsValidIndex(Index))
					{
						AllocSpaceForPrimitive(SceneData->PrimitiveData[Index]);
					}
				}
			}

			// Only create a new uploader here if one of the two dependent upload tasks will use it
			if (bForceFullUpload || DirtyPrimitiveList.Num() > 0 || MaterialUpdateList.Num() > 0)
			{
				SceneData->MaterialUploader = MakeUnique<FUploader>();
			}
		},
		MakeArrayView(
			{
				SceneData->TaskHandles[FreeBufferSpaceTask],
				SceneData->TaskHandles[InitPrimitiveDataTask],
				SceneData->Scene->GetCacheNaniteMaterialBinsTask()
			}
		),
		UE::Tasks::ETaskPriority::Normal,
		bEnableAsync
	);

	auto UploadPrimitiveData = [this](const FPrimitiveData& Data)
	{
		static const uint32 PrimitiveSizeDwords = sizeof(FPackedPrimitiveData) / 4;
		const int32 PersistentIndex = Data.PrimitiveSceneInfo->GetPersistentIndex().Index;

		// Catch when/if no material buffer data is allocated for a primitive we're tracking.
		// This should be indicative of a bug.
		ensure(Data.MaterialBufferSizeDwords != INDEX_NONE);

		check(SceneData->MaterialUploader.IsValid()); // Sanity check
		SceneData->MaterialUploader->PrimitiveDataUploader.Add(Data.Pack(), PersistentIndex);
	};

	// Kick off the primitive data upload task (synced when accessing the buffer)
	SceneData->TaskHandles[UploadPrimitiveDataTask] = GraphBuilder.AddSetupTask(
		[this, UploadPrimitiveData]
		{
			if (bForceFullUpload)
			{
				for (auto& Data : SceneData->PrimitiveData)
				{
					UploadPrimitiveData(Data);
				}
			}
			else
			{
				// Sort the array so we can skip duplicate entries
				DirtyPrimitiveList.Sort();
				int32 LastPersistentIndex = INDEX_NONE;
				for (auto PersistentIndex : DirtyPrimitiveList)
				{
					if (PersistentIndex != LastPersistentIndex &&
						SceneData->PrimitiveData.IsValidIndex(PersistentIndex))
					{
						UploadPrimitiveData(SceneData->PrimitiveData[PersistentIndex]);
					}
					LastPersistentIndex = PersistentIndex;
				}
			}
		},
		MakeArrayView(
			{
			#if WITH_EDITOR
				SceneData->TaskHandles[UpdateHitProxyIDsTask],
			#endif
				SceneData->TaskHandles[AllocMaterialBufferTask],
			}
		),
		UE::Tasks::ETaskPriority::Normal,
		bEnableAsync
	);

	auto UploadMaterialData = [this](const FPrimitiveData& Data)
	{
		auto NaniteProxy = static_cast<const Nanite::FSceneProxyBase*>(Data.PrimitiveSceneInfo->Proxy);	
		auto& MaterialSections = NaniteProxy->GetMaterialSections();
		const int32 NumMaterials = NaniteProxy->GetMaterialMaxIndex() + 1;

		// Sanity checks
		check(SceneData->MaterialUploader.IsValid());
		check(Data.MaterialBufferOffset % FUploader::MaterialScatterStride == 0);
		check(Data.MaterialBufferSizeDwords % FUploader::MaterialScatterStride == 0);

		auto UploadData = SceneData->MaterialUploader->MaterialDataUploader.AddMultiple_GetRef(
			Data.MaterialBufferOffset / FUploader::MaterialScatterStride,
			Data.MaterialBufferSizeDwords
		);
		uint32* OutputBase = UploadData.GetData();
	
		// Fill the material slots
		auto* MaterialSlotsOut = reinterpret_cast<FNaniteMaterialSlot::FPacked*>(OutputBase);
		uint32 MeshPassBit = 1;
		for (auto& PassMaterialSlots : Data.PrimitiveSceneInfo->NaniteMaterialSlots)
		{
			if (Data.MeshPassMask & MeshPassBit)
			{
				uint32 SectionIndex = 0;
				for (auto& MaterialSlot : PassMaterialSlots)
				{
					MaterialSlotsOut[MaterialSections[SectionIndex++].MaterialIndex] = MaterialSlot.Pack();
				}
				MaterialSlotsOut += Data.NumMaterials;
			}
			MeshPassBit <<= 1;
		}
	
		if (Data.bHasUVDensities)
		{
			// Fill in the UV densities of the materials to be used by the domain shader for derivatives
			auto* UVDensitiesOut = reinterpret_cast<FVector4f*>(MaterialSlotsOut);
			for (auto& MaterialSection : MaterialSections)
			{
				FMemory::Memcpy(&UVDensitiesOut[MaterialSection.MaterialIndex], &MaterialSection.LocalUVDensities, sizeof(MaterialSection.LocalUVDensities));
			}
		}
	};

	// Kick off the material data upload task (synced when accessing the buffer)
	SceneData->TaskHandles[UploadMaterialDataTask] = GraphBuilder.AddSetupTask(
		[this, UploadMaterialData]
		{
			if (bForceFullUpload)
			{
				for (auto& Data : SceneData->PrimitiveData)
				{
					UploadMaterialData(Data);
				}
			}
			else
			{
				for (auto PrimitiveSceneInfo : MaterialUpdateList)
				{
					if (PrimitiveSceneInfo->Proxy->IsNaniteMesh())
					{
						const int32 PersistentIndex = PrimitiveSceneInfo->GetPersistentIndex().Index;
						UploadMaterialData(SceneData->PrimitiveData[PersistentIndex]);
					}
				}
			}
		},
		MakeArrayView({ SceneData->TaskHandles[AllocMaterialBufferTask] }),
		UE::Tasks::ETaskPriority::Normal,
		bEnableAsync
	);

	if (!bEnableAsync)
	{
		// If disabling async, just finish the upload immediately
		SceneData->FinishMaterialBufferUpload(GraphBuilder);
	}
}


void FMaterialsSceneExtension::FRenderer::UpdateSceneUniformBuffer(
	FRDGBuilder& GraphBuilder,
	FSceneUniformBuffer& SceneUniformBuffer)
{
	check(SceneData->IsEnabled());

	FNaniteMaterialsParameters Parameters;
	Parameters.PrimitiveMaterialElementStride = sizeof(FPackedPrimitiveData);
	SceneData->FinishMaterialBufferUpload(GraphBuilder, &Parameters);
	SceneUniformBuffer.Set(SceneUB::NaniteMaterials, Parameters);
}

} // namespace Nanite

IMPLEMENT_SCENE_UB_STRUCT(FNaniteMaterialsParameters, NaniteMaterials, Nanite::GetDefaultMaterialsParameters);
