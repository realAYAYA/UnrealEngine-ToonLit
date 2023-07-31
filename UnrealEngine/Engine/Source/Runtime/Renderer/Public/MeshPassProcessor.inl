// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
MeshPassProcessor.inl:
=============================================================================*/

#pragma once

#include "RenderGraphBuilder.h"

static EVRSShadingRate GetShadingRateFromMaterial(EMaterialShadingRate MaterialShadingRate)
{
	if (GRHISupportsPipelineVariableRateShading && GRHIVariableRateShadingEnabled)
	{
		switch (MaterialShadingRate)
		{
		case MSR_1x2:
			return EVRSShadingRate::VRSSR_1x2;
		case MSR_2x1:
			return EVRSShadingRate::VRSSR_2x1;
		case MSR_2x2:
			return EVRSShadingRate::VRSSR_2x2;
		}

		if (GRHISupportsLargerVariableRateShadingSizes)
		{
			switch (MaterialShadingRate)
			{
			case MSR_4x2:
				return EVRSShadingRate::VRSSR_4x2;
			case MSR_2x4:
				return EVRSShadingRate::VRSSR_2x4;
			case MSR_4x4:
				return EVRSShadingRate::VRSSR_4x4;
			}
		}
	}
	return EVRSShadingRate::VRSSR_1x1;
}

