// Copyright Epic Games, Inc. All Rights Reserved.

#include "GLTFMaterialBakingModule.h"
#include "MaterialRenderItem.h"
#include "Engine/TextureRenderTarget2D.h"
#include "GLTFExportMaterialProxy.h"
#include "UObject/UObjectGlobals.h"
#include "GLTFMaterialBakingStructures.h"
#include "GLTFMaterialBakingHelpers.h"
#include "Async/Async.h"
#include "Async/ParallelFor.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceConstant.h"
#include "MaterialEditor/MaterialEditorInstanceConstant.h"
#include "Modules/ModuleManager.h"
#include "RenderingThread.h"
#include "RHISurfaceDataConversion.h"
#include "Misc/ScopedSlowTask.h"
#include "MeshDescription.h"
#include "TextureCompiler.h"
#include "RenderCaptureInterface.h"
#if WITH_EDITOR
#include "Misc/FileHelper.h"
#endif

IMPLEMENT_MODULE(FGLTFMaterialBakingModule, GLTFMaterialBaking);

DEFINE_LOG_CATEGORY_STATIC(LogMaterialBaking, All, All);

#define LOCTEXT_NAMESPACE "MaterialBakingModule"

/** Cvars for advanced features */
static TAutoConsoleVariable<int32> CVarUseMaterialProxyCaching(
	TEXT("GLTFMaterialBaking.UseMaterialProxyCaching"),
	1,
	TEXT("Determines whether or not Material Proxies should be cached to speed up material baking.\n")
	TEXT("0: Turned Off\n")
	TEXT("1: Turned On"),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarSaveIntermediateTextures(
	TEXT("GLTFMaterialBaking.SaveIntermediateTextures"),
	0,
	TEXT("Determines whether or not to save out intermediate BMP images for each flattened material property.\n")
	TEXT("0: Turned Off\n")
	TEXT("1: Turned On"),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarMaterialBakingRDOCCapture(
	TEXT("GLTFMaterialBaking.RenderDocCapture"),
	0,
	TEXT("Determines whether or not to trigger a RenderDoc capture.\n")
	TEXT("0: Turned Off\n")
	TEXT("1: Turned On"),
	ECVF_Default);

namespace FGLTFMaterialBakingModuleImpl
{
	// Custom dynamic mesh allocator specifically tailored for Material Baking.
	// This will always reuse the same couple buffers, so searching linearly is not a problem.
	class FMaterialBakingDynamicMeshBufferAllocator : public FDynamicMeshBufferAllocator
	{
		// This must be smaller than the large allocation blocks on Windows 10 which is currently ~508K.
		// Large allocations uses VirtualAlloc directly without any kind of buffering before
		// releasing pages to the kernel, so it causes lots of soft page fault when
		// memory is first initialized.
		const uint32 SmallestPooledBufferSize = 256*1024;

		TArray<FBufferRHIRef>  IndexBuffers;
		TArray<FBufferRHIRef> VertexBuffers;

		template <typename RefType>
		RefType GetSmallestFit(uint32 SizeInBytes, TArray<RefType>& Array)
		{
			uint32 SmallestFitIndex = UINT32_MAX;
			uint32 SmallestFitSize  = UINT32_MAX;
			for (int32 Index = 0; Index < Array.Num(); ++Index)
			{
				uint32 Size = Array[Index]->GetSize();
				if (Size >= SizeInBytes && (SmallestFitIndex == UINT32_MAX || Size < SmallestFitSize))
				{
					SmallestFitIndex = Index;
					SmallestFitSize  = Size;
				}
			}

			RefType Ref;
			// Do not reuse the smallest fit if it's a lot bigger than what we requested
			if (SmallestFitIndex != UINT32_MAX && SmallestFitSize < SizeInBytes*2)
			{
				Ref = Array[SmallestFitIndex];
				Array.RemoveAtSwap(SmallestFitIndex);
			}

			return Ref;
		}

		virtual FBufferRHIRef AllocIndexBuffer(uint32 NumElements) override
		{
			uint32 BufferSize = GetIndexBufferSize(NumElements);
			if (BufferSize > SmallestPooledBufferSize)
			{
				FBufferRHIRef Ref = GetSmallestFit(GetIndexBufferSize(NumElements), IndexBuffers);
				if (Ref.IsValid())
				{
					return Ref;
				}
			}

			return FDynamicMeshBufferAllocator::AllocIndexBuffer(NumElements);
		}

		virtual void ReleaseIndexBuffer(FBufferRHIRef& IndexBufferRHI) override
		{
			if (IndexBufferRHI->GetSize() > SmallestPooledBufferSize)
			{
				IndexBuffers.Add(MoveTemp(IndexBufferRHI));
			}

			IndexBufferRHI = nullptr;
		}

		virtual FBufferRHIRef AllocVertexBuffer(uint32 Stride, uint32 NumElements) override
		{
			uint32 BufferSize = GetVertexBufferSize(Stride, NumElements);
			if (BufferSize > SmallestPooledBufferSize)
			{
				FBufferRHIRef Ref = GetSmallestFit(BufferSize, VertexBuffers);
				if (Ref.IsValid())
				{
					return Ref;
				}
			}

			return FDynamicMeshBufferAllocator::AllocVertexBuffer(Stride, NumElements);
		}

		virtual void ReleaseVertexBuffer(FBufferRHIRef& VertexBufferRHI) override
		{
			if (VertexBufferRHI->GetSize() > SmallestPooledBufferSize)
			{
				VertexBuffers.Add(MoveTemp(VertexBufferRHI));
			}

			VertexBufferRHI = nullptr;
		}
	};

	class FStagingBufferPool
	{
	public:
		FTexture2DRHIRef CreateStagingBuffer_RenderThread(FRHICommandListImmediate& RHICmdList, int32 Width, int32 Height, EPixelFormat Format, bool bIsSRGB)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(CreateStagingBuffer_RenderThread)

			auto StagingBufferPredicate =
				[Width, Height, Format, bIsSRGB](const FTexture2DRHIRef& Texture2DRHIRef)
				{
					return Texture2DRHIRef->GetSizeX() == Width && Texture2DRHIRef->GetSizeY() == Height && Texture2DRHIRef->GetFormat() == Format && bool(Texture2DRHIRef->GetFlags() & TexCreate_SRGB) == bIsSRGB;
				};

			// Process any staging buffers available for unmapping
			{
				TArray<FTexture2DRHIRef> ToUnmapLocal;
				{
					FScopeLock Lock(&ToUnmapLock);
					ToUnmapLocal = MoveTemp(ToUnmap);
				}

				for (int32 Index = 0, Num = ToUnmapLocal.Num(); Index < Num; ++Index)
				{
					RHICmdList.UnmapStagingSurface(ToUnmapLocal[Index]);
					Pool.Add(MoveTemp(ToUnmapLocal[Index]));
				}
			}

			// Find any pooled staging buffer with suitable properties.
			int32 Index = Pool.IndexOfByPredicate(StagingBufferPredicate);

			if (Index != -1)
			{
				FTexture2DRHIRef StagingBuffer = MoveTemp(Pool[Index]);
				Pool.RemoveAtSwap(Index);
				return StagingBuffer;
			}

			TRACE_CPUPROFILER_EVENT_SCOPE(RHICreateTexture2D)
			FRHIResourceCreateInfo CreateInfo(TEXT("FStagingBufferPool_StagingBuffer"));
			ETextureCreateFlags TextureCreateFlags = TexCreate_CPUReadback;
			if (bIsSRGB)
			{
				TextureCreateFlags |= TexCreate_SRGB;
			}

			return RHICreateTexture(
				FRHITextureCreateDesc::Create2D(CreateInfo.DebugName)
				.SetExtent(Width, Height)
				.SetFormat((EPixelFormat)Format)
				.SetNumMips(1)
				.SetNumSamples(1)
				.SetFlags(TextureCreateFlags)
				.SetInitialState(ERHIAccess::Unknown)
				.SetExtData(CreateInfo.ExtData)
				.SetBulkData(CreateInfo.BulkData)
				.SetGPUMask(CreateInfo.GPUMask)
				.SetClearValue(CreateInfo.ClearValueBinding)
			);
		}

		void ReleaseStagingBufferForUnmap_AnyThread(FTexture2DRHIRef& Texture2DRHIRef)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(ReleaseStagingBufferForUnmap_AnyThread)
			FScopeLock Lock(&ToUnmapLock);
			ToUnmap.Emplace(MoveTemp(Texture2DRHIRef));
		}

		void Clear_RenderThread(FRHICommandListImmediate& RHICmdList)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(Clear_RenderThread)
			for (FTexture2DRHIRef& StagingSurface : ToUnmap)
			{
				RHICmdList.UnmapStagingSurface(StagingSurface);
			}

			ToUnmap.Empty();
			Pool.Empty();
		}

		~FStagingBufferPool()
		{
			check(Pool.Num() == 0);
		}

	private:
		TArray<FTexture2DRHIRef> Pool;

		// Not contented enough to warrant the use of lockless structures.
		FCriticalSection         ToUnmapLock;
		TArray<FTexture2DRHIRef> ToUnmap;
	};

	struct FRenderItemKey
	{
		const FGLTFMeshRenderData* RenderData;
		const FIntPoint  RenderSize;

		FRenderItemKey(const FGLTFMeshRenderData* InRenderData, const FIntPoint& InRenderSize)
			: RenderData(InRenderData)
			, RenderSize(InRenderSize)
		{
		}

		bool operator == (const FRenderItemKey& Other) const
		{
			return RenderData == Other.RenderData &&
				RenderSize == Other.RenderSize;
		}
	};

	uint32 GetTypeHash(const FRenderItemKey& Key)
	{
		return HashCombine(GetTypeHash(Key.RenderData), GetTypeHash(Key.RenderSize));
	}
}

