// Copyright Epic Games, Inc. All Rights Reserved.

#include "MVVM/Views/SOutlinerTrackView.h"
#include "MVVM/ViewModels/ViewModel.h"
#include "MVVM/ICastable.h"
#include "MVVM/Extensions/ITrackExtension.h"
#include "MVVM/Extensions/IObjectBindingExtension.h"
#include "MVVM/ViewModels/ChannelModel.h"
#include "MVVM/ViewModels/TrackModel.h"
#include "MVVM/ViewModels/ViewModelIterators.h"
#include "MVVM/ViewModels/OutlinerViewModel.h"
#include "MVVM/ViewModels/SequencerEditorViewModel.h"
#include "MVVM/Views/STrackAreaView.h"
#include "ISequencerTrackEditor.h"
#include "SSequencerKeyNavigationButtons.h"
#include "SKeyAreaEditorSwitcher.h"

#include "MovieSceneTrack.h"
#include "Tracks/MovieScenePropertyTrack.h"
#include "Tracks/MovieScenePrimitiveMaterialTrack.h"
#include "Tracks/MovieScene3DTransformTrack.h"

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Colors/SColorPicker.h"

#include "Engine/Engine.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "SOutlinerTrackView"

namespace UE
{
namespace Sequencer
{

void SOutlinerTrackView::Construct(
		const FArguments& InArgs,
		TWeakViewModelPtr<IOutlinerExtension> InWeakOutlinerExtension,
		TWeakPtr<FSequencerEditorViewModel> InWeakEditor,
		const TSharedRef<ISequencerTreeViewRow>& InTableRow)
{
	using namespace UE::Sequencer;

	TSharedPtr<FViewModel> DataModel = InWeakOutlinerExtension.Pin().AsModel();
	FSequencerEditorViewModel* EditorViewModel = ICastable::CastWeakPtr<FSequencerEditorViewModel>(InWeakEditor);
	check(DataModel && EditorViewModel);

	ITrackExtension* TrackExtension = DataModel->CastThis<ITrackExtension>();
	checkf(TrackExtension != nullptr, TEXT("It is invalid to create a SOutlinerTrackView widget with a model that does not implement ITrackExtension"));

	TSharedPtr<ISequencerTrackEditor> TrackEditor = EditorViewModel->GetSequencer()->GetTrackEditor(TrackExtension->GetTrack());

	TOptional<FViewModelChildren> TopLevelChannels = DataModel->FindChildList(FTrackModel::GetTopLevelChannelGroupType());

	TAttribute<bool> HoverState = MakeAttributeSP(&InTableRow.Get(), &ISequencerTreeViewRow::IsHovered);

	TSharedRef<SHorizontalBox> BoxPanel = SNew(SHorizontalBox);

	TSharedPtr<IObjectBindingExtension> ObjectBinding = DataModel->FindAncestorOfType<IObjectBindingExtension>();

	UMovieSceneTrack* Track = TrackExtension->GetTrack();
	int32 RowIndex = TrackExtension->GetRowIndex();

	FBuildEditWidgetParams Params;
	Params.NodeIsHovered = HoverState;
	if (DataModel->FindAncestorOfType<ITrackExtension>())
	{
		Params.TrackInsertRowIndex = TrackExtension->GetRowIndex();
	}
	else if (Track && Track->SupportsMultipleRows())
	{
		Params.TrackInsertRowIndex = Track->GetMaxRowIndex()+1;
	}

	TSharedPtr<SWidget> CustomWidget = TrackEditor->BuildOutlinerEditWidget(ObjectBinding ? ObjectBinding->GetObjectGuid() : FGuid(), Track, Params);

	TSharedPtr<FChannelGroupModel> TopLevelChannel = TopLevelChannels ? TopLevelChannels->FindFirstChildOfType<FChannelGroupModel>() : nullptr;
	if (TopLevelChannel)
	{
		TSharedRef<SOverlay> Overlay = SNew(SOverlay);

		Overlay->AddSlot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		[
			SNew(SKeyAreaEditorSwitcher, TopLevelChannel, EditorViewModel->GetSequencer())
		];

		if (CustomWidget.IsValid())
		{
			Overlay->AddSlot()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			[
				CustomWidget.ToSharedRef()
			];
		}

		BoxPanel->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			Overlay
		];

		BoxPanel->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SSequencerKeyNavigationButtons, DataModel, EditorViewModel->GetSequencer())
		];
	}
	else
	{
		if (CustomWidget.IsValid())
		{
			BoxPanel->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				CustomWidget.ToSharedRef()
			];
		}

		bool bHasKeyableAreas = false;
		for (TSharedPtr<FChannelGroupModel> ChannelGroup : DataModel->GetDescendantsOfType<FChannelGroupModel>())
		{
			for (const TWeakViewModelPtr<FChannelModel>& WeakChannel : ChannelGroup->GetChannels())
			{
				if (TViewModelPtr<FChannelModel> Channel = WeakChannel.Pin())
				{
					if (Channel->GetKeyArea()->CanCreateKeyEditor())
					{
						bHasKeyableAreas = true;
						break;
					}
				}
			}
			if (bHasKeyableAreas)
			{
				break;
			}
		}

		if (bHasKeyableAreas)
		{
			BoxPanel->AddSlot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(SSequencerKeyNavigationButtons, DataModel, EditorViewModel->GetSequencer())
			];
		}
	}

	SOutlinerItemViewBase::Construct(
		FArguments(InArgs)
		.CustomContent()
		[
			SNew(SBox)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Right)
			[
				BoxPanel
			]
		]
		.RightGutterContent()
		[
			SNew(SButton)
			.ContentPadding(0)
			.VAlign(VAlign_Fill)
			.IsFocusable(false) // Intentionally false so that it's easier to tab to the next numeric input
			.IsEnabled(!IsReadOnlyAttribute.Get())
			.ButtonStyle(FAppStyle::Get(), "Sequencer.AnimationOutliner.ColorStrip")
			.OnClicked(this, &SOutlinerTrackView::OnSetTrackColor)
			.Content()
			[
				SNew(SBox)
				.WidthOverride(6.f)
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush("WhiteBrush"))
					.ColorAndOpacity(this, &SOutlinerTrackView::GetTrackColorTint)
				]
			]
		], InWeakOutlinerExtension, InWeakEditor, InTableRow);
}

