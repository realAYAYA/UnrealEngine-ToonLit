// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/Nodes/MovieGraphPathTracerPassNode.h"
#include "Graph/Renderers/MovieGraphPathTracerPass.h"
#include "Engine/EngineBaseTypes.h"
#include "RenderUtils.h"
#include "ShowFlags.h"

TUniquePtr<UE::MovieGraph::Rendering::FMovieGraphImagePassBase> UMovieGraphPathTracerRenderPassNode::CreateInstance() const
{
	return MakeUnique<UE::MovieGraph::Rendering::FMovieGraphPathTracerPass>();
}

UMovieGraphPathTracerRenderPassNode::UMovieGraphPathTracerRenderPassNode()
	: SpatialSampleCount(1)
	, bDenoiser(true)
	, bWriteAllSamples(false)
	, bDisableToneCurve(false)
	, bAllowOCIO(true)
{
	ShowFlags->ApplyDefaultShowFlagValue(VMI_PathTracing, true);
	// TODO: Showflag for SetMotionBlur()?

	// To help user knowledge we pre-seed the additional post processing materials with an array of potentially common passes.
	TArray<FString> DefaultPostProcessMaterials;
	DefaultPostProcessMaterials.Add(DefaultDepthAsset);
	DefaultPostProcessMaterials.Add(DefaultMotionVectorsAsset);

	for (FString& MaterialPath : DefaultPostProcessMaterials)
	{
		FMoviePipelinePostProcessPass& NewPass = AdditionalPostProcessMaterials.AddDefaulted_GetRef();
		NewPass.Material = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(MaterialPath));
		NewPass.bEnabled = false;
	}
}

#if WITH_EDITOR
FText UMovieGraphPathTracerRenderPassNode::GetNodeTitle(const bool bGetDescriptive) const
{
	return NSLOCTEXT("MovieGraphNodes", "PathTracedRenderPassGraphNode_Description", "Path Traced Renderer");
}

FSlateIcon UMovieGraphPathTracerRenderPassNode::GetIconAndTint(FLinearColor& OutColor) const
{
	static const FSlateIcon PathTracerRendererIcon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "EditorViewport.PathTracingMode");
	
    OutColor = FLinearColor::White;
    return PathTracerRendererIcon;
}
#endif

void UMovieGraphPathTracerRenderPassNode::SetupImpl(const FMovieGraphRenderPassSetupData& InSetupData)
{
	Super::SetupImpl(InSetupData);

	// Hide the progress display during the render
	if (IConsoleVariable* ProgressDisplayCvar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.PathTracing.ProgressDisplay")))
	{
		bOriginalProgressDisplayCvarValue = ProgressDisplayCvar->GetBool();
		ProgressDisplayCvar->Set(false);
	}

	bool bSupportsPathTracing = false;
	if (IsRayTracingEnabled())
	{
		if (const IConsoleVariable* PathTracingCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.PathTracing")))
		{
			bSupportsPathTracing = PathTracingCVar->GetInt() != 0;
		}
	}

	// Warn if the path tracer is being used, but it's not enabled in the Rendering settings. The path tracer won't work otherwise.
	if (!bSupportsPathTracing)
	{
		// TODO: Ideally this is called in a general-purpose validation step instead, but that framework does not exist yet.
		UE_LOG(LogMovieRenderPipeline, Warning, TEXT("An active Path Traced Renderer node was found, but path tracing support is not enabled. To get "
													 "renders with path tracing, enable 'Support Hardware Ray Tracing' and 'Path Tracing' in the "
													 "project's Rendering settings."));
	}
}

void UMovieGraphPathTracerRenderPassNode::TeardownImpl()
{
	Super::TeardownImpl();

	// Restore the original setting for the progress display
	if (IConsoleVariable* ProgressDisplayCvar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.PathTracing.ProgressDisplay")))
	{
		ProgressDisplayCvar->Set(bOriginalProgressDisplayCvarValue);
	}
}

FString UMovieGraphPathTracerRenderPassNode::GetRendererNameImpl() const
{
	static const FString RendererNameImpl(TEXT("PathTraced"));
	return RendererNameImpl;
}

EViewModeIndex UMovieGraphPathTracerRenderPassNode::GetViewModeIndex() const
{
	return VMI_PathTracing;
}

bool UMovieGraphPathTracerRenderPassNode::GetWriteAllSamples() const
{
	return bWriteAllSamples;
}

TArray<FMoviePipelinePostProcessPass> UMovieGraphPathTracerRenderPassNode::GetAdditionalPostProcessMaterials() const
{
	return AdditionalPostProcessMaterials;
}

int32 UMovieGraphPathTracerRenderPassNode::GetNumSpatialSamples() const
{
	return SpatialSampleCount;
}

int32 UMovieGraphPathTracerRenderPassNode::GetNumSpatialSamplesDuringWarmUp() const
{
	// Path Tracer doesn't have an image history like the deferred renderer, so it doesn't need
	// to run all the spatial samples.
	return 1;
}


bool UMovieGraphPathTracerRenderPassNode::GetDisableToneCurve() const
{
	return bDisableToneCurve;
}

bool UMovieGraphPathTracerRenderPassNode::GetAllowOCIO() const
{
	return bAllowOCIO;
}

bool UMovieGraphPathTracerRenderPassNode::GetAllowDenoiser() const
{
	return bDenoiser;
}

