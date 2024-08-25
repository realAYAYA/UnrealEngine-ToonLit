// Copyright Epic Games, Inc. All Rights Reserved.

/*==============================================================================
	MetalRayTracing.cpp: MetalRT Implementation
==============================================================================*/

#include "MetalRayTracing.h"

#if METAL_RHI_RAYTRACING

#include "MetalRHIContext.h"
#include "MetalShaderTypes.h"
#include "MetalComputePipelineState.h"
#include "BuiltInRayTracingShaders.h"
#include "RayTracingValidationShaders.h"
#include "RayTracingBuiltInResources.h"

static int32 GMetalRayTracingAllowCompaction = 1;
static FAutoConsoleVariableRef CVarMetalRayTracingAllowCompaction(
	TEXT("r.Metal.RayTracing.AllowCompaction"),
	GMetalRayTracingAllowCompaction,
	TEXT("Whether to automatically perform compaction for static acceleration structures to save GPU memory. (default = 1)\n"),
	ECVF_ReadOnly
);

static int32 GRayTracingDebugForceBuildMode = 0;
static FAutoConsoleVariableRef CVarMetalRayTracingDebugForceFastTrace(
	TEXT("r.Metal.RayTracing.DebugForceBuildMode"),
	GRayTracingDebugForceBuildMode,
	TEXT("Forces specific acceleration structure build mode (not runtime-tweakable).\n")
	TEXT("0: Use build mode requested by high-level code (Default)\n")
	TEXT("1: Force fast build mode\n")
	TEXT("2: Force fast trace mode\n"),
	ECVF_ReadOnly
);

static int32 GMetalRayTracingMaxBatchedCompaction = 64;
static FAutoConsoleVariableRef CVarMetalRayTracingMaxBatchedCompaction(
	TEXT("r.Metal.RayTracing.MaxBatchedCompaction"),
	GMetalRayTracingMaxBatchedCompaction,
	TEXT("Maximum of amount of compaction requests and rebuilds per frame. (default = 64)\n"),
	ECVF_ReadOnly
);

static ERayTracingAccelerationStructureFlags GetRayTracingAccelerationStructureBuildFlags(const FRayTracingGeometryInitializer& Initializer)
{
	ERayTracingAccelerationStructureFlags BuildFlags = ERayTracingAccelerationStructureFlags::None;

	if (Initializer.bFastBuild)
	{
		BuildFlags = ERayTracingAccelerationStructureFlags::FastBuild;
	}
	else
	{
		BuildFlags = ERayTracingAccelerationStructureFlags::FastTrace;
	}

	if (Initializer.bAllowUpdate)
	{
		EnumAddFlags(BuildFlags, ERayTracingAccelerationStructureFlags::AllowUpdate);
	}

	if (!Initializer.bFastBuild && !Initializer.bAllowUpdate && Initializer.bAllowCompaction && GMetalRayTracingAllowCompaction)
	{
		EnumAddFlags(BuildFlags, ERayTracingAccelerationStructureFlags::AllowCompaction);
	}

	if (GRayTracingDebugForceBuildMode == 1)
	{
		EnumAddFlags(BuildFlags, ERayTracingAccelerationStructureFlags::FastBuild);
		EnumRemoveFlags(BuildFlags, ERayTracingAccelerationStructureFlags::FastTrace);
	}
	else if (GRayTracingDebugForceBuildMode == 2)
	{
		EnumAddFlags(BuildFlags, ERayTracingAccelerationStructureFlags::FastTrace);
		EnumRemoveFlags(BuildFlags, ERayTracingAccelerationStructureFlags::FastBuild);
	}

	return BuildFlags;
}

static bool ShouldCompactAfterBuild(ERayTracingAccelerationStructureFlags BuildFlags)
{
	return EnumHasAllFlags(BuildFlags, ERayTracingAccelerationStructureFlags::AllowCompaction | ERayTracingAccelerationStructureFlags::FastTrace)
		&& !EnumHasAnyFlags(BuildFlags, ERayTracingAccelerationStructureFlags::AllowUpdate);
}

// Manages all the pending BLAS compaction requests
class FMetalRayTracingCompactionRequestHandler
{
public:
	UE_NONCOPYABLE(FMetalRayTracingCompactionRequestHandler)

	FMetalRayTracingCompactionRequestHandler(FMetalDeviceContext* DeviceContext);
	~FMetalRayTracingCompactionRequestHandler();

	void RequestCompact(FMetalRayTracingGeometry* InRTGeometry);
	bool ReleaseRequest(FMetalRayTracingGeometry* InRTGeometry);

	void Update(FMetalDeviceContext* DeviceContext);

private:
	/** Enqueued requests (waiting on size request submit). */
	TArray<FMetalRayTracingGeometry*> PendingRequests;

	/** Enqueued compaction requests (submitted compaction size requests; waiting on readback and actual compaction). */
	TQueue<FMetalRayTracingGeometry*> ActiveRequests;

	/** Num of active requests enqueued. */
	uint32 NumActiveRequests;

	/** Buffer used for compacted size readback. */
	FMetalBuffer CompactedStructureSizeBuffer;

	/** Size entry allocated in the CompactedStructureSizeBuffer (in element count). */
	uint32_t SizeBufferMaxCapacity;

	/** CompactedStructureSizeBuffer write index. Wrap once equal to SizeBufferMaxCapacity. */
	uint32_t WriteIndex;
};

FMetalRayTracingCompactionRequestHandler::FMetalRayTracingCompactionRequestHandler(FMetalDeviceContext* DeviceContext)
	: SizeBufferMaxCapacity(GMetalRayTracingMaxBatchedCompaction)
	, WriteIndex(0u)
{
	PendingRequests.Reserve(GMetalRayTracingMaxBatchedCompaction);

	CompactedStructureSizeBuffer = FMetalBuffer(DeviceContext->GetDevice().NewBuffer(GMetalRayTracingMaxBatchedCompaction * sizeof(uint32), MTL::ResourceStorageModeShared));
	check(CompactedStructureSizeBuffer);

	NumActiveRequests = 0;
}

