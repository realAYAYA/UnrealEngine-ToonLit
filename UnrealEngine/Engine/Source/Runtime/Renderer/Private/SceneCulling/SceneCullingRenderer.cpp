// Copyright Epic Games, Inc. All Rights Reserved.

#include "SceneCullingRenderer.h"
#include "SceneCulling.h"
#include "RenderGraphUtils.h"
#include "SceneRendererInterface.h"
#include "SystemTextures.h"
#include "SceneRendering.h"
#include "ShaderPrintParameters.h"

static TAutoConsoleVariable<int32> CVarSceneCullingDebugRenderMode(
	TEXT("r.SceneCulling.DebugRenderMode"), 
	0, 
	TEXT("SceneCulling debug render mode.\n")
	TEXT(" 0 = Disabled (default)\n")
	TEXT(" 1 = Enabled"),
	ECVF_RenderThreadSafe);


FInstanceHierarchyParameters& FSceneCullingRenderer::GetShaderParameters(FRDGBuilder& GraphBuilder) 
{ 
	// Sync any update that is in progress.
	SceneCulling.EndUpdate(GraphBuilder, SceneRenderer.GetSceneUniforms(), true);

	// This should not need to be done more than once per frame
	if (CellHeadersRDG == nullptr)
	{
		CellBlockDataRDG = SceneCulling.CellBlockDataBuffer.Register(GraphBuilder);
		CellHeadersRDG = SceneCulling.CellHeadersBuffer.Register(GraphBuilder);
		ItemChunksRDG = SceneCulling.ItemChunksBuffer.Register(GraphBuilder);
		InstanceIdsRDG = SceneCulling.InstanceIdsBuffer.Register(GraphBuilder);

		ExplicitCellBoundsRDG = SceneCulling.bUseExplictBounds ? SceneCulling.ExplicitCellBoundsBuffer.Register(GraphBuilder) :  GSystemTextures.GetDefaultStructuredBuffer<FVector4f>(GraphBuilder);

#if 0
		// Fully upload the buffers for debugging. 
		CellBlockDataRDG = CreateStructuredBuffer(GraphBuilder, TEXT("SceneCulling.BlockData"), SceneCulling.CellBlockData);
		CellHeadersRDG = CreateStructuredBuffer(GraphBuilder, TEXT("SceneCulling.CellHeaders"), SceneCulling.CellHeaders);
		ItemChunksRDG = CreateStructuredBuffer(GraphBuilder, TEXT("SceneCulling.ItemChunks"), SceneCulling.PackedCellChunkData);
		InstanceIdsRDG = CreateStructuredBuffer(GraphBuilder, TEXT("SceneCulling.Items"), SceneCulling.PackedCellData);
#endif

		ShaderParameters.NumCellsPerBlockLog2 = FSpatialHash::NumCellsPerBlockLog2;
		ShaderParameters.CellBlockDimLog2 = FSpatialHash::CellBlockDimLog2;
		ShaderParameters.LocalCellCoordMask = (1U << FSpatialHash::CellBlockDimLog2) - 1U;
		ShaderParameters.FirstLevel = SceneCulling.SpatialHash.GetFirstLevel();
		ShaderParameters.bUseExplicitCellBounds = SceneCulling.bUseExplictBounds;
		ShaderParameters.InstanceHierarchyCellBlockData = GraphBuilder.CreateSRV(CellBlockDataRDG);
		ShaderParameters.InstanceHierarchyCellHeaders = GraphBuilder.CreateSRV(CellHeadersRDG);
		ShaderParameters.InstanceIds = GraphBuilder.CreateSRV(InstanceIdsRDG);
		ShaderParameters.InstanceHierarchyItemChunks = GraphBuilder.CreateSRV(ItemChunksRDG);
		ShaderParameters.ExplicitCellBounds = GraphBuilder.CreateSRV(ExplicitCellBoundsRDG);
	}

	return ShaderParameters; 
}

