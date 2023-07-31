// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Widgets/SCompoundWidget.h"
#include "MVVM/ViewModelPtr.h"
#include "MVVM/Extensions/ITrackLaneExtension.h"


namespace UE::Sequencer
{

class STrackAreaView;

class SEQUENCERCORE_API STrackAreaLaneView
	: public SCompoundWidget
	, public ITrackLaneWidget
{
public:
	SLATE_BEGIN_ARGS(STrackAreaLaneView){}
		SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const FViewModelPtr& InViewModel, TSharedPtr<STrackAreaView> InTrackAreaView);

	/*~ ITrackLaneWidget */
	virtual TSharedRef<const SWidget> AsWidget() const;
	virtual FTrackLaneScreenAlignment GetAlignment(const FTimeToPixel& InTimeToPixel, const FGeometry& InParentGeometry) const;

	FTimeToPixel GetRelativeTimeToPixel() const;

protected:

	FWeakViewModelPtr WeakModel;
	TWeakPtr<STrackAreaView> WeakTrackAreaView;
	TSharedPtr<FTimeToPixel> TrackAreaTimeToPixel;
};

} // namespace UE::Sequencer
