// Copyright Epic Games, Inc. All Rights Reserved.

#include "LumenMeshCards.h"
#include "RendererPrivate.h"
#include "Lumen.h"

extern void BuildMeshCardsDataForMergedInstances(const FLumenPrimitiveGroup& PrimitiveGroup, FMeshCardsBuildData& MeshCardsBuildData, FMatrix& MeshCardsLocalToWorld);

int32 GLumenSceneUploadEveryFrame = 0;
FAutoConsoleVariableRef CVarLumenSceneUploadEveryFrame(
	TEXT("r.LumenScene.UploadEveryFrame"),
	GLumenSceneUploadEveryFrame,
	TEXT("Whether to upload the entire Lumen Scene's data every frame. Useful for debugging."),
	ECVF_RenderThreadSafe
);

TAutoConsoleVariable<int32> CVarLumenSceneUpdateViewOrigin(
	TEXT("r.LumenScene.UpdateViewOrigin"),
	1,
	TEXT("Whether to update view origin for voxel lighting and global distance field. Useful for debugging."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarLumenThreadGroupSize32(
	TEXT("r.Lumen.ThreadGroupSize32"),
	1,
	TEXT("Whether to to prefer dispatches in groups of 32 threads on HW which supports it (instead of standard 64)."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

bool Lumen::ShouldUpdateLumenSceneViewOrigin()
{
	return CVarLumenSceneUpdateViewOrigin.GetValueOnRenderThread() != 0;
}

FVector Lumen::GetLumenSceneViewOrigin(const FViewInfo& View, int32 ClipmapIndex)
{
	FVector CameraOrigin = View.ViewMatrices.GetViewOrigin();

	if (View.ViewState)
	{
		FVector CameraVelocityOffset = View.ViewState->GlobalDistanceFieldData->CameraVelocityOffset;

		if (ClipmapIndex > 0)
		{
			const float ClipmapExtent = Lumen::GetGlobalDFClipmapExtent(ClipmapIndex);
			const float MaxCameraDriftFraction = .75f;
			CameraVelocityOffset.X = FMath::Clamp<float>(CameraVelocityOffset.X, -ClipmapExtent * MaxCameraDriftFraction, ClipmapExtent * MaxCameraDriftFraction);
			CameraVelocityOffset.Y = FMath::Clamp<float>(CameraVelocityOffset.Y, -ClipmapExtent * MaxCameraDriftFraction, ClipmapExtent * MaxCameraDriftFraction);
			CameraVelocityOffset.Z = FMath::Clamp<float>(CameraVelocityOffset.Z, -ClipmapExtent * MaxCameraDriftFraction, ClipmapExtent * MaxCameraDriftFraction);
		}

		CameraOrigin += CameraVelocityOffset;
	}

	// Frozen camera
	if (View.ViewState)
	{
		if (Lumen::ShouldUpdateLumenSceneViewOrigin())
		{
			View.ViewState->GlobalDistanceFieldData->bUpdateViewOrigin = true;
		}
		else
		{
			if (View.ViewState->GlobalDistanceFieldData->bUpdateViewOrigin)
			{
				View.ViewState->GlobalDistanceFieldData->LastViewOrigin = View.ViewMatrices.GetViewOrigin();
				View.ViewState->GlobalDistanceFieldData->bUpdateViewOrigin = false;
			}
		}

		if (!View.ViewState->GlobalDistanceFieldData->bUpdateViewOrigin)
		{
			CameraOrigin = View.ViewState->GlobalDistanceFieldData->LastViewOrigin;
		}
	}

	return CameraOrigin;
}

bool Lumen::UseThreadGroupSize32()
{
	return GRHISupportsWaveOperations && GRHIMinimumWaveSize <= 32 && CVarLumenThreadGroupSize32.GetValueOnRenderThread() != 0;
}

class FLumenCardPageGPUData
{
public:
	// Must match usf
	enum { DataStrideInFloat4s = 5 };
	enum { DataStrideInBytes = DataStrideInFloat4s * sizeof(FVector4f) };

	static void FillData(const FLumenPageTableEntry& RESTRICT PageTableEntry, uint32 ResLevelPageTableOffset, FIntPoint ResLevelSizeInTiles, FVector2D InvPhysicalAtlasSize, FVector4f* RESTRICT OutData)
	{
		// Layout must match GetLumenCardPageData in usf
		const float SizeInTexelsX = PageTableEntry.PhysicalAtlasRect.Max.X - PageTableEntry.PhysicalAtlasRect.Min.X;
		const float SizeInTexelsY = PageTableEntry.PhysicalAtlasRect.Max.Y - PageTableEntry.PhysicalAtlasRect.Min.Y;

		OutData[0].X = *(float*)&PageTableEntry.CardIndex;
		OutData[0].Y = *(float*)&ResLevelPageTableOffset;
		OutData[0].Z = SizeInTexelsX;
		OutData[0].W = SizeInTexelsY;

		OutData[1] = PageTableEntry.CardUVRect;

		OutData[2].X = PageTableEntry.PhysicalAtlasRect.Min.X * InvPhysicalAtlasSize.X;
		OutData[2].Y = PageTableEntry.PhysicalAtlasRect.Min.Y * InvPhysicalAtlasSize.Y;
		OutData[2].Z = PageTableEntry.PhysicalAtlasRect.Max.X * InvPhysicalAtlasSize.X;
		OutData[2].W = PageTableEntry.PhysicalAtlasRect.Max.Y * InvPhysicalAtlasSize.Y;

		OutData[3].X = SizeInTexelsX > 0.0f ? ((PageTableEntry.CardUVRect.Z - PageTableEntry.CardUVRect.X) / SizeInTexelsX) : 0.0f;
		OutData[3].Y = SizeInTexelsY > 0.0f ? ((PageTableEntry.CardUVRect.W - PageTableEntry.CardUVRect.Y) / SizeInTexelsY) : 0.0f;
		OutData[3].Z = *(float*)&ResLevelSizeInTiles.X;
		OutData[3].W = *(float*)&ResLevelSizeInTiles.Y;

		const uint32 LastUpdateFrame = 0;
		OutData[4].X = *(float*)&LastUpdateFrame;
		OutData[4].Y = *(float*)&LastUpdateFrame;
		OutData[4].Z = *(float*)&LastUpdateFrame;
		OutData[4].W = 0.0f;

		static_assert(DataStrideInFloat4s == 5, "Data stride doesn't match");
	}
};

static FIntPoint GetDesiredPhysicalAtlasSizeInPages(float SurfaceCacheResolution)
{
	extern int32 GLumenSceneSurfaceCacheAtlasSize;
	int32 AtlasSizeInPages = FMath::DivideAndRoundUp<uint32>(GLumenSceneSurfaceCacheAtlasSize, Lumen::PhysicalPageSize);
	AtlasSizeInPages = AtlasSizeInPages * SurfaceCacheResolution;
	AtlasSizeInPages = FMath::Clamp(AtlasSizeInPages, 1, 64);
	return FIntPoint(AtlasSizeInPages, AtlasSizeInPages);
}

static FIntPoint GetDesiredPhysicalAtlasSize(float SurfaceCacheResolution)
{
	return GetDesiredPhysicalAtlasSizeInPages(SurfaceCacheResolution) * Lumen::PhysicalPageSize;
}

bool FLumenPrimitiveGroup::HasMergedInstances() const
{
	bool HasInstancesToMerge = false;

	if (PrimitiveInstanceIndex < 0)
	{
		// Check if there is more than 1 instance for merging

		uint32 NumInstances = 0;
		for (const FPrimitiveSceneInfo* PrimitiveSceneInfo : Primitives)
		{
			const TConstArrayView<FPrimitiveInstance> InstanceSceneData = PrimitiveSceneInfo->Proxy->GetInstanceSceneData();
			NumInstances += FMath::Max(InstanceSceneData.Num(), 1);

			if (NumInstances > 1)
			{
				HasInstancesToMerge = true;
				break;
			}
		}
	}

	return HasInstancesToMerge;
}

FLumenSurfaceCacheAllocator::FPageBin::FPageBin(FIntPoint InElementSize)
{
	ensure(InElementSize.GetMax() <= Lumen::PhysicalPageSize);
	ElementSize = InElementSize;
	PageSizeInElements = FIntPoint(Lumen::PhysicalPageSize) / InElementSize;
}

void FLumenSurfaceCacheAllocator::Init(FIntPoint PageAtlasSizeInPages)
{
	PhysicalPageFreeList.SetNum(PageAtlasSizeInPages.X * PageAtlasSizeInPages.Y);
	for (int32 CoordY = 0; CoordY < PageAtlasSizeInPages.Y; ++CoordY)
	{
		for (int32 CoordX = 0; CoordX < PageAtlasSizeInPages.X; ++CoordX)
		{
			const int32 PageFreeListIndex = PageAtlasSizeInPages.X * PageAtlasSizeInPages.Y - 1 - (CoordX + PageAtlasSizeInPages.X * CoordY);
			PhysicalPageFreeList[PageFreeListIndex].X = CoordX;
			PhysicalPageFreeList[PageFreeListIndex].Y = CoordY;
		}
	}
}

FIntPoint FLumenSurfaceCacheAllocator::AllocatePhysicalAtlasPage()
{
	FIntPoint NewPageCoord = FIntPoint(-1, -1);

	if (PhysicalPageFreeList.Num() > 0)
	{
		NewPageCoord = PhysicalPageFreeList.Last();
		PhysicalPageFreeList.Pop();
	}

	return NewPageCoord;
}

void FLumenSurfaceCacheAllocator::FreePhysicalAtlasPage(FIntPoint PageCoord)
{
	if (PageCoord.X >= 0 && PageCoord.Y >= 0)
	{
		PhysicalPageFreeList.Add(PageCoord);
	}
}

void FLumenSurfaceCacheAllocator::Allocate(const FLumenPageTableEntry& Page, FAllocation& Allocation)
{
	if (Page.IsSubAllocation())
	{
		FPageBin* MatchingBin = nullptr;

		for (FPageBin& Bin : PageBins)
		{
			if (Bin.ElementSize == Page.SubAllocationSize)
			{
				MatchingBin = &Bin;
				break;
			}
		}

		if (!MatchingBin)
		{
			PageBins.Add(FPageBin(Page.SubAllocationSize));
			MatchingBin = &PageBins.Last();
		}

		FPageBinAllocation* MatchingBinAllocation = nullptr;

		for (FPageBinAllocation& BinAllocation : MatchingBin->BinAllocations)
		{
			if (BinAllocation.FreeList.Num() > 0)
			{
				MatchingBinAllocation = &BinAllocation;
				break;
			}
		}

		if (!MatchingBinAllocation)
		{
			const FIntPoint PageCoord = AllocatePhysicalAtlasPage();

			if (PageCoord.X >= 0 && PageCoord.Y >= 0)
			{
				MatchingBin->BinAllocations.AddDefaulted(1);

				FPageBinAllocation& NewBinAllocation = MatchingBin->BinAllocations.Last();
				NewBinAllocation.PageCoord = PageCoord;

				NewBinAllocation.FreeList.SetNum(MatchingBin->PageSizeInElements.X * MatchingBin->PageSizeInElements.Y);
				for (int32 ElementsY = 0; ElementsY < MatchingBin->PageSizeInElements.Y; ++ElementsY)
				{
					for (int32 ElementsX = 0; ElementsX < MatchingBin->PageSizeInElements.X; ++ElementsX)
					{
						NewBinAllocation.FreeList[ElementsX + ElementsY * MatchingBin->PageSizeInElements.X] = FIntPoint(ElementsX, ElementsY);
					}
				}

				MatchingBinAllocation = &NewBinAllocation;
			}
		}

		if (MatchingBinAllocation)
		{
			const FIntPoint ElementCoord = MatchingBinAllocation->FreeList.Last();
			MatchingBinAllocation->FreeList.Pop();

			const FIntPoint ElementOffset = MatchingBinAllocation->PageCoord * Lumen::PhysicalPageSize + ElementCoord * MatchingBin->ElementSize;

			Allocation.PhysicalPageCoord = MatchingBinAllocation->PageCoord;
			Allocation.PhysicalAtlasRect.Min = ElementOffset;
			Allocation.PhysicalAtlasRect.Max = ElementOffset + MatchingBin->ElementSize;
		}
	}
	else
	{
		Allocation.PhysicalPageCoord = AllocatePhysicalAtlasPage();
		Allocation.PhysicalAtlasRect.Min = (Allocation.PhysicalPageCoord + 0) * Lumen::PhysicalPageSize;
		Allocation.PhysicalAtlasRect.Max = (Allocation.PhysicalPageCoord + 1) * Lumen::PhysicalPageSize;
	}
}

void FLumenSurfaceCacheAllocator::Free(const FLumenPageTableEntry& Page)
{
	if (Page.IsSubAllocation())
	{
		FPageBin* MatchingBin = nullptr;
		for (FPageBin& Bin : PageBins)
		{
			if (Bin.ElementSize == Page.SubAllocationSize)
			{
				MatchingBin = &Bin;
				break;
			}
		}

		check(MatchingBin);
		bool bRemoved = false;

		for (int32 AllocationIndex = 0; AllocationIndex < MatchingBin->BinAllocations.Num(); AllocationIndex++)
		{
			FPageBinAllocation& BinAllocation = MatchingBin->BinAllocations[AllocationIndex];

			const FIntPoint ElementCoord = (Page.PhysicalAtlasRect.Min - BinAllocation.PageCoord * Lumen::PhysicalPageSize) / MatchingBin->ElementSize;

			if (ElementCoord.X >= 0
				&& ElementCoord.Y >= 0
				&& ElementCoord.X < MatchingBin->PageSizeInElements.X
				&& ElementCoord.Y < MatchingBin->PageSizeInElements.Y)
			{
				BinAllocation.FreeList.Add(ElementCoord);

				if (BinAllocation.FreeList.Num() == MatchingBin->GetNumElements())
				{
					FreePhysicalAtlasPage(BinAllocation.PageCoord);
					MatchingBin->BinAllocations.RemoveAt(AllocationIndex);
				}

				bRemoved = true;
				break;
			}
		}

		check(bRemoved);
	}
	else
	{
		FreePhysicalAtlasPage(Page.PhysicalPageCoord);
	}
}

/**
 * Checks if there's enough free memory in the surface cache to allocate entire mip map level of a card (or a single page)
 */
bool FLumenSurfaceCacheAllocator::IsSpaceAvailable(const FLumenCard& Card, int32 ResLevel, bool bSinglePage) const
{
	FLumenMipMapDesc MipMapDesc;

	Card.GetMipMapDesc(ResLevel, MipMapDesc);

	const int32 ReqSizeInPages = bSinglePage ? 1 : (MipMapDesc.SizeInPages.X * MipMapDesc.SizeInPages.Y);

	if (PhysicalPageFreeList.Num() >= ReqSizeInPages)
	{
		return true;
	}

	// No free pages, but maybe there's some space in one of the existing bins
	if (MipMapDesc.bSubAllocation)
	{
		const FPageBin* MatchingBin = nullptr;

		for (const FPageBin& Bin : PageBins)
		{
			if (Bin.ElementSize == MipMapDesc.Resolution)
			{
				for (const FPageBinAllocation& BinAllocation : Bin.BinAllocations)
				{
					if (BinAllocation.FreeList.Num() > 0)
					{
						return true;
					}
				}

				break;
			}
		}
	}

	return false;
}

void FLumenSurfaceCacheAllocator::GetStats(FStats& Stats) const
{
	Stats.NumFreePages = PhysicalPageFreeList.Num();

	for (const FPageBin& Bin : PageBins)
	{
		uint32 NumFreeElements = 0;

		for (const FPageBinAllocation& BinAllocation : Bin.BinAllocations)
		{
			NumFreeElements += BinAllocation.FreeList.Num();
		}

		const uint32 NumElementsPerPage = Bin.PageSizeInElements.X * Bin.PageSizeInElements.Y;
		const uint32 NumElements = Bin.BinAllocations.Num() * NumElementsPerPage - NumFreeElements;

		Stats.BinNumPages += Bin.BinAllocations.Num();
		Stats.BinNumWastedPages += Bin.BinAllocations.Num() - FMath::DivideAndRoundUp(NumElements, NumElementsPerPage);
		Stats.BinPageFreeTexels += NumFreeElements * Bin.ElementSize.X * Bin.ElementSize.Y;

		if (NumElements > 0)
		{
			FBinStats& NewBinStats = Stats.Bins.AddDefaulted_GetRef();
			NewBinStats.ElementSize = Bin.ElementSize;
			NewBinStats.NumAllocations = NumElements;
			NewBinStats.NumPages = Bin.BinAllocations.Num();
		}
	}

	struct FSortBySize
	{
		FORCEINLINE bool operator()(const FBinStats& A, const FBinStats& B) const
		{
			const int32 AreaA = A.ElementSize.X * A.ElementSize.Y;
			const int32 AreaB = B.ElementSize.X * B.ElementSize.Y;

			if (AreaA == AreaB)
			{
				if (A.ElementSize.X == B.ElementSize.X)
				{
					return A.ElementSize.Y < B.ElementSize.Y;
				}
				else
				{
					return A.ElementSize.X < B.ElementSize.X;
				}
			}

			return AreaA < AreaB;
		}
	};

	Stats.Bins.Sort(FSortBySize());
}

void FLumenSceneData::UploadPageTable(FRDGBuilder& GraphBuilder, FLumenSceneFrameTemporaries& FrameTemporaries)
{
	RDG_EVENT_SCOPE(GraphBuilder, "LumenUploadPageTable");
	RDG_GPU_MASK_SCOPE(GraphBuilder, FRHIGPUMask::All());

	if (GLumenSceneUploadEveryFrame != 0)
	{
		PageTableIndicesToUpdateInBuffer.SetNum(PageTable.Num());

		for (int32 PageIndex = 0; PageIndex < PageTable.Num(); ++PageIndex)
		{
			PageTableIndicesToUpdateInBuffer[PageIndex] = PageIndex;
		}
	}

	const uint32 NumElements = FMath::Max(1024u, FMath::RoundUpToPowerOfTwo(PageTable.Num()));
	const int32 NumElementsToUpload = PageTableIndicesToUpdateInBuffer.Num();

	// PageTableBuffer
	{
		const int32 NumBytesPerElement = 2 * sizeof(uint32);
		FRDGBuffer* PageTableBufferRDG = ResizeByteAddressBufferIfNeeded(GraphBuilder, PageTableBuffer, NumElements * NumBytesPerElement, TEXT("Lumen.PageTable"));
		FrameTemporaries.PageTableBufferSRV = GraphBuilder.CreateSRV(PageTableBufferRDG);

		if (NumElementsToUpload > 0)
		{
			PageTableUploadBuffer.Init(GraphBuilder, NumElementsToUpload, NumBytesPerElement, false, TEXT("Lumen.PageTableUpload"));

			for (int32 PageIndex : PageTableIndicesToUpdateInBuffer)
			{
				if (PageIndex < PageTable.Num())
				{
					uint32 PackedData[2] = { 0, 0 };

					if (PageTable.IsAllocated(PageIndex))
					{
						const FLumenPageTableEntry& Page = PageTable[PageIndex];

						PackedData[0] |= ((Page.SampleAtlasBiasX & 0xFFF) << 0);
						PackedData[0] |= ((Page.SampleAtlasBiasY & 0xFFF) << 12);
						PackedData[0] |= ((Page.SampleCardResLevelX & 0xF) << 24);
						PackedData[0] |= ((Page.SampleCardResLevelY & 0xF) << 28);

						PackedData[1] = Page.SamplePageIndex;
					}

					PageTableUploadBuffer.Add(PageIndex, PackedData);
				}
			}

			PageTableUploadBuffer.ResourceUploadTo(GraphBuilder, PageTableBufferRDG);
		}
	}

	// CardPageBuffer
	{
		const FVector2D InvPhysicalAtlasSize = FVector2D(1.0f) / GetPhysicalAtlasSize();

		TRefCountPtr<FRDGPooledBuffer> CardPageBufferOld = CardPageBuffer;

		const int32 NumBytesPerElement = FLumenCardPageGPUData::DataStrideInFloat4s * sizeof(FVector4f);
		FRDGBuffer* CardPageBufferRDG = ResizeStructuredBufferIfNeeded(GraphBuilder, CardPageBuffer, NumElements * NumBytesPerElement, TEXT("Lumen.PageBuffer"));
		FrameTemporaries.CardPageBufferSRV = GraphBuilder.CreateSRV(CardPageBufferRDG);
		FrameTemporaries.CardPageBufferUAV = GraphBuilder.CreateUAV(CardPageBufferRDG);

		if (NumElementsToUpload > 0)
		{
			FLumenPageTableEntry NullPageTableEntry;

			CardPageUploadBuffer.Init(GraphBuilder, NumElementsToUpload, FLumenCardPageGPUData::DataStrideInBytes, true, TEXT("Lumen.CardPageUploadBuffer"));

			for (int32 PageIndex : PageTableIndicesToUpdateInBuffer)
			{
				if (PageIndex < PageTable.Num())
				{
					uint32 ResLevelPageTableOffset = 0;
					FIntPoint ResLevelSizeInTiles = FIntPoint(0, 0);

					FVector4f* Data = (FVector4f*)CardPageUploadBuffer.Add_GetRef(PageIndex);

					if (PageTable.IsAllocated(PageIndex) && PageTable[PageIndex].IsMapped())
					{
						const FLumenPageTableEntry& PageTableEntry = PageTable[PageIndex];
						const FLumenCard& Card = Cards[PageTableEntry.CardIndex];
						const FLumenSurfaceMipMap& MipMap = Card.GetMipMap(PageTableEntry.ResLevel);

						ResLevelPageTableOffset = MipMap.PageTableSpanOffset;
						ResLevelSizeInTiles = MipMap.GetSizeInPages() * (Lumen::PhysicalPageSize / Lumen::CardTileSize);

						if (PageTableEntry.IsSubAllocation())
						{
							ResLevelSizeInTiles = PageTableEntry.SubAllocationSize / Lumen::CardTileSize;
						}

						FLumenCardPageGPUData::FillData(PageTableEntry, ResLevelPageTableOffset, ResLevelSizeInTiles, InvPhysicalAtlasSize, Data);
					}
					else
					{
						FLumenCardPageGPUData::FillData(NullPageTableEntry, ResLevelPageTableOffset, ResLevelSizeInTiles, InvPhysicalAtlasSize, Data);
					}
				}
			}

			CardPageUploadBuffer.ResourceUploadTo(GraphBuilder, CardPageBufferRDG);
		}

		// Resize also the CardPageLastUsedBuffers
		if (CardPageBufferOld != CardPageBuffer)
		{
			FRDGBufferRef CardPageLastUsedBufferRDG = GraphBuilder.CreateBuffer(
				FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), NumElements), TEXT("Lumen.CardPageLastUsedBuffer"));

			FRDGBufferRef CardPageHighResLastUsedBufferRDG = GraphBuilder.CreateBuffer(
				FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), NumElements), TEXT("Lumen.CardPageHighResLastUsedBuffer"));

			AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(CardPageLastUsedBufferRDG), 0);
			AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(CardPageHighResLastUsedBufferRDG), 0);

			CardPageLastUsedBuffer = GraphBuilder.ConvertToExternalBuffer(CardPageLastUsedBufferRDG);
			CardPageHighResLastUsedBuffer = GraphBuilder.ConvertToExternalBuffer(CardPageHighResLastUsedBufferRDG);
		}
	}

	// Reset arrays, but keep allocated memory for 1024 elements
	PageTableIndicesToUpdateInBuffer.Empty(1024);
}

