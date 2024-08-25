// Copyright Epic Games, Inc. All Rights Reserved.

#include "LumenSurfaceCacheFeedback.h"
#include "SceneRendering.h"
#include "DeferredShadingRenderer.h"
#include "LumenSceneData.h"
#include "Lumen.h"
#include "LumenReflections.h"
#include "LumenVisualize.h"
#include "ScenePrivate.h"

int32 GLumenSurfaceCacheFeedback = 1;
FAutoConsoleVariableRef CVarLumenSurfaceCacheFeedback(
	TEXT("r.LumenScene.SurfaceCache.Feedback"),
	GLumenSurfaceCacheFeedback,
	TEXT("Whether to use surface cache feedback to selectively map higher quality surface cache pages."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLumenSurfaceCacheFeedbackTileSize = 16;
FAutoConsoleVariableRef CVarLumenSurfaceCacheFeedbackTileSize(
	TEXT("r.LumenScene.SurfaceCache.Feedback.TileSize"),
	GLumenSurfaceCacheFeedbackTileSize,
	TEXT("One surface cache feedback element will be writen out per tile. Aligned to a power of two."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float GLumenSurfaceCacheFeedbackResLevelBias = -0.5f;
FAutoConsoleVariableRef CVarLumenSurfaceCacheFeedbackResLevelBias(
	TEXT("r.LumenScene.SurfaceCache.Feedback.ResLevelBias"),
	GLumenSurfaceCacheFeedbackResLevelBias,
	TEXT("Bias resolution of on demand surface cache pages."),
	FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* InVariable)
		{
			Lumen::DebugResetSurfaceCache();
		}),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float GLumenSurfaceCacheFeedbackFeedbackMinPageHits = 16;
FAutoConsoleVariableRef CVarLumenSurfaceCacheFeedbackMinPageHits(
	TEXT("r.LumenScene.SurfaceCache.Feedback.MinPageHits"),
	GLumenSurfaceCacheFeedbackFeedbackMinPageHits,
	TEXT("Min number of page hits to demand a new page."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLumenSurfaceCacheFeedbackMaxUniqueElements = 1024;
FAutoConsoleVariableRef CVarLumenSurfaceCacheFeedbackUniqueElements(
	TEXT("r.LumenScene.SurfaceCache.Feedback.UniqueElements"),
	GLumenSurfaceCacheFeedbackMaxUniqueElements,
	TEXT("Limit of unique surface cache feedback elements. Used to resize buffers."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

uint32 Lumen::GetFeedbackBufferTileSize()
{
	return FMath::RoundUpToPowerOfTwo(FMath::Clamp(GLumenSurfaceCacheFeedbackTileSize, 1, 256));
}

uint32 Lumen::GetFeedbackBufferTileWrapMask()
{
	// Index & TileWrapMask = Index % TileSize
	return GetFeedbackBufferTileSize() - 1;
}

uint32 Lumen::GetFeedbackBufferSize(const FViewFamilyInfo& ViewFamily)
{
	const FSceneTexturesConfig& SceneTexturesConfig = ViewFamily.SceneTexturesConfig;
	const FIntPoint SceneTextureExtentInTiles = FIntPoint::DivideAndRoundUp(SceneTexturesConfig.Extent, Lumen::GetFeedbackBufferTileSize());
	const uint32 FeedbackBufferSize = SceneTextureExtentInTiles.X * SceneTextureExtentInTiles.Y;
	return FeedbackBufferSize;
}

uint32 Lumen::GetCompactedFeedbackBufferSize()
{
	return FMath::RoundUpToPowerOfTwo(FMath::Clamp(GLumenSurfaceCacheFeedbackMaxUniqueElements, 1, 16 * 1024));
}

FLumenSurfaceCacheFeedback::FLumenSurfaceCacheFeedback()
{
	ReadbackBuffers.AddZeroed(MaxReadbackBuffers);
}

FLumenSurfaceCacheFeedback::~FLumenSurfaceCacheFeedback()
{
	for (int32 BufferIndex = 0; BufferIndex < ReadbackBuffers.Num(); ++BufferIndex)
	{
		if (ReadbackBuffers[BufferIndex])
		{
			delete ReadbackBuffers[BufferIndex];
			ReadbackBuffers[BufferIndex] = nullptr;
		}
	}
}

void FLumenSurfaceCacheFeedback::AllocateFeedbackResources(FRDGBuilder& GraphBuilder, FFeedbackResources& Resources, const FViewFamilyInfo& ViewFamily) const
{
	Resources.BufferSize = Lumen::GetFeedbackBufferSize(ViewFamily);

	FRDGBuffer* BufferAllocator = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), 1),
		TEXT("Lumen.FeedbackAllocator"));

	Resources.BufferAllocatorUAV = GraphBuilder.CreateUAV(BufferAllocator, ERDGUnorderedAccessViewFlags::SkipBarrier);
	Resources.BufferAllocatorSRV = GraphBuilder.CreateSRV(BufferAllocator, PF_R32_UINT);

	FRDGBuffer* Buffer = GraphBuilder.CreateBuffer( 
		FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32) * Lumen::FeedbackBufferElementStride, Resources.BufferSize),
		TEXT("Lumen.Feedback"));

	Resources.BufferUAV = GraphBuilder.CreateUAV(Buffer, ERDGUnorderedAccessViewFlags::SkipBarrier);
	Resources.BufferSRV = GraphBuilder.CreateSRV(Buffer, PF_R32G32_UINT);

	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(BufferAllocator, PF_R32_UINT), 0);
	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(Buffer, PF_R32G32_UINT), 0);
}