FSceneInstanceCullingQuery* FSceneCullingRenderer::CullInstances(FRDGBuilder& GraphBuilder, const TConstArrayView<FConvexVolume>& ViewCullVolumes)
{
	SCOPED_NAMED_EVENT(FSceneCullingRenderer_CullInstances, FColor::Emerald);

	if (SceneCulling.IsEnabled())
	{
		FSceneInstanceCullingQuery* Query = GraphBuilder.AllocObject<FSceneInstanceCullingQuery>(*this);

		for (int32 Index = 0; Index < ViewCullVolumes.Num(); ++Index)
		{
			FCullingVolume CullingVolume;
			CullingVolume.ConvexVolume = ViewCullVolumes[Index];
			Query->Add(Index, 1, 1, CullingVolume);
		}

		Query->Dispatch(GraphBuilder);

		return Query;
	}
	return nullptr;
}


class FSceneCullingDebugRender_CS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FSceneCullingDebugRender_CS);
	SHADER_USE_PARAMETER_STRUCT(FSceneCullingDebugRender_CS, FGlobalShader);

	static constexpr int32 NumThreadsPerGroup = 64;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) 
	{ 
		return DoesPlatformSupportNanite(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		OutEnvironment.SetDefine(TEXT("VF_SUPPORTS_PRIMITIVE_SCENE_DATA"), 1);
		OutEnvironment.SetDefine(TEXT("NUM_THREADS_PER_GROUP"), NumThreadsPerGroup);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE( ShaderPrint::FShaderParameters, ShaderPrintUniformBuffer )
		SHADER_PARAMETER_STRUCT_INCLUDE( FInstanceHierarchyParameters, InstanceHierarchyParameters )
		SHADER_PARAMETER(int32, MaxCells)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint32>, ValidCellsMask)
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FSceneCullingDebugRender_CS, "/Engine/Private/SceneCulling/SceneCullingDebugRender.usf", "DebugRender", SF_Compute);


void FSceneCullingRenderer::DebugRender(FRDGBuilder& GraphBuilder, TArrayView<FViewInfo> Views)
{
#if !UE_BUILD_SHIPPING
	int32 MaxCellCount = SceneCulling.CellHeaders.Num();

	if (CVarSceneCullingDebugRenderMode.GetValueOnRenderThread() != 0 && MaxCellCount > 0)
	{
		// Force ShaderPrint on.
		ShaderPrint::SetEnabled(true); 

		// This lags by one frame, so may miss some in one frame, also overallocates since we will cull a lot.
		ShaderPrint::RequestSpaceForLines(MaxCellCount * 12 * Views.Num());

		// Note: we have to construct this as the GPU currently does not have a mapping of what cells are valid.
		//       Normally this comes from the CPU during the broad phase culling. Thus it is only needed here for debug purposes.
		TBitArray<> ValidCellsMask(false, MaxCellCount);
		for (int32 Index = 0; Index < MaxCellCount; ++Index)
		{
			ValidCellsMask[Index] =  (SceneCulling.CellHeaders[Index].ItemChunksOffset & FSceneCulling::InvalidCellFlag) == 0; 
		}
		FRDGBuffer* ValidCellsMaskRdg = CreateStructuredBuffer(GraphBuilder, TEXT("Shadow.Virtual.RevealedPrimitivesMask"), TConstArrayView<uint32>(ValidCellsMask.GetData(), FBitSet::CalculateNumWords(ValidCellsMask.Num())));

		for (auto &View : Views)
		{
			if (ShaderPrint::IsEnabled(View.ShaderPrintData))
			{	
				FSceneCullingDebugRender_CS::FParameters* PassParameters = GraphBuilder.AllocParameters<FSceneCullingDebugRender_CS::FParameters>();
				ShaderPrint::SetParameters(GraphBuilder, PassParameters->ShaderPrintUniformBuffer);
				PassParameters->InstanceHierarchyParameters = GetShaderParameters(GraphBuilder);
				PassParameters->MaxCells = MaxCellCount;
				PassParameters->ValidCellsMask = GraphBuilder.CreateSRV(ValidCellsMaskRdg);

				auto ComputeShader = View.ShaderMap->GetShader<FSceneCullingDebugRender_CS>();

				FComputeShaderUtils::AddPass(
					GraphBuilder,
					RDG_EVENT_NAME("SceneCullingDebugRender"),
					ComputeShader,
					PassParameters,
					FComputeShaderUtils::GetGroupCountWrapped(MaxCellCount, FSceneCullingDebugRender_CS::NumThreadsPerGroup)
				);
			}
		}
	}
#endif
}