template<typename PassShadersType, typename ShaderElementDataType>
void FMeshPassProcessor::BuildMeshDrawCommands(
	const FMeshBatch& RESTRICT MeshBatch,
	uint64 BatchElementMask,
	const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
	const FMaterialRenderProxy& RESTRICT MaterialRenderProxy,
	const FMaterial& RESTRICT MaterialResource,
	const FMeshPassProcessorRenderState& RESTRICT DrawRenderState,
	PassShadersType PassShaders,
	ERasterizerFillMode MeshFillMode,
	ERasterizerCullMode MeshCullMode,
	FMeshDrawCommandSortKey SortKey,
	EMeshPassFeatures MeshPassFeatures,
	const ShaderElementDataType& ShaderElementData)
{
	const FVertexFactory* RESTRICT VertexFactory = MeshBatch.VertexFactory;
	const FPrimitiveSceneInfo* RESTRICT PrimitiveSceneInfo = PrimitiveSceneProxy ? PrimitiveSceneProxy->GetPrimitiveSceneInfo() : nullptr;

	FMeshDrawCommand SharedMeshDrawCommand;
	EFVisibleMeshDrawCommandFlags SharedFlags = EFVisibleMeshDrawCommandFlags::Default;

	if (MaterialResource.MaterialUsesWorldPositionOffset_RenderThread())
	{
		SharedFlags |= EFVisibleMeshDrawCommandFlags::MaterialUsesWorldPositionOffset;
	}

	SharedMeshDrawCommand.SetStencilRef(DrawRenderState.GetStencilRef());
	SharedMeshDrawCommand.PrimitiveType = (EPrimitiveType)MeshBatch.Type;

	FGraphicsMinimalPipelineStateInitializer PipelineState;
	PipelineState.PrimitiveType = (EPrimitiveType)MeshBatch.Type;
	PipelineState.ImmutableSamplerState = MaterialRenderProxy.ImmutableSamplerState;

	EVertexInputStreamType InputStreamType = EVertexInputStreamType::Default;
	if ((MeshPassFeatures & EMeshPassFeatures::PositionOnly) != EMeshPassFeatures::Default)				InputStreamType = EVertexInputStreamType::PositionOnly;
	if ((MeshPassFeatures & EMeshPassFeatures::PositionAndNormalOnly) != EMeshPassFeatures::Default)	InputStreamType = EVertexInputStreamType::PositionAndNormalOnly;

	check(VertexFactory && VertexFactory->IsInitialized());
	FRHIVertexDeclaration* VertexDeclaration = VertexFactory->GetDeclaration(InputStreamType);

	check(!VertexFactory->NeedsDeclaration() || VertexDeclaration);

	FMeshProcessorShaders MeshProcessorShaders = PassShaders.GetUntypedShaders();
	PipelineState.SetupBoundShaderState(VertexDeclaration, MeshProcessorShaders);

	SharedMeshDrawCommand.InitializeShaderBindings(MeshProcessorShaders);

	PipelineState.RasterizerState = GetStaticRasterizerState<true>(MeshFillMode, MeshCullMode);

	check(DrawRenderState.GetDepthStencilState());
	check(DrawRenderState.GetBlendState());

	PipelineState.BlendState = DrawRenderState.GetBlendState();
	PipelineState.DepthStencilState = DrawRenderState.GetDepthStencilState();
	PipelineState.DrawShadingRate = GetShadingRateFromMaterial(MaterialResource.GetShadingRate());

	// PSO Precache hash only needed when PSO precaching is enabled
	if (PipelineStateCache::IsPSOPrecachingEnabled())
	{
		PipelineState.ComputePrecachePSOHash();
	}

	check(VertexFactory && VertexFactory->IsInitialized());
	VertexFactory->GetStreams(FeatureLevel, InputStreamType, SharedMeshDrawCommand.VertexStreams);

#if PSO_PRECACHING_VALIDATE
	PSOCollectorStats::CheckMinimalPipelineStateInCache(PipelineState, (uint32)MeshPassType, VertexFactory->GetType());
#endif // PSO_PRECACHING_VALIDATE

	SharedMeshDrawCommand.PrimitiveIdStreamIndex = VertexFactory->GetPrimitiveIdStreamIndex(FeatureLevel, InputStreamType);

	if (SharedMeshDrawCommand.PrimitiveIdStreamIndex != INDEX_NONE)
	{
		SharedFlags |= EFVisibleMeshDrawCommandFlags::HasPrimitiveIdStreamIndex;
	}

	int32 DataOffset = 0;
	if (PassShaders.VertexShader.IsValid())
	{
		FMeshDrawSingleShaderBindings ShaderBindings = SharedMeshDrawCommand.ShaderBindings.GetSingleShaderBindings(SF_Vertex, DataOffset);
		PassShaders.VertexShader->GetShaderBindings(Scene, FeatureLevel, PrimitiveSceneProxy, MaterialRenderProxy, MaterialResource, DrawRenderState, ShaderElementData, ShaderBindings);
	}

	if (PassShaders.PixelShader.IsValid())
	{
		FMeshDrawSingleShaderBindings ShaderBindings = SharedMeshDrawCommand.ShaderBindings.GetSingleShaderBindings(SF_Pixel, DataOffset);
		PassShaders.PixelShader->GetShaderBindings(Scene, FeatureLevel, PrimitiveSceneProxy, MaterialRenderProxy, MaterialResource, DrawRenderState, ShaderElementData, ShaderBindings);
	}

	if (PassShaders.GeometryShader.IsValid())
	{
		FMeshDrawSingleShaderBindings ShaderBindings = SharedMeshDrawCommand.ShaderBindings.GetSingleShaderBindings(SF_Geometry, DataOffset);
		PassShaders.GeometryShader->GetShaderBindings(Scene, FeatureLevel, PrimitiveSceneProxy, MaterialRenderProxy, MaterialResource, DrawRenderState, ShaderElementData, ShaderBindings);
	}

	SharedMeshDrawCommand.SetDebugData(PrimitiveSceneProxy, &MaterialResource, &MaterialRenderProxy, PassShaders.GetUntypedShaders(), VertexFactory, (uint32)MeshPassType);

	const int32 NumElements = ShouldSkipMeshDrawCommand(MeshBatch, PrimitiveSceneProxy) ? 0 : MeshBatch.Elements.Num();

	for (int32 BatchElementIndex = 0; BatchElementIndex < NumElements; BatchElementIndex++)
	{
		if ((1ull << BatchElementIndex) & BatchElementMask)
		{
			const FMeshBatchElement& BatchElement = MeshBatch.Elements[BatchElementIndex];
			FMeshDrawCommand& MeshDrawCommand = DrawListContext->AddCommand(SharedMeshDrawCommand, NumElements);
			
			EFVisibleMeshDrawCommandFlags Flags = SharedFlags;
			if (BatchElement.bForceInstanceCulling)
			{
				Flags |= EFVisibleMeshDrawCommandFlags::ForceInstanceCulling;
			}
			if (BatchElement.bPreserveInstanceOrder)
			{
				// TODO: add support for bPreserveInstanceOrder on mobile
				if (ensureMsgf(FeatureLevel > ERHIFeatureLevel::ES3_1, TEXT("FMeshBatchElement::bPreserveInstanceOrder is currently only supported on non-mobile platforms.")))
				{					
					Flags |= EFVisibleMeshDrawCommandFlags::PreserveInstanceOrder;
				}
			}

			DataOffset = 0;
			if (PassShaders.VertexShader.IsValid())
			{
				FMeshDrawSingleShaderBindings VertexShaderBindings = MeshDrawCommand.ShaderBindings.GetSingleShaderBindings(SF_Vertex, DataOffset);
				FMeshMaterialShader::GetElementShaderBindings(PassShaders.VertexShader, Scene, ViewIfDynamicMeshCommand, VertexFactory, InputStreamType, FeatureLevel, PrimitiveSceneProxy, MeshBatch, BatchElement, ShaderElementData, VertexShaderBindings, MeshDrawCommand.VertexStreams);
			}

			if (PassShaders.PixelShader.IsValid())
			{
				FMeshDrawSingleShaderBindings PixelShaderBindings = MeshDrawCommand.ShaderBindings.GetSingleShaderBindings(SF_Pixel, DataOffset);
				FMeshMaterialShader::GetElementShaderBindings(PassShaders.PixelShader, Scene, ViewIfDynamicMeshCommand, VertexFactory, EVertexInputStreamType::Default, FeatureLevel, PrimitiveSceneProxy, MeshBatch, BatchElement, ShaderElementData, PixelShaderBindings, MeshDrawCommand.VertexStreams);
			}


			if (PassShaders.GeometryShader.IsValid())
			{
				FMeshDrawSingleShaderBindings GeometryShaderBindings = MeshDrawCommand.ShaderBindings.GetSingleShaderBindings(SF_Geometry, DataOffset);
				FMeshMaterialShader::GetElementShaderBindings(PassShaders.GeometryShader, Scene, ViewIfDynamicMeshCommand, VertexFactory, EVertexInputStreamType::Default, FeatureLevel, PrimitiveSceneProxy, MeshBatch, BatchElement, ShaderElementData, GeometryShaderBindings, MeshDrawCommand.VertexStreams);
			}

			FMeshDrawCommandPrimitiveIdInfo IdInfo = GetDrawCommandPrimitiveId(PrimitiveSceneInfo, BatchElement);

			FMeshProcessorShaders ShadersForDebugging = PassShaders.GetUntypedShaders();
			DrawListContext->FinalizeCommand(MeshBatch, BatchElementIndex, IdInfo, MeshFillMode, MeshCullMode, SortKey, Flags, PipelineState, &ShadersForDebugging, MeshDrawCommand);
		}
	}
}