FLumenSceneData::FLumenSceneData(EShaderPlatform ShaderPlatform, EWorldType::Type WorldType) :
	bFinalLightingAtlasContentsValid(false)
{
	static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.MeshCardRepresentation"));

	bTrackAllPrimitives = (DoesPlatformSupportLumenGI(ShaderPlatform)) && CVar->GetValueOnGameThread() != 0 && WorldType != EWorldType::EditorPreview;
}

FLumenSceneData::FLumenSceneData(bool bInTrackAllPrimitives) :
	bFinalLightingAtlasContentsValid(false)
{
	bTrackAllPrimitives = bInTrackAllPrimitives;
}

FLumenSceneData::~FLumenSceneData()
{
	LLM_SCOPE_BYTAG(Lumen);

	for (int32 CardIndex = 0; CardIndex < Cards.Num(); ++CardIndex)
	{
		if (Cards.IsAllocated(CardIndex))
		{
			RemoveCardFromAtlas(CardIndex);
		}
	}

	Cards.Reset();
	MeshCards.Reset();
}

bool TrackPrimitiveForLumenScene(const FPrimitiveSceneProxy* Proxy)
{
	bool bCanBeTraced = false;
	if (DoesProjectSupportDistanceFields() && Proxy->AffectsDistanceFieldLighting()
		&& (Proxy->SupportsDistanceFieldRepresentation() || Proxy->SupportsHeightfieldRepresentation()))
	{
		bCanBeTraced = true;
	}

#if RHI_RAYTRACING
	if (IsRayTracingEnabled() && Proxy->HasRayTracingRepresentation())
	{
		if ((Proxy->IsVisibleInRayTracing() && (Proxy->IsDrawnInGame() || Proxy->AffectsIndirectLightingWhileHidden())) || Proxy->IsRayTracingFarField())
		{
			bCanBeTraced = true;
		}
	}
#endif

	const bool bAffectsLumen = Proxy->AffectsDynamicIndirectLighting() && Proxy->SupportsMeshCardRepresentation();
	const bool bVisible = Proxy->IsDrawnInGame() || Proxy->AffectsIndirectLightingWhileHidden();
	return bAffectsLumen && bVisible && bCanBeTraced;
}