// We store these for when the Color Picker is canceled so we can restore the old value.
namespace AnimationOutlinerTreeNode
{
	FLinearColor InitialTrackColor;
	bool bFolderPickerWasCancelled;
}

FReply SOutlinerTrackView::OnSetTrackColor()
{
	AnimationOutlinerTreeNode::InitialTrackColor = GetTrackColorTint().GetSpecifiedColor();
	AnimationOutlinerTreeNode::bFolderPickerWasCancelled = false;

	FColorPickerArgs PickerArgs;
	PickerArgs.bUseAlpha = false;
	PickerArgs.DisplayGamma = TAttribute<float>::Create(TAttribute<float>::FGetter::CreateUObject(GEngine, &UEngine::GetDisplayGamma));
	PickerArgs.InitialColorOverride = AnimationOutlinerTreeNode::InitialTrackColor;
	PickerArgs.ParentWidget = GetParentWidget();
	PickerArgs.OnColorCommitted = FOnLinearColorValueChanged::CreateSP(this, &SOutlinerTrackView::OnColorPickerPicked);
	PickerArgs.OnColorPickerWindowClosed = FOnWindowClosed::CreateSP(this, &SOutlinerTrackView::OnColorPickerClosed);
	PickerArgs.OnColorPickerCancelled = FOnColorPickerCancelled::CreateSP(this, &SOutlinerTrackView::OnColorPickerCancelled);

	OpenColorPicker(PickerArgs);
	return FReply::Handled();
}

void SOutlinerTrackView::OnColorPickerPicked(FLinearColor NewFolderColor)
{
	UMovieSceneTrack* Track = GetTrackFromNode();
	if (Track)
	{
		// This is called every time the user adjusts the UI so we don't want to create a transaction for it, just directly
		// modify the track so we can see the change immediately.
		Track->SetColorTint(NewFolderColor.ToFColor(true));
	}
}

void SOutlinerTrackView::OnColorPickerClosed(const TSharedRef<SWindow>& Window)
{
	// Under Unreal UX terms, closing the Color Picker (via the UI) is the same as confirming it since we've been live updating
	// the color. The track already has the latest color change so we undo the change before calling Modify so that Undo sets us
	// to the original color. This is also called in the event of pressing cancel so we need to detect if it was canceled or not.
	if (!AnimationOutlinerTreeNode::bFolderPickerWasCancelled)
	{
		UMovieSceneTrack* Track = GetTrackFromNode();
		if(Track)
		{
			const FScopedTransaction Transaction(LOCTEXT("SetTrackColor", "Set Track Color"));
			FSlateColor CurrentColor = GetTrackColorTint();
			Track->SetColorTint(AnimationOutlinerTreeNode::InitialTrackColor.ToFColor(true));
			Track->Modify();
			Track->SetColorTint(CurrentColor.GetSpecifiedColor().ToFColor(true));
		}
	}
}

void SOutlinerTrackView::OnColorPickerCancelled(FLinearColor NewFolderColor)
{
	AnimationOutlinerTreeNode::bFolderPickerWasCancelled = true;

	// Restore the original color of the track. No transaction will be created when the OnColorPickerClosed callback is called.
	UMovieSceneTrack* Track = GetTrackFromNode();
	if (Track)
	{
		Track->SetColorTint(AnimationOutlinerTreeNode::InitialTrackColor.ToFColor(true));
	}
}

UMovieSceneTrack* SOutlinerTrackView::GetTrackFromNode() const
{
	TViewModelPtr<IOutlinerExtension> Outliner = WeakOutlinerExtension.Pin();
	if (Outliner)
	{
		constexpr bool bIncludeThis = true;
		for (TSharedPtr<ITrackExtension> TrackExtension : Outliner.AsModel()->GetAncestorsOfType<ITrackExtension>(bIncludeThis))
		{
			if (UMovieSceneTrack* Track = TrackExtension->GetTrack())
			{
				return Track;
			}
		}
	}

	return nullptr;
}

FSlateColor SOutlinerTrackView::GetTrackColorTint() const
{
	UMovieSceneTrack* Track = GetTrackFromNode();
	if (Track)
	{
		return STrackAreaView::BlendDefaultTrackColor(Track->GetColorTint());
	}

	return FLinearColor::Transparent;
}

} // namespace Sequencer
} // namespace UE

#undef LOCTEXT_NAMESPACE

