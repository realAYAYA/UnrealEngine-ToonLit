// Copyright Epic Games, Inc. All Rights Reserved.

#include "SPoseSearchDatabaseViewport.h"
#include "Framework/Application/SlateApplication.h"
#include "PoseSearchDatabaseViewportClient.h"
#include "PoseSearchDatabaseViewModel.h"
#include "PoseSearchDatabaseEditor.h"
#include "SPoseSearchDatabaseViewportToolbar.h"
#include "PoseSearchDatabaseEditorCommands.h"

#include "SSimpleTimeSlider.h"
#include "Viewports.h"
#include "Widgets/Input/SButton.h"

namespace UE::PoseSearch
{
	void SDatabaseViewport::Construct(
		const FArguments& InArgs,
		const FDatabasePreviewRequiredArgs& InRequiredArgs)
	{
		PreviewScenePtr = InRequiredArgs.PreviewScene;
		AssetEditorPtr = InRequiredArgs.AssetEditor;

		SEditorViewport::Construct(
			SEditorViewport::FArguments()
			.IsEnabled(FSlateApplication::Get().GetNormalExecutionAttribute())
			.AddMetaData<FTagMetaData>(TEXT("AnimationTools.Viewport"))
		);
	}

	void SDatabaseViewport::BindCommands()
	{
		SEditorViewport::BindCommands();

		const FDatabaseEditorCommands& Commands = FDatabaseEditorCommands::Get();

		TSharedRef<FDatabaseViewModel> ViewModelRef =
			AssetEditorPtr.Pin()->GetViewModelSharedPtr().ToSharedRef();

		CommandList->MapAction(
			Commands.ShowPoseFeaturesNone,
			FExecuteAction::CreateSP(
				ViewModelRef,
				&FDatabaseViewModel::OnSetPoseFeaturesDrawMode,
				EFeaturesDrawMode::None),
			FCanExecuteAction(),
			FIsActionChecked::CreateSP(
				ViewModelRef,
				&FDatabaseViewModel::IsPoseFeaturesDrawMode,
				EFeaturesDrawMode::None));

		CommandList->MapAction(
			Commands.ShowPoseFeaturesAll,
			FExecuteAction::CreateSP(
				ViewModelRef,
				&FDatabaseViewModel::OnSetPoseFeaturesDrawMode,
				EFeaturesDrawMode::All),
			FCanExecuteAction(),
			FIsActionChecked::CreateSP(
				ViewModelRef,
				&FDatabaseViewModel::IsPoseFeaturesDrawMode,
				EFeaturesDrawMode::All));

		CommandList->MapAction(
			Commands.ShowPoseFeaturesDetailed,
			FExecuteAction::CreateSP(
				ViewModelRef,
				&FDatabaseViewModel::OnSetPoseFeaturesDrawMode,
				EFeaturesDrawMode::Detailed),
			FCanExecuteAction(),
			FIsActionChecked::CreateSP(
				ViewModelRef,
				&FDatabaseViewModel::IsPoseFeaturesDrawMode,
				EFeaturesDrawMode::Detailed));

		CommandList->MapAction(
			Commands.ShowAnimationOriginalOnly,
			FExecuteAction::CreateSP(
				ViewModelRef,
				&FDatabaseViewModel::OnSetAnimationPreviewMode,
				EAnimationPreviewMode::OriginalOnly),
			FCanExecuteAction(),
			FIsActionChecked::CreateSP(
				ViewModelRef,
				&FDatabaseViewModel::IsAnimationPreviewMode,
				EAnimationPreviewMode::OriginalOnly));

		CommandList->MapAction(
			Commands.ShowAnimationOriginalAndMirrored,
			FExecuteAction::CreateSP(
				ViewModelRef,
				&FDatabaseViewModel::OnSetAnimationPreviewMode,
				EAnimationPreviewMode::OriginalAndMirrored),
			FCanExecuteAction(),
			FIsActionChecked::CreateSP(
				ViewModelRef,
				&FDatabaseViewModel::IsAnimationPreviewMode,
				EAnimationPreviewMode::OriginalAndMirrored));

		CommandList->MapAction(
			Commands.ShowDisplayRootMotionSpeed,
			FExecuteAction::CreateSP(ViewModelRef, &FDatabaseViewModel::OnToggleDisplayRootMotionSpeed),
			FCanExecuteAction(),
			FIsActionChecked::CreateSP(ViewModelRef, &FDatabaseViewModel::IsDisplayRootMotionSpeedChecked));

		CommandList->MapAction(
			Commands.ShowQuantizeAnimationToPoseData,
			FExecuteAction::CreateSP(ViewModelRef, &FDatabaseViewModel::OnToggleQuantizeAnimationToPoseData),
			FCanExecuteAction(),
			FIsActionChecked::CreateSP(ViewModelRef, &FDatabaseViewModel::IsQuantizeAnimationToPoseDataChecked));

		CommandList->MapAction(
			Commands.ShowBones,
			FExecuteAction::CreateSP(ViewModelRef, &FDatabaseViewModel::OnToggleShowBones),
			FCanExecuteAction(),
			FIsActionChecked::CreateSP(ViewModelRef, &FDatabaseViewModel::IsShowBones));
		
	}

