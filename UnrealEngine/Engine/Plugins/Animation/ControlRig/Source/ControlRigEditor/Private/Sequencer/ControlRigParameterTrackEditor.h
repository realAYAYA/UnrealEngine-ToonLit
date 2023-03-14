// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "Templates/SubclassOf.h"
#include "Widgets/SWidget.h"
#include "ISequencer.h"
#include "MovieSceneTrack.h"
#include "ISequencerSection.h"
#include "ISequencerTrackEditor.h"
#include "Sequencer/MovieSceneControlRigParameterTrack.h"
#include "KeyframeTrackEditor.h"
#include "Sections/MovieScene3DTransformSection.h"
#include "AcquiredResources.h"
#include "MovieSceneToolHelpers.h"
#include "MovieSceneToolsModule.h"
#include "Engine/EngineTypes.h"
#include "EditorUndoClient.h"
#include "ScopedTransaction.h"

struct FAssetData;
class FMenuBuilder;
class USkeleton;
class UMovieSceneControlRigParameterSection;
class UFKControlRig;
class FEditorModeTools;
class FControlRigEditMode;
struct FRigControlModifiedContext;
struct FMovieSceneControlRigSpaceChannel;
struct FMovieSceneChannel;
struct FKeyAddOrDeleteEventItem;
struct FKeyMoveEventItem;

/**
 * Tools for animation tracks
 */
class FControlRigParameterTrackEditor : public FKeyframeTrackEditor<UMovieSceneControlRigParameterTrack>, public IMovieSceneToolsAnimationBakeHelper,
	public FEditorUndoClient
{
public:
	/**
	 * Constructor
	 *
	 * @param InSequencer The sequencer instance to be used by this tool
	 */
	FControlRigParameterTrackEditor(TSharedRef<ISequencer> InSequencer);

	/** Virtual destructor. */
	virtual ~FControlRigParameterTrackEditor();

	/**
	 * Creates an instance of this class.  Called by a sequencer
	 *
	 * @param OwningSequencer The sequencer instance to be used by this tool
	 * @return The new instance of this class
	 */
	static TSharedRef<ISequencerTrackEditor> CreateTrackEditor(TSharedRef<ISequencer> OwningSequencer);

public:

	// ISequencerTrackEditor interface
	virtual void OnRelease() override;
	virtual void BuildObjectBindingContextMenu(FMenuBuilder& MenuBuilder, const TArray<FGuid>& ObjectBindings, const UClass* ObjectClass) override;
	virtual void BuildObjectBindingTrackMenu(FMenuBuilder& MenuBuilder, const TArray<FGuid>& ObjectBindings, const UClass* ObjectClass) override;
	virtual TSharedRef<ISequencerSection> MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding) override;
	virtual bool SupportsType(TSubclassOf<UMovieSceneTrack> Type) const override;
	virtual bool HasTransformKeyBindings() const override { return true; }
	virtual bool CanAddTransformKeysForSelectedObjects() const override;
	virtual void OnAddTransformKeysForSelectedObjects(EMovieSceneTransformChannel Channel);
	virtual bool HasTransformKeyOverridePriority() const override;
	virtual void ObjectImplicitlyAdded(UObject* InObject)  override;
	virtual void ObjectImplicitlyRemoved(UObject* InObject)  override;
	virtual void BuildTrackContextMenu(FMenuBuilder& MenuBuilder, UMovieSceneTrack* InTrack) override;
	virtual bool HandleAssetAdded(UObject* Asset, const FGuid& TargetObjectGuid) override;

	//IMovieSceneToolsAnimationBakeHelper
	virtual void PostEvaluation(UMovieScene* MovieScene, FFrameNumber Frame);

	//FEditorUndoClient Interface
	virtual bool MatchesContext(const FTransactionContext& InContext, const TArray<TPair<UObject*, FTransactionObjectEvent>>& TransactionObjects) const override;
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override { PostUndo(bSuccess); }