void FGLTFMaterialBakingModule::StartupModule()
{
	bEmissiveHDR = false;

	// Set which properties should enforce gamma correction
	SetLinearBake(true);

	// Set which pixel format should be used for the possible baked out material properties
	PerPropertyFormat.Add(MP_EmissiveColor, PF_FloatRGBA);
	PerPropertyFormat.Add(MP_Opacity, PF_B8G8R8A8);
	PerPropertyFormat.Add(MP_OpacityMask, PF_B8G8R8A8);
	PerPropertyFormat.Add(MP_BaseColor, PF_B8G8R8A8);
	PerPropertyFormat.Add(MP_Metallic, PF_B8G8R8A8);
	PerPropertyFormat.Add(MP_Specular, PF_B8G8R8A8);
	PerPropertyFormat.Add(MP_Roughness, PF_B8G8R8A8);
	PerPropertyFormat.Add(MP_Anisotropy, PF_B8G8R8A8);
	PerPropertyFormat.Add(MP_Normal, PF_B8G8R8A8);
	PerPropertyFormat.Add(MP_Tangent, PF_B8G8R8A8);
	PerPropertyFormat.Add(MP_AmbientOcclusion, PF_B8G8R8A8);
	PerPropertyFormat.Add(MP_SubsurfaceColor, PF_B8G8R8A8);
	PerPropertyFormat.Add(MP_CustomData0, PF_B8G8R8A8);
	PerPropertyFormat.Add(MP_CustomData1, PF_B8G8R8A8);
	PerPropertyFormat.Add(MP_Refraction, PF_B8G8R8A8);
	PerPropertyFormat.Add(MP_ShadingModel, PF_B8G8R8A8);
	PerPropertyFormat.Add(TEXT("ClearCoatBottomNormal"), PF_B8G8R8A8);
	PerPropertyFormat.Add(TEXT("TransmittanceColor"), PF_B8G8R8A8);

	// Register callback for modified objects
	FCoreUObjectDelegates::OnObjectModified.AddRaw(this, &FGLTFMaterialBakingModule::OnObjectModified);

	// Register callback on garbage collection
	FCoreUObjectDelegates::GetPreGarbageCollectDelegate().AddRaw(this, &FGLTFMaterialBakingModule::OnPreGarbageCollect);
}