FSceneInstanceCullingQuery* FSceneCullingRenderer::CreateInstanceQuery(FRDGBuilder& GraphBuilder)
{
	SCOPED_NAMED_EVENT(FSceneCullingRenderer_CullInstances, FColor::Emerald);

	if (SceneCulling.IsEnabled())
	{
		FSceneInstanceCullingQuery* Query = GraphBuilder.AllocObject<FSceneInstanceCullingQuery>(*this);
		return Query;
	}
	return nullptr;
}

int32 FSceneInstanceCullingQuery::Add(uint32 FirstPrimaryView, uint32 NumPrimaryViews, uint32 MaxNumViews, const FCullingVolume& CullingVolume)
{
	check(!CullingResult);
	check(!AsyncTaskHandle.IsValid());

	if (SceneCullingRenderer.IsEnabled())
	{
		FCullingJob Job;
		Job.MaxNumViews = MaxNumViews;
		Job.ViewDrawGroup.FirstView = FirstPrimaryView;
		Job.ViewDrawGroup.NumViews = NumPrimaryViews;
		Job.CullingVolume = CullingVolume;
		CullingJobs.Add(Job);

		// return index of the job / view group
		return CullingJobs.Num() - 1;
	}
	return INDEX_NONE;
}

void FSceneInstanceCullingQuery::Dispatch(FRDGBuilder& GraphBuilder, bool bInAllowAsync)
{
	check(!CullingResult);
	check(!AsyncTaskHandle.IsValid());

	const bool bAllowAsync = SceneCullingRenderer.SceneCulling.bUseAsyncUpdate && bInAllowAsync;

	if (!CullingJobs.IsEmpty())
	{
		// Must wait if this query is not running async or we might race against the update task.
		UE::Tasks::FTask UpdateTaskHandle = SceneCullingRenderer.SceneCulling.GetUpdateTaskHandle();
		if (!bAllowAsync && UpdateTaskHandle.IsValid())
		{
			UpdateTaskHandle.Wait();
		}

		CullingResult = GraphBuilder.AllocObject<FSceneInstanceCullResult>();

		AsyncTaskHandle = GraphBuilder.AddSetupTask([this]()
		{
			ComputeResult();
		},
		nullptr, TArray<UE::Tasks::FTask>{ UpdateTaskHandle }, UE::Tasks::ETaskPriority::Normal, bAllowAsync);
	}
}

FSceneInstanceCullResult* FSceneInstanceCullingQuery::GetResult()
{
	SCOPED_NAMED_EVENT(FSceneInstanceCullingQuery_GetResult, FColor::Emerald);

	if (AsyncTaskHandle.IsValid())
	{
		AsyncTaskHandle.Wait();
	}

	if (CullingResult == nullptr && !CullingJobs.IsEmpty())
	{
		ComputeResult();
	}

	return CullingResult;
}

void FSceneInstanceCullingQuery::ComputeResult()
{
	SCOPED_NAMED_EVENT(FSceneInstanceCullingQuery_ComputeResult, FColor::Emerald);
	CullingResult->MaxOccludedCellDraws = 0;
	// loop and append all results
	for (const FCullingJob& CullingJob : CullingJobs)
	{
		int32 PrevNumCellDraws = CullingResult->CellDraws.Num();
		SceneCullingRenderer.SceneCulling.Test(CullingJob.CullingVolume, CullingResult->CellDraws, uint32(CullingResult->ViewDrawGroups.Num()), CullingJob.MaxNumViews, CullingResult->NumInstanceGroups);
		CullingResult->MaxOccludedCellDraws += (CullingResult->CellDraws.Num() - PrevNumCellDraws) * CullingJob.MaxNumViews;
		CullingResult->ViewDrawGroups.Add(CullingJob.ViewDrawGroup);
	}
	CullingResult->UncullableItemChunksOffset = SceneCullingRenderer.SceneCulling.UncullableItemChunksOffset;
	CullingResult->UncullableNumItemChunks = SceneCullingRenderer.SceneCulling.UncullableNumItemChunks;
	CullingResult->SceneCullingRenderer = &SceneCullingRenderer;
}

FSceneInstanceCullingQuery::FSceneInstanceCullingQuery(FSceneCullingRenderer& InSceneCullingRenderer)
	: SceneCullingRenderer(InSceneCullingRenderer)
{
}