private:

	void HandleAddTrackSubMenu(FMenuBuilder& MenuBuilder, TArray<FGuid> ObjectBindings, UMovieSceneTrack* Track);
	void HandleAddControlRigSubMenu(FMenuBuilder& MenuBuilder, TArray<FGuid> ObjectBindings, UMovieSceneTrack* Track);

	void ToggleFilterAssetBySkeleton();
	bool IsToggleFilterAssetBySkeleton();

	void ToggleFilterAssetByAnimatableControls();
	bool IsToggleFilterAssetByAnimatableControls();
	void SelectSequencerNodeInSection(UMovieSceneControlRigParameterSection* ParamSection, const FName& ControlName, bool bSelected);

	/** Control Rig Picked */
	void AddControlRig(UClass* InClass, UObject* BoundActor, FGuid ObjectBinding);
	void AddControlRig(UClass* InClass, UObject* BoundActor, FGuid ObjectBinding, UControlRig* InExistingControlRig);
	void AddControlRigFromComponent(FGuid InGuid);
	void AddFKControlRig(TArray<FGuid> ObjectBindings);
	
	/** Delegate for Selection Changed Event */
	void OnSelectionChanged(TArray<UMovieSceneTrack*> InTracks);

	/** Returns the Edit Mode tools manager hosting the edit mode */
	FEditorModeTools* GetEditorModeTools() const;
	FControlRigEditMode* GetEditMode(bool bForceActivate = false) const;

	/** Delegate for MovieScene Changing so we can see if our track got deleted*/
	void OnSequencerDataChanged(EMovieSceneDataChangeType DataChangeType);

	/** Delegate for when track immediately changes, so we need to do an interaction edit for things like fk/ik*/
	void OnChannelChanged(const FMovieSceneChannelMetaData* MetaData, UMovieSceneSection* InSection);

	/** Delegate for difference focused movie scene sequence*/
	void OnActivateSequenceChanged(FMovieSceneSequenceIDRef ID);

	/** Actor Added Delegate*/
	void HandleActorAdded(AActor* Actor, FGuid TargetObjectGuid);
	/** Add Control Rig Tracks For Skelmesh Components*/
	void AddTrackForComponent(USceneComponent* Component, FGuid Binding);

	/** Control Rig Delegates*/
	void HandleControlModified(UControlRig* Subject, FRigControlElement* ControlElement, const FRigControlModifiedContext& Context);
	void HandleControlSelected(UControlRig* Subject, FRigControlElement* ControlElement, bool bSelected);
	void HandleControlUndoBracket(UControlRig* Subject, bool bOpenUndoBracket);
	void HandleOnInitialized(UControlRig* Subject, const EControlRigState InState, const FName& InEventName);
	void HandleOnControlRigBound(UControlRig* InControlRig);
	void HandleOnObjectBoundToControlRig(UObject* InObject);

	//if rig not set then we clear delegates for everyone
	void ClearOutAllSpaceAndConstraintDelegates(const UControlRig* InOptionalControlRig = nullptr) const;

	/** SpaceChannel Delegates*/
	void HandleOnSpaceAdded(
		UMovieSceneControlRigParameterSection* Section,
		const FName& ControlName,
		FMovieSceneControlRigSpaceChannel* Channel);
	void HandleSpaceKeyDeleted(
		UMovieSceneControlRigParameterSection* Section,
		FMovieSceneControlRigSpaceChannel* Channel,
		const TArray<FKeyAddOrDeleteEventItem>& DeletedItems) const;
	static void HandleSpaceKeyMoved(
		UMovieSceneControlRigParameterSection* Section,
		FMovieSceneControlRigSpaceChannel* Channel,
		const TArray<FKeyMoveEventItem>& MovedItems);

	/** ConstraintChannel Delegates*/
	void HandleOnConstraintAdded(
		IMovieSceneConstrainedSection* InSection,
		FMovieSceneConstraintChannel* InConstraintChannel);
	void HandleConstraintKeyDeleted(
		IMovieSceneConstrainedSection* InSection,
		const FMovieSceneConstraintChannel* InConstraintChannel,
		const TArray<FKeyAddOrDeleteEventItem>& InDeletedItems) const;
	static void HandleConstraintKeyMoved(
		IMovieSceneConstrainedSection* InSection,
		const FMovieSceneConstraintChannel* InConstraintChannel,
		const TArray<FKeyMoveEventItem>& InMovedItems);
	void HandleConstraintRemoved(IMovieSceneConstrainedSection* InSection);

	/** Select control rig if not selected, select controls from key areas */
	void SelectRigsAndControls(UControlRig* Subject, const TArray<const IKeyArea*>& KeyAreas);

	/** Handle Creation for SkelMeshComp or Actor Owner, either may have a binding*/
	FMovieSceneTrackEditor::FFindOrCreateHandleResult FindOrCreateHandleToSceneCompOrOwner(USceneComponent* InComp);

	/** Handle Creation for control rig track given the object binding and the control rig */	
	FMovieSceneTrackEditor::FFindOrCreateTrackResult FindOrCreateControlRigTrackForObject(FGuid ObjectBinding, UControlRig* ControlRig, FName PropertyName = NAME_None, bool bCreateTrackIfMissing = true);

	/** Import FBX*/
	void ImportFBX(UMovieSceneControlRigParameterTrack* InTrack, UMovieSceneControlRigParameterSection* InSection, 
		TArray<FFBXNodeAndChannels>* NodeAndChannels);
	/** Find Track for given ControlRig*/
	UMovieSceneControlRigParameterTrack* FindTrack(UControlRig* InControlRig);

	/** Select Bones to Animate on FK Rig*/
	void SelectFKBonesToAnimate(UFKControlRig* FKControlRig, UMovieSceneControlRigParameterTrack* Track);

	/** Toggle FK Control Rig*/
	void ToggleFKControlRig(UMovieSceneControlRigParameterTrack* Track, UFKControlRig* FKControlRig);

	/** Convert to FK Control Rig*/
	void ConvertToFKControlRig(FGuid ObjectBinding, UObject* BoundObject, USkeletalMeshComponent* SkelMeshComp, USkeleton* Skeleton);

	/** Bake To Control Rig Sub Menu*/
	void BakeToControlRigSubMenu(FMenuBuilder& MenuBuilder, FGuid ObjectBinding, UObject* BoundObject, USkeletalMeshComponent* SkelMeshComp,USkeleton* Skeleton);
	
	/** Bake To Control Rig Sub Menu*/
	void BakeToControlRig(UClass* InClass, FGuid ObjectBinding,UObject* BoundObject, USkeletalMeshComponent* SkelMeshComp, USkeleton* Skeleton);

	/** Set Up EditMode for Specified Control Rig*/
	void SetUpEditModeIfNeeded(UControlRig* ControlRig);