FMetalRayTracingCompactionRequestHandler::~FMetalRayTracingCompactionRequestHandler()
{
	check(PendingRequests.IsEmpty());
	SafeReleaseMetalBuffer(CompactedStructureSizeBuffer);
}

void FMetalRayTracingCompactionRequestHandler::RequestCompact(FMetalRayTracingGeometry* InRTGeometry)
{
	check(InRTGeometry->GetAccelerationStructureRead()
		  && InRTGeometry->GetAccelerationStructureRead()->IsAccelerationStructure()
		  && InRTGeometry->GetAccelerationStructureRead()->AccelerationStructureHandle.GetPtr());
	ERayTracingAccelerationStructureFlags GeometryBuildFlags = GetRayTracingAccelerationStructureBuildFlags(InRTGeometry->Initializer);
	check(EnumHasAllFlags(GeometryBuildFlags, ERayTracingAccelerationStructureFlags::AllowCompaction) &&
		EnumHasAllFlags(GeometryBuildFlags, ERayTracingAccelerationStructureFlags::FastTrace) &&
		!EnumHasAnyFlags(GeometryBuildFlags, ERayTracingAccelerationStructureFlags::AllowUpdate));

	PendingRequests.Add(InRTGeometry);
}

bool FMetalRayTracingCompactionRequestHandler::ReleaseRequest(FMetalRayTracingGeometry* InRTGeometry)
{
	PendingRequests.Remove(InRTGeometry);
	return true;
}

void FMetalRayTracingCompactionRequestHandler::Update(FMetalDeviceContext* DeviceContext)
{
	// Early exit to avoid unecessary encoding breaks.
	if (PendingRequests.IsEmpty() && ActiveRequests.IsEmpty())
	{
		return;
	}

	check(CompactedStructureSizeBuffer);

	// Submit build commands.
	MTL::Device* Device = DeviceContext->GetDevice();
	FMetalRenderPass& RenderPass = DeviceContext->GetCurrentRenderPass();
	FMetalCommandEncoder& Encoder = RenderPass.GetCurrentCommandEncoder();
	Encoder.EndEncoding();
	DeviceContext->GetCurrentState().SetStateDirty();

	Encoder.BeginAccelerationStructureCommandEncoding();
	MTL::AccelerationStructureCommandEncoder* CommandEncoder = Encoder.GetAccelerationStructureCommandEncoder();
	check(CommandEncoder.GetPtr());

	// Process pending requests.
	for (FMetalRayTracingGeometry* Geometry : PendingRequests)
	{
		Geometry->CompactionSizeIndex = WriteIndex;
		WriteIndex = (++WriteIndex % SizeBufferMaxCapacity);

		CommandEncoder.WriteCompactedAccelerationStructureSize(Geometry->GetAccelerationStructureRead()->AccelerationStructureHandle, CompactedStructureSizeBuffer, Geometry->CompactionSizeIndex * sizeof(uint32_t));

		ActiveRequests.Enqueue(Geometry);
		NumActiveRequests++;

		// enqueued enough requests for this update round
		if (NumActiveRequests >= GMetalRayTracingMaxBatchedCompaction)
		{
			break;
		}
	}
	PendingRequests.Empty();

	// Try to readback active requests.
	uint32_t* CompactedSizes = (uint32_t*)CompactedStructureSizeBuffer.GetContents();

	// Process active requests.
	while (!ActiveRequests.IsEmpty())
	{
		FMetalRayTracingGeometry* ActiveRequestsTail = *ActiveRequests.Peek();
		check(ActiveRequestsTail);

		if (!ActiveRequestsTail->bHasPendingCompactionRequests)
		{
			ActiveRequests.Pop();
			continue;
		}

		// TODO: Assuming the compacted size may always be zero (e.g. corrupted BLAS/incorrect descriptor/API bug...), we should probably track the active readback index; and move forward if the index is stuck (after X number of frames)
		
		// Break whenever we encounter the first incomplete request (i.e. size == 0).
		uint32_t CompactedSize = CompactedSizes[ActiveRequestsTail->CompactionSizeIndex];
		if (CompactedSize == 0u)
		{
			break;
		}

		MTL::AccelerationStructure* SrcBLAS = ActiveRequestsTail->GetAccelerationStructureRead()->AccelerationStructureHandle;
		MTL::AccelerationStructure* CompactedBLAS = ActiveRequestsTail->GetAccelerationStructureWrite()->AccelerationStructureHandle;
		check(CompactedBLAS);

		CommandEncoder.CopyAndCompactAccelerationStructure(SrcBLAS, CompactedBLAS);

		ActiveRequestsTail->NextAccelerationStructure();
		ActiveRequests.Pop();
		NumActiveRequests--;
	}

	Encoder.EndEncoding();
}

/** Fills a MTLPrimitiveAccelerationStructureDescriptor with infos provided by the UE5 geometry descriptor.
 * This function assumes that GeometryDescriptors has already been allocated, and that you are responsible of its lifetime.
 */
