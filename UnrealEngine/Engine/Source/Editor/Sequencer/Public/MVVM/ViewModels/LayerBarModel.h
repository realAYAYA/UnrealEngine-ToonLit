// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MVVM/Extensions/IDraggableTrackAreaExtension.h"
#include "MVVM/Extensions/ILayerBarExtension.h"
#include "MVVM/Extensions/ISelectableExtension.h"
#include "MVVM/Extensions/ISnappableExtension.h"
#include "MVVM/Extensions/IStretchableExtension.h"
#include "MVVM/Extensions/ITrackLaneExtension.h"
#include "MVVM/Extensions/LinkedOutlinerExtension.h"
#include "MVVM/Extensions/ViewModelExtensionCollection.h"
#include "MVVM/ICastable.h"
#include "MVVM/ViewModels/ViewModel.h"
#include "Math/Range.h"
#include "Templates/SharedPointer.h"

namespace UE::Sequencer { class ILayerBarExtension; }
namespace UE::Sequencer { template <typename T> struct TAutoRegisterViewModelTypeID; }
struct FFrameNumber;

namespace UE
{
namespace Sequencer
{

class FSequenceModel;

class SEQUENCER_API FLayerBarModel
	: public FViewModel
	, public FLinkedOutlinerExtension
	, public ITrackLaneExtension
	, public ISelectableExtension
	, public ISnappableExtension
	, public IDraggableTrackAreaExtension
	, public IStretchableExtension
	, protected TViewModelExtensionCollection<ILayerBarExtension>
{
public:

	UE_SEQUENCER_DECLARE_CASTABLE(FLayerBarModel, FViewModel
		, FLinkedOutlinerExtension
		, ITrackLaneExtension
		, ISelectableExtension
		, ISnappableExtension
		, IDraggableTrackAreaExtension
		, IStretchableExtension
	);

	FLayerBarModel(TWeakPtr<FViewModel> LayerRoot);
	~FLayerBarModel();

	/*~ ITrackLaneExtension Interface */
	TSharedPtr<ITrackLaneWidget> CreateTrackLaneView(const FCreateTrackLaneViewParams& InParams) override;
	FTrackLaneVirtualAlignment ArrangeVirtualTrackLaneView() const override;

	/*~ ISelectableExtension Interface */
	ESelectionIntent IsSelectable() const override;

	/*~ ISnappableExtension Interface */
	void AddToSnapField(const ISnapCandidate& Candidate, ISnapField& SnapField) const override;

	/*~ IDraggableTrackAreaExtension Interface */
	bool CanDrag() const override;
	void OnBeginDrag(IDragOperation& DragOperation) override;
	void OnEndDrag(IDragOperation& DragOperation) override;

	/*~ IStretchableExtension Interface */
	void OnInitiateStretch(IStretchOperation& StretchOperation, EStretchConstraint Constraint, FStretchParameters* InOutGlobalParameters) override;

public:

	TRange<FFrameNumber> ComputeRange() const;

	void Offset(FFrameNumber Offset);

private:

	/*~ FViewModel interface */
	void OnConstruct() override;
	void OnDestruct() override;

	/*~ TViewModelExtensionCollection Interface */
	void OnExtensionsDirtied() override;
};

} // namespace Sequencer
} // namespace UE