void FGLTFMaterialBakingModule::ShutdownModule()
{
	// Unregister customization and callback
	FPropertyEditorModule* PropertyEditorModule = FModuleManager::GetModulePtr<FPropertyEditorModule>("PropertyEditor");

	if (PropertyEditorModule)
	{
		PropertyEditorModule->UnregisterCustomPropertyTypeLayout(TEXT("PropertyEntry"));
	}

	FCoreUObjectDelegates::OnObjectModified.RemoveAll(this);
	FCoreUObjectDelegates::GetPreGarbageCollectDelegate().RemoveAll(this);

	CleanupMaterialProxies();
}

void FGLTFMaterialBakingModule::BakeMaterials(const TArray<FGLTFMaterialData*>& MaterialSettings, const TArray<FGLTFMeshRenderData*>& MeshSettings, TArray<FGLTFBakeOutput>& Output)
{
	// Translate old material data to extended types
	TArray<FGLTFMaterialDataEx> MaterialDataExs;
	MaterialDataExs.Reserve(MaterialSettings.Num());
	for (const FGLTFMaterialData* MaterialData : MaterialSettings)
	{
		FGLTFMaterialDataEx& MaterialDataEx = MaterialDataExs.AddDefaulted_GetRef();
		MaterialDataEx.Material = MaterialData->Material;
		MaterialDataEx.bPerformBorderSmear = MaterialData->bPerformBorderSmear;
		MaterialDataEx.BlendMode = MaterialData->BlendMode;
		MaterialDataEx.bTangentSpaceNormal = MaterialData->bTangentSpaceNormal;
		MaterialDataEx.BackgroundColor = MaterialData->BackgroundColor;

		for (const TPair<EMaterialProperty, FIntPoint>& PropertySizePair : MaterialData->PropertySizes)
		{
			MaterialDataEx.PropertySizes.Add(PropertySizePair.Key, PropertySizePair.Value);
		}
	}

	// Build an array of pointers to the extended type
	TArray<FGLTFMaterialDataEx*> MaterialSettingsEx;
	MaterialSettingsEx.Reserve(MaterialDataExs.Num());
	for (FGLTFMaterialDataEx& MaterialDataEx : MaterialDataExs)
	{
		MaterialSettingsEx.Add(&MaterialDataEx);
	}

	TArray<FGLTFBakeOutputEx> BakeOutputExs;
	BakeMaterials(MaterialSettingsEx, MeshSettings, BakeOutputExs);

	// Translate extended bake output to old types
	Output.Reserve(BakeOutputExs.Num());
	for (FGLTFBakeOutputEx& BakeOutputEx : BakeOutputExs)
	{
		FGLTFBakeOutput& BakeOutput = Output.AddDefaulted_GetRef();
		BakeOutput.EmissiveScale = BakeOutputEx.EmissiveScale;

		for (TPair<FGLTFMaterialPropertyEx, FIntPoint>& PropertySizePair : BakeOutputEx.PropertySizes)
		{
			BakeOutput.PropertySizes.Add(PropertySizePair.Key.Type, PropertySizePair.Value);
		}

		for (TPair<FGLTFMaterialPropertyEx, TArray<FColor>>& PropertyDataPair : BakeOutputEx.PropertyData)
		{
			BakeOutput.PropertyData.Add(PropertyDataPair.Key.Type, MoveTemp(PropertyDataPair.Value));
		}

		for (TPair<FGLTFMaterialPropertyEx, TArray<FFloat16Color>>& PropertyDataPair : BakeOutputEx.HDRPropertyData)
		{
			BakeOutput.HDRPropertyData.Add(PropertyDataPair.Key.Type, MoveTemp(PropertyDataPair.Value));
		}
	}
}