static void FillPrimitiveAccelerationStructureDesc(MTL::PrimitiveAccelerationStructureDescriptor* AccelerationStructureDescriptor, const FRayTracingGeometryInitializer& Initializer,  NSMutableArray<MTLAccelerationStructureGeometryDescriptor*>*& GeometryDescriptors)
{
	// Populate Segments Descriptors.
	FMetalRHIBuffer* IndexBuffer = ResourceCast(Initializer.IndexBuffer.GetReference());

	int32 SegmentIndex = 0;
	for (const FRayTracingGeometrySegment& Segment : Initializer.Segments)
	{
		check(Segment.NumPrimitives > 0);

		// Vertex Buffer Infos
		FMetalRHIBuffer* VertexBuffer = ResourceCast(Segment.VertexBuffer.GetReference());
		check(VertexBuffer);

		MTL::AccelerationStructureTriangleGeometryDescriptor GeometryDescriptor = MTL::AccelerationStructureTriangleGeometryDescriptor();
		GeometryDescriptor.SetOpaque(Segment.bForceOpaque);
		GeometryDescriptor.SetTriangleCount((Segment.bEnabled) ? Segment.NumPrimitives : 0);
		GeometryDescriptor.SetAllowDuplicateIntersectionFunctionInvocation(Segment.bAllowDuplicateAnyHitShaderInvocation);

		// Index Buffer Infos
		if (IndexBuffer != nullptr)
		{
			const FMetalBuffer& IndexBufferRes = IndexBuffer->GetCurrentBuffer();

			GeometryDescriptor.SetIndexType(IndexBuffer->GetIndexType());
			GeometryDescriptor.SetIndexBuffer(IndexBufferRes);
			GeometryDescriptor.SetIndexBufferOffset(IndexBufferRes.GetOffset() + Initializer.IndexBufferOffset);
		}

		const FMetalBuffer& VertexBufferRes = VertexBuffer->GetCurrentBuffer();

		GeometryDescriptor.SetVertexBuffer(VertexBufferRes);
		GeometryDescriptor.SetVertexBufferOffset(VertexBufferRes.GetOffset() + Segment.VertexBufferOffset);
		GeometryDescriptor.SetVertexBufferStride(Segment.VertexBufferStride);

		[GeometryDescriptors addObject: GeometryDescriptor];
		SegmentIndex++;
	}

	// Populate Acceleration Structure Descriptor.
	MTL::AccelerationStructureUsage Usage = MTL::AccelerationStructureUsage::None;

	if (Initializer.bAllowUpdate)
		Usage = MTL::AccelerationStructureUsageRefit;
	else if (Initializer.bFastBuild)
		Usage = MTL::AccelerationStructureUsagePreferFastBuild;

	AccelerationStructureDescriptor.SetUsage(Usage);
	AccelerationStructureDescriptor.SetGeometryDescriptors((__bridge NSArray*)GeometryDescriptors);

	// Explicity retain the descriptor (will be re-used for refit and compaction).
	[AccelerationStructureDescriptor retain];
}

static FRayTracingAccelerationStructureSize CalcRayTracingGeometrySize(MTL::AccelerationStructureDescriptor* AccelerationStructureDescriptor)
{
	// Fill and return the descriptor.
	FMetalDeviceContext& Context = GetMetalDeviceContext();
	MTL::Device* Device = Context.GetDevice();

	MTL::AccelerationStructureSizes DescriptorSize = Device.AccelerationStructureSizesWithDescriptor(AccelerationStructureDescriptor);

	FRayTracingAccelerationStructureSize SizeInfo = {};
	SizeInfo.ResultSize = Align(DescriptorSize.accelerationStructureSize, GRHIRayTracingAccelerationStructureAlignment);
	SizeInfo.BuildScratchSize = Align(DescriptorSize.buildScratchBufferSize, GRHIRayTracingScratchBufferAlignment);
	SizeInfo.UpdateScratchSize = Align(DescriptorSize.refitScratchBufferSize, GRHIRayTracingScratchBufferAlignment);

	return SizeInfo;
}

FRayTracingAccelerationStructureSize FMetalDynamicRHI::RHICalcRayTracingGeometrySize(FRHICommandListBase& RHICmdList, const FRayTracingGeometryInitializer& Initializer)
{
    MTL_SCOPED_AUTORELEASE_POOL;
	
	MTL::PrimitiveAccelerationStructureDescriptor AccelerationStructureDescriptor = MTL::PrimitiveAccelerationStructureDescriptor();
	NSMutableArray<MTLAccelerationStructureGeometryDescriptor*>* GeometryDescriptors = [[NSMutableArray<MTLAccelerationStructureGeometryDescriptor*> new] init];
	FillPrimitiveAccelerationStructureDesc(AccelerationStructureDescriptor, Initializer, GeometryDescriptors);
	[GeometryDescriptors release];

	return CalcRayTracingGeometrySize(AccelerationStructureDescriptor);
}

FRayTracingAccelerationStructureSize FMetalDynamicRHI::RHICalcRayTracingSceneSize(uint32 MaxInstances, ERayTracingAccelerationStructureFlags Flags)
{
	// TODO: Do we need to take in account the flags provided by the function call?
	// TODO: Can we get away with the instance count only? (works on AS; what about AMD?)
    MTL_SCOPED_AUTORELEASE_POOL;
    
    MTL::InstanceAccelerationStructureDescriptor InstanceDescriptor = MTL::InstanceAccelerationStructureDescriptor();
    InstanceDescriptor.SetInstanceCount(MaxInstances);

    return CalcRayTracingGeometrySize(InstanceDescriptor);
}

