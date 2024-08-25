// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/Nodes/MovieGraphDeferredPassNode.h"
#include "Graph/Renderers/MovieGraphDeferredPass.h"

TUniquePtr<UE::MovieGraph::Rendering::FMovieGraphImagePassBase> UMovieGraphDeferredRenderPassNode::CreateInstance() const
{
	return MakeUnique<UE::MovieGraph::Rendering::FMovieGraphDeferredPass>();
}

UMovieGraphDeferredRenderPassNode::UMovieGraphDeferredRenderPassNode()
	: SpatialSampleCount(1)
	, AntiAliasingMethod(EAntiAliasingMethod::AAM_TSR)
	, bWriteAllSamples(false)
	, bDisableToneCurve(false)
	, bAllowOCIO(true)
	, ViewModeIndex(VMI_Lit)
{

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

void UMovieGraphDeferredRenderPassNode::GetFormatResolveArgs(FMovieGraphResolveArgs& OutMergedFormatArgs, const FMovieGraphRenderDataIdentifier& InRenderDataIdentifier) const
{
	OutMergedFormatArgs.FilenameArguments.Add(TEXT("ss_count"), FString::FromInt(SpatialSampleCount));
	OutMergedFormatArgs.FileMetadata.Add(TEXT("unreal/sampling/spatialSampleCount"), FString::FromInt(SpatialSampleCount));
}

#if WITH_EDITOR
FText UMovieGraphDeferredRenderPassNode::GetNodeTitle(const bool bGetDescriptive) const
{
	return NSLOCTEXT("MovieGraphNodes", "DeferredRenderPassGraphNode_Description", "Deferred Renderer");
}

FSlateIcon UMovieGraphDeferredRenderPassNode::GetIconAndTint(FLinearColor& OutColor) const
{
	static const FSlateIcon DeferredRendererIcon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelViewport.HighResScreenshot");
	
	OutColor = FLinearColor::White;
	return DeferredRendererIcon;
}
#endif

FString UMovieGraphDeferredRenderPassNode::GetRendererNameImpl() const
{
	static const FString RendererNameImpl(TEXT("Deferred"));
	return RendererNameImpl;
}

EViewModeIndex UMovieGraphDeferredRenderPassNode::GetViewModeIndex() const
{
	return ViewModeIndex;
}

bool UMovieGraphDeferredRenderPassNode::GetWriteAllSamples() const
{
	return bWriteAllSamples;
}

TArray<FMoviePipelinePostProcessPass> UMovieGraphDeferredRenderPassNode::GetAdditionalPostProcessMaterials() const
{
	return AdditionalPostProcessMaterials;
}

int32 UMovieGraphDeferredRenderPassNode::GetNumSpatialSamples() const
{
	return SpatialSampleCount;
}

bool UMovieGraphDeferredRenderPassNode::GetDisableToneCurve() const
{
	return bDisableToneCurve;
}

bool UMovieGraphDeferredRenderPassNode::GetAllowOCIO() const
{
	return bAllowOCIO;
}

EAntiAliasingMethod UMovieGraphDeferredRenderPassNode::GetAntiAliasingMethod() const
{
	return AntiAliasingMethod;
}