void FGLTFMaterialBakingModule::BakeMaterials(const TArray<FGLTFMaterialDataEx*>& MaterialSettings, const TArray<FGLTFMeshRenderData*>& MeshSettings, TArray<FGLTFBakeOutputEx>& Output)
{
	UE_LOG(LogMaterialBaking, Verbose, TEXT("Performing material baking for %d materials"), MaterialSettings.Num());
	for (int32 i = 0; i < MaterialSettings.Num(); i++)
	{
		if (MaterialSettings[i]->Material && MeshSettings[i]->MeshDescription)
		{
			UE_LOG(LogMaterialBaking, Verbose, TEXT("    [%5d] Material: %-50s Vertices: %8d    Triangles: %8d"), i, *MaterialSettings[i]->Material->GetName(), MeshSettings[i]->MeshDescription->Vertices().Num(), MeshSettings[i]->MeshDescription->Triangles().Num());
		}
	}

	RenderCaptureInterface::FScopedCapture RenderCapture(CVarMaterialBakingRDOCCapture.GetValueOnAnyThread() == 1, TEXT("MaterialBaking"));

	TRACE_CPUPROFILER_EVENT_SCOPE(FMaterialBakingModule::BakeMaterials)

	checkf(MaterialSettings.Num() == MeshSettings.Num(), TEXT("Number of material settings does not match that of MeshSettings"));
	const int32 NumMaterials = MaterialSettings.Num();
	const bool bSaveIntermediateTextures = CVarSaveIntermediateTextures.GetValueOnAnyThread() == 1;

	using namespace FGLTFMaterialBakingModuleImpl;
	FMaterialBakingDynamicMeshBufferAllocator MaterialBakingDynamicMeshBufferAllocator;

	FScopedSlowTask Progress(NumMaterials, LOCTEXT("BakeMaterials", "Baking Materials..."), true );
	Progress.MakeDialog(true);

	TArray<uint32> ProcessingOrder;
	ProcessingOrder.Reserve(MeshSettings.Num());
	for (int32 Index = 0; Index < MeshSettings.Num(); ++Index)
	{
		ProcessingOrder.Add(Index);
	}

	// Start with the biggest mesh first so we can always reuse the same vertex/index buffers.
	// This will decrease the number of allocations backed by newly allocated memory from the OS,
	// which will reduce soft page faults while copying into that memory.
	// Soft page faults are now incredibly expensive on Windows 10.
	Algo::SortBy(
		ProcessingOrder,
		[&MeshSettings](const uint32 Index){ return MeshSettings[Index]->MeshDescription ? MeshSettings[Index]->MeshDescription->Vertices().Num() : 0; },
		TGreater<>()
	);

	Output.SetNum(NumMaterials);

	struct FPipelineContext
	{
		typedef TFunction<void (FRHICommandListImmediate& RHICmdList)> FReadCommand;
		FReadCommand ReadCommand;
	};

	// Distance between the command sent to rendering and the GPU read-back of the result
	// to minimize sync time waiting on GPU.
	const int32 PipelineDepth = 16;
	int32 PipelineIndex = 0;
	FPipelineContext PipelineContext[PipelineDepth];

	// This will create and prepare FMeshMaterialRenderItem for each property sizes we're going to need
	auto PrepareRenderItems_AnyThread =
		[&](int32 MaterialIndex)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PrepareRenderItems);

		TMap<FGLTFMaterialBakingModuleImpl::FRenderItemKey, FGLTFMeshMaterialRenderItem*>* RenderItems = new TMap<FRenderItemKey, FGLTFMeshMaterialRenderItem *>();
		const FGLTFMaterialDataEx* CurrentMaterialSettings = MaterialSettings[MaterialIndex];
		const FGLTFMeshRenderData* CurrentMeshSettings = MeshSettings[MaterialIndex];

		for (TMap<FGLTFMaterialPropertyEx, FIntPoint>::TConstIterator PropertySizeIterator = CurrentMaterialSettings->PropertySizes.CreateConstIterator(); PropertySizeIterator; ++PropertySizeIterator)
		{
			FRenderItemKey RenderItemKey(CurrentMeshSettings, PropertySizeIterator.Value());
			if (RenderItems->Find(RenderItemKey) == nullptr)
			{
				RenderItems->Add(RenderItemKey, new FGLTFMeshMaterialRenderItem(PropertySizeIterator.Value(), CurrentMeshSettings, &MaterialBakingDynamicMeshBufferAllocator));
			}
		}

		return RenderItems;
	};

	// We reuse the pipeline depth to prepare render items in advance to avoid stalling the game thread
	int NextRenderItem = 0;
	TFuture<TMap<FRenderItemKey, FGLTFMeshMaterialRenderItem*>*> PreparedRenderItems[PipelineDepth];
	for (; NextRenderItem < NumMaterials && NextRenderItem < PipelineDepth; ++NextRenderItem)
	{
		PreparedRenderItems[NextRenderItem] =
			Async(
				EAsyncExecution::ThreadPool,
				[&PrepareRenderItems_AnyThread, &ProcessingOrder, NextRenderItem]()
				{
					return PrepareRenderItems_AnyThread(ProcessingOrder[NextRenderItem]);
				}
			);
	}

	// Create all material proxies right away to start compiling shaders asynchronously and avoid stalling the baking process as much as possible
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(CreateMaterialProxies)

		for (int32 Index = 0; Index < NumMaterials; ++Index)
		{
			int32 MaterialIndex = ProcessingOrder[Index];
			const FGLTFMaterialDataEx* CurrentMaterialSettings = MaterialSettings[MaterialIndex];

			TArray<UTexture*> MaterialTextures;
			CurrentMaterialSettings->Material->GetUsedTextures(MaterialTextures, EMaterialQualityLevel::Num, true, GMaxRHIFeatureLevel, true);

			// Force load materials used by the current material
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(LoadTexturesForMaterial)

				FTextureCompilingManager::Get().FinishCompilation(MaterialTextures);

				for (UTexture* Texture : MaterialTextures)
				{
					if (Texture != NULL)
					{
						UTexture2D* Texture2D = Cast<UTexture2D>(Texture);
						if (Texture2D)
						{
							Texture2D->SetForceMipLevelsToBeResident(30.0f);
							Texture2D->WaitForStreaming();
						}
					}
				}
			}

			for (TMap<FGLTFMaterialPropertyEx, FIntPoint>::TConstIterator PropertySizeIterator = CurrentMaterialSettings->PropertySizes.CreateConstIterator(); PropertySizeIterator; ++PropertySizeIterator)
			{
				// They will be stored in the pool and compiled asynchronously
				CreateMaterialProxy(CurrentMaterialSettings, PropertySizeIterator.Key());
			}
		}
	}

	TAtomic<uint32>    NumTasks(0);
	FStagingBufferPool StagingBufferPool;

	for (int32 Index = 0; Index < NumMaterials; ++Index)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(BakeOneMaterial)

		Progress.EnterProgressFrame(1.0f, FText::Format(LOCTEXT("BakingMaterial", "Baking Material {0}/{1}"), Index, NumMaterials));

		int32 MaterialIndex = ProcessingOrder[Index];
		TMap<FRenderItemKey, FGLTFMeshMaterialRenderItem*>* RenderItems;

		{
			TRACE_CPUPROFILER_EVENT_SCOPE(WaitOnPreparedRenderItems)
			RenderItems = PreparedRenderItems[Index % PipelineDepth].Get();
		}

		// Prepare the next render item in advance
		if (NextRenderItem < NumMaterials)
		{
			check((NextRenderItem % PipelineDepth) == (Index % PipelineDepth));
			PreparedRenderItems[NextRenderItem % PipelineDepth] =
				Async(
					EAsyncExecution::ThreadPool,
					[&PrepareRenderItems_AnyThread, NextMaterialIndex = ProcessingOrder[NextRenderItem]]()
					{
						return PrepareRenderItems_AnyThread(NextMaterialIndex);
					}
				);
			NextRenderItem++;
		}

		const FGLTFMaterialDataEx* CurrentMaterialSettings = MaterialSettings[MaterialIndex];
		const FGLTFMeshRenderData* CurrentMeshSettings = MeshSettings[MaterialIndex];
		FGLTFBakeOutputEx& CurrentOutput = Output[MaterialIndex];

		TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*CurrentMaterialSettings->Material->GetName())

		TArray<FGLTFMaterialPropertyEx> MaterialPropertiesToBakeOut;
		CurrentMaterialSettings->PropertySizes.GenerateKeyArray(MaterialPropertiesToBakeOut);

		const int32 NumPropertiesToRender = MaterialPropertiesToBakeOut.Num();
		if (NumPropertiesToRender > 0)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(RenderOneMaterial)

			// Ensure data in memory will not change place passed this point to avoid race conditions
			CurrentOutput.PropertySizes = CurrentMaterialSettings->PropertySizes;
			for (int32 PropertyIndex = 0; PropertyIndex < NumPropertiesToRender; ++PropertyIndex)
			{
				const FGLTFMaterialPropertyEx& Property = MaterialPropertiesToBakeOut[PropertyIndex];
				CurrentOutput.PropertyData.Add(Property);
				if (bEmissiveHDR && Property == MP_EmissiveColor)
				{
					CurrentOutput.HDRPropertyData.Add(Property);
				}
			}

			for (int32 PropertyIndex = 0; PropertyIndex < NumPropertiesToRender; ++PropertyIndex)
			{
				const FGLTFMaterialPropertyEx& Property = MaterialPropertiesToBakeOut[PropertyIndex];

				TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*Property.ToString())

				FGLTFExportMaterialProxy* ExportMaterialProxy = CreateMaterialProxy(CurrentMaterialSettings, Property);

				if (!ExportMaterialProxy->IsCompilationFinished())
				{
					TRACE_CPUPROFILER_EVENT_SCOPE(WaitForMaterialProxyCompilation)
					ExportMaterialProxy->FinishCompilation();
				}

				// Lookup gamma and format settings for property, if not found use default values
				const EPropertyColorSpace* OverrideColorSpace = PerPropertyColorSpace.Find(Property);
				const EPropertyColorSpace ColorSpace = OverrideColorSpace ? *OverrideColorSpace : DefaultColorSpace;
				const EPixelFormat PixelFormat = PerPropertyFormat.Contains(Property) ? PerPropertyFormat[Property] : PF_B8G8R8A8;

				// It is safe to reuse the same render target for each draw pass since they all execute sequentially on the GPU and are copied to staging buffers before
				// being reused.
				UTextureRenderTarget2D* RenderTarget = CreateRenderTarget((ColorSpace == EPropertyColorSpace::Linear), PixelFormat, CurrentOutput.PropertySizes[Property], CurrentMaterialSettings->BackgroundColor);
				if (RenderTarget != nullptr)
				{
					// Perform everything left of the operation directly on the render thread since we need to modify some RenderItem's properties
					// for each render pass and we can't do that without costly synchronization (flush) between the game thread and render thread.
					// Everything slow to execute has already been prepared on the game thread anyway.
					ENQUEUE_RENDER_COMMAND(RenderOneMaterial)(
						[this, RenderItems, RenderTarget, Property, ExportMaterialProxy, &PipelineContext, PipelineIndex, &StagingBufferPool, &NumTasks, bSaveIntermediateTextures, &MaterialSettings, &MeshSettings, MaterialIndex, &Output](FRHICommandListImmediate& RHICmdList)
						{
							const FGLTFMaterialDataEx* CurrentMaterialSettings = MaterialSettings[MaterialIndex];
							const FGLTFMeshRenderData* CurrentMeshSettings = MeshSettings[MaterialIndex];

							FGLTFMeshMaterialRenderItem& RenderItem = *RenderItems->FindChecked(FRenderItemKey(CurrentMeshSettings, FIntPoint(RenderTarget->GetSurfaceWidth(), RenderTarget->GetSurfaceHeight())));

							FSceneViewFamily ViewFamily(FSceneViewFamily::ConstructionValues(RenderTarget->GetRenderTargetResource(), nullptr,
								FEngineShowFlags(ESFIM_Game))
								.SetTime(FGameTime())
								.SetGammaCorrection(RenderTarget->GetRenderTargetResource()->GetDisplayGamma()));

							RenderItem.MaterialRenderProxy = ExportMaterialProxy;
							RenderItem.ViewFamily = &ViewFamily;

							FTextureRenderTargetResource* RenderTargetResource = RenderTarget->GetRenderTargetResource();
							FCanvas Canvas(RenderTargetResource, nullptr, FGameTime::GetTimeSinceAppStart(), GMaxRHIFeatureLevel);
							Canvas.SetAllowedModes(FCanvas::Allow_Flush);
							Canvas.SetRenderTargetRect(FIntRect(0, 0, RenderTarget->GetSurfaceWidth(), RenderTarget->GetSurfaceHeight()));
							Canvas.SetBaseTransform(Canvas.CalcBaseTransform2D(RenderTarget->GetSurfaceWidth(), RenderTarget->GetSurfaceHeight()));

							// Do rendering
							Canvas.Clear(RenderTarget->ClearColor);
							FCanvas::FCanvasSortElement& SortElement = Canvas.GetSortElement(Canvas.TopDepthSortKey());
							SortElement.RenderBatchArray.Add(&RenderItem);
							Canvas.Flush_RenderThread(RHICmdList);
							SortElement.RenderBatchArray.Empty();

							FTexture2DRHIRef StagingBufferRef = StagingBufferPool.CreateStagingBuffer_RenderThread(RHICmdList, RenderTargetResource->GetSizeX(), RenderTargetResource->GetSizeY(), RenderTarget->GetFormat(), RenderTarget->IsSRGB());
							FGPUFenceRHIRef GPUFence = RHICreateGPUFence(TEXT("MaterialBackingFence"));

							FResolveRect Rect(0, 0, RenderTargetResource->GetSizeX(), RenderTargetResource->GetSizeY());
							PRAGMA_DISABLE_DEPRECATION_WARNINGS
							RHICmdList.CopyToResolveTarget(RenderTargetResource->GetRenderTargetTexture(), StagingBufferRef, FResolveParams(Rect));
							PRAGMA_ENABLE_DEPRECATION_WARNINGS
							RHICmdList.WriteGPUFence(GPUFence);

							// Prepare a lambda for final processing that will be executed asynchronously
							NumTasks++;
							auto FinalProcessing_AnyThread =
								[&NumTasks, bSaveIntermediateTextures, CurrentMaterialSettings, &StagingBufferPool, &Output, Property, MaterialIndex, bEmissiveHDR = bEmissiveHDR](FTexture2DRHIRef& StagingBuffer, void * Data, int32 DataWidth, int32 DataHeight)
								{
									TRACE_CPUPROFILER_EVENT_SCOPE(FinalProcessing)

									FGLTFBakeOutputEx& CurrentOutput  = Output[MaterialIndex];
									TArray<FColor>& OutputColor = CurrentOutput.PropertyData[Property];
									FIntPoint& OutputSize       = CurrentOutput.PropertySizes[Property];

									OutputColor.SetNum(OutputSize.X * OutputSize.Y);

									if (Property.Type == MP_EmissiveColor)
									{
										// Only one thread will write to CurrentOutput.EmissiveScale since there can be only one emissive channel property per FBakeOutputEx
										FGLTFMaterialBakingModule::ProcessEmissiveOutput((const FFloat16Color*)Data, DataWidth, OutputSize, OutputColor, CurrentOutput.EmissiveScale, CurrentMaterialSettings->BackgroundColor);

										if (bEmissiveHDR)
										{
											TArray<FFloat16Color>& OutputHDRColor = CurrentOutput.HDRPropertyData[Property];
											OutputHDRColor.SetNum(OutputSize.X * OutputSize.Y);
											ConvertRawR16G16B16A16FDataToFFloat16Color(OutputSize.X, OutputSize.Y, (uint8*)Data, DataWidth * sizeof(FFloat16Color), OutputHDRColor.GetData());
										}
									}
									else
									{
										TRACE_CPUPROFILER_EVENT_SCOPE(ConvertRawB8G8R8A8DataToFColor)

										check(StagingBuffer->GetFormat() == PF_B8G8R8A8);
										ConvertRawB8G8R8A8DataToFColor(OutputSize.X, OutputSize.Y, (uint8*)Data, DataWidth * sizeof(FColor), OutputColor.GetData());
									}

									// We can't unmap ourself since we're not on the render thread
									StagingBufferPool.ReleaseStagingBufferForUnmap_AnyThread(StagingBuffer);

									if (CurrentMaterialSettings->bPerformBorderSmear)
									{
										// This will resize the output to a single pixel if the result is monochrome.
										FGLTFMaterialBakingHelpers::PerformUVBorderSmearAndShrink(OutputColor, OutputSize.X, OutputSize.Y, CurrentMaterialSettings->BackgroundColor);
									}
#if WITH_EDITOR
									// If saving intermediates is turned on
									if (bSaveIntermediateTextures)
									{
										TRACE_CPUPROFILER_EVENT_SCOPE(SaveIntermediateTextures)
										FString TrimmedPropertyName = Property.ToString();

										const FString DirectoryPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectIntermediateDir() + TEXT("MaterialBaking/"));
										FString FilenameString = FString::Printf(TEXT("%s%s-%d-%s.bmp"), *DirectoryPath, *CurrentMaterialSettings->Material->GetName(), MaterialIndex, *TrimmedPropertyName);
										FFileHelper::CreateBitmap(*FilenameString, CurrentOutput.PropertySizes[Property].X, CurrentOutput.PropertySizes[Property].Y, CurrentOutput.PropertyData[Property].GetData());
									}
#endif // WITH_EDITOR
									NumTasks--;
								};

							// Run previous command if we're going to overwrite it meaning pipeline depth has been reached
							if (PipelineContext[PipelineIndex].ReadCommand)
							{
								PipelineContext[PipelineIndex].ReadCommand(RHICmdList);
							}

							// Generate a texture reading command that will be executed once it reaches the end of the pipeline
							PipelineContext[PipelineIndex].ReadCommand =
								[FinalProcessing_AnyThread, StagingBufferRef = MoveTemp(StagingBufferRef), GPUFence = MoveTemp(GPUFence)](FRHICommandListImmediate& RHICmdList) mutable
								{
									TRACE_CPUPROFILER_EVENT_SCOPE(MapAndEnqueue)

									void * Data = nullptr;
									int32 Width; int32 Height;
									RHICmdList.MapStagingSurface(StagingBufferRef, GPUFence.GetReference(), Data, Width, Height);

									// Schedule the copy and processing on another thread to free up the render thread as much as possible
									Async(
										EAsyncExecution::ThreadPool,
										[FinalProcessing_AnyThread, Data, Width, Height, StagingBufferRef = MoveTemp(StagingBufferRef)]() mutable
										{
											FinalProcessing_AnyThread(StagingBufferRef, Data, Width, Height);
										}
									);
								};
						}
					);

					PipelineIndex = (PipelineIndex + 1) % PipelineDepth;
				}
			}

		}

		// Destroying Render Items must happen on the render thread to ensure
		// they are not used anymore.
		ENQUEUE_RENDER_COMMAND(DestroyRenderItems)(
			[RenderItems](FRHICommandListImmediate& RHICmdList)
			{
				for (auto RenderItem : (*RenderItems))
				{
					delete RenderItem.Value;
				}

				delete RenderItems;
			}
		);
	}

	ENQUEUE_RENDER_COMMAND(ProcessRemainingReads)(
		[&PipelineContext, PipelineDepth, PipelineIndex](FRHICommandListImmediate& RHICmdList)
		{
			// Enqueue remaining reads
			for (int32 Index = 0; Index < PipelineDepth; Index++)
			{
				int32 LocalPipelineIndex = (PipelineIndex + Index) % PipelineDepth;

				if (PipelineContext[LocalPipelineIndex].ReadCommand)
				{
					PipelineContext[LocalPipelineIndex].ReadCommand(RHICmdList);
				}
			}
		}
	);

	// Wait until every tasks have been queued so that NumTasks is only decreasing
	FlushRenderingCommands();

	// Wait for any remaining final processing tasks
	while (NumTasks.Load(EMemoryOrder::Relaxed) > 0)
	{
		FPlatformProcess::Sleep(0.1f);
	}

	// Wait for all tasks to have been processed before clearing the staging buffers
	FlushRenderingCommands();

	ENQUEUE_RENDER_COMMAND(ClearStagingBufferPool)(
		[&StagingBufferPool](FRHICommandListImmediate& RHICmdList)
		{
			StagingBufferPool.Clear_RenderThread(RHICmdList);
		}
	);

	// Wait for StagingBufferPool clear to have executed before exiting the function
	FlushRenderingCommands();

	if (!CVarUseMaterialProxyCaching.GetValueOnAnyThread())
	{
		CleanupMaterialProxies();
	}
}