FMetalRayTracingGeometry::FMetalRayTracingGeometry(FRHICommandListBase& RHICmdList, const FRayTracingGeometryInitializer& InInitializer)
	: FRHIRayTracingGeometry(InInitializer)
	, bHasPendingCompactionRequests(false)
{
	uint32 IndexBufferStride = 0;

	if (Initializer.IndexBuffer)
	{
		// In case index buffer in initializer is not yet in valid state during streaming we assume the geometry is using UINT32 format.
		IndexBufferStride = Initializer.IndexBuffer->GetSize() > 0
			? Initializer.IndexBuffer->GetStride()
			: 4;
	}
	checkf(!Initializer.IndexBuffer || (IndexBufferStride == 2 || IndexBufferStride == 4), TEXT("Index buffer must be 16 or 32 bit if in use."));

	GeomArray = [NSMutableArray arrayWithCapacity:Initializer.Segments.Num()];

	AccelerationStructureDescriptor = MTL::PrimitiveAccelerationStructureDescriptor();
	RebuildDescriptors();

	// NOTE: We do not use the RHI API in order to avoid re-filling another descriptor.
	SizeInfo = CalcRayTracingGeometrySize(AccelerationStructureDescriptor);

	AccelerationStructureIndex = 0;

	// If this RayTracingGeometry going to be used as streaming destination
	// we don't want to allocate its memory as it will be replaced later by streamed version
	// but we still need correct SizeInfo as it is used to estimate its memory requirements outside of RHI.
	if (Initializer.Type == ERayTracingGeometryInitializerType::StreamingDestination)
	{
		return;
	}

	FString DebugNameString = Initializer.DebugName.ToString();
	FRHIResourceCreateInfo BlasBufferCreateInfo(*DebugNameString);
	for (uint32 i = 0; i < MaxNumAccelerationStructure; i++)
	{
		AccelerationStructure[i] = ResourceCast(RHICmdList.CreateBuffer(SizeInfo.ResultSize, BUF_AccelerationStructure, 0, ERHIAccess::BVHWrite, BlasBufferCreateInfo).GetReference());
		check(AccelerationStructure[i]);

		AccelerationStructure[i]->AccelerationStructureHandle.SetLabel(TCHAR_TO_ANSI(*DebugNameString));
	}
}

FMetalRayTracingGeometry::~FMetalRayTracingGeometry()
{
	ReleaseUnderlyingResource();
}

void FMetalRayTracingGeometry::ReleaseUnderlyingResource()
{
	[GeomArray removeAllObjects];
	[GeomArray release];

	for (uint32 i = 0; i < MaxNumAccelerationStructure; i++)
	{
		AccelerationStructure[i].SafeRelease();
		AccelerationStructure[i] = nullptr;
	}
}

void FMetalRayTracingGeometry::SetInitializer(const FRayTracingGeometryInitializer& InInitializer)
{
	checkf(InitializedType == ERayTracingGeometryInitializerType::StreamingDestination, TEXT("Only FMetalRayTracingGeometry that was created as StreamingDestination can update their initializer."));
	Initializer = InInitializer;

	// HitGroup Parameters Update is handled by the Scene
}

void FMetalRayTracingGeometry::Swap(FMetalRayTracingGeometry& Other)
{
	::Swap(AccelerationStructureDescriptor, Other.AccelerationStructureDescriptor);
	for (uint32 i = 0; i < MaxNumAccelerationStructure; i++)
	{
		::Swap(AccelerationStructure[i], Other.AccelerationStructure[i]);
	}
	::Swap(AccelerationStructureIndex, Other.AccelerationStructureIndex);
}

void FMetalRayTracingGeometry::RemoveCompactionRequest()
{
	if (bHasPendingCompactionRequests)
	{
		check(GetAccelerationStructureRead());
		bool bRequestFound = GetMetalDeviceContext().GetRayTracingCompactionRequestHandler()->ReleaseRequest(this);
		check(bRequestFound);
		bHasPendingCompactionRequests = false;
	}
}

void FMetalRayTracingGeometry::RebuildDescriptors()
{
	[GeomArray removeAllObjects];

	FMetalDeviceContext& Context = GetMetalDeviceContext();
	MTL::Device* Device = Context.GetDevice();

	AccelerationStructureDescriptor = MTL::PrimitiveAccelerationStructureDescriptor();
	FillPrimitiveAccelerationStructureDesc(AccelerationStructureDescriptor, Initializer, GeomArray);
}

FMetalRayTracingScene::FMetalRayTracingScene(FRayTracingSceneInitializer2 InInitializer)
	: Initializer(MoveTemp(InInitializer))
{
	const ERayTracingAccelerationStructureFlags BuildFlags = ERayTracingAccelerationStructureFlags::FastTrace; // #yuriy_todo: pass this in

	SizeInfo = {};

	const uint32 NumLayers = Initializer.NumNativeInstancesPerLayer.Num();
	check(NumLayers > 0);

	Layers.SetNum(NumLayers);

	for (uint32 LayerIndex = 0; LayerIndex < NumLayers; ++LayerIndex)
	{
		FLayerData& Layer = Layers[LayerIndex];

		MTL::InstanceAccelerationStructureDescriptor* InstanceDescriptor;
		InstanceDescriptor.SetInstanceCount(Initializer.NumNativeInstancesPerLayer[LayerIndex]);

		Layer.SizeInfo = CalcRayTracingGeometrySize(InstanceDescriptor);

		Layer.BufferOffset = Align(SizeInfo.ResultSize, GRHIRayTracingAccelerationStructureAlignment);
		Layer.ScratchBufferOffset = Align(SizeInfo.BuildScratchSize, GRHIRayTracingScratchBufferAlignment);

		SizeInfo.ResultSize = Layer.BufferOffset + Layer.SizeInfo.ResultSize;
		SizeInfo.BuildScratchSize = Layer.BufferOffset + Layer.SizeInfo.BuildScratchSize;
	}

	MutableAccelerationStructures = [[NSMutableArray<id<MTLAccelerationStructure>> new] init];

	const uint32 ParameterBufferSize = FMath::Max<uint32>(1, Initializer.NumTotalSegments) * sizeof(FHitGroupSystemRootConstants);
	FRHIResourceCreateInfo ParameterBufferCreateInfo(TEXT("RayTracingSceneMetadata"));
	//PerInstanceGeometryParameterBuffer = ResourceCast(FRHICommandListExecutor::GetImmediateCommandList().CreateBuffer(ParameterBufferSize,  BUF_StructuredBuffer | BUF_ShaderResource, sizeof(FHitGroupSystemRootConstants), ERHIAccess::SRVMask, ParameterBufferCreateInfo).GetReference());

	//FShaderResourceViewInitializer ViewInitializer(PerInstanceGeometryParameterBuffer);
	//PerInstanceGeometryParameterSRV = new FMetalShaderResourceView(ViewInitializer);
}