bool TrackPrimitiveInstanceForLumenScene(const FMatrix& LocalToWorld, const FBox& LocalBoundingBox, bool bEmissiveLightSource)
{
	const FVector LocalToWorldScale = LocalToWorld.GetScaleVector();
	const FVector ScaledBoundSize = LocalBoundingBox.GetSize() * LocalToWorldScale;
	const FVector FaceSurfaceArea(ScaledBoundSize.Y * ScaledBoundSize.Z, ScaledBoundSize.X * ScaledBoundSize.Z, ScaledBoundSize.Y * ScaledBoundSize.X);
	const float LargestFaceArea = FaceSurfaceArea.GetMax();

	const float MinFaceSurfaceArea = LumenMeshCards::GetCardMinSurfaceArea(bEmissiveLightSource);
	return LargestFaceArea > MinFaceSurfaceArea;
}

// Add function is a member of FScene, because it needs to add the primitive to all FLumenSceneData at once
void FScene::LumenAddPrimitive(FPrimitiveSceneInfo* InPrimitive)
{
	LLM_SCOPE_BYTAG(Lumen);

	if (DefaultLumenSceneData->bTrackAllPrimitives)
	{
		const FPrimitiveSceneProxy* Proxy = InPrimitive->Proxy;
		bool bTrackPrimitveForLumenScene = TrackPrimitiveForLumenScene(Proxy);

		for (FLumenSceneDataIterator LumenSceneData = GetLumenSceneDataIterator(); LumenSceneData; ++LumenSceneData)
		{
			// We copy this flag over when creating per-view lumen scene data, validate that it's still the same
			check(LumenSceneData->bTrackAllPrimitives == DefaultLumenSceneData->bTrackAllPrimitives);

			LumenSceneData->PrimitivesToUpdateMeshCards.Add(InPrimitive->GetIndex());

			if (bTrackPrimitveForLumenScene)
			{
				ensure(!LumenSceneData->PendingAddOperations.Contains(InPrimitive));
				ensure(!LumenSceneData->PendingUpdateOperations.Contains(InPrimitive));
				LumenSceneData->PendingAddOperations.Add(InPrimitive);
			}
		}
	}
}

// Update function is a member of FScene, because it needs to update the primitive for all FLumenSceneData at once
void FScene::LumenUpdatePrimitive(FPrimitiveSceneInfo* InPrimitive)
{
	LLM_SCOPE_BYTAG(Lumen);

	if (DefaultLumenSceneData->bTrackAllPrimitives
		&& TrackPrimitiveForLumenScene(InPrimitive->Proxy)
		&& InPrimitive->LumenPrimitiveGroupIndices.Num() > 0)
	{
		for (FLumenSceneDataIterator LumenSceneData = GetLumenSceneDataIterator(); LumenSceneData; ++LumenSceneData)
		{
			if (!LumenSceneData->PendingUpdateOperations.Contains(InPrimitive)
				&& !LumenSceneData->PendingAddOperations.Contains(InPrimitive))
			{
				LumenSceneData->PendingUpdateOperations.Add(InPrimitive);
			}
		}
	}
}

// Update function is a member of FScene, because it needs to update the primitive for all FLumenSceneData at once
void FScene::LumenInvalidateSurfaceCacheForPrimitive(FPrimitiveSceneInfo* InPrimitive)
{
	LLM_SCOPE_BYTAG(Lumen);

	if (DefaultLumenSceneData->bTrackAllPrimitives
		&& TrackPrimitiveForLumenScene(InPrimitive->Proxy)
		&& InPrimitive->LumenPrimitiveGroupIndices.Num() > 0)
	{
		for (FLumenSceneDataIterator LumenSceneData = GetLumenSceneDataIterator(); LumenSceneData; ++LumenSceneData)
		{
			if (!LumenSceneData->PendingSurfaceCacheInvalidationOperations.Contains(InPrimitive))
			{
				LumenSceneData->PendingSurfaceCacheInvalidationOperations.Add(InPrimitive);
			}
		}
	}
}

// Remove function is a member of FScene, because it needs to remove the primitive from all FLumenSceneData at once, mainly due to
// the FLumenPrimitiveGroupRemoveInfo constructor needing access to LumenPrimitiveGroupIndices from FPrimitiveSceneInfo, before it
// gets reset.
void FScene::LumenRemovePrimitive(FPrimitiveSceneInfo* InPrimitive, int32 PrimitiveIndex)
{
	LLM_SCOPE_BYTAG(Lumen);

	const FPrimitiveSceneProxy* Proxy = InPrimitive->Proxy;

	if (DefaultLumenSceneData->bTrackAllPrimitives
		&& TrackPrimitiveForLumenScene(Proxy))
	{
		for (FLumenSceneDataIterator LumenSceneData = GetLumenSceneDataIterator(); LumenSceneData; ++LumenSceneData)
		{
			LumenSceneData->PendingAddOperations.Remove(InPrimitive);
			LumenSceneData->PendingUpdateOperations.Remove(InPrimitive);
			LumenSceneData->PendingSurfaceCacheInvalidationOperations.Remove(InPrimitive);
			LumenSceneData->PendingRemoveOperations.Add(FLumenPrimitiveGroupRemoveInfo(InPrimitive, PrimitiveIndex));
		}

		InPrimitive->LumenPrimitiveGroupIndices.Reset();
	}
}

FLumenSceneDataIterator::FLumenSceneDataIterator(const FScene* InScene)
	: NextSceneData(InScene->PerViewOrGPULumenSceneData)
{
	Scene = InScene;
	LumenSceneData = nullptr;

	if (InScene->DefaultLumenSceneData)
	{
		LumenSceneData = InScene->DefaultLumenSceneData;
	}
	else
	{
		// Call increment operator to search for GPU or view specific Lumen scene data
		operator++();
	}
}

FLumenSceneDataIterator& FLumenSceneDataIterator::operator++()
{
	if (NextSceneData)
	{
		LumenSceneData = NextSceneData->Value;
		++NextSceneData;
	}
	else
	{
		LumenSceneData = nullptr;
	}
	return *this;
}

void FLumenSceneData::ResetAndConsolidate()
{
	// Reset arrays, but keep allocated memory for 1024 elements
	PendingAddOperations.Reset();
	PendingRemoveOperations.Reset(1024);
	PendingUpdateOperations.Reset();
	PendingUpdateOperations.Reserve(1024);
	PendingSurfaceCacheInvalidationOperations.Reset();
	PendingSurfaceCacheInvalidationOperations.Reserve(64);

	// Batch consolidate SparseSpanArrays
	PrimitiveGroups.Consolidate();
	Heightfields.Consolidate();
	MeshCards.Consolidate();
	Cards.Consolidate();
	PageTable.Consolidate();
}

void FLumenSceneData::UpdatePrimitiveInstanceOffset(int32 PrimitiveIndex)
{
	if (bTrackAllPrimitives)
	{
		PrimitivesToUpdateMeshCards.Add(PrimitiveIndex);
	}
}