FRDGBufferUAVRef FLumenSurfaceCacheFeedback::GetDummyFeedbackAllocatorUAV(FRDGBuilder& GraphBuilder) const
{
	FRDGBufferRef DummyBufferAllocator = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), 1),
		TEXT("Lumen.DummyFeedbackAllocator"));

	return GraphBuilder.CreateUAV(DummyBufferAllocator, ERDGUnorderedAccessViewFlags::SkipBarrier);
}

FRDGBufferUAVRef FLumenSurfaceCacheFeedback::GetDummyFeedbackUAV(FRDGBuilder& GraphBuilder) const
{
	FRDGBufferRef DummyBuffer = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32) * Lumen::FeedbackBufferElementStride, 1),
		TEXT("Lumen.DummyFeedback"));

	return GraphBuilder.CreateUAV(DummyBuffer, ERDGUnorderedAccessViewFlags::SkipBarrier);
}

// Setups FBuildFeedbackHashTableCS arguments to run one lane per feedback element
class FBuildFeedbackHashTableIndirectArgsCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FBuildFeedbackHashTableIndirectArgsCS);
	SHADER_USE_PARAMETER_STRUCT(FBuildFeedbackHashTableIndirectArgsCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWBuildHashTableIndirectArgs)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, FeedbackBufferAllocator)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint2>, FeedbackBuffer)
		SHADER_PARAMETER(uint32, FeedbackBufferSize)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), 1);
	}
};

IMPLEMENT_GLOBAL_SHADER(FBuildFeedbackHashTableIndirectArgsCS, "/Engine/Private/Lumen/SurfaceCache/LumenSurfaceCacheFeedback.usf", "BuildFeedbackHashTableIndirectArgsCS", SF_Compute);


// Takes a list of feedback elements and builds a hash table with element counts
class FBuildFeedbackHashTableCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FBuildFeedbackHashTableCS)
	SHADER_USE_PARAMETER_STRUCT(FBuildFeedbackHashTableCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		RDG_BUFFER_ACCESS(BuildHashTableIndirectArgs, ERHIAccess::IndirectArgs)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWHashTableKeys)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWHashTableElementIndices)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWHashTableElementCounts)
		SHADER_PARAMETER(uint32, HashTableSize)
		SHADER_PARAMETER(uint32, HashTableIndexWrapMask)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, FeedbackBufferAllocator)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint2>, FeedbackBuffer)
		SHADER_PARAMETER(uint32, FeedbackBufferSize)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static uint32 GetGroupSize()
	{
		return 64;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}
};

