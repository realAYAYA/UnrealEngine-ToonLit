// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "MVVM/Extensions/IGeometryExtension.h"
#include "MVVM/Extensions/IOutlinerExtension.h"
#include "MVVM/Extensions/ISortableExtension.h"
#include "MVVM/ICastable.h"
#include "MVVM/ViewModelPtr.h"
#include "MVVM/ViewModels/ViewModel.h"
#include "Misc/Optional.h"
#include "Templates/SharedPointer.h"
#include "UObject/NameTypes.h"
#include "Widgets/Views/STableRow.h"

class FDragDropEvent;
class SWidget;
namespace UE::Sequencer { template <typename T> struct TAutoRegisterViewModelTypeID; }

namespace UE
{
namespace Sequencer
{

class SEQUENCERCORE_API FOutlinerSpacer
	: public FViewModel
	, public FGeometryExtensionShim
	, public FOutlinerExtensionShim
	, public ISortableExtension
	, public IOutlinerDropTargetOutlinerExtension
{
public:

	UE_SEQUENCER_DECLARE_CASTABLE(FOutlinerSpacer, FViewModel, IGeometryExtension, IOutlinerExtension, ISortableExtension, IOutlinerDropTargetOutlinerExtension);

	FOutlinerSpacer(float InDesiredSpacerHeight);
	~FOutlinerSpacer();

	void SetDesiredHeight(float InDesiredSpacerHeight)
	{
		DesiredSpacerHeight = InDesiredSpacerHeight;
	}

public:

	/*~ IOutlinerExtension */
	bool HasBackground() const override;
	FName GetIdentifier() const override;
	FOutlinerSizing GetOutlinerSizing() const override;
	TSharedPtr<SWidget> CreateContextMenuWidget(const FCreateOutlinerContextMenuWidgetParams& InParams) override;

	/*~ ISortableExtension */
	virtual void SortChildren() override;
	virtual FSortingKey GetSortingKey() const override;
	virtual void SetCustomOrder(int32 InCustomOrder) override;

	/*~ IOutlinerDropTargetOutlinerExtension */
	TOptional<EItemDropZone> CanAcceptDrop(const FViewModelPtr& TargetModel, const FDragDropEvent& DragDropEvent, EItemDropZone InItemDropZone) override;
	void PerformDrop(const FViewModelPtr& TargetModel, const FDragDropEvent& DragDropEvent, EItemDropZone InItemDropZone) override;

private:

	float DesiredSpacerHeight;
	int32 CustomOrder;
};

} // namespace Sequencer
} // namespace UE