void UpdateLumenScenePrimitives(FRHIGPUMask GPUMask, FScene* Scene)
{
	LLM_SCOPE_BYTAG(Lumen);
	TRACE_CPUPROFILER_EVENT_SCOPE(UpdateLumenScenePrimitives);
	QUICK_SCOPE_CYCLE_COUNTER(UpdateLumenScenePrimitives);

	// Remove primitives
	for (FLumenSceneDataIterator LumenSceneData = Scene->GetLumenSceneDataIterator(); LumenSceneData; ++LumenSceneData)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(RemoveLumenPrimitives);
		QUICK_SCOPE_CYCLE_COUNTER(RemoveLumenPrimitives);

		TSparseUniqueList<int32, SceneRenderingAllocator> PrimitiveGroupsToRemove;

		// Delete primitives
		for (const FLumenPrimitiveGroupRemoveInfo& RemoveInfo : LumenSceneData->PendingRemoveOperations)
		{
			for (int32 PrimitiveGroupIndex : RemoveInfo.LumenPrimitiveGroupIndices)
			{
				FLumenPrimitiveGroup& PrimitiveGroup = LumenSceneData->PrimitiveGroups[PrimitiveGroupIndex];

				for (int32 PrimitiveIndex = 0; PrimitiveIndex < PrimitiveGroup.Primitives.Num(); ++PrimitiveIndex)
				{
					if (PrimitiveGroup.Primitives[PrimitiveIndex] == RemoveInfo.Primitive)
					{
						PrimitiveGroup.Primitives.RemoveAtSwap(PrimitiveIndex, 1, false);
						break;
					}
				}

				PrimitiveGroupsToRemove.Add(PrimitiveGroupIndex);
			}
		}

		// Delete empty Primitive Groups
		for (int32 PrimitiveGroupIndex : PrimitiveGroupsToRemove.Array)
		{
			FLumenPrimitiveGroup& PrimitiveGroup = LumenSceneData->PrimitiveGroups[PrimitiveGroupIndex];

			LumenSceneData->RemoveMeshCards(PrimitiveGroup);

			if (PrimitiveGroup.RayTracingGroupMapElementId.IsValid())
			{
				if (PrimitiveGroup.Primitives.Num() == 0)
				{
					LumenSceneData->RayTracingGroups.RemoveByElementId(PrimitiveGroup.RayTracingGroupMapElementId);
					PrimitiveGroup.RayTracingGroupMapElementId = Experimental::FHashElementId();
				}
				else
				{
					// Update bounds
					FBox WorldSpaceBoundingBox;
					WorldSpaceBoundingBox.Init();
					for (const FPrimitiveSceneInfo* Primitive : PrimitiveGroup.Primitives)
					{
						WorldSpaceBoundingBox += Primitive->Proxy->GetBounds().GetBox();
					}
					PrimitiveGroup.WorldSpaceBoundingBox = WorldSpaceBoundingBox;
				}
			}

			if (PrimitiveGroup.Primitives.Num() == 0)
			{
				LumenSceneData->PrimitiveGroups.RemoveSpan(PrimitiveGroupIndex, 1);
			}
		}
	}

	// Add primitives
	for (FLumenSceneDataIterator LumenSceneData = Scene->GetLumenSceneDataIterator(); LumenSceneData; ++LumenSceneData)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(AddLumenPrimitives);
		QUICK_SCOPE_CYCLE_COUNTER(AddLumenPrimitives);

		for (FPrimitiveSceneInfo* ScenePrimitiveInfo : LumenSceneData->PendingAddOperations)
		{
			FPrimitiveSceneProxy* SceneProxy = ScenePrimitiveInfo->Proxy;
			const TConstArrayView<FPrimitiveInstance> InstanceSceneData = SceneProxy->GetInstanceSceneData();
			const int32 NumInstances = FMath::Max(InstanceSceneData.Num(), 1);
			bool bAnyInstanceValid = false;
			{
				const FMatrix& PrimitiveToWorld = SceneProxy->GetLocalToWorld();

				for (int32 InstanceIndex = 0; InstanceIndex < NumInstances; ++InstanceIndex)
				{
					FBox LocalBoundingBox = SceneProxy->GetLocalBounds().GetBox();
					FMatrix LocalToWorld = PrimitiveToWorld;

					if (InstanceIndex < InstanceSceneData.Num())
					{
						const FPrimitiveInstance& PrimitiveInstance = InstanceSceneData[InstanceIndex];
						LocalToWorld = PrimitiveInstance.LocalToPrimitive.ToMatrix() * PrimitiveToWorld;
						LocalBoundingBox = SceneProxy->GetInstanceLocalBounds(InstanceIndex).ToBox();
					}

					if (TrackPrimitiveInstanceForLumenScene(LocalToWorld, LocalBoundingBox, SceneProxy->IsEmissiveLightSource()))
					{
						bAnyInstanceValid = true;
						break;
					}
				}
			}

			if (bAnyInstanceValid)
			{
				ensure(ScenePrimitiveInfo->LumenPrimitiveGroupIndices.Num() == 0);

				// First try to merge components
				extern int32 GLumenMeshCardsMergeComponents;
				if (GLumenMeshCardsMergeComponents != 0 
					&& SceneProxy->GetRayTracingGroupId() != FPrimitiveSceneProxy::InvalidRayTracingGroupId
					&& !SceneProxy->IsEmissiveLightSource())
				{
					const Experimental::FHashElementId RayTracingGroupMapElementId = LumenSceneData->RayTracingGroups.FindOrAddId(SceneProxy->GetRayTracingGroupId(), -1);
					int32& PrimitiveGroupIndex = LumenSceneData->RayTracingGroups.GetByElementId(RayTracingGroupMapElementId).Value;

					if (PrimitiveGroupIndex >= 0)
					{
						ScenePrimitiveInfo->LumenPrimitiveGroupIndices.Add(PrimitiveGroupIndex);

						FLumenPrimitiveGroup& PrimitiveGroup = LumenSceneData->PrimitiveGroups[PrimitiveGroupIndex];
						ensure(PrimitiveGroup.RayTracingGroupMapElementId == RayTracingGroupMapElementId);

						LumenSceneData->RemoveMeshCards(PrimitiveGroup);
						PrimitiveGroup.bValidMeshCards = true;
						PrimitiveGroup.Primitives.Add(ScenePrimitiveInfo);

						FBox WorldSpaceBoundingBox;
						WorldSpaceBoundingBox.Init();
						for (const FPrimitiveSceneInfo* PrimitiveInfoInGroup : PrimitiveGroup.Primitives)
						{
							WorldSpaceBoundingBox += PrimitiveInfoInGroup->Proxy->GetBounds().GetBox();
						}
						PrimitiveGroup.WorldSpaceBoundingBox = WorldSpaceBoundingBox;
					}
					else
					{
						PrimitiveGroupIndex = LumenSceneData->PrimitiveGroups.AddSpan(1);
						ensure(ScenePrimitiveInfo->LumenPrimitiveGroupIndices.Num() == 0);
						ScenePrimitiveInfo->LumenPrimitiveGroupIndices.Add(PrimitiveGroupIndex);

						FLumenPrimitiveGroup& PrimitiveGroup = LumenSceneData->PrimitiveGroups[PrimitiveGroupIndex];
						PrimitiveGroup.RayTracingGroupMapElementId = RayTracingGroupMapElementId;
						PrimitiveGroup.PrimitiveInstanceIndex = -1;
						PrimitiveGroup.CardResolutionScale = 1.0f;
						PrimitiveGroup.WorldSpaceBoundingBox = SceneProxy->GetBounds().GetBox();
						PrimitiveGroup.MeshCardsIndex = -1;
						PrimitiveGroup.bValidMeshCards = true;
						PrimitiveGroup.bFarField = SceneProxy->IsRayTracingFarField();
						PrimitiveGroup.bHeightfield = false;
						PrimitiveGroup.LightingChannelMask = SceneProxy->GetLightingChannelMask();
						PrimitiveGroup.Primitives.Reset();
						PrimitiveGroup.Primitives.Add(ScenePrimitiveInfo);
					}
				}
				else
				{
					const FMatrix& LocalToWorld = SceneProxy->GetLocalToWorld();

					bool bMergedInstances = false;

					if (NumInstances > 1)
					{
						// Check if we can merge all instances into one MeshCards
						extern int32 GLumenMeshCardsMergeInstances;
						extern float GLumenMeshCardsMergedMaxWorldSize;

						const FBox PrimitiveBox = SceneProxy->GetBounds().GetBox();
						const FRenderBounds PrimitiveBounds = FRenderBounds(PrimitiveBox);

						if (GLumenMeshCardsMergeInstances
							&& NumInstances > 1
							&& PrimitiveBox.GetSize().GetMax() < GLumenMeshCardsMergedMaxWorldSize)
						{
							FRenderBounds LocalBounds;
							double TotalInstanceSurfaceArea = 0;

							for (int32 InstanceIndex = 0; InstanceIndex < NumInstances; ++InstanceIndex)
							{
								const FPrimitiveInstance& Instance = InstanceSceneData[InstanceIndex];
								const FRenderBounds& RenderBoundingBox = SceneProxy->GetInstanceLocalBounds(InstanceIndex);
								const FRenderBounds InstanceBounds = RenderBoundingBox.TransformBy(Instance.LocalToPrimitive);
								LocalBounds += InstanceBounds;
								const double InstanceSurfaceArea = BoxSurfaceArea((FVector)InstanceBounds.GetExtent());
								TotalInstanceSurfaceArea += InstanceSurfaceArea;
							}

							const double BoundsSurfaceArea = BoxSurfaceArea((FVector)LocalBounds.GetExtent());
							const float SurfaceAreaRatio = BoundsSurfaceArea / TotalInstanceSurfaceArea;

							extern float GLumenMeshCardsMergeInstancesMaxSurfaceAreaRatio;
							extern float GLumenMeshCardsMergedResolutionScale;

							if (SurfaceAreaRatio < GLumenMeshCardsMergeInstancesMaxSurfaceAreaRatio)
							{
								const int32 PrimitiveGroupIndex = LumenSceneData->PrimitiveGroups.AddSpan(1);
								ScenePrimitiveInfo->LumenPrimitiveGroupIndices.Add(PrimitiveGroupIndex);

								FLumenPrimitiveGroup& PrimitiveGroup = LumenSceneData->PrimitiveGroups[PrimitiveGroupIndex];
								PrimitiveGroup.PrimitiveInstanceIndex = -1;
								PrimitiveGroup.CardResolutionScale = FMath::Sqrt(1.0f / SurfaceAreaRatio) * GLumenMeshCardsMergedResolutionScale;
								PrimitiveGroup.WorldSpaceBoundingBox = LocalBounds.TransformBy(LocalToWorld).ToBox();
								PrimitiveGroup.MeshCardsIndex = -1;
								PrimitiveGroup.HeightfieldIndex = -1;
								PrimitiveGroup.bValidMeshCards = true;
								PrimitiveGroup.bFarField = SceneProxy->IsRayTracingFarField();
								PrimitiveGroup.bHeightfield = false;
								PrimitiveGroup.LightingChannelMask = SceneProxy->GetLightingChannelMask();
								PrimitiveGroup.bEmissiveLightSource = SceneProxy->IsEmissiveLightSource();
								PrimitiveGroup.Primitives.Reset();
								PrimitiveGroup.Primitives.Add(ScenePrimitiveInfo);

								bMergedInstances = true;
							}

							#define LOG_LUMEN_PRIMITIVE_ADDS 0
							#if LOG_LUMEN_PRIMITIVE_ADDS
							{
								UE_LOG(LogRenderer, Log, TEXT("AddLumenPrimitive %s: Instances: %u, Merged: %u, SurfaceAreaRatio: %.1f"),
									*LumenPrimitive.Primitive->Proxy->GetOwnerName().ToString(),
									NumInstances,
									LumenPrimitive.bMergedInstances ? 1 : 0,
									SurfaceAreaRatio);
							}
							#endif
						}

						if (!bMergedInstances)
						{
							ScenePrimitiveInfo->LumenPrimitiveGroupIndices.SetNum(NumInstances);

							for (int32 InstanceIndex = 0; InstanceIndex < NumInstances; ++InstanceIndex)
							{
								const int32 PrimitiveGroupIndex = LumenSceneData->PrimitiveGroups.AddSpan(1);
								ScenePrimitiveInfo->LumenPrimitiveGroupIndices[InstanceIndex] = PrimitiveGroupIndex;

								const FPrimitiveInstance& PrimitiveInstance = InstanceSceneData[InstanceIndex];
								const FRenderBounds& RenderBoundingBox = SceneProxy->GetInstanceLocalBounds(InstanceIndex);

								FLumenPrimitiveGroup& PrimitiveGroup = LumenSceneData->PrimitiveGroups[PrimitiveGroupIndex];
								PrimitiveGroup.PrimitiveInstanceIndex = InstanceIndex;
								PrimitiveGroup.CardResolutionScale = 1.0f;
								PrimitiveGroup.WorldSpaceBoundingBox = RenderBoundingBox.TransformBy(PrimitiveInstance.LocalToPrimitive.ToMatrix() * LocalToWorld).ToBox();
								PrimitiveGroup.MeshCardsIndex = -1;
								PrimitiveGroup.HeightfieldIndex = -1;
								PrimitiveGroup.bValidMeshCards = true;
								PrimitiveGroup.bFarField = SceneProxy->IsRayTracingFarField();
								PrimitiveGroup.bHeightfield = false;
								PrimitiveGroup.LightingChannelMask = SceneProxy->GetLightingChannelMask();
								PrimitiveGroup.bEmissiveLightSource = SceneProxy->IsEmissiveLightSource();
								PrimitiveGroup.Primitives.Reset();
								PrimitiveGroup.Primitives.Add(ScenePrimitiveInfo);
							}
						}
					}
					else
					{
						const int32 PrimitiveGroupIndex = LumenSceneData->PrimitiveGroups.AddSpan(1);
						ScenePrimitiveInfo->LumenPrimitiveGroupIndices.Add(PrimitiveGroupIndex);

						FLumenPrimitiveGroup& PrimitiveGroup = LumenSceneData->PrimitiveGroups[PrimitiveGroupIndex];
						PrimitiveGroup.PrimitiveInstanceIndex = 0;
						PrimitiveGroup.CardResolutionScale = 1.0f;
						PrimitiveGroup.WorldSpaceBoundingBox = SceneProxy->GetBounds().GetBox();
						PrimitiveGroup.MeshCardsIndex = -1;
						PrimitiveGroup.HeightfieldIndex = -1;
						PrimitiveGroup.bValidMeshCards = true;
						PrimitiveGroup.bFarField = SceneProxy->IsRayTracingFarField();
						PrimitiveGroup.bHeightfield = SceneProxy->SupportsHeightfieldRepresentation();
						PrimitiveGroup.LightingChannelMask = SceneProxy->GetLightingChannelMask();
						PrimitiveGroup.bEmissiveLightSource = SceneProxy->IsEmissiveLightSource();
						PrimitiveGroup.Primitives.Reset();
						PrimitiveGroup.Primitives.Add(ScenePrimitiveInfo);
					}
				}
			}
		}
	}

	// UpdateLumenPrimitives
	for (FLumenSceneDataIterator LumenSceneData = Scene->GetLumenSceneDataIterator(); LumenSceneData; ++LumenSceneData)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UpdateLumenPrimitives);
		QUICK_SCOPE_CYCLE_COUNTER(UpdateLumenPrimitives);

		for (TSet<FPrimitiveSceneInfo*>::TIterator It(LumenSceneData->PendingUpdateOperations); It; ++It)
		{
			FPrimitiveSceneInfo* PrimitiveSceneInfo = *It;

			if (PrimitiveSceneInfo->LumenPrimitiveGroupIndices.Num() > 0)
			{
				const FCardRepresentationData* CardRepresentationData = PrimitiveSceneInfo->Proxy->GetMeshCardRepresentation();
				const FMatrix& PrimitiveToWorld = PrimitiveSceneInfo->Proxy->GetLocalToWorld();

				const TConstArrayView<FPrimitiveInstance> InstanceSceneData = PrimitiveSceneInfo->Proxy->GetInstanceSceneData();

				for (int32 PrimitiveGroupIndex : PrimitiveSceneInfo->LumenPrimitiveGroupIndices)
				{
					FLumenPrimitiveGroup& PrimitiveGroup = LumenSceneData->PrimitiveGroups[PrimitiveGroupIndex];

					if (PrimitiveGroup.PrimitiveInstanceIndex >= 0)
					{
						FBox WorldSpaceBoundingBox = PrimitiveSceneInfo->Proxy->GetBounds().GetBox();

						if (PrimitiveGroup.PrimitiveInstanceIndex < InstanceSceneData.Num())
						{
							const FPrimitiveInstance& PrimitiveInstance = InstanceSceneData[PrimitiveGroup.PrimitiveInstanceIndex];
							const FRenderBounds& RenderBoundingBox = PrimitiveSceneInfo->Proxy->GetInstanceLocalBounds(PrimitiveGroup.PrimitiveInstanceIndex); 
							WorldSpaceBoundingBox = RenderBoundingBox.ToBox().TransformBy(PrimitiveInstance.LocalToPrimitive.ToMatrix() * PrimitiveToWorld);
						}

						PrimitiveGroup.WorldSpaceBoundingBox = WorldSpaceBoundingBox;
						LumenSceneData->UpdateMeshCards(PrimitiveToWorld, PrimitiveGroup.MeshCardsIndex, CardRepresentationData->MeshCardsBuildData);
					}
				}
			}
		}
	}

	// InvalidateLumenSurfaceCacheForPrimitives
	for (FLumenSceneDataIterator LumenSceneData = Scene->GetLumenSceneDataIterator(); LumenSceneData; ++LumenSceneData)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(InvalidateLumenSurfaceCacheForPrimitives);
		QUICK_SCOPE_CYCLE_COUNTER(InvalidateLumenSurfaceCacheForPrimitives);

		for (TSet<FPrimitiveSceneInfo*>::TIterator It(LumenSceneData->PendingSurfaceCacheInvalidationOperations); It; ++It)
		{
			FPrimitiveSceneInfo* PrimitiveSceneInfo = *It;

			if (PrimitiveSceneInfo->LumenPrimitiveGroupIndices.Num() > 0)
			{
				const FCardRepresentationData* CardRepresentationData = PrimitiveSceneInfo->Proxy->GetMeshCardRepresentation();
				const FMatrix& PrimitiveToWorld = PrimitiveSceneInfo->Proxy->GetLocalToWorld();

				const TConstArrayView<FPrimitiveInstance> InstanceSceneData = PrimitiveSceneInfo->Proxy->GetInstanceSceneData();

				for (int32 PrimitiveGroupIndex : PrimitiveSceneInfo->LumenPrimitiveGroupIndices)
				{
					const FLumenPrimitiveGroup& PrimitiveGroup = LumenSceneData->PrimitiveGroups[PrimitiveGroupIndex];
					if (PrimitiveGroup.MeshCardsIndex >= 0)
					{
						LumenSceneData->InvalidateSurfaceCache(GPUMask, PrimitiveGroup.MeshCardsIndex);
					}
				}
			}
		}
	}

	for (FLumenSceneDataIterator LumenSceneData = Scene->GetLumenSceneDataIterator(); LumenSceneData; ++LumenSceneData)
	{
		LumenSceneData->ResetAndConsolidate();
	}
}

