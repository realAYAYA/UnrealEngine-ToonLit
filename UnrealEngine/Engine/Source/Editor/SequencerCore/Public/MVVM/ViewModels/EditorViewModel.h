// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "MVVM/ICastable.h"
#include "MVVM/ViewModelPtr.h"
#include "MVVM/ViewModelTypeID.h"
#include "MVVM/ViewModels/ViewModel.h"
#include "MVVM/ViewModels/ViewDensity.h"
#include "MVVM/ViewModels/ViewModelHierarchy.h"
#include "Misc/FrameRate.h"
#include "Templates/SharedPointer.h"
#include "TimeToPixel.h"
#include "UObject/StrongObjectPtr.h"

class USequencerScriptingLayer;

namespace UE::Sequencer
{

class FOutlinerViewModel;
class FTrackAreaViewModel;
class FSequencerCoreSelection;

template <typename T> struct TAutoRegisterViewModelTypeID;

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

	/** Gets the selection class */
	TSharedPtr<FSequencerCoreSelection> GetSelection() const;

	/** Gets the scripting layer */
	USequencerScriptingLayer* GetScriptingLayer() const;

	/** Returns the current view density for this editor */
	FViewDensityInfo GetViewDensity() const;

	/** Returns the current view density for this editor */
	void SetViewDensity(const FViewDensityInfo& InViewDensity);

	/** Returns whether this editor is currently read-only */
	virtual bool IsReadOnly() const;

	/** Returns the inverse of read-only - useful for direct bindings to IsEnabled for widgets */
	bool IsEditable() const
	{
		return !IsReadOnly();
	}

protected:

	/** Initializes this view model before panels and root models are created */
	virtual void PreInitializeEditorImpl() {}
	/** Creates the root data model for this editor. */
	virtual TSharedPtr<FViewModel> CreateRootModelImpl();
	/** Creates the outliner for this editor. */
	virtual TSharedPtr<FOutlinerViewModel> CreateOutlinerImpl();
	/** Creates the track-area for this editor. */
	virtual TSharedPtr<FTrackAreaViewModel> CreateTrackAreaImpl();
	/** Creates the selection class for this editor. */
	virtual TSharedPtr<FSequencerCoreSelection> CreateSelectionImpl();
	/** Creates the scripting layer class for this editor. */
	virtual USequencerScriptingLayer* CreateScriptingLayerImpl();
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
	/** Cached pointer to the selection class */
	TSharedPtr<FSequencerCoreSelection> Selection;
	/** Cached pointer to the scripting layer class */
	TStrongObjectPtr<USequencerScriptingLayer> ScriptingLayer;

	/** The root view-model for the data being edited */
	TSharedPtr<FViewModel> RootDataModel;

	/** This editor's current view density defining how condensed the elements appear */
	FViewDensityInfo ViewDensity;
};

} // namespace UE::Sequencer