template<typename PassShadersType>
void FMeshPassProcessor::AddGraphicsPipelineStateInitializer(
	const FVertexFactoryType* RESTRICT VertexFactoryType,
	const FMaterial& RESTRICT MaterialResource,
	const FMeshPassProcessorRenderState& RESTRICT DrawRenderState,
	const FGraphicsPipelineRenderTargetsInfo& RESTRICT RenderTargetsInfo,
	PassShadersType PassShaders,
	ERasterizerFillMode MeshFillMode,
	ERasterizerCullMode MeshCullMode,
	EPrimitiveType PrimitiveType,
	EMeshPassFeatures MeshPassFeatures, 
	TArray<FPSOPrecacheData>& PSOInitializers)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FMeshPassProcessor::AddGraphicsPipelineStateInitializer);

	FGraphicsMinimalPipelineStateInitializer MinimalPipelineStateInitializer;
	MinimalPipelineStateInitializer.PrimitiveType = PrimitiveType;
	
	// Ignore immutable samplers for now - should be passed in?
	//PipelineState.ImmutableSamplerState = MaterialRenderProxy.ImmutableSamplerState;
	
	EVertexInputStreamType InputStreamType = EVertexInputStreamType::Default;
	if ((MeshPassFeatures & EMeshPassFeatures::PositionOnly) != EMeshPassFeatures::Default)				InputStreamType = EVertexInputStreamType::PositionOnly;
	if ((MeshPassFeatures & EMeshPassFeatures::PositionAndNormalOnly) != EMeshPassFeatures::Default)	InputStreamType = EVertexInputStreamType::PositionAndNormalOnly;

	FVertexDeclarationElementList Elements;
	VertexFactoryType->GetShaderPSOPrecacheVertexFetchElements(InputStreamType, Elements);
	FRHIVertexDeclaration* VertexDeclaration = PipelineStateCache::GetOrCreateVertexDeclaration(Elements);
	
	FMeshProcessorShaders MeshProcessorShaders = PassShaders.GetUntypedShaders();
	MinimalPipelineStateInitializer.SetupBoundShaderState(VertexDeclaration, MeshProcessorShaders);

	MinimalPipelineStateInitializer.RasterizerState = GetStaticRasterizerState<true>(MeshFillMode, MeshCullMode);

	check(DrawRenderState.GetDepthStencilState());
	check(DrawRenderState.GetBlendState());

	MinimalPipelineStateInitializer.BlendState = DrawRenderState.GetBlendState();
	MinimalPipelineStateInitializer.DepthStencilState = DrawRenderState.GetDepthStencilState();
	MinimalPipelineStateInitializer.DrawShadingRate = GetShadingRateFromMaterial(MaterialResource.GetShadingRate());

	MinimalPipelineStateInitializer.ComputePrecachePSOHash();
#if PSO_PRECACHING_VALIDATE
	PSOCollectorStats::AddMinimalPipelineStateToCache(MinimalPipelineStateInitializer, (uint32)MeshPassType, VertexFactoryType);