IMPLEMENT_GLOBAL_SHADER(FBuildFeedbackHashTableCS, "/Engine/Private/Lumen/SurfaceCache/LumenSurfaceCacheFeedback.usf", "BuildFeedbackHashTableCS", SF_Compute);

// Compacts feedback element hash table into a unique and tightly packed array of feedback elements with counts
class FCompactFeedbackHashTableCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FCompactFeedbackHashTableCS)
	SHADER_USE_PARAMETER_STRUCT(FCompactFeedbackHashTableCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint2>, RWCompactedFeedbackBuffer)
		SHADER_PARAMETER(uint32, CompactedFeedbackBufferSize)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, HashTableElementIndices)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, HashTableElementCounts)
		SHADER_PARAMETER(uint32, HashTableSize)
		SHADER_PARAMETER(uint32, HashTableIndexWrapMask)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, FeedbackBufferAllocator)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint2>, FeedbackBuffer)
		SHADER_PARAMETER(uint32, FeedbackBufferSize)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static uint32 GetGroupSize()
	{
		return 64;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}
};

IMPLEMENT_GLOBAL_SHADER(FCompactFeedbackHashTableCS, "/Engine/Private/Lumen/SurfaceCache/LumenSurfaceCacheFeedback.usf", "CompactFeedbackHashTableCS", SF_Compute);

