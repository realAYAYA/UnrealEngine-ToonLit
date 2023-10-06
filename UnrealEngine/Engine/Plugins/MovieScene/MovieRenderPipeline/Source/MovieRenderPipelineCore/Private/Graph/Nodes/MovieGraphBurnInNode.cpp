// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/Nodes/MovieGraphBurnInNode.h"

#include "Engine/TextureRenderTarget2D.h"
#include "Framework/Application/SlateApplication.h"
#include "Graph/MovieGraphBlueprintLibrary.h"
#include "Graph/MovieGraphBurnInWidget.h"
#include "Graph/MovieGraphDataTypes.h"
#include "Graph/MovieGraphDefaultRenderer.h"
#include "Graph/MovieGraphPipeline.h"
#include "MovieRenderPipelineCoreModule.h"
#include "OpenColorIODisplayExtension.h"
#include "Slate/WidgetRenderer.h"
#include "Styling/AppStyle.h"
#include "TextureResource.h"
#include "Widgets/SVirtualWindow.h"

const FString UMovieGraphBurnInNode::RendererName = FString("BurnIn");

#if WITH_EDITOR
FText UMovieGraphBurnInNode::GetNodeTitle(const bool bGetDescriptive) const
{
	return NSLOCTEXT("MovieGraphNodes", "BurnInGraphNode_Description", "Burn In");
}

FLinearColor UMovieGraphBurnInNode::GetNodeTitleColor() const
{
	return FLinearColor(0.572f, 0.274f, 1.f);
}

FSlateIcon UMovieGraphBurnInNode::GetIconAndTint(FLinearColor& OutColor) const
{
	static const FSlateIcon DeferredRendererIcon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "SequenceRecorder.TabIcon");

	OutColor = FLinearColor::White;
	return DeferredRendererIcon;
}
#endif	// WITH_EDITOR

void UMovieGraphBurnInNode::SetupImpl(const FMovieGraphRenderPassSetupData& InSetupData)
{
	for (const FMovieGraphRenderPassLayerData& LayerData : InSetupData.Layers)
	{
		TUniquePtr<FMovieGraphBurnInPass> RendererInstance = MakeUnique<FMovieGraphBurnInPass>();
		RendererInstance->Setup(InSetupData.Renderer, LayerData);
		CurrentInstances.Add(MoveTemp(RendererInstance));
	}

	constexpr bool bApplyGammaCorrection = false;
	WidgetRenderer = MakeShared<FWidgetRenderer>(bApplyGammaCorrection);
}

void UMovieGraphBurnInNode::TeardownImpl()
{
	for (const TUniquePtr<FMovieGraphBurnInPass>& Instance : CurrentInstances)
	{
		Instance->Teardown();
	}
	
	CurrentInstances.Reset();

	if (FSlateApplication::IsInitialized())
	{
		for (const TTuple<const FIntPoint, TSharedPtr<SVirtualWindow>>& Entry : SharedVirtualWindows)
		{
			const TSharedPtr<SVirtualWindow>& VirtualWindow = Entry.Value;
			if (VirtualWindow.IsValid())
			{
				FSlateApplication::Get().UnregisterVirtualWindow(VirtualWindow.ToSharedRef());
			}
		}
	}

	SharedVirtualWindows.Empty();
	WidgetRenderer = nullptr;
	BurnInWidgetInstances.Empty();
}

void UMovieGraphBurnInNode::RenderImpl(const FMovieGraphTraversalContext& InFrameTraversalContext, const FMovieGraphTimeStepData& InTimeData)
{
	for (const TUniquePtr<FMovieGraphBurnInPass>& Instance : CurrentInstances)
	{
		Instance->Render(InFrameTraversalContext, InTimeData);
	}
}

void UMovieGraphBurnInNode::GatherOutputPassesImpl(TArray<FMovieGraphRenderDataIdentifier>& OutExpectedPasses) const
{
	for (const TUniquePtr<FMovieGraphBurnInPass>& Instance : CurrentInstances)
    {
    	Instance->GatherOutputPassesImpl(OutExpectedPasses);
    }
}