private:

	/** Command Bindings added by the Transform Track Editor to Sequencer and curve editor. */
	TSharedPtr<FUICommandList> CommandBindings;
	FAcquiredResources AcquiredResources;

public:

	void AddControlKeys(USceneComponent *InSceneComp, UControlRig* InControlRig, FName PropertyName,
		FName ParameterName, EControlRigContextChannelToKey ChannelsToKey, ESequencerKeyMode KeyMode,
		float InLocalTime, const bool bInConstraintSpace = false);
	void GetControlRigKeys(UControlRig* InControlRig, FName ParameterName, EControlRigContextChannelToKey ChannelsToKey,
		ESequencerKeyMode KeyMode, UMovieSceneControlRigParameterSection* SectionToKey,	FGeneratedTrackKeys& OutGeneratedKeys,
		const bool bInConstraintSpace = false);
	FKeyPropertyResult AddKeysToControlRig(
		USceneComponent *InSceneComp, UControlRig* InControlRig, FFrameNumber KeyTime, FFrameNumber EvaluateTime, FGeneratedTrackKeys& GeneratedKeys,
		ESequencerKeyMode KeyMode, TSubclassOf<UMovieSceneTrack> TrackClass, FName ControlRigName, FName RigControlName);
	FKeyPropertyResult AddKeysToControlRigHandle(USceneComponent *InSceneComp, UControlRig* InControlRig,
		FGuid ObjectHandle, FFrameNumber KeyTime, FFrameNumber EvaluateTime, FGeneratedTrackKeys& GeneratedKeys,
		ESequencerKeyMode KeyMode, TSubclassOf<UMovieSceneTrack> TrackClass, FName ControlRigName, FName RigControlName);
	/**
	 * Modify the passed in Generated Keys by the current tracks values and weight at the passed in time.

	 * @param Object The handle to the object modify
	 * @param Track The track we are modifying
	 * @param SectionToKey The Sections Channels we will be modifiying
	 * @param Time The Time at which to evaluate
	 * @param InOutGeneratedTrackKeys The Keys we need to modify. We change these values.
	 * @param Weight The weight we need to modify the values by.
	 */
	bool ModifyOurGeneratedKeysByCurrentAndWeight(UObject* Object, UControlRig* InControlRig, FName RigControlName, UMovieSceneTrack *Track, UMovieSceneSection* SectionToKey, FFrameNumber EvaluateTime, FGeneratedTrackKeys& InOutGeneratedTotalKeys, float Weight) const;

	//**Function to collapse all layers from this section onto the first absoluate layer.*/
	static bool CollapseAllLayers(TSharedPtr<ISequencer>& SequencerPtr, UMovieSceneTrack* OwnerTrack, UMovieSceneControlRigParameterSection* ParameterSection, bool bKeyReduce = false, float Tolerance = 0.001f);