void FLumenSurfaceCacheFeedback::SubmitFeedbackBuffer(
	const FViewInfo& View,
	FRDGBuilder& GraphBuilder,
	FLumenSurfaceCacheFeedback::FFeedbackResources& FeedbackResources)
{
	if (ReadbackBuffersNumPending == MaxReadbackBuffers)
	{
		// Return when queue is full. It is NOT safe to EnqueueCopy on a buffer that already has a pending copy.
		return;
	}

	RDG_EVENT_SCOPE(GraphBuilder, "Submit Lumen surface cache feedback");

	const uint32 CompactedFeedbackBufferSize = Lumen::GetCompactedFeedbackBufferSize();
	const uint32 HashTableSize = 2 * CompactedFeedbackBufferSize;
	const uint32 HashTableIndexWrapMask = HashTableSize - 1;

	FRDGBufferDesc CompactedFeedbackBufferDesc(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32) * Lumen::FeedbackBufferElementStride, CompactedFeedbackBufferSize));
	CompactedFeedbackBufferDesc.Usage |= BUF_SourceCopy;

	FRDGBufferRef CompactedFeedbackBuffer = GraphBuilder.CreateBuffer(CompactedFeedbackBufferDesc, TEXT("Lumen.CompactedFeedback"));

	// Need to clear this buffer, as first element will be used as an allocator
	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(CompactedFeedbackBuffer, PF_R32_UINT), 0);

	FRDGBufferDesc HashTableBufferDesc(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), HashTableSize));
	FRDGBufferRef HashTableKeyBuffer = GraphBuilder.CreateBuffer(HashTableBufferDesc, TEXT("Lumen.HashTableKeys"));
	FRDGBufferRef HashTableElementIndexBuffer = GraphBuilder.CreateBuffer(HashTableBufferDesc, TEXT("Lumen.HashTableElementIndices"));
	FRDGBufferRef HashTableElementCountBuffer = GraphBuilder.CreateBuffer(HashTableBufferDesc, TEXT("Lumen.HashTableElementCounts"));

	// Hash table depends on empty slots to be 0
	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(HashTableKeyBuffer, PF_R32_UINT), 0);
	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(HashTableElementCountBuffer, PF_R32_UINT), 0);

	FRDGBufferRef BuildHashTableIndirectArgBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>(1), TEXT("Lumen.BuildHashTableIndirectArgs"));

	// Set indirect dispatch arguments for hash table building
	{
		FBuildFeedbackHashTableIndirectArgsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FBuildFeedbackHashTableIndirectArgsCS::FParameters>();

		PassParameters->RWBuildHashTableIndirectArgs = GraphBuilder.CreateUAV(BuildHashTableIndirectArgBuffer, PF_R32_UINT);

		PassParameters->FeedbackBufferAllocator = FeedbackResources.BufferAllocatorSRV;
		PassParameters->FeedbackBuffer = FeedbackResources.BufferSRV;
		PassParameters->FeedbackBufferSize = FeedbackResources.BufferSize;

		auto ComputeShader = View.ShaderMap->GetShader<FBuildFeedbackHashTableIndirectArgsCS>();
		const FIntVector GroupSize = FIntVector(1, 1, 1);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("Hash table indirect arguments"),
			ComputeShader,
			PassParameters,
			GroupSize);
	}
	
	// Build hash table of feedback elements
	{
		FBuildFeedbackHashTableCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FBuildFeedbackHashTableCS::FParameters>();

		PassParameters->View = View.ViewUniformBuffer;
		PassParameters->BuildHashTableIndirectArgs = BuildHashTableIndirectArgBuffer;

		PassParameters->RWHashTableKeys = GraphBuilder.CreateUAV(HashTableKeyBuffer);
		PassParameters->RWHashTableElementIndices = GraphBuilder.CreateUAV(HashTableElementIndexBuffer);
		PassParameters->RWHashTableElementCounts = GraphBuilder.CreateUAV(HashTableElementCountBuffer);
		PassParameters->HashTableSize = HashTableSize;
		PassParameters->HashTableIndexWrapMask = HashTableIndexWrapMask;

		PassParameters->FeedbackBufferAllocator = FeedbackResources.BufferAllocatorSRV;
		PassParameters->FeedbackBuffer = FeedbackResources.BufferSRV;
		PassParameters->FeedbackBufferSize = FeedbackResources.BufferSize;

		auto ComputeShader = View.ShaderMap->GetShader<FBuildFeedbackHashTableCS>();

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("Build feedback hash table"),
			ComputeShader,
			PassParameters,
			BuildHashTableIndirectArgBuffer,
			0);
	}

	// Compact hash table into an array of unique feedback elements
	{
		FCompactFeedbackHashTableCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FCompactFeedbackHashTableCS::FParameters>();

		PassParameters->View = View.ViewUniformBuffer;

		PassParameters->RWCompactedFeedbackBuffer = GraphBuilder.CreateUAV(CompactedFeedbackBuffer);
		PassParameters->CompactedFeedbackBufferSize = CompactedFeedbackBufferSize;

		PassParameters->HashTableElementIndices = GraphBuilder.CreateSRV(HashTableElementIndexBuffer, PF_R32_UINT);
		PassParameters->HashTableElementCounts = GraphBuilder.CreateSRV(HashTableElementCountBuffer, PF_R32_UINT);
		PassParameters->HashTableSize = HashTableSize;
		PassParameters->HashTableIndexWrapMask = HashTableIndexWrapMask;

		PassParameters->FeedbackBufferAllocator = FeedbackResources.BufferAllocatorSRV;
		PassParameters->FeedbackBuffer = FeedbackResources.BufferSRV;
		PassParameters->FeedbackBufferSize = FeedbackResources.BufferSize;

		auto ComputeShader = View.ShaderMap->GetShader<FCompactFeedbackHashTableCS>();
		const FIntVector GroupSize = FComputeShaderUtils::GetGroupCount(HashTableSize, FCompactFeedbackHashTableCS::GetGroupSize());

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("Compact feedback hash table"),
			ComputeShader,
			PassParameters,
			GroupSize);
	}

	if (ReadbackBuffers[ReadbackBuffersWriteIndex] == nullptr)
	{
		FRHIGPUBufferReadback* GPUBufferReadback = new FRHIGPUBufferReadback(TEXT("Lumen.SurfaceCacheFeedbackBuffer"));
		ReadbackBuffers[ReadbackBuffersWriteIndex] = GPUBufferReadback;
	}

	FRHIGPUBufferReadback* ReadbackBuffer = ReadbackBuffers[ReadbackBuffersWriteIndex];

	AddReadbackBufferPass(GraphBuilder, RDG_EVENT_NAME("Readback"), CompactedFeedbackBuffer,
		[ReadbackBuffer, CompactedFeedbackBuffer](FRHICommandList& RHICmdList)
		{
			ReadbackBuffer->EnqueueCopy(RHICmdList, CompactedFeedbackBuffer->GetRHI(), 0u);
		});

	ReadbackBuffersWriteIndex = (ReadbackBuffersWriteIndex + 1) % MaxReadbackBuffers;
	ReadbackBuffersNumPending = FMath::Min(ReadbackBuffersNumPending + 1, MaxReadbackBuffers);

	++FrameIndex;
}

