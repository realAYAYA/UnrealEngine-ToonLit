// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
//#include "SceneManagement.h"
#include "SceneRendering.h"

template <typename PassParametersType>
inline void AddSimpleElementCollectorPass(const FSimpleElementCollector& SimpleElementCollector, FRDGBuilder& GraphBuilder, PassParametersType* PassParameters, const FViewInfo& View, const FMeshPassProcessorRenderState& DrawRenderState)
{
	GraphBuilder.AddPass(
		RDG_EVENT_NAME("EditorPrimitives"),
		PassParameters,
		ERDGPassFlags::Raster,
		[&View, &SimpleElementCollector, DrawRenderState](FRHICommandListImmediate& RHICmdList)
		{
			RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);

			SimpleElementCollector.DrawBatchedElements(RHICmdList, DrawRenderState, View, EBlendModeFilter::OpaqueAndMasked, SDPG_World);
			SimpleElementCollector.DrawBatchedElements(RHICmdList, DrawRenderState, View, EBlendModeFilter::OpaqueAndMasked, SDPG_Foreground);
		}
	);
}

template <typename PassParametersType>
inline void AddSimpleElementCollectorPass(const FSimpleElementCollector& SimpleElementCollector, FRDGBuilder& GraphBuilder, PassParametersType* PassParameters, const FViewInfo& View, const FMeshPassProcessorRenderState& DrawRenderState, ESceneDepthPriorityGroup SceneDepthPriorityGroup)
{
	GraphBuilder.AddPass(
		RDG_EVENT_NAME("EditorPrimitives"),
		PassParameters,
		ERDGPassFlags::Raster,
		[&View, &SimpleElementCollector, DrawRenderState, SceneDepthPriorityGroup](FRHICommandListImmediate& RHICmdList)
		{
			RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);

			SimpleElementCollector.DrawBatchedElements(RHICmdList, DrawRenderState, View, EBlendModeFilter::OpaqueAndMasked, SceneDepthPriorityGroup);
		}
	);
}

template <typename PassParametersType>
inline void AddBatchedElementsPass(const FBatchedElements& BatchedElements, FRDGBuilder& GraphBuilder, PassParametersType* PassParameters, const FViewInfo& View, const FMeshPassProcessorRenderState& DrawRenderState)
{
	GraphBuilder.AddPass(
		RDG_EVENT_NAME("BatchedElements"),
		PassParameters,
		ERDGPassFlags::Raster,
		[&View, &BatchedElements, DrawRenderState](FRHICommandListImmediate& RHICmdList)
		{
			RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);

			// Draw the view's batched simple elements(lines, sprites, etc).
			BatchedElements.Draw(RHICmdList, DrawRenderState, View.FeatureLevel, View, false);
		}
	);
}