void FLumenSceneData::RemoveAllMeshCards()
{
	LLM_SCOPE_BYTAG(Lumen);
	QUICK_SCOPE_CYCLE_COUNTER(RemoveAllCards);

	for (FLumenPrimitiveGroup& PrimitiveGroup : PrimitiveGroups)
	{
		RemoveMeshCards(PrimitiveGroup);
	}
}

bool FLumenSceneData::UpdateAtlasSize()
{
	extern int32 GLumenSurfaceCacheCompress;

	ESurfaceCacheCompression NewCompression = ESurfaceCacheCompression::Disabled;
	if (GLumenSurfaceCacheCompress == 1 && GRHISupportsUAVFormatAliasing)
	{
		NewCompression = ESurfaceCacheCompression::UAVAliasing;
	}
	else if (GLumenSurfaceCacheCompress == 2)
	{
		NewCompression = ESurfaceCacheCompression::CopyTextureRegion;
	}

	if (PhysicalAtlasSize != GetDesiredPhysicalAtlasSize(SurfaceCacheResolution) || PhysicalAtlasCompression != NewCompression)
	{
		RemoveAllMeshCards();

		PhysicalAtlasSize = GetDesiredPhysicalAtlasSize(SurfaceCacheResolution);
		SurfaceCacheAllocator.Init(GetDesiredPhysicalAtlasSizeInPages(SurfaceCacheResolution));
		UnlockedAllocationHeap.Clear();
		for (uint32 GPUIndex = 0; GPUIndex < GNumExplicitGPUsForRendering; GPUIndex++)
		{
			LastCapturedPageHeap[GPUIndex].Clear();
			PagesToRecaptureHeap[GPUIndex].Clear();
		}

		PhysicalAtlasCompression = NewCompression;
		return true;
	}

	return false;
}