FRHIGPUBufferReadback* FLumenSurfaceCacheFeedback::GetLatestReadbackBuffer()
{
	FRHIGPUBufferReadback* LatestReadbackBuffer = nullptr;

	// Find latest buffer that is ready
	while (ReadbackBuffersNumPending > 0)
	{
		uint32 Index = (ReadbackBuffersWriteIndex + MaxReadbackBuffers - ReadbackBuffersNumPending) % MaxReadbackBuffers;
		if (ReadbackBuffers[Index]->IsReady())
		{
			--ReadbackBuffersNumPending;
			LatestReadbackBuffer = ReadbackBuffers[Index];
		}
		else
		{
			break;
		}
	}

	return LatestReadbackBuffer;
}

void FLumenSceneData::UpdateSurfaceCacheFeedback(FFeedbackData FeedbackData, const TArray<FVector, TInlineAllocator<2>>& LumenSceneCameraOrigins, TArray<FSurfaceCacheRequest>& SurfaceCacheRequests, const FViewFamilyInfo& ViewFamily, int32 RequestHistogram[Lumen::NumDistanceBuckets])
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UpdateSurfaceCacheFeedback);

	NumHiResPagesToAdd = 0;

	if (FeedbackData.Data)
	{
		const int32 HeaderSize = 1;
		const int32 NumFeedbackElements = FMath::Min<int32>(FeedbackData.Data[0], FeedbackData.NumElements - HeaderSize);

		for (int32 FeedbackElementIndex = 0; FeedbackElementIndex < NumFeedbackElements; ++FeedbackElementIndex)
		{
			const uint32 PackedA = FeedbackData.Data[(FeedbackElementIndex + HeaderSize) * Lumen::FeedbackBufferElementStride + 0];
			const uint32 PackedB = FeedbackData.Data[(FeedbackElementIndex + HeaderSize) * Lumen::FeedbackBufferElementStride + 1];

			int32 CardIndex = PackedA & 0xFFFFFF;
			uint16 DesiredResLevel = FMath::Clamp(PackedA >> 24, Lumen::MinResLevel, Lumen::MaxResLevel);

			FIntPoint LocalPageCoord;
			LocalPageCoord.X = (PackedB >> 0) & 0xFF;
			LocalPageCoord.Y = (PackedB >> 8) & 0xFF;

			const uint32 PageHitNum = (PackedB >> 16) & 0xFFFF;

			if (PageHitNum > GLumenSurfaceCacheFeedbackFeedbackMinPageHits
				&& CardIndex < Cards.Num() 
				&& Cards.IsAllocated(CardIndex))
			{
				FLumenCard& Card = Cards[CardIndex];

				FLumenMipMapDesc MipMapDesc;
				Card.GetMipMapDesc(DesiredResLevel, MipMapDesc);

				LocalPageCoord.X = FMath::Clamp(LocalPageCoord.X, 0, MipMapDesc.SizeInPages.X - 1);
				LocalPageCoord.Y = FMath::Clamp(LocalPageCoord.Y, 0, MipMapDesc.SizeInPages.Y - 1);

				const uint16 LocalPageIndex = LocalPageCoord.X + LocalPageCoord.Y * MipMapDesc.SizeInPages.X;

				FVirtualPageIndex PageIndex(CardIndex, DesiredResLevel, LocalPageIndex);
				const FLumenSurfaceMipMap MipMap = Card.GetMipMap(PageIndex.ResLevel);
				const int32 PageTableIndex = MipMap.PageTableSpanSize > 0 ? MipMap.GetPageTableIndex(PageIndex.LocalPageIndex) : -1;

				if (PageTableIndex >= 0 && PageTable[PageTableIndex].IsMapped())
				{
					// Update last used time for existing pages
					if (!MipMap.bLocked)
					{
						UnlockedAllocationHeap.Update(SurfaceCacheFeedback.GetFrameIndex(), PageTableIndex);
					}
				}
				else
				{
					float DistanceSquared = FLT_MAX; // LWC_TODO

					for (FVector CameraOrigin : LumenSceneCameraOrigins)
					{
						DistanceSquared = FMath::Min(DistanceSquared, Card.WorldOBB.ComputeSquaredDistanceToPoint(CameraOrigin));
					}
					float Distance = FMath::Sqrt(DistanceSquared);

					// Change priority based on the normalized number of hits and make those request less important than low res resident pages
					const float NormalizeNumberOfHits = PageHitNum / float(Lumen::GetFeedbackBufferSize(ViewFamily));
					Distance += 2500.0f + 2500.0f * (1.0f - NormalizeNumberOfHits);

					// Requested missing page
					FSurfaceCacheRequest Request;
					Request.CardIndex = PageIndex.CardIndex;
					Request.ResLevel = PageIndex.ResLevel;
					Request.LocalPageIndex = PageIndex.LocalPageIndex;
					Request.Distance = Distance;
					SurfaceCacheRequests.Add(Request);

					uint32 Bin = Lumen::GetMeshCardDistanceBin(Distance);
					RequestHistogram[Bin]++;

					ensure(!Request.IsLockedMip());

					++NumHiResPagesToAdd;
				}
			}
		}
	}
}

