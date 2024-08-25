// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaSequenceItemShared.h"
#include "EditorUndoClient.h"
#include "Framework/Commands/UIAction.h"
#include "IAvaSequencer.h"
#include "IAvaSequencerProvider.h"

class AActor;
class FAvaEaseCurveTool;
class FAvaEditorSelection;
class FAvaSequencerAction;
class FAvaSequencerCleanView;
class FAvaSequencerController;
class FExtender;
class FMenuBuilder;
class FProperty;
class FUICommandList;
class IAvaSequenceColumn;
class IAvaSequencerController;
class IPropertyHandle;
class ISequencer;
class ISequencerObjectChangeListener;
class SAvaSequenceTree;
class SHeaderRow;
class SWidget;
class UAvaSequence;
class UAvaSequencerSettings;
class UMovieScene3DTransformTrack;
class UMovieScene;
class UMovieSceneTrack;
class USequencerSettings;
class UToolMenu;
enum class EMovieSceneTransformChannel : uint32;
struct FAvaSequencePreset;
struct FAvaSequencerArgs;
struct FMovieSceneBinding;
struct FMovieScenePossessable;
struct FSequencerInitParams;
struct FToolMenuContext;
template<typename ItemType> class STreeView;

namespace UE::Sequencer
{
	class SOutlinerView;
}

class FAvaSequencer : public IAvaSequencer, public FEditorUndoClient, public TSharedFromThis<FAvaSequencer>
{
public:
	explicit FAvaSequencer(IAvaSequencerProvider& InProvider, FAvaSequencerArgs&& InArgs);

	virtual ~FAvaSequencer() override;

	void BindCommands();

	TSharedPtr<FUICommandList> GetCommandList() const { return CommandList; }

	TSharedPtr<IAvaSequenceColumn> FindSequenceColumn(FName InColumnName) const;

	/** Makes sure the Sequencer is instantiated */
	void EnsureSequencer();

	TSharedRef<ISequencer> CreateSequencer();

	IAvaSequencerProvider& GetProvider() { return Provider; }

	void GetSelectedObjects(const TArray<FGuid>& InObjectGuids
		, TArray<UObject*>& OutSelectedActors
		, TArray<UObject*>& OutSelectedComponents
		, TArray<UObject*>& OutSelectedObjects) const;

	bool IsBindingSelected(const FMovieSceneBinding& InBinding) const;

	/** Syncs from Sequencer Selection to Editor Selection */
	void OnSequencerSelectionChanged(TArray<FGuid> InObjectGuids);

	/** Gets the extender to use for sequencers context sensitive menus and toolbars */
	TSharedRef<FExtender> GetAddTrackSequencerExtender(const TSharedRef<FUICommandList> InCommandList
		, const TArray<UObject*> InContextSensitiveObjects);

	/** Extends the sequencer add track menu */
	void ExtendSequencerAddTrackMenu(FMenuBuilder& OutAddTrackMenuBuilder
		, const TArray<UObject*> InContextObjects);

	void NotifyViewedSequenceChanged(UAvaSequence* InOldSequence);

	UAvaSequence* CreateSequence() const;

	UObject* GetPlaybackContext() const;

	TSharedRef<SWidget> GetSequenceTreeWidget();

	TSharedRef<SWidget> CreatePlayerToolBar(const TSharedRef<FUICommandList>& InCommandList);

	void OnSequenceSearchChanged(const FText& InSearchText, FText& OutErrorMessage);

	void OnActivateSequence(FMovieSceneSequenceIDRef InSequenceID);

	/** Called when an Sequence has Started Playing in either Sequencer or the Sequence Panel */
	void NotifyOnSequencePlayed();

	/** Called when an Sequence has Stopped Playing in either Sequencer or the Sequence Panel */
	void NotifyOnSequenceStopped();

	void OnMovieSceneBindingsPasted(const TArray<FMovieSceneBinding>& InBindings);

