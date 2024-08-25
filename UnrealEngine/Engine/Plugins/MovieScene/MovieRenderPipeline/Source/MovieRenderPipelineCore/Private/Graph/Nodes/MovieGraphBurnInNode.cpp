// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/Nodes/MovieGraphBurnInNode.h"

#include "Framework/Application/SlateApplication.h"
#include "Graph/MovieGraphBurnInWidget.h"
#include "Graph/MovieGraphDataTypes.h"
#include "Graph/MovieGraphDefaultRenderer.h"
#include "Graph/MovieGraphPipeline.h"
#include "MovieRenderPipelineCoreModule.h"
#include "Styling/AppStyle.h"
#include "TextureResource.h"

const FString UMovieGraphBurnInNode::RendererName = FString("BurnIn");
const FString UMovieGraphBurnInNode::DefaultBurnInWidgetAsset = TEXT("/MovieRenderPipeline/Blueprints/Graph/DefaultGraphBurnIn.DefaultGraphBurnIn_C");

UMovieGraphBurnInNode::UMovieGraphBurnInNode()
	: BurnInClass(DefaultBurnInWidgetAsset)
{
}

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
	static const FSlateIcon BurnInIcon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "SequenceRecorder.TabIcon");

	OutColor = FLinearColor::White;
	return BurnInIcon;
}
#endif	// WITH_EDITOR

TUniquePtr<UMovieGraphWidgetRendererBaseNode::FMovieGraphWidgetPass> UMovieGraphBurnInNode::GeneratePass()
{
	return MakeUnique<FMovieGraphBurnInPass>();
}

void UMovieGraphBurnInNode::GatherOutputPassesImpl(UMovieGraphEvaluatedConfig* InConfig, TArray<FMovieGraphRenderDataIdentifier>& OutExpectedPasses) const
{
	for (const TUniquePtr<FMovieGraphWidgetPass>& Instance : CurrentInstances)
	{
		// Only generate passes if there's a valid burn-in class
		if (StaticCast<FMovieGraphBurnInPass*>(Instance.Get())->GetBurnInClass())
		{
			Instance->GatherOutputPasses(OutExpectedPasses);
		}
	}
}

void UMovieGraphBurnInNode::TeardownImpl()
{
	Super::TeardownImpl();
	
	BurnInWidgetInstances.Empty();
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
	FMovieGraphWidgetPass::Setup(InRenderer, InLayer);
	
	RenderDataIdentifier.SubResourceName = TEXT("burnin");
}

TSharedPtr<SWidget> UMovieGraphBurnInNode::FMovieGraphBurnInPass::GetWidget()
{
	if (const TObjectPtr<UMovieGraphBurnInWidget> BurnInWidget = GetBurnInWidget())
	{
		return BurnInWidget->TakeWidget();
	}
	
	return nullptr;
}

TObjectPtr<UMovieGraphBurnInWidget> UMovieGraphBurnInNode::FMovieGraphBurnInPass::GetBurnInWidget() const
{
	const UMovieGraphPipeline* Pipeline = Renderer->GetOwningGraph();

	UClass* LoadedBurnInClass = GetBurnInClass();
	if (!LoadedBurnInClass)
	{
		UE_LOG(LogMovieRenderPipeline, Error, TEXT("The burn-in widget provided in layer '%s' for renderer '%s' is not valid."), *LayerData.BranchName.ToString(), *Renderer->GetClass()->GetName());
		return nullptr;
	}
	
	// The CDO contains the resources which are shared with all FMovieGraphBurnInPass instances
	UMovieGraphBurnInNode* BurnInCDO = RenderPassNode->GetClass()->GetDefaultObject<UMovieGraphBurnInNode>();
	const TObjectPtr<UMovieGraphBurnInWidget> BurnInWidget = BurnInCDO->GetOrCreateBurnInWidget(LoadedBurnInClass, Pipeline->GetWorld());
	if (!BurnInWidget)
	{
		const UMovieGraphBurnInNode* BurnInNode = CastChecked<UMovieGraphBurnInNode>(RenderPassNode);
		UE_LOG(LogMovieRenderPipeline, Error, TEXT("Unable to load burn-in widget at path: %s"), *BurnInNode->BurnInClass.GetAssetPath().ToString());
		return nullptr;
	}

	return BurnInWidget;
}

void UMovieGraphBurnInNode::FMovieGraphBurnInPass::Render(const FMovieGraphTraversalContext& InFrameTraversalContext, const FMovieGraphTimeStepData& InTimeData)
{
	// Update the widget with the latest frame information
	if (const TObjectPtr<UMovieGraphBurnInWidget> BurnInWidget = GetBurnInWidget())
	{
		UMovieGraphPipeline* Pipeline = Renderer->GetOwningGraph();
		BurnInWidget->UpdateForGraph(Pipeline, Pipeline->GetTimeStepInstance()->GetCalculatedTimeData().EvaluatedConfig);
	}
	
	FMovieGraphWidgetPass::Render(InFrameTraversalContext, InTimeData);
}

int32 UMovieGraphBurnInNode::FMovieGraphBurnInPass::GetCompositingSortOrder() const
{
	// Burn-ins should always appear over all other passes
	return 100;
}

UClass* UMovieGraphBurnInNode::FMovieGraphBurnInPass::GetBurnInClass() const
{
	const UMovieGraphBurnInNode* BurnInNode = CastChecked<UMovieGraphBurnInNode>(RenderPassNode);
	return BurnInNode->BurnInClass.TryLoadClass<UMovieGraphBurnInWidget>();
}