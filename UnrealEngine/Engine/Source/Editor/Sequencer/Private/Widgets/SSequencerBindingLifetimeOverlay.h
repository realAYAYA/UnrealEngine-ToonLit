// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Math/Range.h"
#include "Misc/FrameNumber.h"

#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

#include "MVVM/Extensions/ITrackLaneExtension.h"
#include "MVVM/Extensions/ViewModelExtensionCollection.h"
#include "MVVM/ViewModels/BindingLifetimeOverlayModel.h"

class ISequencer;
class FSequencer;
class FScopedTransaction;
class FSequencerDisplayNode;
struct FTimeToPixel;

namespace UE
{
	namespace Sequencer
	{
		class FObjectBindingModel;
		class FSequencerEditorViewModel;
		class FViewModel;
		class STrackAreaView;

		class SSequencerBindingLifetimeOverlay
			: public SCompoundWidget
			, public ITrackLaneWidget
		{
		public:
			SLATE_BEGIN_ARGS(SSequencerBindingLifetimeOverlay) {}
			SLATE_END_ARGS()

			void Construct(const FArguments& InArgs, TWeakPtr<STrackAreaView> InWeakTrackArea, TWeakPtr<FSequencerEditorViewModel> InWeakEditor, TWeakPtr<FBindingLifetimeOverlayModel> InWeakBindingLifetimeOverlayModel);

			TSharedPtr<FBindingLifetimeOverlayModel> GetBindingLifetimeOverlayModel() const;
			TSharedPtr<FSequencer> GetSequencer() const;



		private:

			/*~ ITrackLaneWidget interface */
			TSharedRef<const SWidget> AsWidget() const override;
			FTrackLaneScreenAlignment GetAlignment(const FTimeToPixel& TimeToPixel, const FGeometry& InParentGeometry) const override;
			int32 GetOverlapPriority() const override;

			/*~ SCompoundWidget interface */
			int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

		private:

			TWeakPtr<FBindingLifetimeOverlayModel> WeakBindingLifetimeOverlayModel;
			TWeakPtr<FSequencerEditorViewModel> WeakEditor;
			TWeakPtr<STrackAreaView> WeakTrackArea;
			TSharedPtr<FTimeToPixel> TimeToPixel;
		};

	} // namespace Sequencer
} // namespace UE