FMetalRayTracingScene::~FMetalRayTracingScene()
{
	PerInstanceGeometryParameters.Empty();
	AccelerationStructureBuffer.SafeRelease();
    //PerInstanceGeometryParameterBuffer.SafeRelease();
	//PerInstanceGeometryParameterSRV.SafeRelease();
	InstanceBufferSRV.SafeRelease();

	[MutableAccelerationStructures removeAllObjects];
	[MutableAccelerationStructures release];
}

void FMetalRayTracingScene::BindBuffer(FRHIBuffer* InBuffer, uint32 InBufferOffset)
{
	check(IsInRHIThread() || !IsRunningRHIInSeparateThread());
	check(SizeInfo.ResultSize + InBufferOffset <= InBuffer->GetSize());

	AccelerationStructureBuffer = ResourceCast(InBuffer);

	FMetalDeviceContext& Context = GetMetalDeviceContext();
	MTL::Device* Device = Context.GetDevice();

	for (auto& Layer : Layers)
	{
		checkf(Layer.ShaderResourceView == nullptr, TEXT("Binding multiple buffers is not currently supported."));

		const uint32 LayerOffset = InBufferOffset + Layer.BufferOffset;
		check(LayerOffset % GRHIRayTracingAccelerationStructureAlignment == 0);

		check(AccelerationStructureBuffer->IsAccelerationStructure());

		FShaderResourceViewInitializer ViewInitializer(AccelerationStructureBuffer, LayerOffset, 0);
		Layer.ShaderResourceView = new FMetalShaderResourceView(ViewInitializer);

		FString DebugNameString = Initializer.DebugName.ToString();
		DebugNameString = (DebugNameString.IsEmpty()) ? TEXT("TLAS") : DebugNameString;

		AccelerationStructureBuffer->AccelerationStructureHandle.SetLabel(TCHAR_TO_ANSI(*DebugNameString));
	}
}

void FMetalRayTracingScene::BuildPerInstanceGeometryParameterBuffer()
{
	[MutableAccelerationStructures removeAllObjects];
	PerInstanceGeometryParameters.Empty();

	const uint32 ParameterBufferSize = FMath::Max<uint32>(1, Initializer.NumTotalSegments) * sizeof(FHitGroupSystemRootConstants);
	//check(PerInstanceGeometryParameterBuffer->GetSize() >= ParameterBufferSize);

	check(IsInRHIThread() || !IsRunningRHIInSeparateThread());

	//void* MappedBuffer = PerInstanceGeometryParameterBuffer->Lock(true, RLM_WriteOnly, 0, ParameterBufferSize);
	//FHitGroupSystemRootConstants* MappedParameters = reinterpret_cast<FHitGroupSystemRootConstants*>(MappedBuffer);
	uint32 ParameterIndex = 0;

	for (FRHIRayTracingGeometry* GeometryRHI : Initializer.PerInstanceGeometries)
	{
		FMetalRayTracingGeometry* Geometry = ResourceCast(GeometryRHI);
		Geometry->SceneIndex = ParameterIndex;

		MTL::AccelerationStructure* AS = Geometry->GetAccelerationStructureRead()->AccelerationStructureHandle;
		[MutableAccelerationStructures addObject:AS];
		InstanceBufferSRV->ReferencedResources.Add(TTuple<MTL::Resource*, MTL::ResourceUsage>((MTL::Resource*)(AS), MTL::ResourceUsageRead));

		const FRayTracingGeometryInitializer& GeometryInitializer = Geometry->GetInitializer();

		FMetalRHIBuffer* IndexBuffer = ResourceCast(GeometryInitializer.IndexBuffer.GetReference());
		const uint32 IndexStride = IndexBuffer ? IndexBuffer->GetStride() : 0;
		const uint32 IndexOffsetInBytes = GeometryInitializer.IndexBufferOffset;

		for (const FRayTracingGeometrySegment& Segment : GeometryInitializer.Segments)
		{
			FMetalRHIBuffer* VertexBuffer = ResourceCast(Segment.VertexBuffer.GetReference());
			checkf(VertexBuffer, TEXT("All ray tracing geometry segments must have a valid vertex buffer"));

			FHitGroupSystemRootConstants SysRootConstants;
			SysRootConstants.SetVertexAndIndexStride(Segment.VertexBufferStride, IndexStride);
			SysRootConstants.UserData = 0;
			SysRootConstants.BaseInstanceIndex = 0;

			FMetalRayTracingGeometryParameters SegmentParameters;
			if (IndexStride)
			{
				SysRootConstants.IndexBufferOffsetInBytes = IndexOffsetInBytes + IndexStride * Segment.FirstPrimitive * FMetalRayTracingGeometry::IndicesPerPrimitive;
				SegmentParameters.IndexBuffer = IndexBuffer;
			}
			else
			{
				SysRootConstants.IndexBufferOffsetInBytes = 0;
				SegmentParameters.IndexBuffer = nullptr;
			}

			SegmentParameters.VertexBuffer = VertexBuffer;
			SegmentParameters.RootConstantsBufferOffsetInBytes = ParameterIndex * sizeof(FHitGroupSystemRootConstants);
			SegmentParameters.VertexBufferOffset = Segment.VertexBufferOffset;

			PerInstanceGeometryParameters.Add(SegmentParameters);

			check(ParameterIndex < Initializer.NumTotalSegments);
			//MappedParameters[ParameterIndex] = SysRootConstants;
			ParameterIndex++;
		}
	}

	check(ParameterIndex == Initializer.NumTotalSegments);
	//PerInstanceGeometryParameterBuffer->Unlock();
}