FIntPoint FLumenSurfaceCacheFeedback::GetFeedbackBufferTileJitter() const
{
	const uint32 TileSize = Lumen::GetFeedbackBufferTileSize();
	const uint32 TileSizeLog2 = FMath::CeilLogTwo(TileSize);
	const uint32 SequenceSize = FMath::Square(TileSize);
	const uint32 PixelIndex = FrameIndex % SequenceSize;
	const uint32 PixelAddress = ReverseBits(PixelIndex) >> (32U - 2 * TileSizeLog2);

	FIntPoint TileJitter;
	TileJitter.X = FMath::ReverseMortonCode2(PixelAddress);
	TileJitter.Y = FMath::ReverseMortonCode2(PixelAddress >> 1);

	return TileJitter;
}

void FDeferredShadingSceneRenderer::BeginGatheringLumenSurfaceCacheFeedback(FRDGBuilder& GraphBuilder, const FViewInfo& View, FLumenSceneFrameTemporaries& FrameTemporaries)
{
	const FPerViewPipelineState& ViewPipelineState = GetViewPipelineState(View);
	const bool bLumenActive = ViewPipelineState.DiffuseIndirectMethod == EDiffuseIndirectMethod::Lumen || ViewPipelineState.ReflectionsMethod == EReflectionsMethod::Lumen;

	if (bLumenActive && GLumenSurfaceCacheFeedback != 0)
	{
		FLumenSceneData& LumenSceneData = *Scene->GetLumenSceneData(View);

		const bool bVisualizeUsesFeedback = LumenVisualize::UseSurfaceCacheFeedback(ViewFamily.EngineShowFlags);
		const bool bReflectionsUseFeedback = ViewPipelineState.ReflectionsMethod == EReflectionsMethod::Lumen && LumenReflections::UseSurfaceCacheFeedback();

		if (!Lumen::IsSurfaceCacheFrozen() && (bVisualizeUsesFeedback || bReflectionsUseFeedback))
		{
			ensure(FrameTemporaries.SurfaceCacheFeedbackResources.BufferUAV == nullptr);

			LumenSceneData.SurfaceCacheFeedback.AllocateFeedbackResources(GraphBuilder, FrameTemporaries.SurfaceCacheFeedbackResources, ViewFamily);
		}

		if (LumenSceneData.CardPageLastUsedBuffer && LumenSceneData.CardPageHighResLastUsedBuffer)
		{
			FRDGBuffer* CardPageLastUsedBuffer = GraphBuilder.RegisterExternalBuffer(LumenSceneData.CardPageLastUsedBuffer);
			FrameTemporaries.CardPageLastUsedBufferSRV = GraphBuilder.CreateSRV(CardPageLastUsedBuffer);
			FrameTemporaries.CardPageLastUsedBufferUAV = GraphBuilder.CreateUAV(CardPageLastUsedBuffer, ERDGUnorderedAccessViewFlags::SkipBarrier);

			FRDGBuffer* CardPageHighResLastUsedBuffer = GraphBuilder.RegisterExternalBuffer(LumenSceneData.CardPageHighResLastUsedBuffer);
			FrameTemporaries.CardPageHighResLastUsedBufferSRV = GraphBuilder.CreateSRV(CardPageHighResLastUsedBuffer);
			FrameTemporaries.CardPageHighResLastUsedBufferUAV = GraphBuilder.CreateUAV(CardPageHighResLastUsedBuffer, ERDGUnorderedAccessViewFlags::SkipBarrier);
		}
	}
}