TSharedPtr<SVirtualWindow> UMovieGraphBurnInNode::GetOrCreateVirtualWindow(const FIntPoint& InResolution)
{
	if (const TSharedPtr<SVirtualWindow>* ExistingVirtualWindow = SharedVirtualWindows.Find(InResolution))
	{
		return *ExistingVirtualWindow;
	}

	const TSharedPtr<SVirtualWindow> NewVirtualWindow = SNew(SVirtualWindow).Size(FVector2D(InResolution.X, InResolution.Y));
	SharedVirtualWindows.Emplace(InResolution, NewVirtualWindow);

	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().RegisterVirtualWindow(NewVirtualWindow.ToSharedRef());
	}

	return NewVirtualWindow;
}

TObjectPtr<UMovieGraphBurnInWidget> UMovieGraphBurnInNode::GetOrCreateBurnInWidget(UClass* InWidgetClass, UWorld* InOwner)
{
	if (const TObjectPtr<UMovieGraphBurnInWidget>* ExistingBurnInWidget = BurnInWidgetInstances.Find(InWidgetClass))
	{
		return *ExistingBurnInWidget;
	}

	TObjectPtr<UMovieGraphBurnInWidget> BurnInWidget = CreateWidget<UMovieGraphBurnInWidget>(InOwner, InWidgetClass);
	if (BurnInWidget)
	{
		BurnInWidgetInstances.Emplace(InWidgetClass, BurnInWidget);
	}

	return BurnInWidget;
}

void UMovieGraphBurnInNode::FMovieGraphBurnInPass::Setup(TWeakObjectPtr<UMovieGraphDefaultRenderer> InRenderer, const FMovieGraphRenderPassLayerData& InLayer)
{
	LayerData = InLayer;
	Renderer = InRenderer;
	RenderPassNode = CastChecked<UMovieGraphBurnInNode>(InLayer.RenderPassNode);

	RenderDataIdentifier.RootBranchName = LayerData.BranchName;
	RenderDataIdentifier.RendererName = RenderPassNode->GetRendererName();
	RenderDataIdentifier.SubResourceName = TEXT("burnin");

	UE::MovieGraph::DefaultRenderer::FCameraInfo CameraInfo = Renderer->GetCameraInfo(LayerData.CameraIdentifier);
	RenderDataIdentifier.CameraName = CameraInfo.CameraName;
}

void UMovieGraphBurnInNode::FMovieGraphBurnInPass::Teardown()
{
	// Nothing to do
}