void FLumenSceneData::MapSurfaceCachePage(const FLumenSurfaceMipMap& MipMap, int32 PageTableIndex, FRHIGPUMask GPUMask)
{
	FLumenPageTableEntry& PageTableEntry = PageTable[PageTableIndex];
	if (!PageTableEntry.IsMapped())
	{
		FLumenSurfaceCacheAllocator::FAllocation Allocation;
		SurfaceCacheAllocator.Allocate(PageTableEntry, Allocation);

		PageTableEntry.PhysicalPageCoord = Allocation.PhysicalPageCoord;
		PageTableEntry.PhysicalAtlasRect = Allocation.PhysicalAtlasRect;

		if (PageTableEntry.IsMapped())
		{
			PageTableEntry.SamplePageIndex = PageTableIndex;
			PageTableEntry.SampleAtlasBiasX = PageTableEntry.PhysicalAtlasRect.Min.X / Lumen::MinCardResolution;
			PageTableEntry.SampleAtlasBiasY = PageTableEntry.PhysicalAtlasRect.Min.Y / Lumen::MinCardResolution;
			PageTableEntry.SampleCardResLevelX = MipMap.ResLevelX;
			PageTableEntry.SampleCardResLevelY = MipMap.ResLevelY;

			for (uint32 GPUIndex = 0; GPUIndex < GNumExplicitGPUsForRendering; GPUIndex++)
			{
				LastCapturedPageHeap[GPUIndex].Add(
#if WITH_MGPU
					GPUMask.Contains(GPUIndex) ? GetSurfaceCacheUpdateFrameIndex() : 0,
#else
					GetSurfaceCacheUpdateFrameIndex(),
#endif
					PageTableIndex);
			}

			if (!MipMap.bLocked)
			{
				UnlockedAllocationHeap.Add(SurfaceCacheFeedback.GetFrameIndex(), PageTableIndex);
			}
		}

		PageTableIndicesToUpdateInBuffer.Add(PageTableIndex);
	}
}

void FLumenSceneData::UnmapSurfaceCachePage(bool bLocked, FLumenPageTableEntry& Page, int32 PageIndex)
{
	if (Page.IsMapped())
	{
		for (uint32 GPUIndex = 0; GPUIndex < GNumExplicitGPUsForRendering; GPUIndex++)
		{
			LastCapturedPageHeap[GPUIndex].Remove(PageIndex);
			PagesToRecaptureHeap[GPUIndex].Remove(PageIndex);
		}

		if (!bLocked)
		{
			UnlockedAllocationHeap.Remove(PageIndex);
		}

		SurfaceCacheAllocator.Free(Page);

		Page.PhysicalPageCoord.X = -1;
		Page.PhysicalPageCoord.Y = -1;
		Page.SampleAtlasBiasX = 0;
		Page.SampleAtlasBiasY = 0;
		Page.SampleCardResLevelX = 0;
		Page.SampleCardResLevelY = 0;
	}
}

void FLumenSceneData::ReallocVirtualSurface(FLumenCard& Card, int32 CardIndex, int32 ResLevel, bool bLockPages)
{
	FLumenSurfaceMipMap& MipMap = Card.GetMipMap(ResLevel);

	if (MipMap.PageTableSpanSize > 0 && MipMap.bLocked != bLockPages)
	{
		// Virtual memory is already allocated, but need to change the bLocked flag for any mapped pages
	
		if (MipMap.bLocked)
		{
			// Unlock all pages
			for (int32 LocalPageIndex = 0; LocalPageIndex < MipMap.SizeInPagesX * MipMap.SizeInPagesY; ++LocalPageIndex)
			{
				const int32 PageTableIndex = MipMap.GetPageTableIndex(LocalPageIndex);
				FLumenPageTableEntry& PageTableEntry = PageTable[PageTableIndex];
				if (PageTableEntry.IsMapped())
				{
					UnlockedAllocationHeap.Add(SurfaceCacheFeedback.GetFrameIndex(), PageTableIndex);
				}
			}

			MipMap.bLocked = false;
		}
		else
		{
			// Lock all pages
			for (int32 LocalPageIndex = 0; LocalPageIndex < MipMap.SizeInPagesX * MipMap.SizeInPagesY; ++LocalPageIndex)
			{
				const int32 PageTableIndex = MipMap.GetPageTableIndex(LocalPageIndex);
				FLumenPageTableEntry& PageTableEntry = PageTable[PageTableIndex];
				if (PageTableEntry.IsMapped())
				{
					UnlockedAllocationHeap.Remove(PageTableIndex);
				}
			}

			MipMap.bLocked = true;
		}
	}
	else if (MipMap.PageTableSpanSize == 0)
	{
		// Allocate virtual memory for the given mip map

		FLumenMipMapDesc MipMapDesc;
		Card.GetMipMapDesc(ResLevel, MipMapDesc);

		MipMap.bLocked = bLockPages;
		MipMap.SizeInPagesX = MipMapDesc.SizeInPages.X;
		MipMap.SizeInPagesY = MipMapDesc.SizeInPages.Y;
		MipMap.ResLevelX = MipMapDesc.ResLevelX;
		MipMap.ResLevelY = MipMapDesc.ResLevelY;
		MipMap.PageTableSpanSize = MipMapDesc.SizeInPages.X * MipMapDesc.SizeInPages.Y;
		MipMap.PageTableSpanOffset = PageTable.AddSpan(MipMap.PageTableSpanSize);

		for (int32 LocalPageIndex = 0; LocalPageIndex < MipMapDesc.SizeInPages.X * MipMapDesc.SizeInPages.Y; ++LocalPageIndex)
		{
			const int32 PageTableIndex = MipMap.GetPageTableIndex(LocalPageIndex);

			FLumenPageTableEntry& PageTableEntry = PageTable[PageTableIndex];
			PageTableEntry.CardIndex = CardIndex;
			PageTableEntry.ResLevel = ResLevel;
			PageTableEntry.SubAllocationSize = MipMapDesc.bSubAllocation ? MipMapDesc.Resolution : FIntPoint(-1, -1);
			PageTableEntry.SampleAtlasBiasX = 0;
			PageTableEntry.SampleAtlasBiasY = 0;
			PageTableEntry.SampleCardResLevelX = 0;
			PageTableEntry.SampleCardResLevelY = 0;

			const int32 LocalPageCoordX = LocalPageIndex % MipMapDesc.SizeInPages.X;
			const int32 LocalPageCoordY = LocalPageIndex / MipMapDesc.SizeInPages.X;

			FVector4f CardUVRect;
			CardUVRect.X = float(LocalPageCoordX + 0.0f) / MipMapDesc.SizeInPages.X;
			CardUVRect.Y = float(LocalPageCoordY + 0.0f) / MipMapDesc.SizeInPages.Y;
			CardUVRect.Z = float(LocalPageCoordX + 1.0f) / MipMapDesc.SizeInPages.X;
			CardUVRect.W = float(LocalPageCoordY + 1.0f) / MipMapDesc.SizeInPages.Y;

			// Every page has a 0.5 texel border for correct bilinear sampling
			// This border is only needed on interior page edges
			{
				FVector2D CardBorderOffset;
				CardBorderOffset = FVector2D(0.5f * (Lumen::PhysicalPageSize - Lumen::VirtualPageSize));
				CardBorderOffset.X *= (CardUVRect.Z - CardUVRect.X) / Lumen::PhysicalPageSize;
				CardBorderOffset.Y *= (CardUVRect.W - CardUVRect.Y) / Lumen::PhysicalPageSize;

				if (LocalPageCoordX > 0)
				{
					CardUVRect.X -= CardBorderOffset.X;
				}
				if (LocalPageCoordY > 0)
				{
					CardUVRect.Y -= CardBorderOffset.Y;
				}
				if (LocalPageCoordX < MipMapDesc.SizeInPages.X - 1)
				{
					CardUVRect.Z += CardBorderOffset.X;
				}
				if (LocalPageCoordY < MipMapDesc.SizeInPages.Y - 1)
				{
					CardUVRect.W += CardBorderOffset.Y;
				}
			}

			PageTableEntry.CardUVRect = CardUVRect;

			PageTableIndicesToUpdateInBuffer.Add(PageTableIndex);
		}

		Card.UpdateMinMaxAllocatedLevel();
		CardIndicesToUpdateInBuffer.Add(CardIndex);
	}
}

void FLumenSceneData::FreeVirtualSurface(FLumenCard& Card, uint8 FromResLevel, uint8 ToResLevel)
{
	if (Card.IsAllocated())
	{
		for (uint8 ResLevel = FromResLevel; ResLevel <= ToResLevel; ++ResLevel)
		{
			FLumenSurfaceMipMap& MipMap = Card.GetMipMap(ResLevel);

			if (MipMap.IsAllocated())
			{
				// Unmap pages
				for (int32 LocalPageIndex = 0; LocalPageIndex < MipMap.SizeInPagesX * MipMap.SizeInPagesY; ++LocalPageIndex)
				{
					const int32 PageTableIndex = MipMap.GetPageTableIndex(LocalPageIndex);

					FLumenPageTableEntry& PageTableEntry = PageTable[PageTableIndex];
					UnmapSurfaceCachePage(MipMap.bLocked, PageTableEntry, PageTableIndex);
					PageTableEntry = FLumenPageTableEntry();
				}

				if (MipMap.PageTableSpanSize > 0)
				{
					PageTable.RemoveSpan(MipMap.PageTableSpanOffset, MipMap.PageTableSpanSize);

					for (int32 SpanOffset = 0; SpanOffset < MipMap.PageTableSpanSize; ++SpanOffset)
					{
						PageTableIndicesToUpdateInBuffer.Add(MipMap.PageTableSpanOffset + SpanOffset);
					}

					MipMap.PageTableSpanOffset = -1;
					MipMap.PageTableSpanSize = 0;
					MipMap.bLocked = false;
				}
			}
		}

		Card.UpdateMinMaxAllocatedLevel();
	}
}

/**
 * Remove any empty virtual mip allocations, and flatten page search by walking 
 * though the sparse mip maps and reusing lower res resident pages
 */