void FDeferredShadingSceneRenderer::FinishGatheringLumenSurfaceCacheFeedback(FRDGBuilder& GraphBuilder, const FViewInfo& View, FLumenSceneFrameTemporaries& FrameTemporaries)
{
	const FPerViewPipelineState& ViewPipelineState = GetViewPipelineState(View);
	const bool bLumenActive = ViewPipelineState.DiffuseIndirectMethod == EDiffuseIndirectMethod::Lumen || ViewPipelineState.ReflectionsMethod == EReflectionsMethod::Lumen;

	if (bLumenActive && GLumenSurfaceCacheFeedback != 0)
	{
		FLumenSceneData& LumenSceneData = *Scene->GetLumenSceneData(View);

		if (FrameTemporaries.SurfaceCacheFeedbackResources.BufferUAV)
		{
			LumenSceneData.SurfaceCacheFeedback.SubmitFeedbackBuffer(Views[0], GraphBuilder, FrameTemporaries.SurfaceCacheFeedbackResources);

			FrameTemporaries.SurfaceCacheFeedbackResources = {};
		}

		if (FrameTemporaries.CardPageLastUsedBufferUAV && FrameTemporaries.CardPageHighResLastUsedBufferUAV)
		{
			GraphBuilder.QueueBufferExtraction(FrameTemporaries.CardPageLastUsedBufferUAV->GetParent(), &LumenSceneData.CardPageLastUsedBuffer);
			GraphBuilder.QueueBufferExtraction(FrameTemporaries.CardPageHighResLastUsedBufferUAV->GetParent(), &LumenSceneData.CardPageHighResLastUsedBuffer);
		}
	}

	if (FrameTemporaries.AlbedoAtlas)
	{
		FLumenSceneData& LumenSceneData = *Scene->GetLumenSceneData(View);

		GraphBuilder.QueueTextureExtraction(FrameTemporaries.DepthAtlas, &LumenSceneData.DepthAtlas);
		GraphBuilder.QueueTextureExtraction(FrameTemporaries.AlbedoAtlas, &LumenSceneData.AlbedoAtlas);
		GraphBuilder.QueueTextureExtraction(FrameTemporaries.OpacityAtlas, &LumenSceneData.OpacityAtlas);
		GraphBuilder.QueueTextureExtraction(FrameTemporaries.NormalAtlas, &LumenSceneData.NormalAtlas);
		GraphBuilder.QueueTextureExtraction(FrameTemporaries.EmissiveAtlas, &LumenSceneData.EmissiveAtlas);
		GraphBuilder.QueueTextureExtraction(FrameTemporaries.DirectLightingAtlas, &LumenSceneData.DirectLightingAtlas);
		GraphBuilder.QueueTextureExtraction(FrameTemporaries.IndirectLightingAtlas, &LumenSceneData.IndirectLightingAtlas);
		GraphBuilder.QueueTextureExtraction(FrameTemporaries.RadiosityNumFramesAccumulatedAtlas, &LumenSceneData.RadiosityNumFramesAccumulatedAtlas);
		GraphBuilder.QueueTextureExtraction(FrameTemporaries.FinalLightingAtlas, &LumenSceneData.FinalLightingAtlas);
	}
}