void FMetalRayTracingScene::BuildAccelerationStructure(
		FMetalRHICommandContext& CommandContext,
		FMetalRHIBuffer* InScratchBuffer, uint32 ScratchOffset,
		FMetalRHIBuffer* InstanceBuffer, uint32 InstanceOffset)
{
	check(AccelerationStructureBuffer.IsValid());
	check(InstanceBuffer != nullptr);

	FMetalBuffer CurInstanceBuffer = InstanceBuffer->GetCurrentBuffer();
	check(CurInstanceBuffer);

	uint32 InstanceBufferOffset = InstanceOffset + static_cast<uint32>(CurInstanceBuffer.GetOffset());

	// Create SRV first (since we collect BLAS to map in BuildPerInstanceGeometryParameterBuffer()).
	FShaderResourceViewInitializer ViewInitializer(InstanceBuffer, InstanceOffset, 0);
	InstanceBufferSRV = new FMetalShaderResourceView(ViewInitializer);

	BuildPerInstanceGeometryParameterBuffer();

	TRefCountPtr<FMetalRHIBuffer> ScratchBuffer;
	if (InScratchBuffer == nullptr)
	{
		FRHIResourceCreateInfo ScratchBufferCreateInfo(TEXT("BuildScratchTLAS"));
		ScratchBuffer = ResourceCast(RHICreateBuffer(SizeInfo.BuildScratchSize, BUF_UnorderedAccess | BUF_StructuredBuffer, 0, ERHIAccess::UAVCompute, ScratchBufferCreateInfo).GetReference());
		InScratchBuffer = ScratchBuffer.GetReference();
		ScratchOffset = 0;
	}

	FMetalDeviceContext& Context = GetMetalDeviceContext();
	MTL::Device* Device = Context.GetDevice();

	// Reset current renderpass to kick off acceleration structures build
	FMetalRenderPass& RenderPass = Context.GetCurrentRenderPass();
	FMetalCommandEncoder& Encoder = RenderPass.GetCurrentCommandEncoder();

	// Crappy workaround for inline raytracing: we must bind the TLAS instances descriptors to emulate a missing intrinsic (GetBindingRecordOffset).
	// Since inline RT uses the regular compute pipeline, we must do the binding in the regular path (in FMetalRenderPass).
	// Let's hope we'll be able to find a better way later...
	RenderPass.SetRayTracingInstanceBufferSRV(InstanceBufferSRV);

	Encoder.EndEncoding();
	Context.GetCurrentState().SetStateDirty();

	Encoder.BeginAccelerationStructureCommandEncoding();
	MTL::AccelerationStructureCommandEncoder* CommandEncoder = Encoder.GetAccelerationStructureCommandEncoder();
	check(CommandEncoder.GetPtr());

	const uint32 NumLayers = Initializer.NumNativeInstancesPerLayer.Num();

	FMetalBuffer CurScratchBuffer = InScratchBuffer->GetCurrentBuffer();
	check(CurScratchBuffer);

	check(Layers.Num() && NumLayers > 0);
	for (uint32 LayerIndex = 0; LayerIndex < NumLayers; ++LayerIndex)
	{
		FLayerData& Layer = Layers[LayerIndex];

		MTL::InstanceAccelerationStructureDescriptor* InstanceDescriptor = MTL::InstanceAccelerationStructureDescriptor();
		InstanceDescriptor.SetInstanceCount(Initializer.NumNativeInstancesPerLayer[LayerIndex]);
		InstanceDescriptor.SetInstanceDescriptorBuffer(CurInstanceBuffer);
		InstanceDescriptor.SetInstanceDescriptorBufferOffset(InstanceBufferOffset);
		InstanceDescriptor.SetInstancedAccelerationStructures((__bridge NSArray*)MutableAccelerationStructures);
		InstanceDescriptor.SetInstanceDescriptorStride(GRHIRayTracingInstanceDescriptorSize);
		InstanceDescriptor.SetInstanceDescriptorType(MTL::AccelerationStructureInstanceDescriptorType::UserID);

		MTL::AccelerationStructure* AS = ResourceCast(Layer.ShaderResourceView->GetBuffer())->AccelerationStructureHandle;
		CommandEncoder.BuildAccelerationStructure(AS, InstanceDescriptor, CurScratchBuffer, ScratchOffset);
	}

	Encoder.EndEncoding();
}


void FMetalRHICommandContext::RHIBuildAccelerationStructure(const FRayTracingSceneBuildParams& SceneBuildParams)
{
	FMetalRayTracingScene* const Scene = ResourceCast(SceneBuildParams.Scene);
	FMetalRHIBuffer* const ScratchBuffer = ResourceCast(SceneBuildParams.ScratchBuffer);
	FMetalRHIBuffer* const InstanceBuffer = ResourceCast(SceneBuildParams.InstanceBuffer);
	Scene->BuildAccelerationStructure(
		*this,
		ScratchBuffer, SceneBuildParams.ScratchBufferOffset,
		InstanceBuffer, SceneBuildParams.InstanceBufferOffset);
}

void FMetalDynamicRHI::RHITransferRayTracingGeometryUnderlyingResource(FRHICommandListBase& RHICmdList, FRHIRayTracingGeometry* DestGeometry, FRHIRayTracingGeometry* SrcGeometry)
{
	check(DestGeometry);
	FMetalRayTracingGeometry* Dest = ResourceCast(DestGeometry);

	if (!SrcGeometry)
	{
		Dest->ReleaseUnderlyingResource();
	}
	else
	{
		FMetalRayTracingGeometry* Src = ResourceCast(SrcGeometry);
		Dest->Swap(*Src);
	}
}

