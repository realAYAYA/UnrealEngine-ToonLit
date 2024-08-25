// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "CoreTypes.h"
#include "Delegates/Delegate.h"
#include "Input/DragAndDrop.h"
#include "MVVM/ICastable.h"
#include "MVVM/ViewModelPtr.h"
#include "MVVM/ViewModelTypeID.h"
#include "MVVM/ViewModels/ViewModel.h"
#include "Templates/SharedPointer.h"

class FDragDropOperation;
class SWidget;

namespace UE::Sequencer
{

class FEditorViewModel;
class FViewModel;
class IOutlinerExtension;

class SEQUENCERCORE_API FOutlinerViewModel
	: public FViewModel
{
public:

	UE_SEQUENCER_DECLARE_CASTABLE(FOutlinerViewModel, FViewModel);

	FSimpleMulticastDelegate OnRefreshed;

	/**
	 * Constructor
	 */
	FOutlinerViewModel();

	/**
	 * Destructor
	 */
	virtual ~FOutlinerViewModel() {}

	/**
	 * Initialize the view model with a given root model
	 */
	void Initialize(const FWeakViewModelPtr& InWeakRootDataModel);

	/**
	 * Get the editor view-model provided by the creation context
	 */
	TSharedPtr<FEditorViewModel> GetEditor() const;

	/**
	 * Get the logical root item of the outliner hierarchy
	 */
	FViewModelPtr GetRootItem() const;

	/**
	 * Retrieve a flat list of the root items in this outliner model
	 */
	TArrayView<const TWeakViewModelPtr<IOutlinerExtension>> GetTopLevelItems() const;

	/**
	 * Set the single hovered model in the tree
	 */
	void SetHoveredItem(TViewModelPtr<IOutlinerExtension> InHoveredModel);

	/**
	 * Get the single hovered model in the tree, possibly nullptr
	 */
	TViewModelPtr<IOutlinerExtension> GetHoveredItem() const;

	/**
	 * Unpins any pinned nodes in this tree
	 */
	void UnpinAllNodes();

	/**
	 * Called when the default context menu is opened
	 */
	virtual TSharedPtr<SWidget> CreateContextMenuWidget();

	/**
	 * Initate a new drag operation from the specified dragged models.
	 */
	virtual TSharedRef<FDragDropOperation> InitiateDrag(TArray<TWeakViewModelPtr<IOutlinerExtension>>&& InDraggedModels);

	/**
	 * Request that this tree be updated
	 */
	virtual void RequestUpdate();


private:

	void HandleDataHierarchyChanged();

private:

	/** The root data */
	FWeakViewModelPtr WeakRootDataModel;
	/** A cached list of the root data's children, which are the outliner's root items */
	TArray<TWeakViewModelPtr<IOutlinerExtension>> RootItems;

	/** The currently mouse-hovered item */
	TWeakViewModelPtr<IOutlinerExtension> WeakHoveredItem;

	/** Whether root items should be re-evaluated */
	bool bRootItemsInvalidated;
};

} // namespace UE::Sequencer