void FLumenSceneData::UpdateCardMipMapHierarchy(FLumenCard& Card)
{
	// Remove any mip map virtual allocations, which don't have any pages mapped
	for (int32 ResLevel = Card.MinAllocatedResLevel; ResLevel <= Card.MaxAllocatedResLevel; ++ResLevel)
	{
		FLumenSurfaceMipMap& MipMap = Card.GetMipMap(ResLevel);

		if (MipMap.IsAllocated())
		{
			bool IsAnyPageMapped = false;

			for (int32 LocalPageIndex = 0; LocalPageIndex < MipMap.SizeInPagesX * MipMap.SizeInPagesY; ++LocalPageIndex)
			{
				const int32 PageIndex = MipMap.GetPageTableIndex(LocalPageIndex);
				if (GetPageTableEntry(PageIndex).IsMapped())
				{
					IsAnyPageMapped = true;
					break;
				}
			}

			if (!IsAnyPageMapped)
			{
				FreeVirtualSurface(Card, ResLevel, ResLevel);
			}
		}
	}
	Card.UpdateMinMaxAllocatedLevel();


	int32 ParentResLevel = Card.MinAllocatedResLevel;

	for (int32 ResLevel = ParentResLevel + 1; ResLevel <= Card.MaxAllocatedResLevel; ++ResLevel)
	{
		FLumenSurfaceMipMap& MipMap = Card.GetMipMap(ResLevel);

		if (MipMap.PageTableSpanSize > 0)
		{
			for (int32 LocalPageIndex = 0; LocalPageIndex < MipMap.SizeInPagesX * MipMap.SizeInPagesY; ++LocalPageIndex)
			{
				const int32 PageIndex = MipMap.GetPageTableIndex(LocalPageIndex);
				FLumenPageTableEntry& PageTableEntry = GetPageTableEntry(PageIndex);

				if (!PageTableEntry.IsMapped())
				{
					FIntPoint LocalPageCoord;
					LocalPageCoord.X = LocalPageIndex % MipMap.SizeInPagesX;
					LocalPageCoord.Y = LocalPageIndex / MipMap.SizeInPagesX;

					FLumenSurfaceMipMap& ParentMipMap = Card.GetMipMap(ParentResLevel);
					const FIntPoint ParentLocalPageCoord = (LocalPageCoord * ParentMipMap.GetSizeInPages()) / MipMap.GetSizeInPages();
					const int32 ParentLocalPageIndex = ParentLocalPageCoord.X + ParentLocalPageCoord.Y * ParentMipMap.SizeInPagesX;

					const int32 ParentPageIndex = ParentMipMap.GetPageTableIndex(ParentLocalPageIndex);
					FLumenPageTableEntry& ParentPageTableEntry = GetPageTableEntry(ParentPageIndex);

					PageTableEntry.SamplePageIndex = ParentPageTableEntry.SamplePageIndex;
					PageTableEntry.SampleAtlasBiasX = ParentPageTableEntry.SampleAtlasBiasX;
					PageTableEntry.SampleAtlasBiasY = ParentPageTableEntry.SampleAtlasBiasY;
					PageTableEntry.SampleCardResLevelX = ParentPageTableEntry.SampleCardResLevelX;
					PageTableEntry.SampleCardResLevelY = ParentPageTableEntry.SampleCardResLevelY;

					PageTableIndicesToUpdateInBuffer.Add(PageIndex);
				}
			}

			ParentResLevel = ResLevel;
		}
	}
}

void FLumenSceneData::CopyInitialData(const FLumenSceneData& SourceSceneData)
{
	// bTrackAllPrimitives is assumed to be the same across all FLumenSceneData
	bTrackAllPrimitives = SourceSceneData.bTrackAllPrimitives;

	PrimitiveGroups = SourceSceneData.PrimitiveGroups;
	RayTracingGroups = SourceSceneData.RayTracingGroups;
	LandscapePrimitives = SourceSceneData.LandscapePrimitives;
	PendingAddOperations = SourceSceneData.PendingAddOperations;
	PendingUpdateOperations = SourceSceneData.PendingUpdateOperations;
	PendingRemoveOperations = SourceSceneData.PendingRemoveOperations;

	// Newly created scene data has no mesh cards yet, so clear mesh card indices
	for (FLumenPrimitiveGroup& PrimitiveGroup : PrimitiveGroups)
	{
		PrimitiveGroup.MeshCardsIndex = -1;
	}
}

#if WITH_MGPU
// When the GPU mask changes for a view specific Lumen scene, we copy all data cross GPU to avoid transient glitches and
// potentially corrupt data.  Copying is expensive, but the assumption is that the GPU index will only change for debugging
// or performance tweaks, not during steady rendering.
void FLumenSceneData::UpdateGPUMask(FRDGBuilder& GraphBuilder, const FLumenSceneFrameTemporaries& FrameTemporaries, FLumenViewState& LumenViewState, FRHIGPUMask ViewGPUMask)
{
	check(bViewSpecific == true);

	if (!bViewSpecificMaskInitialized)
	{
		ViewSpecificMask = ViewGPUMask;
		bViewSpecificMaskInitialized = true;
	}
	else
	{
		if (ViewSpecificMask != ViewGPUMask)
		{
			FLumenSceneData* LumenSceneData = this;
			FRHIGPUMask SourceGPUMask = ViewSpecificMask;
			FRHIGPUMask DestGPUMask = ViewGPUMask;

			ViewSpecificMask = ViewGPUMask;

			// Since we're copying all GPU state cross-GPU, it should now be coherent, and we can copy the per-GPU page state as well
			for (uint32 DestGPUIndex : FRHIGPUMask::All())
			{
				if (!SourceGPUMask.Contains(DestGPUIndex))
				{
					// Copy operator is deleted, need to manually copy item by item
					const FBinaryHeap<uint32, uint32>& SourceHeap = LastCapturedPageHeap[SourceGPUMask.GetFirstIndex()];
					FBinaryHeap<uint32, uint32>& DestHeap = LastCapturedPageHeap[DestGPUIndex];

					uint32 IndexSize = SourceHeap.GetIndexSize();
					for (uint32 Index = 0; Index < IndexSize; Index++)
					{
						// We add items to heaps in tandem, so all should have the same indices
						check(SourceHeap.IsPresent(Index) == DestHeap.IsPresent(Index));

						if (SourceHeap.IsPresent(Index))
						{
							DestHeap.Update(SourceHeap.GetKey(Index), Index);
						}
					}
				}
			}

			// Make array of resources from FLumenSceneFrameTemporaries, since it's on the stack, and can't be passed to the Pass
			TArray<FRDGTextureRef, TFixedAllocator<9>> FrameTemporaryTextures;

			#define ADD_LUMEN_FRAME_TEMPORARY(NAME) \
				if (FrameTemporaries.NAME) FrameTemporaryTextures.Add(FrameTemporaries.NAME)

			ADD_LUMEN_FRAME_TEMPORARY(AlbedoAtlas);
			ADD_LUMEN_FRAME_TEMPORARY(OpacityAtlas);
			ADD_LUMEN_FRAME_TEMPORARY(NormalAtlas);
			ADD_LUMEN_FRAME_TEMPORARY(EmissiveAtlas);
			ADD_LUMEN_FRAME_TEMPORARY(DepthAtlas);

			ADD_LUMEN_FRAME_TEMPORARY(DirectLightingAtlas);
			ADD_LUMEN_FRAME_TEMPORARY(IndirectLightingAtlas);
			ADD_LUMEN_FRAME_TEMPORARY(RadiosityNumFramesAccumulatedAtlas);
			ADD_LUMEN_FRAME_TEMPORARY(FinalLightingAtlas);

			#undef ADD_LUMEN_FRAME_TEMPORARY

			AddPass(GraphBuilder, RDG_EVENT_NAME("LumenCrossGPUTransfer"),
				[LumenSceneData, LumenView = &LumenViewState, FrameTemporaryTextures, SourceGPUMask, DestGPUMask](FRHICommandListImmediate& RHICmdList)
			{
				uint32 SourceGPUIndex = SourceGPUMask.GetFirstIndex();

				TArray<FTransferResourceParams> CrossGPUTransferResources;
				for (uint32 DestGPUIndex : FRHIGPUMask::All())
				{
					if (!SourceGPUMask.Contains(DestGPUIndex))
					{
						#define TRANSFER_LUMEN_RESOURCE(NAME) \
							if (LumenSceneData->NAME) CrossGPUTransferResources.Add(FTransferResourceParams(LumenSceneData->NAME->GetRHI(), SourceGPUIndex, DestGPUIndex, false, false))

						TRANSFER_LUMEN_RESOURCE(CardBuffer);
						TRANSFER_LUMEN_RESOURCE(MeshCardsBuffer);
						TRANSFER_LUMEN_RESOURCE(HeightfieldBuffer);
						TRANSFER_LUMEN_RESOURCE(SceneInstanceIndexToMeshCardsIndexBuffer);
						TRANSFER_LUMEN_RESOURCE(CardPageBuffer);

						TRANSFER_LUMEN_RESOURCE(CardPageLastUsedBuffer);
						TRANSFER_LUMEN_RESOURCE(CardPageHighResLastUsedBuffer);

						TRANSFER_LUMEN_RESOURCE(AlbedoAtlas);
						TRANSFER_LUMEN_RESOURCE(OpacityAtlas);
						TRANSFER_LUMEN_RESOURCE(NormalAtlas);
						TRANSFER_LUMEN_RESOURCE(EmissiveAtlas);
						TRANSFER_LUMEN_RESOURCE(DepthAtlas);

						TRANSFER_LUMEN_RESOURCE(DirectLightingAtlas);
						TRANSFER_LUMEN_RESOURCE(IndirectLightingAtlas);
						TRANSFER_LUMEN_RESOURCE(RadiosityNumFramesAccumulatedAtlas);
						TRANSFER_LUMEN_RESOURCE(FinalLightingAtlas);

						TRANSFER_LUMEN_RESOURCE(RadiosityTraceRadianceAtlas);
						TRANSFER_LUMEN_RESOURCE(RadiosityTraceHitDistanceAtlas);
						TRANSFER_LUMEN_RESOURCE(RadiosityProbeSHRedAtlas);
						TRANSFER_LUMEN_RESOURCE(RadiosityProbeSHGreenAtlas);
						TRANSFER_LUMEN_RESOURCE(RadiosityProbeSHBlueAtlas);

						TRANSFER_LUMEN_RESOURCE(PageTableBuffer);

						#undef TRANSFER_LUMEN_RESOURCE

						for (FRDGTextureRef Texture : FrameTemporaryTextures)
						{
							CrossGPUTransferResources.Add(FTransferResourceParams(Texture->GetRHI(), SourceGPUIndex, DestGPUIndex, false, false));
						}

						// Also transfer the view state resources cross GPU
						LumenView->AddCrossGPUTransfers(SourceGPUIndex, DestGPUIndex, CrossGPUTransferResources);
					}
				}

				RHICmdList.TransferResources(CrossGPUTransferResources);
			});
		}
	}
}
#endif  // WITH_MGPU

/**
 * Evict all pages on demand, useful for debugging
 */
void FLumenSceneData::ForceEvictEntireCache()
{
	TSparseUniqueList<int32, SceneRenderingAllocator> DirtyCards;

	while (EvictOldestAllocation(/*MaxFramesSinceLastUsed*/ 0, DirtyCards))
	{
	}

	for (int32 CardIndex : DirtyCards.Array)
	{
		FLumenCard& Card = Cards[CardIndex];
		UpdateCardMipMapHierarchy(Card);
		CardIndicesToUpdateInBuffer.Add(CardIndex);
	}
}

bool FLumenSceneData::EvictOldestAllocation(uint32 MaxFramesSinceLastUsed, TSparseUniqueList<int32, SceneRenderingAllocator>& DirtyCards)
{
	if (UnlockedAllocationHeap.Num() > 0)
	{
		const uint32 PageTableIndex = UnlockedAllocationHeap.Top();
		const uint32 LastFrameUsed = UnlockedAllocationHeap.GetKey(PageTableIndex);

		if (uint32(LastFrameUsed + MaxFramesSinceLastUsed) <= SurfaceCacheFeedback.GetFrameIndex())
		{
			UnlockedAllocationHeap.Pop();

			FLumenPageTableEntry& Page = PageTable[PageTableIndex];
			if (Page.IsMapped())
			{
				UnmapSurfaceCachePage(false, Page, PageTableIndex);
				DirtyCards.Add(Page.CardIndex);
			}

			return true;
		}
	}

	return false;
}