void FGLTFMaterialBakingModule::SetEmissiveHDR(bool bHDR)
{
	bEmissiveHDR = bHDR;
}

void FGLTFMaterialBakingModule::SetLinearBake(bool bCorrectLinear)
{
	// PerPropertyGamma ultimately sets whether the render target is linear
	PerPropertyColorSpace.Reset();
	if (bCorrectLinear)
	{
		DefaultColorSpace = EPropertyColorSpace::Linear;
		PerPropertyColorSpace.Add(MP_BaseColor, EPropertyColorSpace::sRGB);
		PerPropertyColorSpace.Add(MP_EmissiveColor, EPropertyColorSpace::sRGB);
		PerPropertyColorSpace.Add(MP_SubsurfaceColor, EPropertyColorSpace::sRGB);
		PerPropertyColorSpace.Add(TEXT("TransmittanceColor"), EPropertyColorSpace::sRGB);
	}
	else
	{
		DefaultColorSpace = EPropertyColorSpace::sRGB;
		PerPropertyColorSpace.Add(MP_Normal, EPropertyColorSpace::Linear);
		PerPropertyColorSpace.Add(MP_Opacity, EPropertyColorSpace::Linear);
		PerPropertyColorSpace.Add(MP_OpacityMask, EPropertyColorSpace::Linear);
		PerPropertyColorSpace.Add(MP_Refraction, EPropertyColorSpace::Linear);
		PerPropertyColorSpace.Add(MP_ShadingModel, EPropertyColorSpace::Linear);
		PerPropertyColorSpace.Add(TEXT("ClearCoatBottomNormal"), EPropertyColorSpace::Linear);
	}
}