private:
	FDelegateHandle SelectionChangedHandle;
	FDelegateHandle SequencerChangedHandle;
	FDelegateHandle OnActivateSequenceChangedHandle;
	FDelegateHandle CurveChangedHandle;
	FDelegateHandle OnChannelChangedHandle;
	FDelegateHandle OnMovieSceneChannelChangedHandle;
	FDelegateHandle OnActorAddedToSequencerHandle;

	//map used for space channel delegates since we loose them

	//used to sync curve editor selections/displays on next tick for performance reasons
	TSet<FName> DisplayedControls;
	TSet<FName> UnDisplayedControls;
	bool bCurveDisplayTickIsPending;
	void BindControlRig(UControlRig* ControlRig);
	void UnbindControlRig(UControlRig* ControlRig);
	void UnbindAllControlRigs();
	TArray<TWeakObjectPtr<UControlRig>> BoundControlRigs;

private:

	/** Guard to stop infinite loops when handling control selections*/
	bool bIsDoingSelection;

	/** A flag to determine if the next update coming from the timer should be skipped */
	bool bSkipNextSelectionFromTimer;

	/** Whether or not we should check Skeleton when filtering*/
	bool bFilterAssetBySkeleton;

	/** Whether or not we should check for Animatable Controls when filtering*/
	bool bFilterAssetByAnimatableControls;

	/** Handle to help updating selection on tick tick to avoid too many flooded selections*/
	FTimerHandle UpdateSelectionTimerHandle;

	/** Array of sections that are getting undone, we need to recreate any space channel add,move key delegates to them*/
	mutable TArray<UMovieSceneControlRigParameterSection*> SectionsGettingUndone;

	/** An index counter for the opened undo brackets */
	int32 ControlUndoBracket;

	/** A transaction used to group multiple key events */
	TSharedPtr<FScopedTransaction> ControlUndoTransaction;

	/** Controls if the control rig track for the default animating rig should be created */
	static bool bAutoGenerateControlRigTrack;

	/** Set of delegate handles we have added delegate's too, need to clear them*/
	TSet<FDelegateHandle> ConstraintHandlesToClear;

	friend class FControlRigBlueprintActions;
};



/** Class for control rig sections */
class FControlRigParameterSection : public FSequencerSection
{
public:

	/**
	* Creates a new control rig property section.
	*
	* @param InSection The section object which is being displayed and edited.
	* @param InSequencer The sequencer which is controlling this parameter section.
	*/
	FControlRigParameterSection(UMovieSceneSection& InSection, TWeakPtr<ISequencer> InSequencer)
		: FSequencerSection(InSection), WeakSequencer(InSequencer)
	{
	}

public:

	virtual void BuildSectionContextMenu(FMenuBuilder& MenuBuilder, const FGuid& InObjectBinding) override;

	//~ ISequencerSection interface
	virtual bool RequestDeleteCategory(const TArray<FName>& CategoryNamePath) override;
	virtual bool RequestDeleteKeyArea(const TArray<FName>& KeyAreaNamePath) override;
protected:
	/** Add Sub Menu */
	void AddAnimationSubMenuForFK(FMenuBuilder& MenuBuilder, FGuid ObjectBinding, USkeleton* Skeleton, UMovieSceneControlRigParameterSection* Section);

	/** Animation sub menu filter function */
	bool ShouldFilterAssetForFK(const FAssetData& AssetData);

	/** Animation asset selected */
	void OnAnimationAssetSelectedForFK(const FAssetData& AssetData,FGuid ObjectBinding, UMovieSceneControlRigParameterSection* Section);

	/** Animation asset enter pressed */
	void OnAnimationAssetEnterPressedForFK(const TArray<FAssetData>& AssetData, FGuid ObjectBinding, UMovieSceneControlRigParameterSection* Section);

	/** Select controls channels from selected pressed*/
	void ShowSelectedControlsChannels();
	/** Show all control channels*/
	void ShowAllControlsChannels();

	void KeyZeroValue();
	void KeyWeightValue(float Val);
	void CollapseAllLayers();

	/** The sequencer which is controlling this section. */
	TWeakPtr<ISequencer> WeakSequencer;

};