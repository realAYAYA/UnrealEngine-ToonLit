// Copyright Epic Games, Inc. All Rights Reserved.
/**
* View for containing details for various controls
*/
#pragma once

#include "CoreMinimal.h"
#include "EditMode/ControlRigBaseDockableView.h"
#include "Widgets/SWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Misc/FrameNumber.h"
#include "IDetailsView.h"
#include "Rigs/RigHierarchy.h"
#include "IDetailCustomization.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "AnimDetailsProxy.h"
#include "Engine/TimerHandle.h"

class ISequencer;
class UControlRig;
class FUICommandList;
class UMovieSceneTrack;
class SControlRigDetails;

struct FArrayOfPropertyTracks
{
	TArray<UMovieSceneTrack*> PropertyTracks;
};

class FSequencerTracker
{
public:
	FSequencerTracker() = default;
	~FSequencerTracker();
	void SetSequencerAndDetails(TWeakPtr<ISequencer> InWeakSequencer, SControlRigDetails* InControlRigDetails);
	TMap<UObject*, FArrayOfPropertyTracks>& GetObjectsTracked() { return ObjectsTracked; }
private:
	void UpdateSequencerBindings(const TArray<FGuid>& SequencerBindings);
	FDelegateHandle OnSelectionChangedHandle;
	TWeakPtr<ISequencer> WeakSequencer;
	TMap<UObject*, FArrayOfPropertyTracks> ObjectsTracked;
	SControlRigDetails* ControlRigDetails = nullptr;
};

class FControlRigEditModeGenericDetails : public IDetailCustomization
{
public:
	FControlRigEditModeGenericDetails() = delete;
	FControlRigEditModeGenericDetails(FEditorModeTools* InModeTools) : ModeTools(InModeTools) {}

	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance(FEditorModeTools* InModeTools)
	{
		return MakeShareable(new FControlRigEditModeGenericDetails(InModeTools));
	}

	/** IDetailCustomization interface */
	virtual void CustomizeDetails(class IDetailLayoutBuilder& DetailLayout) override;

protected:
	FEditorModeTools* ModeTools = nullptr;
};

class SControlRigDetails: public SCompoundWidget, public FControlRigBaseDockableView
{

	SLATE_BEGIN_ARGS(SControlRigDetails)
	{}
	SLATE_END_ARGS()
	~SControlRigDetails();

	void Construct(const FArguments& InArgs, FControlRigEditMode& InEditMode);

	//FControlRigBaseDockableView overrides
	virtual void SetEditMode(FControlRigEditMode& InEditMode) override;

	/** Display or edit set up for property */
	bool ShouldShowPropertyOnDetailCustomization(const struct FPropertyAndParent& InPropertyAndParent) const;
	bool IsReadOnlyPropertyOnDetailCustomization(const struct FPropertyAndParent& InPropertyAndParent) const;
	void SelectedSequencerObjects(const TMap<UObject*, FArrayOfPropertyTracks>& ObjectsTracked);

private:

	void UpdateProxies();
	void HandleSequencerObjects(TMap<UObject*, FArrayOfPropertyTracks>& InObjectsTracked);
	virtual void HandleControlSelected(UControlRig* Subject, FRigControlElement* InControl, bool bSelected) override;

	TSharedPtr<IDetailsView> AllControlsView;

private:
	/*~ Keyboard interaction */
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

	FSequencerTracker SequencerTracker;

	/** Handle for the timer used to recreate detail panel */
	FTimerHandle NextTickTimerHandle;

};