bool FGLTFMaterialBakingModule::IsLinearBake(FGLTFMaterialPropertyEx Property)
{
	const EPropertyColorSpace* OverrideColorSpace = PerPropertyColorSpace.Find(Property);
	const EPropertyColorSpace ColorSpace = OverrideColorSpace ? *OverrideColorSpace : DefaultColorSpace;
	return ColorSpace == EPropertyColorSpace::Linear;
}

static void DeleteCachedMaterialProxy(FGLTFExportMaterialProxy* Proxy)
{
	ENQUEUE_RENDER_COMMAND(DeleteCachedMaterialProxy)(
		[Proxy](FRHICommandListImmediate& RHICmdList)
		{
			delete Proxy;
		});
}

void FGLTFMaterialBakingModule::CleanupMaterialProxies()
{
	for (auto Iterator : MaterialProxyPool)
	{
		DeleteCachedMaterialProxy(Iterator.Value.Value);
	}
	MaterialProxyPool.Reset();
}

UTextureRenderTarget2D* FGLTFMaterialBakingModule::CreateRenderTarget(bool bInForceLinearGamma, EPixelFormat InPixelFormat, const FIntPoint& InTargetSize, const FColor& BackgroundColor)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FMaterialBakingModule::CreateRenderTarget)

	UTextureRenderTarget2D* RenderTarget = nullptr;
	const int32 MaxTextureSize = 1 << (MAX_TEXTURE_MIP_COUNT - 1); // Don't use GetMax2DTextureDimension() as this is for the RHI only.
	const FIntPoint ClampedTargetSize(FMath::Clamp(InTargetSize.X, 1, MaxTextureSize), FMath::Clamp(InTargetSize.Y, 1, MaxTextureSize));
	auto RenderTargetComparison = [bInForceLinearGamma, InPixelFormat, ClampedTargetSize](const UTextureRenderTarget2D* CompareRenderTarget) -> bool
	{
		return (CompareRenderTarget->SizeX == ClampedTargetSize.X && CompareRenderTarget->SizeY == ClampedTargetSize.Y && CompareRenderTarget->OverrideFormat == InPixelFormat && CompareRenderTarget->bForceLinearGamma == bInForceLinearGamma);
	};

	// Find any pooled render target with suitable properties.
	UTextureRenderTarget2D** FindResult = RenderTargetPool.FindByPredicate(RenderTargetComparison);

	if (FindResult)
	{
		RenderTarget = *FindResult;
	}
	else
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(CreateNewRenderTarget)

		// Not found - create a new one.
		RenderTarget = NewObject<UTextureRenderTarget2D>();
		check(RenderTarget);
		RenderTarget->AddToRoot();
		RenderTarget->ClearColor = bInForceLinearGamma ? BackgroundColor.ReinterpretAsLinear() : FLinearColor(BackgroundColor);
		RenderTarget->TargetGamma = 0.0f;
		RenderTarget->InitCustomFormat(ClampedTargetSize.X, ClampedTargetSize.Y, InPixelFormat, bInForceLinearGamma);

		RenderTargetPool.Add(RenderTarget);
	}

	checkf(RenderTarget != nullptr, TEXT("Unable to create or find valid render target"));
	return RenderTarget;
}