void FMetalRHICommandContext::RHIBuildAccelerationStructures(const TArrayView<const FRayTracingGeometryBuildParams> Params, const FRHIBufferRange& ScratchBufferRange)
{
	checkf(ScratchBufferRange.Buffer != nullptr, TEXT("BuildAccelerationStructures requires valid scratch buffer"));

	// Update geometry vertex buffers
	for (const FRayTracingGeometryBuildParams& P : Params)
	{
		FMetalRayTracingGeometry* const Geometry = ResourceCast(P.Geometry.GetReference());

		if (P.Segments.Num())
		{
			checkf(P.Segments.Num() == Geometry->Initializer.Segments.Num(),
				TEXT("If updated segments are provided, they must exactly match existing geometry segments. Only vertex buffer bindings may change."));

			for (int32 i = 0; i < P.Segments.Num(); ++i)
			{
				checkf(P.Segments[i].MaxVertices <= Geometry->Initializer.Segments[i].MaxVertices,
					TEXT("Maximum number of vertices in a segment (%u) must not be smaller than what was declared during FRHIRayTracingGeometry creation (%u), as this controls BLAS memory allocation."),
					P.Segments[i].MaxVertices, Geometry->Initializer.Segments[i].MaxVertices
				);

				Geometry->Initializer.Segments[i].VertexBuffer = P.Segments[i].VertexBuffer;
				Geometry->Initializer.Segments[i].VertexBufferElementType = P.Segments[i].VertexBufferElementType;
				Geometry->Initializer.Segments[i].VertexBufferStride = P.Segments[i].VertexBufferStride;
				Geometry->Initializer.Segments[i].VertexBufferOffset = P.Segments[i].VertexBufferOffset;
			}

			// We must update the descriptor if any segments have changed.
			Geometry->RebuildDescriptors();
		}
	}

	uint32 ScratchBufferSize = ScratchBufferRange.Size ? ScratchBufferRange.Size : ScratchBufferRange.Buffer->GetSize();

	checkf(ScratchBufferSize + ScratchBufferRange.Offset <= ScratchBufferRange.Buffer->GetSize(),
		TEXT("BLAS scratch buffer range size is %lld bytes with offset %lld, but the buffer only has %lld bytes. "),
		ScratchBufferRange.Size, ScratchBufferRange.Offset, ScratchBufferRange.Buffer->GetSize());

	const uint64 ScratchAlignment = GRHIRayTracingScratchBufferAlignment;
	FMetalRHIBuffer* ScratchBuffer = ResourceCast(ScratchBufferRange.Buffer);
	uint32 ScratchBufferOffset = static_cast<uint32>(ScratchBufferRange.Offset);

	MTL::Device* Device = Context->GetDevice();

	TArray<TTuple<FMetalRayTracingGeometry*, uint32>, TInlineAllocator<32>> GeometryToBuild;
	TArray<TTuple<FMetalRayTracingGeometry*, uint32>, TInlineAllocator<32>> GeometryToRefit;
	GeometryToBuild.Reserve(Params.Num());
	GeometryToRefit.Reserve(Params.Num());

	for (const FRayTracingGeometryBuildParams& P : Params)
	{
		FMetalRayTracingGeometry* const Geometry = ResourceCast(P.Geometry.GetReference());
		const bool bIsUpdate = P.BuildMode == EAccelerationStructureBuildMode::Update;

		uint64 ScratchBufferRequiredSize = bIsUpdate ? Geometry->SizeInfo.UpdateScratchSize : Geometry->SizeInfo.BuildScratchSize;
		checkf(ScratchBufferRequiredSize + ScratchBufferOffset <= ScratchBufferSize,
			TEXT("BLAS scratch buffer size is %ld bytes with offset %ld (%ld bytes available), but the build requires %lld bytes. "),
			ScratchBufferSize, ScratchBufferOffset, ScratchBufferSize - ScratchBufferOffset, ScratchBufferRequiredSize);

		if (!bIsUpdate)
		{
			GeometryToBuild.Add(TTuple<FMetalRayTracingGeometry*, uint32>(Geometry, ScratchBufferOffset));
		}
		else
		{
			GeometryToRefit.Add(TTuple<FMetalRayTracingGeometry*, uint32>(Geometry, ScratchBufferOffset));
		}

		ScratchBufferOffset = Align(ScratchBufferOffset + ScratchBufferRequiredSize, ScratchAlignment);

		// TODO: Add a CVAR to toggle validation (e.g. r.Metal.RayTracingValidate).
		//FRayTracingValidateGeometryBuildParamsCS::Dispatch(FRHICommandListExecutor::GetImmediateCommandList(), P);
	}

	FMetalBuffer ScratchBufferRes = ScratchBuffer->GetCurrentBufferOrNil();
	check(ScratchBufferRes);

	// Submit build commands.
	FMetalRenderPass& RenderPass = Context->GetCurrentRenderPass();
	FMetalCommandEncoder& Encoder = RenderPass.GetCurrentCommandEncoder();
	Encoder.EndEncoding();
	Context->GetCurrentState().SetStateDirty();

	Encoder.BeginAccelerationStructureCommandEncoding();
	MTL::AccelerationStructureCommandEncoder* CommandEncoder = Encoder.GetAccelerationStructureCommandEncoder();
	check(CommandEncoder.GetPtr());

	for (TTuple<FMetalRayTracingGeometry*, uint32>& BuildRequest : GeometryToBuild)
	{
		FMetalRayTracingGeometry* Geometry = BuildRequest.Key;
		uint32 ScratchOffset = BuildRequest.Value;

		CommandEncoder.BuildAccelerationStructure(
			Geometry->GetAccelerationStructureRead()->AccelerationStructureHandle,
			Geometry->AccelerationStructureDescriptor,
			ScratchBufferRes,
			ScratchOffset
		);
	}

	for (TTuple<FMetalRayTracingGeometry*, uint32>& RefitRequest : GeometryToRefit)
	{
		FMetalRayTracingGeometry* Geometry = RefitRequest.Key;
		uint32 ScratchOffset = RefitRequest.Value;

		MTL::AccelerationStructure* SrcBLAS = Geometry->GetAccelerationStructureRead()->AccelerationStructureHandle;
		MTL::AccelerationStructure* DstBLAS = Geometry->GetAccelerationStructureWrite()->AccelerationStructureHandle;

		CommandEncoder.RefitAccelerationStructure(
			SrcBLAS,
			Geometry->AccelerationStructureDescriptor,
			&DstBLAS,
			ScratchBufferRes,
			ScratchOffset
		);

		Geometry->NextAccelerationStructure();
	}

	Encoder.EndEncoding();

	for (const FRayTracingGeometryBuildParams& P : Params)
	{
		FMetalRayTracingGeometry* const Geometry = ResourceCast(P.Geometry.GetReference());
		const bool bIsUpdate = P.BuildMode == EAccelerationStructureBuildMode::Update;

		if (!bIsUpdate)
		{
			ERayTracingAccelerationStructureFlags GeometryBuildFlags = GetRayTracingAccelerationStructureBuildFlags(Geometry->Initializer);
			if (ShouldCompactAfterBuild(GeometryBuildFlags))
			{
				GetMetalDeviceContext().GetRayTracingCompactionRequestHandler()->RequestCompact(Geometry);
				Geometry->bHasPendingCompactionRequests = true;
			}
		}
	}
}

