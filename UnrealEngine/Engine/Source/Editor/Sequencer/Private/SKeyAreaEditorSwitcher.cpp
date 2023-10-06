// Copyright Epic Games, Inc. All Rights Reserved.

#include "SKeyAreaEditorSwitcher.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Sequencer.h"
#include "SequencerCommonHelpers.h"
#include "IKeyArea.h"
#include "MVVM/ViewModels/ChannelModel.h"
#include "MVVM/Extensions/IObjectBindingExtension.h"
#include "MVVM/Views/SKeyNavigationButtons.h"

namespace UE
{
namespace Sequencer
{

void SKeyAreaEditorSwitcher::Construct(const FArguments& InArgs, TSharedPtr<FChannelGroupModel> InModel, TWeakPtr<ISequencer> InWeakSequencer)
{
	WeakModel = InModel;
	WeakSequencer = InWeakSequencer;
	CachedChannelsSerialNumber = 0;
}

int32 SKeyAreaEditorSwitcher::GetWidgetIndex() const
{
	return VisibleIndex;
}

void SKeyAreaEditorSwitcher::Rebuild()
{
	TSharedPtr<FChannelGroupModel> Model     = WeakModel.Pin();
	TSharedPtr<ISequencer>         Sequencer = WeakSequencer.Pin();
	if (!Model || !Sequencer)
	{
		// Empty our cache so we don't persistently rebuild
		CachedKeyAreas.Empty();

		// Node is no longer valid so just make this a null widget
		ChildSlot
		[
			SNullWidget::NullWidget
		];
		return;
	}

	const bool bIsEnabled = !Sequencer->IsReadOnly();

	// Index 0 is always the spacer node
	VisibleIndex = 0;

	TSharedRef<SWidgetSwitcher> Switcher = SNew(SWidgetSwitcher)
		.IsEnabled(bIsEnabled)
		.WidgetIndex(this, &SKeyAreaEditorSwitcher::GetWidgetIndex)

		+ SWidgetSwitcher::Slot()
		[
			SNullWidget::NullWidget
		];

	TSharedPtr<IObjectBindingExtension> ParentObjectBinding = Model->FindAncestorOfType<IObjectBindingExtension>();
	FGuid ObjectBindingID = ParentObjectBinding.IsValid() ? ParentObjectBinding->GetObjectGuid() : FGuid();

	for (TSharedRef<IKeyArea> KeyArea : CachedKeyAreas)
	{
		if (!KeyArea->CanCreateKeyEditor())
		{
			// Always generate a slot so that indices line up correctly
			Switcher->AddSlot()
			[
				SNullWidget::NullWidget
			];
		}
		else
		{
			Switcher->AddSlot()
			[
				SNew(SBox)
				.IsEnabled(bIsEnabled)
				.MinDesiredWidth(100)
				.HAlign(HAlign_Left)
				[
					KeyArea->CreateKeyEditor(Sequencer, ObjectBindingID)
				]
			];
		}
	}

	ChildSlot
	[
		Switcher
	];
}

void SKeyAreaEditorSwitcher::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	TSharedPtr<FChannelGroupModel> Model = WeakModel.Pin();
	TSharedPtr<ISequencer>         Sequencer = WeakSequencer.Pin();
	if (!Model || !Sequencer)
	{
		if (CachedKeyAreas.Num() != 0)
		{
			CachedKeyAreas.Empty();

			// Node is not valid but we have a valid cache - we need to rebuild the switcher now
			Rebuild();
		}
		return;
	}
	else
	{
		const uint32 NewChannelsSerialNumber = Model->GetChannelsSerialNumber();
		const bool bRebuild = (NewChannelsSerialNumber != CachedChannelsSerialNumber);
		if (bRebuild)
		{
			CachedChannelsSerialNumber = NewChannelsSerialNumber;
			CachedKeyAreas = Model->GetAllKeyAreas();
			Rebuild();
		}

		// Figure out which widget is active based on which section the current time is at.
		TArray<UMovieSceneSection*> AllSections;
		for (const TSharedRef<IKeyArea>& KeyArea : CachedKeyAreas)
		{
			AllSections.Add(KeyArea->GetOwningSection());
		}

		const int32 ActiveKeyArea = SequencerHelpers::GetSectionFromTime(AllSections, Sequencer->GetLocalTime().Time.FrameNumber);
		if (ActiveKeyArea != INDEX_NONE)
		{
			// Index 0 is the spacer node, so add 1 to the key area index to get the widget index
			VisibleIndex = 1 + ActiveKeyArea;
		}
		else
		{
			VisibleIndex = 0;
		}
	}
}

} // namespace Sequencer
} // namespace UE
