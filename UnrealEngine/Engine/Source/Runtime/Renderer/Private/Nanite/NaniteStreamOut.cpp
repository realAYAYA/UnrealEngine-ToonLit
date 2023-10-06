// Copyright Epic Games, Inc. All Rights Reserved.

#include "NaniteStreamOut.h"

#include "Rendering/NaniteStreamingManager.h"

#include "NaniteShared.h"

#include "PrimitiveSceneInfo.h"
#include "SceneInterface.h"

#include "ShaderParameterStruct.h"
#include "GlobalShader.h"
#include "RenderGraphUtils.h"

#include "ScenePrivate.h"

DECLARE_GPU_STAT(NaniteStreamOutData);

static bool GNaniteStreamOutCacheTraversalData = true;
static FAutoConsoleVariableRef CVarNaniteStreamOutCacheTraversalData(
	TEXT("r.Nanite.StreamOut.CacheTraversalData"),
	GNaniteStreamOutCacheTraversalData,
	TEXT("Cache traversal data during count pass to be able to skip traversal during stream out pass."),
	ECVF_RenderThreadSafe
);

static const uint32 CandididateClusterSizeInUints = 3;

namespace Nanite
{
	BEGIN_SHADER_PARAMETER_STRUCT(FQueueParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FQueuePassState>, QueueState)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWByteAddressBuffer, NodesAndClusterBatches)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWByteAddressBuffer, CandididateClusters)
		SHADER_PARAMETER(uint32, MaxNodes)
		SHADER_PARAMETER(uint32, MaxCandidateClusters)
	END_SHADER_PARAMETER_STRUCT()

	class FInitClusterBatchesCS : public FNaniteGlobalShader
	{
		DECLARE_GLOBAL_SHADER(FInitClusterBatchesCS);
		SHADER_USE_PARAMETER_STRUCT(FInitClusterBatchesCS, FNaniteGlobalShader);

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			SHADER_PARAMETER_RDG_BUFFER_UAV(RWByteAddressBuffer, NodesAndClusterBatches)
			SHADER_PARAMETER(uint32, MaxCandidateClusters)
			SHADER_PARAMETER(uint32, MaxNodes)
		END_SHADER_PARAMETER_STRUCT()
	};
	IMPLEMENT_GLOBAL_SHADER(FInitClusterBatchesCS, "/Engine/Private/Nanite/NaniteStreamOut.usf", "InitClusterBatches", SF_Compute);

	class FInitCandidateNodesCS : public FNaniteGlobalShader
	{
		DECLARE_GLOBAL_SHADER(FInitCandidateNodesCS);
		SHADER_USE_PARAMETER_STRUCT(FInitCandidateNodesCS, FNaniteGlobalShader);

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			SHADER_PARAMETER_RDG_BUFFER_UAV(RWByteAddressBuffer, NodesAndClusterBatches)
			SHADER_PARAMETER(uint32, MaxCandidateClusters)
			SHADER_PARAMETER(uint32, MaxNodes)
		END_SHADER_PARAMETER_STRUCT()
	};
	IMPLEMENT_GLOBAL_SHADER(FInitCandidateNodesCS, "/Engine/Private/Nanite/NaniteStreamOut.usf", "InitCandidateNodes", SF_Compute);

	class FInitQueueCS : public FNaniteGlobalShader
	{
		DECLARE_GLOBAL_SHADER(FInitQueueCS);
		SHADER_USE_PARAMETER_STRUCT(FInitQueueCS, FNaniteGlobalShader);

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			SHADER_PARAMETER_STRUCT_INCLUDE(FQueueParameters, QueueParameters)

			SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer, StreamOutRequests)
			SHADER_PARAMETER(uint32, NumRequests)

			SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, MeshDataBuffer)

			SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, VertexAndIndexAllocator)
			SHADER_PARAMETER(uint32, CurrentAllocationFrameIndex)
			SHADER_PARAMETER(uint32, NumAllocationFrames)
			SHADER_PARAMETER(uint32, VertexBufferSize)
			SHADER_PARAMETER(uint32, IndexBufferSize)
		END_SHADER_PARAMETER_STRUCT()

		class FAllocateRangesDim : SHADER_PERMUTATION_BOOL("ALLOCATE_VERTICES_AND_TRIANGLES_RANGES");
		using FPermutationDomain = TShaderPermutationDomain<FAllocateRangesDim>;
	};
	IMPLEMENT_GLOBAL_SHADER(FInitQueueCS, "/Engine/Private/Nanite/NaniteStreamOut.usf", "InitQueue", SF_Compute);

	struct FNaniteStreamOutTraversalCS : public FNaniteGlobalShader
	{
		DECLARE_GLOBAL_SHADER(FNaniteStreamOutTraversalCS);
		SHADER_USE_PARAMETER_STRUCT(FNaniteStreamOutTraversalCS, FNaniteGlobalShader);

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneUniformParameters, Scene)

			SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, HierarchyBuffer)
			SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, ClusterPageData)
			SHADER_PARAMETER(FIntVector4, PageConstants)

			SHADER_PARAMETER_STRUCT_INCLUDE(FQueueParameters, QueueParameters)

			SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, AuxiliaryDataBufferRW)

			SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, MeshDataBuffer)
			SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<float>, VertexBuffer)
			SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, IndexBuffer)

			SHADER_PARAMETER_RDG_BUFFER_UAV(RWByteAddressBuffer, OutputClustersRW)
			SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, OutputClustersStateRW)

			SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer, StreamOutRequests)
			SHADER_PARAMETER(uint32, NumRequests)

			SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer, SegmentMappingBuffer)

			SHADER_PARAMETER(float, StreamOutCutError)

			SHADER_PARAMETER_STRUCT_INCLUDE(ShaderPrint::FShaderParameters, ShaderPrint)
		END_SHADER_PARAMETER_STRUCT()

		class FCountVerticesAndTrianglesDim : SHADER_PERMUTATION_BOOL("NANITE_STREAM_OUT_COUNT_VERTICES_AND_TRIANGLES");
		class FCacheClustersDim : SHADER_PERMUTATION_BOOL("NANITE_STREAM_OUT_CACHE_CLUSTERS");
		using FPermutationDomain = TShaderPermutationDomain<FCountVerticesAndTrianglesDim, FCacheClustersDim>;

		static inline void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
		{
			FNaniteGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

			OutEnvironment.SetDefine(TEXT("NANITE_HIERARCHY_TRAVERSAL"), 1);

			OutEnvironment.SetDefine(TEXT("VF_SUPPORTS_PRIMITIVE_SCENE_DATA"), 1);
		}
	};
	IMPLEMENT_GLOBAL_SHADER(FNaniteStreamOutTraversalCS, "/Engine/Private/Nanite/NaniteStreamOut.usf", "NaniteStreamOutTraversalCS", SF_Compute);

	class FAllocateRangesCS : public FNaniteGlobalShader
	{
		DECLARE_GLOBAL_SHADER(FAllocateRangesCS);
		SHADER_USE_PARAMETER_STRUCT(FAllocateRangesCS, FNaniteGlobalShader);

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer, StreamOutRequests)
			SHADER_PARAMETER(uint32, NumRequests)

			SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, MeshDataBuffer)

			SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, VertexAndIndexAllocator)
			SHADER_PARAMETER(uint32, CurrentAllocationFrameIndex)
			SHADER_PARAMETER(uint32, NumAllocationFrames)
			SHADER_PARAMETER(uint32, VertexBufferSize)
			SHADER_PARAMETER(uint32, IndexBufferSize)

			SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, OutputClustersStateRW)
			SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, StreamOutDispatchIndirectArgsRW)

			SHADER_PARAMETER_STRUCT_INCLUDE(ShaderPrint::FShaderParameters, ShaderPrint)
		END_SHADER_PARAMETER_STRUCT()
	};
	IMPLEMENT_GLOBAL_SHADER(FAllocateRangesCS, "/Engine/Private/Nanite/NaniteStreamOut.usf", "AllocateRangesCS", SF_Compute);

	struct FNaniteStreamOutCS : public FNaniteGlobalShader
	{
		DECLARE_GLOBAL_SHADER(FNaniteStreamOutCS);
		SHADER_USE_PARAMETER_STRUCT(FNaniteStreamOutCS, FNaniteGlobalShader);

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, ClusterPageData)
			SHADER_PARAMETER(FIntVector4, PageConstants)

			SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, AuxiliaryDataBufferRW)

			SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, MeshDataBuffer)
			SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<float>, VertexBuffer)
			SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, IndexBuffer)

			SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, OutputClusters)
			SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, OutputClustersStateRW)

			SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer, StreamOutRequests)
			SHADER_PARAMETER(uint32, NumRequests)

			SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer, SegmentMappingBuffer)

			SHADER_PARAMETER_STRUCT_INCLUDE(ShaderPrint::FShaderParameters, ShaderPrint)

			RDG_BUFFER_ACCESS(IndirectArgs, ERHIAccess::IndirectArgs)
		END_SHADER_PARAMETER_STRUCT()

		static inline void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
		{
			FNaniteGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		}
	};
	IMPLEMENT_GLOBAL_SHADER(FNaniteStreamOutCS, "/Engine/Private/Nanite/NaniteStreamOut.usf", "NaniteStreamOutCS", SF_Compute);

	static void AddPassInitNodesAndClusterBatchesUAV(FRDGBuilder& GraphBuilder, FGlobalShaderMap* ShaderMap, FRDGBufferUAVRef UAVRef, uint32 MaxNodes, uint32 MaxCandidateClusters, uint32 MaxCullingBatches)
	{
		LLM_SCOPE_BYTAG(Nanite);

		{
			FInitCandidateNodesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FInitCandidateNodesCS::FParameters>();
			PassParameters->NodesAndClusterBatches = UAVRef;
			PassParameters->MaxCandidateClusters = MaxCandidateClusters;
			PassParameters->MaxNodes = MaxNodes;

			auto ComputeShader = ShaderMap->GetShader<FInitCandidateNodesCS>();
			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("NaniteStreamOut::InitNodes"),
				ComputeShader,
				PassParameters,
				FComputeShaderUtils::GetGroupCountWrapped(MaxNodes, 64)
			);
		}

		{
			FInitClusterBatchesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FInitClusterBatchesCS::FParameters>();
			PassParameters->NodesAndClusterBatches = UAVRef;
			PassParameters->MaxCandidateClusters = MaxCandidateClusters;
			PassParameters->MaxNodes = MaxNodes;

			auto ComputeShader = ShaderMap->GetShader<FInitClusterBatchesCS>();
			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("NaniteStreamOut::InitCullingBatches"),
				ComputeShader,
				PassParameters,
				FComputeShaderUtils::GetGroupCountWrapped(MaxCullingBatches, 64)
			);
		}
	}

	static void AddInitQueuePass(
		FRDGBuilder& GraphBuilder,
		FGlobalShaderMap* ShaderMap,
		FQueueParameters& QueueParameters,
		FRDGBufferSRVRef RequestsDataSRV,
		uint32 NumRequests,
		bool bAllocateRanges,
		FRDGBufferUAVRef MeshDataBufferUAV,
		FRDGBufferUAVRef VertexAndIndexAllocatorUAV,
		uint32 CurrentAllocationFrameIndex,
		uint32 NumAllocationFrames,
		uint32 VertexBufferSize,
		uint32 IndexBufferSize)
	{
		// Reset queue to empty state
		AddClearUAVPass(GraphBuilder, QueueParameters.QueueState, 0);

		// Init queue with requests
		{
			FInitQueueCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FInitQueueCS::FParameters>();
			PassParameters->QueueParameters = QueueParameters;

			PassParameters->StreamOutRequests = RequestsDataSRV;
			PassParameters->NumRequests = NumRequests;

			PassParameters->MeshDataBuffer = MeshDataBufferUAV;

			PassParameters->VertexAndIndexAllocator = VertexAndIndexAllocatorUAV;
			PassParameters->CurrentAllocationFrameIndex = CurrentAllocationFrameIndex;
			PassParameters->NumAllocationFrames = NumAllocationFrames;
			PassParameters->VertexBufferSize = VertexBufferSize;
			PassParameters->IndexBufferSize = IndexBufferSize;

			FInitQueueCS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FInitQueueCS::FAllocateRangesDim>(bAllocateRanges);

			auto ComputeShader = ShaderMap->GetShader<FInitQueueCS>(PermutationVector);

			FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("NaniteStreamOut::InitQueue"), ComputeShader, PassParameters, FComputeShaderUtils::GetGroupCountWrapped(NumRequests, 64));
		}
	}

	static void GetQueueParams(FRDGBuilder& GraphBuilder, FGlobalShaderMap* ShaderMap, TRefCountPtr<FRDGPooledBuffer>& NodesAndClusterBatchesBuffer, FQueueParameters& OutQueueParameters)
	{
		const uint32 MaxNodes = Nanite::FGlobalResources::GetMaxNodes();
		const uint32 MaxCullingBatches = Nanite::FGlobalResources::GetMaxClusterBatches();
		const uint32 MaxCandidateClusters = Nanite::FGlobalResources::GetMaxCandidateClusters();

		FRDGBufferRef QueueState = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32) + 2 * (6 * sizeof(uint32)), 1), TEXT("NaniteStreamOut.QueueState"));

		// Allocate buffer for nodes and cluster batches
		FRDGBufferRef NodesAndClusterBatchesBufferRDG = nullptr;
		if (NodesAndClusterBatchesBuffer.IsValid())
		{
			NodesAndClusterBatchesBufferRDG = GraphBuilder.RegisterExternalBuffer(NodesAndClusterBatchesBuffer, TEXT("NaniteStreamOut.NodesAndClusterBatchesBuffer"));
		}
		else
		{
			RDG_GPU_MASK_SCOPE(GraphBuilder, FRHIGPUMask::All());

			const uint32 CandididateNodeSizeInUints = 3;
			FRDGBufferDesc Desc = FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), MaxCullingBatches + MaxNodes * CandididateNodeSizeInUints);
			Desc.Usage = EBufferUsageFlags(Desc.Usage | BUF_ByteAddressBuffer);
			NodesAndClusterBatchesBufferRDG = GraphBuilder.CreateBuffer(Desc, TEXT("NaniteStreamOut.NodesAndClusterBatchesBuffer"));
			AddPassInitNodesAndClusterBatchesUAV(GraphBuilder, ShaderMap, GraphBuilder.CreateUAV(NodesAndClusterBatchesBufferRDG), MaxNodes, MaxCandidateClusters, MaxCullingBatches);
			NodesAndClusterBatchesBuffer = GraphBuilder.ConvertToExternalBuffer(NodesAndClusterBatchesBufferRDG);
		}

		// Allocate candidate cluster buffer
		FRDGBufferRef CandididateClustersBuffer = nullptr;
		{
			FRDGBufferDesc Desc = FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), MaxCandidateClusters * CandididateClusterSizeInUints);
			Desc.Usage = EBufferUsageFlags(Desc.Usage | BUF_ByteAddressBuffer);
			CandididateClustersBuffer = GraphBuilder.CreateBuffer(Desc, TEXT("NaniteStreamOut.CandididateClustersBuffer"));
		}

		OutQueueParameters.QueueState = GraphBuilder.CreateUAV(QueueState);
		OutQueueParameters.NodesAndClusterBatches = GraphBuilder.CreateUAV(NodesAndClusterBatchesBufferRDG);
		OutQueueParameters.CandididateClusters = GraphBuilder.CreateUAV(CandididateClustersBuffer);
		OutQueueParameters.MaxNodes = MaxNodes;
		OutQueueParameters.MaxCandidateClusters = MaxCandidateClusters;
	}

	void StreamOutData(
		FRDGBuilder& GraphBuilder,
		FGlobalShaderMap* ShaderMap,
		FSceneUniformBuffer &SceneUniformBuffer,
		TRefCountPtr<FRDGPooledBuffer>& NodesAndClusterBatchesBuffer,
		float CutError,
		uint32 NumRequests,
		FRDGBufferRef RequestBuffer,
		FRDGBufferRef SegmentMappingBuffer,
		FRDGBufferRef MeshDataBuffer,
		FRDGBufferRef AuxiliaryDataBuffer,
		FRDGBufferRef VertexBuffer,
		uint32 MaxNumVertices,
		FRDGBufferRef IndexBuffer,
		uint32 MaxNumIndices)
	{
		RDG_GPU_STAT_SCOPE(GraphBuilder, NaniteStreamOutData);

		FQueueParameters QueueParameters;
		GetQueueParams(GraphBuilder, ShaderMap, NodesAndClusterBatchesBuffer, QueueParameters);

		FRDGBufferRef VertexAndIndexAllocatorBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), 2), TEXT("NaniteStreamOut.VertexAndIndexAllocatorBuffer"));
		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(VertexAndIndexAllocatorBuffer), 0);

		const uint32 MaxCandidateClusters = Nanite::FGlobalResources::GetMaxCandidateClusters();

		// Allocate output cluster buffer
		FRDGBufferRef OutputClustersBuffer = nullptr;
		{
			FRDGBufferDesc Desc = FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), MaxCandidateClusters * CandididateClusterSizeInUints);
			Desc.Usage = EBufferUsageFlags(Desc.Usage | BUF_ByteAddressBuffer);
			OutputClustersBuffer = GraphBuilder.CreateBuffer(Desc, TEXT("NaniteStreamOut.OutputClustersBuffer"));
		}

		FRDGBufferRef OutputClustersStateBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), 2), TEXT("NaniteStreamOut.OutputClustersStateBuffer"));
		FRDGBufferUAVRef OutputClustersStateUAV = GraphBuilder.CreateUAV(OutputClustersStateBuffer);
		AddClearUAVPass(GraphBuilder, OutputClustersStateUAV, 0);

		AddInitQueuePass(
			GraphBuilder,
			ShaderMap,
			QueueParameters,
			GraphBuilder.CreateSRV(RequestBuffer),
			NumRequests,
			false,
			GraphBuilder.CreateUAV(MeshDataBuffer),
			GraphBuilder.CreateUAV(VertexAndIndexAllocatorBuffer),
			0,
			1,
			MaxNumVertices,
			MaxNumIndices);

		// count pass
		{
			auto* PassParameters = GraphBuilder.AllocParameters<FNaniteStreamOutTraversalCS::FParameters>();
			PassParameters->Scene = SceneUniformBuffer.GetBuffer(GraphBuilder);

			PassParameters->QueueParameters = QueueParameters;

			PassParameters->HierarchyBuffer = Nanite::GStreamingManager.GetHierarchySRV(GraphBuilder);
			PassParameters->ClusterPageData = Nanite::GStreamingManager.GetClusterPageDataSRV(GraphBuilder);
			PassParameters->PageConstants.X = 0;
			PassParameters->PageConstants.Y = Nanite::GStreamingManager.GetMaxStreamingPages();

			PassParameters->StreamOutRequests = GraphBuilder.CreateSRV(RequestBuffer);
			PassParameters->NumRequests = NumRequests;

			PassParameters->SegmentMappingBuffer = GraphBuilder.CreateSRV(SegmentMappingBuffer);

			PassParameters->AuxiliaryDataBufferRW = nullptr;

			PassParameters->MeshDataBuffer = GraphBuilder.CreateUAV(MeshDataBuffer);
			PassParameters->VertexBuffer = nullptr;
			PassParameters->IndexBuffer = nullptr;

			PassParameters->OutputClustersRW = GraphBuilder.CreateUAV(OutputClustersBuffer);
			PassParameters->OutputClustersStateRW = OutputClustersStateUAV;
			
			PassParameters->StreamOutCutError = CutError;

			ShaderPrint::SetParameters(GraphBuilder, PassParameters->ShaderPrint);

			FNaniteStreamOutTraversalCS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FNaniteStreamOutTraversalCS::FCountVerticesAndTrianglesDim>(true);
			PermutationVector.Set<FNaniteStreamOutTraversalCS::FCacheClustersDim>(GNaniteStreamOutCacheTraversalData);

			auto ComputeShader = ShaderMap->GetShader<FNaniteStreamOutTraversalCS>(PermutationVector);

			const FIntVector GroupCount = FIntVector(GRHIPersistentThreadGroupCount, 1, 1);
			FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("NaniteStreamOut::CountVerticesAndTriangles"), ComputeShader, PassParameters, GroupCount);
		}

		// write pass
		if(!GNaniteStreamOutCacheTraversalData)
		{
			AddInitQueuePass(
				GraphBuilder,
				ShaderMap,
				QueueParameters,
				GraphBuilder.CreateSRV(RequestBuffer),
				NumRequests,
				true,
				GraphBuilder.CreateUAV(MeshDataBuffer),
				GraphBuilder.CreateUAV(VertexAndIndexAllocatorBuffer),
				0,
				1,
				MaxNumVertices,
				MaxNumIndices);

			auto* PassParameters = GraphBuilder.AllocParameters<FNaniteStreamOutTraversalCS::FParameters>();

			PassParameters->Scene = SceneUniformBuffer.GetBuffer(GraphBuilder);

			PassParameters->QueueParameters = QueueParameters;

			PassParameters->HierarchyBuffer = Nanite::GStreamingManager.GetHierarchySRV(GraphBuilder);
			PassParameters->ClusterPageData = Nanite::GStreamingManager.GetClusterPageDataSRV(GraphBuilder);
			PassParameters->PageConstants.X = 0;
			PassParameters->PageConstants.Y = Nanite::GStreamingManager.GetMaxStreamingPages();

			PassParameters->StreamOutRequests = GraphBuilder.CreateSRV(RequestBuffer);
			PassParameters->NumRequests = NumRequests;

			PassParameters->SegmentMappingBuffer = GraphBuilder.CreateSRV(SegmentMappingBuffer);

			PassParameters->AuxiliaryDataBufferRW = GraphBuilder.CreateUAV(AuxiliaryDataBuffer);

			PassParameters->MeshDataBuffer = GraphBuilder.CreateUAV(MeshDataBuffer);
			PassParameters->VertexBuffer = GraphBuilder.CreateUAV(VertexBuffer);
			PassParameters->IndexBuffer = GraphBuilder.CreateUAV(IndexBuffer);

			PassParameters->StreamOutCutError = CutError;

			ShaderPrint::SetParameters(GraphBuilder, PassParameters->ShaderPrint);

			FNaniteStreamOutTraversalCS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FNaniteStreamOutTraversalCS::FCountVerticesAndTrianglesDim>(false);
			PermutationVector.Set<FNaniteStreamOutTraversalCS::FCacheClustersDim>(false);

			auto ComputeShader = ShaderMap->GetShader<FNaniteStreamOutTraversalCS>(PermutationVector);

			const FIntVector GroupCount = FIntVector(GRHIPersistentThreadGroupCount, 1, 1);
			FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("NaniteStreamOut::StreamOutWithTraversal"), ComputeShader, PassParameters, GroupCount);
		}
		else
		{
			FRDGBufferRef StreamOutDispatchIndirectArgsBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>(), TEXT("NaniteStreamOut.DispatchIndirectArgs"));

			// allocate vertex and index buffer ranges
			{
				auto* PassParameters = GraphBuilder.AllocParameters<FAllocateRangesCS::FParameters>();

				PassParameters->StreamOutRequests = GraphBuilder.CreateSRV(RequestBuffer);
				PassParameters->NumRequests = NumRequests;

				PassParameters->MeshDataBuffer = GraphBuilder.CreateUAV(MeshDataBuffer);

				PassParameters->VertexAndIndexAllocator = GraphBuilder.CreateUAV(VertexAndIndexAllocatorBuffer);
				PassParameters->CurrentAllocationFrameIndex = 0;
				PassParameters->NumAllocationFrames = 1;
				PassParameters->VertexBufferSize = MaxNumVertices;
				PassParameters->IndexBufferSize = MaxNumIndices;

				PassParameters->OutputClustersStateRW = OutputClustersStateUAV;
				PassParameters->StreamOutDispatchIndirectArgsRW = GraphBuilder.CreateUAV(StreamOutDispatchIndirectArgsBuffer);

				ShaderPrint::SetParameters(GraphBuilder, PassParameters->ShaderPrint);

				auto ComputeShader = ShaderMap->GetShader<FAllocateRangesCS>();

				FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("NaniteStreamOut::AllocateRanges"), ComputeShader, PassParameters, FComputeShaderUtils::GetGroupCountWrapped(NumRequests, 64));
			}

			// stream out mesh data
			{
				auto* PassParameters = GraphBuilder.AllocParameters<FNaniteStreamOutCS::FParameters>();

				PassParameters->ClusterPageData = Nanite::GStreamingManager.GetClusterPageDataSRV(GraphBuilder);
				PassParameters->PageConstants.X = 0;
				PassParameters->PageConstants.Y = Nanite::GStreamingManager.GetMaxStreamingPages();

				PassParameters->StreamOutRequests = GraphBuilder.CreateSRV(RequestBuffer);
				PassParameters->NumRequests = NumRequests;

				PassParameters->SegmentMappingBuffer = GraphBuilder.CreateSRV(SegmentMappingBuffer);

				PassParameters->AuxiliaryDataBufferRW = GraphBuilder.CreateUAV(AuxiliaryDataBuffer);

				PassParameters->MeshDataBuffer = GraphBuilder.CreateUAV(MeshDataBuffer);
				PassParameters->VertexBuffer = GraphBuilder.CreateUAV(VertexBuffer);
				PassParameters->IndexBuffer = GraphBuilder.CreateUAV(IndexBuffer);

				PassParameters->OutputClusters = GraphBuilder.CreateSRV(OutputClustersBuffer);
				PassParameters->OutputClustersStateRW = OutputClustersStateUAV;

				PassParameters->IndirectArgs = StreamOutDispatchIndirectArgsBuffer;

				ShaderPrint::SetParameters(GraphBuilder, PassParameters->ShaderPrint);

				auto ComputeShader = ShaderMap->GetShader<FNaniteStreamOutCS>();

				FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("NaniteStreamOut::StreamOut"), ComputeShader, PassParameters, StreamOutDispatchIndirectArgsBuffer, 0);
			}
		}
	}
} // namespace Nanite