FGLTFExportMaterialProxy* FGLTFMaterialBakingModule::CreateMaterialProxy(const FGLTFMaterialDataEx* MaterialSettings, const FGLTFMaterialPropertyEx& Property)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FMaterialBakingModule::CreateMaterialProxy)

	FGLTFExportMaterialProxy* Proxy = nullptr;

	// Find all pooled material proxy matching this material
	TArray<FMaterialPoolValue> Entries;
	MaterialProxyPool.MultiFind(MaterialSettings->Material, Entries);

	// Look for the matching property
	for (FMaterialPoolValue& Entry : Entries)
	{
		if (Entry.Key == Property && Entry.Value->ProxyBlendMode == MaterialSettings->BlendMode && Entry.Value->bTangentSpaceNormal == MaterialSettings->bTangentSpaceNormal)
		{
			Proxy = Entry.Value;
			break;
		}
	}

	// Not found, create a new entry
	if (Proxy == nullptr)
	{
		Proxy = new FGLTFExportMaterialProxy(MaterialSettings->Material, Property.Type, Property.CustomOutput.ToString(), false /* bInSynchronousCompilation */, MaterialSettings->BlendMode, MaterialSettings->bTangentSpaceNormal);
		MaterialProxyPool.Add(MaterialSettings->Material, FMaterialPoolValue(Property, Proxy));
	}

	return Proxy;
}

