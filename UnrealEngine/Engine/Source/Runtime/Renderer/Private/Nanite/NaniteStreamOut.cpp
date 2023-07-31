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

namespace Nanite
{
	BEGIN_SHADER_PARAMETER_STRUCT(FQueueParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer< FQueuePassState >, QueueState)
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

	struct FNaniteStreamOutCS : public FNaniteGlobalShader
	{
		DECLARE_GLOBAL_SHADER(FNaniteStreamOutCS);
		SHADER_USE_PARAMETER_STRUCT(FNaniteStreamOutCS, FNaniteGlobalShader);

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			SHADER_PARAMETER_SRV(StructuredBuffer<float4>, GPUScenePrimitiveSceneData)

			SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, HierarchyBuffer)
			SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, ClusterPageData)
			SHADER_PARAMETER(FIntVector4, PageConstants)

			SHADER_PARAMETER_STRUCT_INCLUDE(FQueueParameters, QueueParameters)

			SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, AuxiliaryDataBufferRW)

			SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, MeshDataBuffer)
			SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<float>, VertexBuffer)
			SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, IndexBuffer)

			SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer, StreamOutRequests)
			SHADER_PARAMETER(uint32, NumRequests)

			SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer, SegmentMappingBuffer)

			SHADER_PARAMETER(float, StreamOutCutError)

			SHADER_PARAMETER_STRUCT_INCLUDE(ShaderPrint::FShaderParameters, ShaderPrint)
		END_SHADER_PARAMETER_STRUCT()

		class FCountVerticesAndTrianglesDim : SHADER_PERMUTATION_BOOL("NANITE_STREAM_OUT_COUNT_VERTICES_AND_TRIANGLES");
		using FPermutationDomain = TShaderPermutationDomain<FCountVerticesAndTrianglesDim>;

		static constexpr uint32 ThreadGroupSize = 64;

		static inline void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
		{
			FNaniteGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

			OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), ThreadGroupSize);

			OutEnvironment.SetDefine(TEXT("NANITE_HIERARCHY_TRAVERSAL"), 1);

			OutEnvironment.SetDefine(TEXT("VF_SUPPORTS_PRIMITIVE_SCENE_DATA"), 1);
			OutEnvironment.SetDefine(TEXT("USE_GLOBAL_GPU_SCENE_DATA"), 1);
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
			FRDGBufferDesc Desc = FRDGBufferDesc::CreateStructuredDesc(4, MaxCullingBatches + MaxNodes * CandididateNodeSizeInUints);
			Desc.Usage = EBufferUsageFlags(Desc.Usage | BUF_ByteAddressBuffer);
			NodesAndClusterBatchesBufferRDG = GraphBuilder.CreateBuffer(Desc, TEXT("NaniteStreamOut.NodesAndClusterBatchesBuffer"));
			AddPassInitNodesAndClusterBatchesUAV(GraphBuilder, ShaderMap, GraphBuilder.CreateUAV(NodesAndClusterBatchesBufferRDG), MaxNodes, MaxCandidateClusters, MaxCullingBatches);
			NodesAndClusterBatchesBuffer = GraphBuilder.ConvertToExternalBuffer(NodesAndClusterBatchesBufferRDG);
		}

		// Allocate candidate cluster buffer
		FRDGBufferRef CandididateClustersBuffer = nullptr;
		{
			const uint32 CandididateClusterSizeInUints = 3;
			FRDGBufferDesc Desc = FRDGBufferDesc::CreateStructuredDesc(4, MaxCandidateClusters * CandididateClusterSizeInUints);
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
		FShaderResourceViewRHIRef GPUScenePrimitiveBufferSRV,
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

		FRDGBufferUAVRef AuxiliaryDataBufferUAV = GraphBuilder.CreateUAV(AuxiliaryDataBuffer);

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
			FNaniteStreamOutCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FNaniteStreamOutCS::FParameters>();
			PassParameters->GPUScenePrimitiveSceneData = GPUScenePrimitiveBufferSRV;

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
			
			PassParameters->StreamOutCutError = CutError;

			ShaderPrint::SetParameters(GraphBuilder, PassParameters->ShaderPrint);

			FNaniteStreamOutCS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FNaniteStreamOutCS::FCountVerticesAndTrianglesDim>(true);

			auto ComputeShader = ShaderMap->GetShader<FNaniteStreamOutCS>(PermutationVector);

			const FIntVector GroupCount = FIntVector(GRHIPersistentThreadGroupCount, 1, 1);
			FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("NaniteStreamOut::CountVerticesAndTriangles"), ComputeShader, PassParameters, GroupCount);
		}

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

		// write pass
		{
			FNaniteStreamOutCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FNaniteStreamOutCS::FParameters>();

			PassParameters->GPUScenePrimitiveSceneData = GPUScenePrimitiveBufferSRV;

			PassParameters->QueueParameters = QueueParameters;

			PassParameters->HierarchyBuffer = Nanite::GStreamingManager.GetHierarchySRV(GraphBuilder);
			PassParameters->ClusterPageData = Nanite::GStreamingManager.GetClusterPageDataSRV(GraphBuilder);
			PassParameters->PageConstants.X = 0;
			PassParameters->PageConstants.Y = Nanite::GStreamingManager.GetMaxStreamingPages();

			PassParameters->StreamOutRequests = GraphBuilder.CreateSRV(RequestBuffer);
			PassParameters->NumRequests = NumRequests;

			PassParameters->SegmentMappingBuffer = GraphBuilder.CreateSRV(SegmentMappingBuffer);

			PassParameters->AuxiliaryDataBufferRW = AuxiliaryDataBufferUAV;

			PassParameters->MeshDataBuffer = GraphBuilder.CreateUAV(MeshDataBuffer);
			PassParameters->VertexBuffer = GraphBuilder.CreateUAV(VertexBuffer);
			PassParameters->IndexBuffer = GraphBuilder.CreateUAV(IndexBuffer);

			PassParameters->StreamOutCutError = CutError;

			ShaderPrint::SetParameters(GraphBuilder, PassParameters->ShaderPrint);

			FNaniteStreamOutCS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FNaniteStreamOutCS::FCountVerticesAndTrianglesDim>(false);

			auto ComputeShader = ShaderMap->GetShader<FNaniteStreamOutCS>(PermutationVector);

			const FIntVector GroupCount = FIntVector(GRHIPersistentThreadGroupCount, 1, 1);
			FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("NaniteStreamOut::StreamOut"), ComputeShader, PassParameters, GroupCount);
		}
	}
} // namespace Nanite