void UMovieGraphBurnInNode::FMovieGraphBurnInPass::Render(const FMovieGraphTraversalContext& InFrameTraversalContext, const FMovieGraphTimeStepData& InTimeData)
{
	UMovieGraphPipeline* Pipeline = Renderer->GetOwningGraph();
	
	const FIntPoint OutputResolution = UMovieGraphBlueprintLibrary::GetEffectiveOutputResolution(InTimeData.EvaluatedConfig, LayerData.BranchName);
	const int32 MaxResolution = GetMax2DTextureDimension();
	if ((OutputResolution.X > MaxResolution) || (OutputResolution.Y > MaxResolution))
	{
		UE_LOG(LogMovieRenderPipeline, Error, TEXT("Resolution %dx%d exceeds maximum allowed by GPU. Burn-ins do not support high-resolution tiling and thus can't exceed %dx%d."), OutputResolution.X, OutputResolution.Y, MaxResolution, MaxResolution);
		return;
	}

	UClass* LoadedBurnInClass = RenderPassNode->BurnInClass.TryLoadClass<UMovieGraphBurnInWidget>();
	if (!LoadedBurnInClass)
	{
		UE_LOG(LogMovieRenderPipeline, Error, TEXT("The burn-in widget provided in layer '%s' for renderer '%s' is not valid."), *LayerData.BranchName.ToString(), *Renderer->GetClass()->GetName());
		return;
	}

	// Create the render target the widget will be rendered into
	UE::MovieGraph::DefaultRenderer::FRenderTargetInitParams RenderTargetInitParams;
	RenderTargetInitParams.Size = OutputResolution;
	RenderTargetInitParams.TargetGamma = FOpenColorIORendering::DefaultDisplayGamma;
	RenderTargetInitParams.PixelFormat = PF_B8G8R8A8;
	UTextureRenderTarget2D* RenderTarget = Renderer->GetOrCreateViewRenderTarget(RenderTargetInitParams);
	
	// The CDO contains the resources which are shared with all FMovieGraphBurnInPass instances
	UMovieGraphBurnInNode* BurnInCDO = RenderPassNode->GetClass()->GetDefaultObject<UMovieGraphBurnInNode>();
	const TSharedPtr<SVirtualWindow> VirtualWindow = BurnInCDO->GetOrCreateVirtualWindow(OutputResolution);
	const TSharedPtr<FWidgetRenderer> BurnInWidgetRenderer = BurnInCDO->WidgetRenderer;
	UMovieGraphBurnInWidget* BurnInWidget = BurnInCDO->GetOrCreateBurnInWidget(LoadedBurnInClass, Pipeline->GetWorld());
	if (!BurnInWidget)
	{
		UE_LOG(LogMovieRenderPipeline, Error, TEXT("Unable to load burn-in widget at path: %s"), *RenderPassNode->BurnInClass.GetAssetPath().ToString());
		return;
	}

	if (InFrameTraversalContext.Time.bIsFirstTemporalSampleForFrame)
	{
		// Put the widget in our window
		VirtualWindow->SetContent(BurnInWidget->TakeWidget());

		// Update the widget with the latest frame information
		BurnInWidget->UpdateForGraph(Pipeline);
		
		// Draw the widget to the render target
		BurnInWidgetRenderer->DrawWindow(RenderTarget, VirtualWindow->GetHittestGrid(), VirtualWindow.ToSharedRef(),
			1.f, OutputResolution, InTimeData.FrameDeltaTime);

		FRenderTarget* BackbufferRenderTarget = RenderTarget->GameThread_GetRenderTargetResource();
		TSharedPtr<UE::MovieGraph::IMovieGraphOutputMerger> OutputMerger = Pipeline->GetOutputMerger();

		ENQUEUE_RENDER_COMMAND(BurnInRenderTargetResolveCommand)(
			[InTimeData, InFrameTraversalContext, RenderTargetInitParams, RenderDataIdentifier = this->RenderDataIdentifier, bComposite = RenderPassNode->bCompositeOntoFinalImage, BackbufferRenderTarget, OutputMerger, OutputResolution](FRHICommandListImmediate& RHICmdList)
			{
				const FIntRect SourceRect = FIntRect(0, 0, BackbufferRenderTarget->GetSizeXY().X, BackbufferRenderTarget->GetSizeXY().Y);

				// Read the data back to the CPU
				TArray<FColor> RawPixels;
				RawPixels.SetNumUninitialized(SourceRect.Width() * SourceRect.Height());

				FReadSurfaceDataFlags ReadDataFlags(ERangeCompressionMode::RCM_MinMax);
				ReadDataFlags.SetLinearToGamma(false);

				{
					// TODO: The readback is taking ~37ms on a 4k image. This is definitely an area that should be a target for optimization in the future.
					TRACE_CPUPROFILER_EVENT_SCOPE(MRQ::FMovieGraphBurnInPass::ReadSurfaceData);
					RHICmdList.ReadSurfaceData(BackbufferRenderTarget->GetRenderTargetTexture(), SourceRect, RawPixels, ReadDataFlags);
				}

				// Take our per-frame Traversal Context and update it with context specific to this sample.
				FMovieGraphTraversalContext UpdatedTraversalContext = InFrameTraversalContext;
				UpdatedTraversalContext.Time = InTimeData;
				UpdatedTraversalContext.RenderDataIdentifier = RenderDataIdentifier;

				TSharedRef<UE::MovieGraph::FMovieGraphSampleState> SampleStatePayload =
					MakeShared<UE::MovieGraph::FMovieGraphSampleState>();
				SampleStatePayload->TraversalContext = MoveTemp(UpdatedTraversalContext);
				SampleStatePayload->BackbufferResolution = RenderTargetInitParams.Size;
				SampleStatePayload->bRequiresAccumulator = false;
				SampleStatePayload->bFetchFromAccumulator = false;
				SampleStatePayload->bCompositeOnOtherRenders = bComposite;

				TUniquePtr<FImagePixelData> PixelData =
					MakeUnique<TImagePixelData<FColor>>(OutputResolution, TArray64<FColor>(MoveTemp(RawPixels)), SampleStatePayload);

				OutputMerger->OnCompleteRenderPassDataAvailable_AnyThread(MoveTemp(PixelData));
			});
	}
}

void UMovieGraphBurnInNode::FMovieGraphBurnInPass::GatherOutputPassesImpl(TArray<FMovieGraphRenderDataIdentifier>& OutExpectedPasses) const
{
	OutExpectedPasses.Add(RenderDataIdentifier);
}