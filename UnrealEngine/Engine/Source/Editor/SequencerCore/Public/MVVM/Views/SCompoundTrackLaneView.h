// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SPanel.h"
#include "Layout/Children.h"

struct FTimeToPixel;
struct FGeometry;

class FPaintArgs;
class FSlateWindowElementList;

namespace UE::Sequencer
{

class STrackLane;
class ITrackLaneWidget;

DECLARE_DELEGATE_RetVal_OneParam(FTimeToPixel, FGetTimeToPixel, const FGeometry&);

/**
 * 
 */
class SEQUENCERCORE_API SCompoundTrackLaneView
	: public SPanel
{
public:

	SLATE_BEGIN_ARGS(SCompoundTrackLaneView){}

		SLATE_ARGUMENT(FGetTimeToPixel, TimeToPixel)

	SLATE_END_ARGS()

	SCompoundTrackLaneView();
	~SCompoundTrackLaneView();

	void Construct(const FArguments& InArgs);

	void AddWeakWidget(TSharedPtr<ITrackLaneWidget> InWidget, TWeakPtr<STrackLane> InOwningLane);
	void AddStrongWidget(TSharedPtr<ITrackLaneWidget> InWidget, TWeakPtr<STrackLane> InOwningLane);

	/*~ SPanel Interface */
	void OnArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const override;
	FVector2D ComputeDesiredSize(float) const override;
	FChildren* GetChildren() override;

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:

	struct FSlot : TSlotBase<FSlot>
	{
		FSlot(TSharedPtr<ITrackLaneWidget> InInterface, TWeakPtr<STrackLane> InOwningLane);
		FSlot(TWeakPtr<ITrackLaneWidget> InWeakInterface, TWeakPtr<STrackLane> InOwningLane);

		TSharedPtr<ITrackLaneWidget> GetInterface() const
		{
			return Interface ? Interface : WeakInterface.Pin();
		}

		TSharedPtr<ITrackLaneWidget> Interface;
		TWeakPtr<ITrackLaneWidget> WeakInterface;

		TWeakPtr<STrackLane> WeakOwningLane;
	};

	/** All the widgets in the panel */
	TPanelChildren<FSlot> Children;

	FGetTimeToPixel TimeToPixelDelegate;
};

} // namespace UE::Sequencer

