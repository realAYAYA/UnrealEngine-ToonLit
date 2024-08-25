// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/Commands/UICommandList.h"
#include "MVVM/ViewModels/EditorViewModel.h"
#include "SequencerCustomizationManager.h"
#include "Toolkits/AssetEditorToolkit.h"

struct FMovieSceneSequenceID;

class ISequencer;
class FSequencer;
class UMovieSceneSequence;
struct FSequencerHostCapabilities;

namespace UE::Sequencer
{

class FSequenceModel;
class FSequencerSelection;
class STrackAreaView;
struct ITrackAreaHotspot;

/**
 * Main view-model for the Sequencer editor.
 */
class SEQUENCER_API FSequencerEditorViewModel : public FEditorViewModel
{
public:

	UE_SEQUENCER_DECLARE_CASTABLE(FSequencerEditorViewModel, FEditorViewModel);

	FSequencerEditorViewModel(TSharedRef<ISequencer> InSequencer, const FSequencerHostCapabilities& InHostCapabilities);
	~FSequencerEditorViewModel();

	/** Retrieve this editor's selection class */
	TSharedPtr<FSequencerSelection> GetSelection() const;

	// @todo_sequencer_mvvm remove this later
	TSharedPtr<ISequencer> GetSequencer() const;
	// @todo_sequencer_mvvm remove this ASAP
	TSharedPtr<FSequencer> GetSequencerImpl() const;

	// @todo_sequencer_mvvm move this to the root view-model
	void SetSequence(UMovieSceneSequence* InRootSequence);

	TViewModelPtr<FSequenceModel> GetRootSequenceModel() const;

public:

	/** Adjust sequencer customizations based on the currently focused sequence */
	bool UpdateSequencerCustomizations(const UMovieSceneSequence* PreviousFocusedSequence);

	/** Get the active customization infos */
	TArrayView<const FSequencerCustomizationInfo> GetActiveCustomizationInfos() const;

	/** Build a combined menu extender */
	TSharedPtr<FExtender> GetSequencerMenuExtender(
			TSharedPtr<FExtensibilityManager> ExtensibilityManager, const TArray<UObject*>& ContextObjects,
			TFunctionRef<const FOnGetSequencerMenuExtender&(const FSequencerCustomizationInfo&)> Endpoint, FViewModelPtr InViewModel) const;

	/** Gets the pinned track area view-model. */
	TSharedPtr<FTrackAreaViewModel> GetPinnedTrackArea() const;

	/** Gets the current hotspots across any of our track areas */
	TSharedPtr<ITrackAreaHotspot> GetHotspot() const;

	void HandleDataHierarchyChanged();

protected:

	virtual void PreInitializeEditorImpl() override;
	virtual void InitializeEditorImpl() override;
	virtual TSharedPtr<FViewModel> CreateRootModelImpl() override;
	virtual TSharedPtr<FOutlinerViewModel> CreateOutlinerImpl() override;
	virtual TSharedPtr<FTrackAreaViewModel> CreateTrackAreaImpl() override;
	virtual TSharedPtr<FSequencerCoreSelection> CreateSelectionImpl() override;
	virtual USequencerScriptingLayer* CreateScriptingLayerImpl() override;
	virtual bool IsReadOnly() const override;

	void OnTrackAreaHotspotChanged(TSharedPtr<ITrackAreaHotspot> NewHotspot);

protected:

	/** Owning sequencer */
	TWeakPtr<ISequencer> WeakSequencer;
	
	/** Pinned track area (the main track area is owned by the base class) */
	TSharedPtr<FTrackAreaViewModel> PinnedTrackArea;

	/** Whether we have a curve editor extension */
	bool bSupportsCurveEditor;
	
	/** The current hotspot, from any of our track areas */
	TSharedPtr<ITrackAreaHotspot> CurrentHotspot;

	/** Cached node paths to be used to compare when the hierarchy changes */
	TMap<TWeakPtr<FViewModel>, FString> NodePaths;

	/** Active customizations. */
	TArray<TUniquePtr<ISequencerCustomization>> ActiveCustomizations;
	TArray<FSequencerCustomizationInfo> ActiveCustomizationInfos;
};

} // namespace UE::Sequencer