#endif // PSO_PRECACHING_VALIDATE

	// NOTE: AsGraphicsPipelineStateInitializer will create the RHIShaders internally if they are not cached yet
	FGraphicsPipelineStateInitializer PipelineStateInitializer = MinimalPipelineStateInitializer.AsGraphicsPipelineStateInitializer();
	ApplyTargetsInfo(PipelineStateInitializer, RenderTargetsInfo);

	FPSOPrecacheData PSOPrecacheData;
	PSOPrecacheData.PSOInitializer = PipelineStateInitializer;
#if PSO_PRECACHING_VALIDATE
	PSOPrecacheData.MeshPassType = (uint32)MeshPassType;
	PSOPrecacheData.VertexFactoryType = VertexFactoryType;
#endif // PSO_PRECACHING_VALIDATE
	PSOInitializers.Add(PSOPrecacheData);
}


/**
* Provides a callback to build FMeshDrawCommands and then submits them immediately.  Useful for legacy / editor code paths.
* Does many dynamic allocations - do not use for game rendering.
*/
template<typename LambdaType>
void DrawDynamicMeshPass(const FSceneView& View, FRHICommandList& RHICmdList, const LambdaType& BuildPassProcessorLambda, bool bForceStereoInstancingOff = false)
{
	FDynamicMeshDrawCommandStorage DynamicMeshDrawCommandStorage;
	FMeshCommandOneFrameArray VisibleMeshDrawCommands;
	FGraphicsMinimalPipelineStateSet GraphicsMinimalPipelineStateSet;
	bool NeedsShaderInitialisation;

	FDynamicPassMeshDrawListContext DynamicMeshPassContext(DynamicMeshDrawCommandStorage, VisibleMeshDrawCommands, GraphicsMinimalPipelineStateSet, NeedsShaderInitialisation);

	BuildPassProcessorLambda(&DynamicMeshPassContext);

	// We assume all dynamic passes are in stereo if it is enabled in the view, so we apply ISR to them
	const uint32 InstanceFactor = (!bForceStereoInstancingOff && View.IsInstancedStereoPass()) ? 2 : 1;
	DrawDynamicMeshPassPrivate(View, RHICmdList, VisibleMeshDrawCommands, DynamicMeshDrawCommandStorage, GraphicsMinimalPipelineStateSet, NeedsShaderInitialisation, InstanceFactor);
}

template <typename ParameterStructType, typename LambdaType>
void AddDrawDynamicMeshPass(
	FRDGBuilder& GraphBuilder,
	FRDGEventName&& EventName,
	const ParameterStructType* PassParameters,
	const FSceneView& View,
	FIntRect ViewRect,
	const LambdaType& BuildPassProcessorLambda,
	bool bForceStereoInstancingOff = false)
{
	// We assume all dynamic passes are in stereo if it is enabled in the view, so we apply ISR to them
	const uint32 InstanceFactor = (!bForceStereoInstancingOff && View.IsInstancedStereoPass()) ? 2 : 1;

	struct FContext
	{
		FDynamicMeshDrawCommandStorage DynamicMeshDrawCommandStorage;
		FMeshCommandOneFrameArray VisibleMeshDrawCommands;
		FGraphicsMinimalPipelineStateSet GraphicsMinimalPipelineStateSet;
		bool NeedsShaderInitialisation;
	};

	FContext& Context = *GraphBuilder.AllocObject<FContext>();

	GraphBuilder.AddSetupTask([&Context, BuildPassProcessorLambda]
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(SetupDynamicMeshPass);
		FDynamicPassMeshDrawListContext DynamicMeshPassContext(Context.DynamicMeshDrawCommandStorage, Context.VisibleMeshDrawCommands, Context.GraphicsMinimalPipelineStateSet, Context.NeedsShaderInitialisation);
		BuildPassProcessorLambda(&DynamicMeshPassContext);
	});

	GraphBuilder.AddPass(
		MoveTemp(EventName),
		PassParameters,
		ERDGPassFlags::Raster,
		[&Context, &View, ViewRect, InstanceFactor](FRHICommandList& RHICmdList)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(SetupDynamicMeshPass);
		RHICmdList.SetViewport(ViewRect.Min.X, ViewRect.Min.Y, 0.0f, ViewRect.Max.X, ViewRect.Max.Y, 1.0f);
		DrawDynamicMeshPassPrivate(View, RHICmdList, Context.VisibleMeshDrawCommands, Context.DynamicMeshDrawCommandStorage, Context.GraphicsMinimalPipelineStateSet, Context.NeedsShaderInitialisation, InstanceFactor);
	});
}