void FMetalRHICommandContext::RHIBindAccelerationStructureMemory(FRHIRayTracingScene* InScene, FRHIBuffer* InBuffer, uint32 InBufferOffset)
{
	FMetalRayTracingScene* MetalScene = ResourceCast(InScene);
	MetalScene->BindBuffer(InBuffer, InBufferOffset);
}

void FMetalRHICommandContext::RHIClearRayTracingBindings(FRHIRayTracingScene* Scene)
{
	// TODO:
}

void FMetalRHICommandContext::RHIRayTraceDispatch(FRHIRayTracingPipelineState* RayTracingPipelineState, FRHIRayTracingShader* RayGenShader,
	   FRHIRayTracingScene* SceneRHI,
	   const FRayTracingShaderBindings& GlobalResourceBindings,
	   uint32 Width, uint32 Height)
{
	checkNoEntry();
}

void FMetalRHICommandContext::RHIRayTraceDispatchIndirect(FRHIRayTracingPipelineState* RayTracingPipelineState, FRHIRayTracingShader* RayGenShader,
	FRHIRayTracingScene* SceneRHI,
	const FRayTracingShaderBindings& GlobalResourceBindings,
	FRHIBuffer* ArgumentBuffer, uint32 ArgumentOffset)
{
	checkNoEntry();
}

void FMetalRHICommandContext::RHISetRayTracingHitGroup(
	   FRHIRayTracingScene* InScene, uint32 InstanceIndex, uint32 SegmentIndex, uint32 ShaderSlot,
	   FRHIRayTracingPipelineState* InPipeline, uint32 HitGroupIndex,
	   uint32 NumUniformBuffers, FRHIUniformBuffer* const* UniformBuffers,
	   uint32 LooseParameterDataSize, const void* LooseParameterData,
	   uint32 UserData)
{
	checkNoEntry();
}

void FMetalRHICommandContext::RHISetRayTracingCallableShader(
	   FRHIRayTracingScene* Scene, uint32 ShaderSlotInScene,
	   FRHIRayTracingPipelineState* Pipeline, uint32 ShaderIndexInPipeline,
	   uint32 NumUniformBuffers, FRHIUniformBuffer* const* UniformBuffers,
	   uint32 UserData)
{
	checkNoEntry();
}

void FMetalRHICommandContext::RHISetRayTracingMissShader(
	   FRHIRayTracingScene* InScene, uint32 ShaderSlotInScene,
	   FRHIRayTracingPipelineState* InPipeline, uint32 ShaderIndexInPipeline,
	   uint32 NumUniformBuffers, FRHIUniformBuffer* const* UniformBuffers,
	   uint32 UserData)
{
	checkNoEntry();
}

void FMetalRHICommandContext::RHISetRayTracingBindings(
	FRHIRayTracingScene* InScene, FRHIRayTracingPipelineState* InPipeline,
	uint32 NumBindings, const FRayTracingLocalShaderBindings* Bindings,
	ERayTracingBindingType BindingType)
{
	checkNoEntry();
}

FRayTracingSceneRHIRef FMetalDynamicRHI::RHICreateRayTracingScene(FRayTracingSceneInitializer2 Initializer)
{
    MTL_SCOPED_AUTORELEASE_POOL;
    return new FMetalRayTracingScene(MoveTemp(Initializer));
}

FRayTracingGeometryRHIRef FMetalDynamicRHI::RHICreateRayTracingGeometry(FRHICommandListBase& RHICmdList, const FRayTracingGeometryInitializer& Initializer)
{
    MTL_SCOPED_AUTORELEASE_POOL;
    return new FMetalRayTracingGeometry(RHICmdList, Initializer);
}

FRayTracingPipelineStateRHIRef FMetalDynamicRHI::RHICreateRayTracingPipelineState(const FRayTracingPipelineStateInitializer& Initializer)
{
	checkNoEntry();
	return nullptr;
}

void FMetalDeviceContext::InitializeRayTracing()
{
	// Explicitly request a pointer to the DeviceContext since the CompactionHandler
	// is initialized before the global getter is setup.
	RayTracingCompactionRequestHandler = new FMetalRayTracingCompactionRequestHandler(this);
}

void FMetalDeviceContext::UpdateRayTracing()
{
	RayTracingCompactionRequestHandler->Update(this);
}

void FMetalDeviceContext::CleanUpRayTracing()
{
	delete RayTracingCompactionRequestHandler;
}
#endif // METAL_RHI_RAYTRACING