	const TArray<FAvaSequenceItemPtr>& GetRootSequenceItems() const { return RootSequenceItems; }

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnViewedSequenceChanged, UAvaSequence*);
	FOnViewedSequenceChanged& GetOnViewedSequenceChanged() { return OnViewedSequenceChanged; }

	void AddTransformKey(EMovieSceneTransformChannel InTransformChannel);

	void ApplyDefaultPresetToSelection(FName InPresetName);
	void ApplyCustomPresetToSelection(FName InPresetName);
	void ApplyPresetToSelection(const FAvaSequencePreset& InPreset);

	bool AddSequence_CanExecute() const;
	void AddSequence_Execute();

	bool DuplicateSequence_CanExecute() const;
	void DuplicateSequence_Execute();

	bool ExportSequence_IsVisible() const;
	bool ExportSequence_CanExecute() const;
	void ExportSequence_Execute();

	bool DeleteSequence_CanExecute() const;
	void DeleteSequence_Execute();

	bool RelabelSequence_CanExecute() const;
	void RelabelSequence_Execute();

	bool PlaySelected_CanExecute() const;
	void PlaySelected_Execute();

	bool ContinueSelected_CanExecute() const;
	void ContinueSelected_Execute();

	bool StopSelected_CanExecute() const;
	void StopSelected_Execute();

	/**
 	 * Attempts to find the best matching subobject of the given Parent Object
 	 * based on the information provided by the given possessable
 	 * @param InParentObject the outer from which to find the object
 	 * @param InPossessable the possessable to retrieve things like name (label) and object class
 	 * @return the matching subobject if found, or nullptr if not found
 	 */
	static UObject* FindObjectToPossess(UObject* InParentObject, const FMovieScenePossessable& InPossessable);

	void FixBindingPaths();

	void FixInvalidBindings();

	/** Gets the Object Name to use for a Possessable Name */
	static FString GetObjectName(UObject* InObject);

	/**
	 * Recursively finds the most appropriate Resolution Context for a given Parent Guid of a Possessable
	 * Uses InFindObjectsFunc to resolve the object.
	 */
	static UObject* FindResolutionContext(UAvaSequence& InSequence
		, UMovieScene& InMovieScene
		, const FGuid& InParentPossessableGuid
		, UObject* InPlaybackContext
		, TFunctionRef<TArray<UObject*, TInlineAllocator<1>>(const FGuid& InGuid, UObject* InContextChecked)> InFindObjectsFunc);

	static UObject* FindResolutionContext(UAvaSequence& InSequence
		, UMovieScene& InMovieScene
		, const FGuid& InParentGuid
		, UObject* InPlaybackContext);

	bool FixPossessable(UAvaSequence& InSequence
		, const FMovieScenePossessable& InPossessable
		, UObject* InPlaybackContext
		, TSet<const FMovieScenePossessable*>& InOutProcessedPossessables);

	void FixBindingHierarchy();

	TSharedRef<FAvaEaseCurveTool> GetEaseCurveTool() const;
	
	TArrayView<TWeakObjectPtr<>> ResolveBoundObjects(const FGuid& InBindingId, class UMovieSceneSequence* Sequence) const;

	//~ Begin IAvaSequencer
	virtual const IAvaSequencerProvider& GetProvider() const override { return Provider; }
	virtual TSharedRef<ISequencer> GetSequencer() const override;
	virtual USequencerSettings* GetSequencerSettings() const override;
	virtual void SetBaseCommandList(const TSharedPtr<FUICommandList>& InBaseCommandList) override;
	virtual UAvaSequence* GetViewedSequence() const override;
	virtual UAvaSequence* GetDefaultSequence() const override;
	virtual void SetViewedSequence(UAvaSequence* InSequenceToView) override;
	virtual TArray<UAvaSequence*> GetSequencesForObject(UObject* InObject) const override;
	virtual TSharedRef<SWidget> CreateSequenceWidget() override;
	virtual void OnActorsCopied(FString& InOutCopiedData, TConstArrayView<AActor*> InCopiedActors) override;
	virtual void OnActorsPasted(FStringView InPastedData, const TMap<FName, AActor*>& InPastedActors) override;
	virtual void OnEditorSelectionChanged(const FAvaEditorSelection& InEditorSelection) override;
	virtual void NotifyOnSequenceTreeChanged() override;
	//~ End IAvaSequencer

	//~ Begin FEditorUndoClient Interface
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override { PostUndo(bSuccess); }
	// End of FEditorUndoClient

private:
	TSharedPtr<UE::Sequencer::SOutlinerView> GetOutlinerView() const;

	void InitSequencerCommandList();

	void ExecuteSequencerDuplication(FExecuteAction InExecuteAction);

	void OnUpdateCameraCut(UObject* InCameraObject, bool bInJumpCut);

	FOnViewedSequenceChanged OnViewedSequenceChanged;

	IAvaSequencerProvider& Provider;

	/** The active sequencer to use. This is set to Weak as FSequencer::Tick does not allow to have more than 1 ref (outside of menus) */
	TWeakPtr<ISequencer> SequencerWeak;

	/** the instanced sequencer if no external sequencer specified. Will be null if using an External Sequencer */
	TSharedPtr<ISequencer> InstancedSequencer;

	TWeakPtr<UE::Sequencer::SOutlinerView> OutlinerViewWeak; 

	TSharedPtr<IAvaSequencerController> SequencerController;

	TSharedRef<FUICommandList> CommandList;

	TWeakObjectPtr<UAvaSequence> ViewedSequenceWeak;

	FDelegateHandle SequencerAddTrackExtenderHandle;

	TMap<FName, TSharedPtr<IAvaSequenceColumn>> SequenceColumns;

	TArray<FAvaSequenceItemPtr> RootSequenceItems;

	TSharedPtr<SHeaderRow> SequenceTreeHeaderRow;

	TSharedPtr<SAvaSequenceTree> SequenceTree;

	TSharedPtr<STreeView<FAvaSequenceItemPtr>> SequenceTreeView;

	TSharedRef<FAvaSequencerCleanView> CleanView;

	FDelegateHandle OnSequenceStartedHandle;

	FDelegateHandle OnSequenceFinishedHandle;

	TArray<TSharedRef<FAvaSequencerAction>> SequencerActions;

	/** prevents reentry when synchronizing Sequencer <-> Provider Selection  */
	bool bUpdatingSelection = false;

	bool bSelectedFromSequencer = false;

	/** Whether custom clean playback mode will ever be used (set via FAvaSequencerArgs::bUseCustomCleanPlaybackMode) */
	const bool bUseCustomCleanPlaybackMode;

	/** Whether FAvaSequencer is allowed to select to/from the ISequencer instance */
	const bool bCanProcessSequencerSelections;

	/** Selected sequence details sections to restore when a new sequence is selected. */
	TSet<FName> SelectedSections;

	TSharedPtr<FAvaEaseCurveTool> EaseCurveTool;
};
