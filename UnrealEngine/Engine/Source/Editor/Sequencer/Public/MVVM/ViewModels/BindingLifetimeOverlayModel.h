// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MVVM/Extensions/ITrackLaneExtension.h"
#include "MVVM/Extensions/IBindingLifetimeExtension.h"
#include "MVVM/Extensions/LinkedOutlinerExtension.h"
#include "MVVM/Extensions/ViewModelExtensionCollection.h"
#include "MVVM/ICastable.h"
#include "MVVM/ViewModels/ViewModel.h"
#include "Math/Range.h"
#include "Templates/SharedPointer.h"
#include "SequencerEditorViewModel.h"

namespace UE::Sequencer { template <typename T> struct TAutoRegisterViewModelTypeID; }
struct FFrameNumber;

namespace UE
{
	namespace Sequencer
	{
		class SEQUENCER_API FBindingLifetimeOverlayModel
			: public FViewModel
			, public FLinkedOutlinerExtension
			, public ITrackLaneExtension
		{
		public:

			UE_SEQUENCER_DECLARE_CASTABLE(FBindingLifetimeOverlayModel, FViewModel
				, FLinkedOutlinerExtension
				, ITrackLaneExtension
			);

			FBindingLifetimeOverlayModel(TWeakPtr<FViewModel> LayerRoot, TWeakPtr<FSequencerEditorViewModel> InEditorViewModel, TViewModelPtr<IBindingLifetimeExtension> InBindingLifetimeTrack);
			~FBindingLifetimeOverlayModel();

			/*~ ITrackLaneExtension Interface */
			TSharedPtr<ITrackLaneWidget> CreateTrackLaneView(const FCreateTrackLaneViewParams& InParams) override;
			FTrackLaneVirtualAlignment ArrangeVirtualTrackLaneView() const override;

			const TArray<FFrameNumberRange>& GetInverseLifetimeRange() const;

		private:

			TWeakPtr<FSequencerEditorViewModel> WeakEditorViewModel;
			TViewModelPtr<IBindingLifetimeExtension> BindingLifetimeTrack;
		};

	} // namespace Sequencer
} // namespace UE