void FLumenSceneData::DumpStats(const FDistanceFieldSceneData& DistanceFieldSceneData, bool bDumpMeshDistanceFields, bool bDumpPrimitiveGroups)
{
	const FIntPoint PageAtlasSizeInPages = GetDesiredPhysicalAtlasSizeInPages(SurfaceCacheResolution);
	const int32 NumPhysicalPages = PageAtlasSizeInPages.X * PageAtlasSizeInPages.Y;

	int32 NumCards = 0;
	int32 NumVisibleCards = 0;
	FLumenCard::FSurfaceStats SurfaceStats;

	for (const FLumenCard& Card : Cards)
	{
		++NumCards;

		if (Card.bVisible)
		{
			++NumVisibleCards;

			Card.GetSurfaceStats(PageTable, SurfaceStats);
		}
	}

	int32 NumPrimitiveGroups = 0;
	int32 NumPrimitivesMerged = 0;
	int32 NumInstancesMerged = 0;
	int32 NumMeshCards = 0;
	uint32 NumFarFieldPrimitiveGroups = 0;
	uint32 NumFarFieldMeshCards = 0;
	uint32 NumFarFieldCards = 0;
	FLumenCard::FSurfaceStats FarFieldSurfaceStats;
	SIZE_T PrimitiveGroupsAllocatedMemory = PrimitiveGroups.GetAllocatedSize();

	for (const FLumenPrimitiveGroup& PrimitiveGroup : PrimitiveGroups)
	{
		++NumPrimitiveGroups;

		if (PrimitiveGroup.HasMergedInstances())
		{
			for (const FPrimitiveSceneInfo* ScenePrimitive : PrimitiveGroup.Primitives)
			{
				++NumPrimitivesMerged;

				const TConstArrayView<FPrimitiveInstance> InstanceSceneData = ScenePrimitive->Proxy->GetInstanceSceneData();
				NumInstancesMerged += InstanceSceneData.Num();
			}
		}

		if (PrimitiveGroup.MeshCardsIndex >= 0)
		{
			++NumMeshCards;
		}

		if (PrimitiveGroup.bFarField)
		{
			++NumFarFieldPrimitiveGroups;

			if (PrimitiveGroup.MeshCardsIndex >= 0)
			{
				++NumFarFieldMeshCards;

				const FLumenMeshCards& MeshCardsInstance = MeshCards[PrimitiveGroup.MeshCardsIndex];
				NumFarFieldCards += MeshCardsInstance.NumCards;

				for (uint32 LocalCardIndex = 0; LocalCardIndex < MeshCardsInstance.NumCards; ++LocalCardIndex)
				{
					const FLumenCard& LumenCard = Cards[MeshCardsInstance.FirstCardIndex + LocalCardIndex];
					if (LumenCard.IsAllocated())
					{
						LumenCard.GetSurfaceStats(PageTable, FarFieldSurfaceStats);
					}
				}
			}
		}

		PrimitiveGroupsAllocatedMemory += PrimitiveGroup.Primitives.GetAllocatedSize();
	}

	FLumenSurfaceCacheAllocator::FStats AllocatorStats;
	SurfaceCacheAllocator.GetStats(AllocatorStats);

	UE_LOG(LogRenderer, Log, TEXT("*** LumenScene Stats ***"));
	UE_LOG(LogRenderer, Log, TEXT("  Mesh SDF Objects: %d"), DistanceFieldSceneData.NumObjectsInBuffer);
	UE_LOG(LogRenderer, Log, TEXT("  Primitive groups: %d"), NumPrimitiveGroups);
	UE_LOG(LogRenderer, Log, TEXT("  Merged primitives: %d"), NumPrimitivesMerged);
	UE_LOG(LogRenderer, Log, TEXT("  Merged instances: %d"), NumInstancesMerged);
	UE_LOG(LogRenderer, Log, TEXT("  Mesh cards: %d"), NumMeshCards);
	UE_LOG(LogRenderer, Log, TEXT("  Cards: %d"), NumCards);

	UE_LOG(LogRenderer, Log, TEXT("*** Surface cache ***"));
	UE_LOG(LogRenderer, Log, TEXT("  Allocated %d physical pages out of %d"), NumPhysicalPages - AllocatorStats.NumFreePages, NumPhysicalPages);
	UE_LOG(LogRenderer, Log, TEXT("  Bin pages: %d, wasted pages: %d, free texels: %.3fM"), AllocatorStats.BinNumPages, AllocatorStats.BinNumWastedPages, AllocatorStats.BinPageFreeTexels / (1024.0f * 1024.0f));
	UE_LOG(LogRenderer, Log, TEXT("  Virtual texels: %.3fM"), SurfaceStats.NumVirtualTexels / (1024.0f * 1024.0f));
	UE_LOG(LogRenderer, Log, TEXT("  Locked virtual texels: %.3fM"), SurfaceStats.NumLockedVirtualTexels / (1024.0f * 1024.0f));
	UE_LOG(LogRenderer, Log, TEXT("  Physical texels: %.3fM, usage: %.3f%%"), SurfaceStats.NumPhysicalTexels / (1024.0f * 1024.0f), (100.0f * SurfaceStats.NumPhysicalTexels) / (PhysicalAtlasSize.X * PhysicalAtlasSize.Y));
	UE_LOG(LogRenderer, Log, TEXT("  Locked Physical texels: %.3fM, usage: %.3f%%"), SurfaceStats.NumLockedPhysicalTexels / (1024.0f * 1024.0f), (100.0f * SurfaceStats.NumLockedPhysicalTexels) / (PhysicalAtlasSize.X * PhysicalAtlasSize.Y));
	UE_LOG(LogRenderer, Log, TEXT("  Dropped res levels: %u"), SurfaceStats.DroppedResLevels);
	UE_LOG(LogRenderer, Log, TEXT("  Mesh cards to add: %d"), NumMeshCardsToAdd);
	UE_LOG(LogRenderer, Log, TEXT("  Locked cards to update: %d"), NumLockedCardsToUpdate);
	UE_LOG(LogRenderer, Log, TEXT("  Hi-res pages to add: %d"), NumHiResPagesToAdd);

	UE_LOG(LogRenderer, Log, TEXT("*** Far Field ***"));
	UE_LOG(LogRenderer, Log, TEXT("  Primitive groups: %d"), NumFarFieldPrimitiveGroups);
	UE_LOG(LogRenderer, Log, TEXT("  Mesh cards: %d"), NumFarFieldMeshCards);
	UE_LOG(LogRenderer, Log, TEXT("  Cards: %d"), NumFarFieldCards);
	UE_LOG(LogRenderer, Log, TEXT("  Virtual texels: %.3fM"), FarFieldSurfaceStats.NumVirtualTexels / (1024.0f * 1024.0f));
	UE_LOG(LogRenderer, Log, TEXT("  Locked virtual texels: %.3fM"), FarFieldSurfaceStats.NumLockedVirtualTexels / (1024.0f * 1024.0f));
	UE_LOG(LogRenderer, Log, TEXT("  Physical texels: %.3fM, usage: %.3f%%"), FarFieldSurfaceStats.NumPhysicalTexels / (1024.0f * 1024.0f), (100.0f * FarFieldSurfaceStats.NumPhysicalTexels) / (PhysicalAtlasSize.X * PhysicalAtlasSize.Y));
	UE_LOG(LogRenderer, Log, TEXT("  Locked Physical texels: %.3fM, usage: %.3f%%"), FarFieldSurfaceStats.NumLockedPhysicalTexels / (1024.0f * 1024.0f), (100.0f * FarFieldSurfaceStats.NumLockedPhysicalTexels) / (PhysicalAtlasSize.X * PhysicalAtlasSize.Y));
	UE_LOG(LogRenderer, Log, TEXT("  Dropped res levels: %u"), FarFieldSurfaceStats.DroppedResLevels);

	UE_LOG(LogRenderer, Log, TEXT("*** Surface cache Bin Allocator ***"));
	for (const FLumenSurfaceCacheAllocator::FBinStats& Bin : AllocatorStats.Bins)
	{
		UE_LOG(LogRenderer, Log, TEXT("  %3d,%3d bin has %5d allocations using %3d pages"), Bin.ElementSize.X, Bin.ElementSize.Y, Bin.NumAllocations, Bin.NumPages);
	}

	UE_LOG(LogRenderer, Log, TEXT("*** CPU Memory ***"));
	UE_LOG(LogRenderer, Log, TEXT("  Primitive groups allocated memory: %.3fMb"), PrimitiveGroupsAllocatedMemory / (1024.0f * 1024.0f));
	UE_LOG(LogRenderer, Log, TEXT("  Cards allocated memory: %.3fMb"), Cards.GetAllocatedSize() / (1024.0f * 1024.0f));
	UE_LOG(LogRenderer, Log, TEXT("  MeshCards allocated memory: %.3fMb"), MeshCards.GetAllocatedSize() / (1024.0f * 1024.0f));

	UE_LOG(LogRenderer, Log, TEXT("*** GPU Memory ***"));
	UE_LOG(LogRenderer, Log, TEXT("  CardBuffer: %.3fMb"), TryGetSize(CardBuffer) / (1024.0f * 1024.0f));
	UE_LOG(LogRenderer, Log, TEXT("  MeshCardsBuffer: %.3fMb"), TryGetSize(MeshCardsBuffer) / (1024.0f * 1024.0f));
	UE_LOG(LogRenderer, Log, TEXT("  PageTable: %.3fMb"), TryGetSize(PageTableBuffer) / (1024.0f * 1024.0f));
	UE_LOG(LogRenderer, Log, TEXT("  CardPages: %.3fMb"), TryGetSize(CardPageBuffer) / (1024.0f * 1024.0f));
	UE_LOG(LogRenderer, Log, TEXT("  SceneInstanceIndexToMeshCardsIndexBuffer: %.3fMb"), TryGetSize(SceneInstanceIndexToMeshCardsIndexBuffer) / (1024.0f * 1024.0f));

	if (bDumpMeshDistanceFields)
	{
		DistanceFieldSceneData.ListMeshDistanceFields(true);
	}

	if (bDumpPrimitiveGroups)
	{
#if STATS
		UE_LOG(LogRenderer, Log, TEXT("*** LumenScene Primitives ***"));

		for (const FLumenPrimitiveGroup& PrimitiveGroup : PrimitiveGroups)
		{
			for (const FPrimitiveSceneInfo* ScenePrimitive : PrimitiveGroup.Primitives)
			{
				if (ScenePrimitive && ScenePrimitive->Proxy)
				{
					UE_LOG(LogRenderer, Log, TEXT("Group:%d InstanceIndex:%d %s"), 
						PrimitiveGroup.RayTracingGroupMapElementId.GetIndex(),
						PrimitiveGroup.PrimitiveInstanceIndex,
						*ScenePrimitive->Proxy->GetStatId().GetName().ToString());
				}
			}
		}
#endif
	}
}