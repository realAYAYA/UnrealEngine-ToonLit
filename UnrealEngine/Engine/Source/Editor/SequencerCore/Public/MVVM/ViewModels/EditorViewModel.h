// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "MVVM/ICastable.h"
#include "MVVM/ViewModelPtr.h"
#include "MVVM/ViewModelTypeID.h"
#include "MVVM/ViewModels/ViewModel.h"
#include "MVVM/ViewModels/ViewModelHierarchy.h"
#include "Misc/FrameRate.h"
#include "Templates/SharedPointer.h"
#include "TimeToPixel.h"

namespace UE::Sequencer { template <typename T> struct TAutoRegisterViewModelTypeID; }

namespace UE
{
namespace Sequencer
{

class FOutlinerViewModel;
class FTrackAreaViewModel;

/**
 * This represents to root view-model for a sequencer-like editor.
 *
 * This view-model assumes that the editor includes at least an outliner area and a track area.
 * Other panels in the editor can be added to the list of panels.
 *
 * The data being edited is represented by a root view-model which doesn't change: if the editor
 * needs to edit a different piece of data, this should change *inside* the root view-model, and
 * call the necessary code to refresh the entire view-model hierarchy.
 * Note that this root view-model is *not* parented under the editor view-model.
 */
class SEQUENCERCORE_API FEditorViewModel
	: public FViewModel
{
public:

	UE_SEQUENCER_DECLARE_CASTABLE(FEditorViewModel, FViewModel);

	/** Children type for editor panels */
	static EViewModelListType GetEditorPanelType();

	/** Builds a new editor view model. */
	FEditorViewModel();
	/** Destroys this editor view model. */
	~FEditorViewModel();

	/** Initializes the outliner and track-area for this editor. */
	void InitializeEditor();

	/** Gets the root view-model of the sequence being edited. */
	FViewModelPtr GetRootModel() const;

	/** Gets the editor panels' view models. */
	FViewModelChildren GetEditorPanels();

	/** Gets the outliner view-model. */
	TSharedPtr<FOutlinerViewModel>  GetOutliner() const;

	/** Gets the track area view-model. */
	TSharedPtr<FTrackAreaViewModel> GetTrackArea() const;

	/** Returns whether this editor is currently read-only */
	virtual bool IsReadOnly() const;

protected:

	/** Initializes this view model before panels and root models are created */
	virtual void PreInitializeEditorImpl() {}
	/** Creates the root data model for this editor. */
	virtual TSharedPtr<FViewModel> CreateRootModelImpl();
	/** Creates the outliner for this editor. */
	virtual TSharedPtr<FOutlinerViewModel> CreateOutlinerImpl();
	/** Creates the track-area for this editor. */
	virtual TSharedPtr<FTrackAreaViewModel> CreateTrackAreaImpl();
	/** Creates any other panels for this editor. */
	virtual void InitializeEditorImpl() {}

private:

	friend class FOutlinerViewModel;

	/** List of panel view-models for this editor */
	FViewModelListHead PanelList;

	/** Cached pointer to the outliner panel view-model */
	TSharedPtr<FOutlinerViewModel>  Outliner;
	/** Cached pointer to the track area panel view-model */
	TSharedPtr<FTrackAreaViewModel> TrackArea;

	/** The root view-model for the data being edited */
	TSharedPtr<FViewModel>  RootDataModel;
};

} // namespace Sequencer
} // namespace UE