	TSharedRef<FEditorViewportClient> SDatabaseViewport::MakeEditorViewportClient()
	{
		ViewportClient = MakeShared<FDatabaseViewportClient>(
			PreviewScenePtr.Pin().ToSharedRef(),
			SharedThis(this),
			AssetEditorPtr.Pin().ToSharedRef());
		ViewportClient->ViewportType = LVT_Perspective;
		ViewportClient->bSetListenerPosition = false;
		ViewportClient->SetViewLocation(EditorViewportDefs::DefaultPerspectiveViewLocation);
		ViewportClient->SetViewRotation(EditorViewportDefs::DefaultPerspectiveViewRotation);

		return ViewportClient.ToSharedRef();
	}

	TSharedPtr<SWidget> SDatabaseViewport::MakeViewportToolbar()
	{
		return SAssignNew(ViewportToolbar, SPoseSearchDatabaseViewportToolBar, SharedThis(this));
	}


	TSharedRef<SEditorViewport> SDatabaseViewport::GetViewportWidget()
	{
		return SharedThis(this);
	}

	TSharedPtr<FExtender> SDatabaseViewport::GetExtenders() const
	{
		TSharedPtr<FExtender> Result(MakeShareable(new FExtender));
		return Result;
	}

	void SDatabaseViewport::OnFloatingButtonClicked()
	{
	}

	void SDatabasePreview::Construct(const FArguments& InArgs, const FDatabasePreviewRequiredArgs& InRequiredArgs)
	{
		SliderColor = InArgs._SliderColor;
		SliderScrubTime = InArgs._SliderScrubTime;
		SliderViewRange = InArgs._SliderViewRange;
		OnSliderScrubPositionChanged = InArgs._OnSliderScrubPositionChanged;

		OnBackwardEnd = InArgs._OnBackwardEnd;
		OnBackwardStep = InArgs._OnBackwardStep;
		OnBackward = InArgs._OnBackward;
		OnPause = InArgs._OnPause;
		OnForward = InArgs._OnForward;
		OnForwardStep = InArgs._OnForwardStep;
		OnForwardEnd = InArgs._OnForwardEnd;

		FSlimHorizontalToolBarBuilder ToolBarBuilder(
			TSharedPtr<const FUICommandList>(), 
			FMultiBoxCustomization::None, 
			nullptr, true);

		auto AddToolBarButton = [&ToolBarBuilder](FName ButtonStyleName, FOnButtonClickedEvent& OnClicked)
		{
			ToolBarBuilder.AddToolBarWidget(
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), ButtonStyleName)
				.OnClicked_Lambda([&OnClicked]() 
				{
					if (OnClicked.IsBound())
					{
						OnClicked.Execute();
						return FReply::Handled();
					}
					return FReply::Unhandled();
				}));
		};

		//ToolBarBuilder.SetStyle(&FAppStyle::Get(), "PaletteToolBar");
		ToolBarBuilder.BeginSection("Preview");
		{
			AddToolBarButton("Animation.Backward_End", OnBackwardEnd);
			AddToolBarButton("Animation.Backward_Step", OnBackwardStep);
			AddToolBarButton("Animation.Backward", OnBackward);
			AddToolBarButton("Animation.Pause", OnPause);
			AddToolBarButton("Animation.Forward", OnForward);
			AddToolBarButton("Animation.Forward_Step", OnForwardStep);
			AddToolBarButton("Animation.Forward_End", OnForwardEnd);
		}

		ChildSlot
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SNew(SDatabaseViewport, InRequiredArgs)
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					ToolBarBuilder.MakeWidget()
				]
				+SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SSimpleTimeSlider)
					.ClampRangeHighlightSize(0.15f)
					.ClampRangeHighlightColor_Lambda([this]()
						{
							return SliderColor.Get();
						})
					.ScrubPosition_Lambda([this]()
						{
							return SliderScrubTime.Get();
						})
					.ViewRange_Lambda([this]()
						{
							return SliderViewRange.Get();
						})
					.ClampRange_Lambda([this]()
						{
							return SliderViewRange.Get();
						})
					.OnScrubPositionChanged_Lambda([this](double NewScrubTime, bool bIsScrubbing)
						{
							if (bIsScrubbing)
							{
								OnSliderScrubPositionChanged.ExecuteIfBound(NewScrubTime, bIsScrubbing);
							}
						})
				]
			]
		];
	}
}