void FGLTFMaterialBakingModule::ProcessEmissiveOutput(const FFloat16Color* Color16, int32 Color16Pitch, const FIntPoint& OutputSize, TArray<FColor>& OutputColor, float& EmissiveScale, const FColor& BackgroundColor)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FMaterialBakingModule::ProcessEmissiveOutput)

	const int32 NumThreads = [&]()
	{
		return FPlatformProcess::SupportsMultithreading() ? FPlatformMisc::NumberOfCores() : 1;
	}();

	float* MaxValue = new float[NumThreads];
	FMemory::Memset(MaxValue, 0, NumThreads * sizeof(MaxValue[0]));
	const int32 LinesPerThread = FMath::CeilToInt((float)OutputSize.Y / (float)NumThreads);
	const FFloat16Color BackgroundFloat16 = FFloat16Color(FLinearColor(BackgroundColor)); // Can assume emissive always uses sRGB

	// Find maximum float value across texture
	ParallelFor(NumThreads, [&Color16, LinesPerThread, MaxValue, OutputSize, Color16Pitch, BackgroundFloat16](int32 Index)
	{
		const int32 EndY = FMath::Min((Index + 1) * LinesPerThread, OutputSize.Y);
		float& CurrentMaxValue = MaxValue[Index];
		for (int32 PixelY = Index * LinesPerThread; PixelY < EndY; ++PixelY)
		{
			const int32 SrcYOffset = PixelY * Color16Pitch;
			for (int32 PixelX = 0; PixelX < OutputSize.X; PixelX++)
			{
				const FFloat16Color& Pixel16 = Color16[PixelX + SrcYOffset];
				// Find maximum channel value across texture
				if (!(Pixel16 == BackgroundFloat16))
				{
					CurrentMaxValue = FMath::Max(CurrentMaxValue, FMath::Max3(Pixel16.R.GetFloat(), Pixel16.G.GetFloat(), Pixel16.B.GetFloat()));
				}
			}
		}
	});

	const float GlobalMaxValue = [&MaxValue, NumThreads]
	{
		float TempValue = 0.0f;
		for (int32 ThreadIndex = 0; ThreadIndex < NumThreads; ++ThreadIndex)
		{
			TempValue = FMath::Max(TempValue, MaxValue[ThreadIndex]);
		}

		return TempValue;
	}();

	if (GlobalMaxValue <= 0.01f)
	{
		// Black emissive, drop it
	}

	// Now convert Float16 to Color using the scale
	OutputColor.SetNumUninitialized(OutputSize.X * OutputSize.Y);
	const float Scale = 255.0f / GlobalMaxValue;
	ParallelFor(NumThreads, [&Color16, LinesPerThread, &OutputColor, OutputSize, Color16Pitch, Scale, BackgroundFloat16, BackgroundColor](int32 Index)
	{
		const int32 EndY = FMath::Min((Index + 1) * LinesPerThread, OutputSize.Y);
		for (int32 PixelY = Index * LinesPerThread; PixelY < EndY; ++PixelY)
		{
			const int32 SrcYOffset = PixelY * Color16Pitch;
			const int32 DstYOffset = PixelY * OutputSize.X;

			for (int32 PixelX = 0; PixelX < OutputSize.X; PixelX++)
			{
				const FFloat16Color& Pixel16 = Color16[PixelX + SrcYOffset];
				FColor& Pixel8 = OutputColor[PixelX + DstYOffset];

				if (Pixel16 == BackgroundFloat16)
				{
					Pixel8 = BackgroundColor;
				}
				else
				{
					Pixel8.R = (uint8)FMath::RoundToInt(Pixel16.R.GetFloat() * Scale);
					Pixel8.G = (uint8)FMath::RoundToInt(Pixel16.G.GetFloat() * Scale);
					Pixel8.B = (uint8)FMath::RoundToInt(Pixel16.B.GetFloat() * Scale);
				}

				Pixel8.A = 255;
			}
		}
	});

	// This scale will be used in the proxy material to get the original range of emissive values outside of 0-1
	EmissiveScale = GlobalMaxValue;
}

void FGLTFMaterialBakingModule::OnObjectModified(UObject* Object)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FMaterialBakingModule::OnObjectModified)

	if (CVarUseMaterialProxyCaching.GetValueOnAnyThread())
	{
		UMaterialInterface* MaterialToInvalidate = Cast<UMaterialInterface>(Object);
		if (!MaterialToInvalidate)
		{
			// Check to see if the object is a material editor instance constant and if so, retrieve its source instance
			UMaterialEditorInstanceConstant* EditorInstance = Cast<UMaterialEditorInstanceConstant>(Object);
			if (EditorInstance && EditorInstance->SourceInstance)
			{
				MaterialToInvalidate = EditorInstance->SourceInstance;
			}
		}

		if (MaterialToInvalidate)
		{
			// Search our proxy pool for materials or material instances that refer to MaterialToInvalidate
			for (auto It = MaterialProxyPool.CreateIterator(); It; ++It)
			{
				TWeakObjectPtr<UMaterialInterface> PoolMaterialPtr = It.Key();

				// Remove stale entries from the pool
				bool bMustDelete = PoolMaterialPtr.IsValid();
				if (!bMustDelete)
				{
					bMustDelete = PoolMaterialPtr == MaterialToInvalidate;
				}

				// No match - Test the MaterialInstance hierarchy
				if (!bMustDelete)
				{
					UMaterialInstance* MaterialInstance = Cast<UMaterialInstance>(PoolMaterialPtr);
					while (!bMustDelete && MaterialInstance && MaterialInstance->Parent != nullptr)
					{
						bMustDelete = MaterialInstance->Parent == MaterialToInvalidate;
						MaterialInstance = Cast<UMaterialInstance>(MaterialInstance->Parent);
					}
				}

				// We have a match, remove the entry from our pool
				if (bMustDelete)
				{
					DeleteCachedMaterialProxy(It.Value().Value);
					It.RemoveCurrent();
				}
			}
		}
	}
}

void FGLTFMaterialBakingModule::OnPreGarbageCollect()
{
	CleanupMaterialProxies();
}

#undef LOCTEXT_NAMESPACE //"MaterialBakingModule"
