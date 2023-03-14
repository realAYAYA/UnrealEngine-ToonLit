// Copyright Epic Games, Inc. All Rights Reserved.

#include "MVVM/Views/STrackAreaLaneView.h"
#include "MVVM/Views/STrackAreaView.h"
#include "MVVM/ViewModels/TrackAreaViewModel.h"
#include "TimeToPixel.h"


namespace UE::Sequencer
{

void STrackAreaLaneView::Construct(const FArguments& InArgs, const FViewModelPtr& InViewModel, TSharedPtr<STrackAreaView> InTrackAreaView)
{
	WeakModel = InViewModel;
	WeakTrackAreaView = InTrackAreaView;
	TrackAreaTimeToPixel = InTrackAreaView->GetTimeToPixel();

	ChildSlot
	[
		InArgs._Content.Widget
	];
}

TSharedRef<const SWidget> STrackAreaLaneView::AsWidget() const
{
	return AsShared();
}

FTrackLaneScreenAlignment STrackAreaLaneView::GetAlignment(const FTimeToPixel& InTimeToPixel, const FGeometry& InParentGeometry) const
{
	TSharedPtr<ITrackLaneExtension> TrackLaneExtension = WeakModel.ImplicitPin();
	if (TrackLaneExtension)
	{
		FTrackLaneVirtualAlignment VirtualAlignment = TrackLaneExtension->ArrangeVirtualTrackLaneView();
		FTrackLaneScreenAlignment  ScreenAlignment  = VirtualAlignment.ToScreen(InTimeToPixel, InParentGeometry);

		return ScreenAlignment;
	}
	return FTrackLaneScreenAlignment();
}

FTimeToPixel STrackAreaLaneView::GetRelativeTimeToPixel() const
{
	FTimeToPixel RelativeTimeToPixel = *TrackAreaTimeToPixel;

	TSharedPtr<ITrackLaneExtension> TrackLaneExtension = WeakModel.ImplicitPin();
	if (TrackLaneExtension)
	{
		FTrackLaneVirtualAlignment VirtualAlignment = TrackLaneExtension->ArrangeVirtualTrackLaneView();
		if (VirtualAlignment.Range.GetLowerBound().IsClosed())
		{
			RelativeTimeToPixel = TrackAreaTimeToPixel->RelativeTo(VirtualAlignment.Range.GetLowerBoundValue());
		}
	}

	return RelativeTimeToPixel;
}

} // namespace UE::Sequencer
