// Copyright Epic Games, Inc. All Rights Reserved.

/**
* Space Picker View
*/
#pragma once

#include "CoreMinimal.h"
#include "EditMode/ControlRigBaseDockableView.h"
#include "Widgets/SWidget.h"
#include "Widgets/SCompoundWidget.h"
#include "Misc/FrameNumber.h"
#include "ControlRig.h"
#include "Editor/SRigSpacePickerWidget.h"


class ISequencer;
class SExpandableArea;
class SSearchableRigHierarchyTreeView;

class SControlRigSpacePicker : public SCompoundWidget, public FControlRigBaseDockableView
{

	SLATE_BEGIN_ARGS(SControlRigSpacePicker)
	{}
	SLATE_END_ARGS()
		~SControlRigSpacePicker();

	void Construct(const FArguments& InArgs, FControlRigEditMode& InEditMode);

private:
	virtual void HandleControlSelected(UControlRig* Subject, FRigControlElement* InControl, bool bSelected) override;

	/** Space picker widget*/
	TSharedPtr<SRigSpacePickerWidget> SpacePickerWidget;
	TSharedPtr<SExpandableArea> PickerExpander;

	const FRigControlElementCustomization* HandleGetControlElementCustomization(URigHierarchy* InHierarchy, const FRigElementKey& InControlKey);
	void HandleActiveSpaceChanged(URigHierarchy* InHierarchy, const FRigElementKey& InControlKey, const FRigElementKey& InSpaceKey);
	void HandleSpaceListChanged(URigHierarchy* InHierarchy, const FRigElementKey& InControlKey, const TArray<FRigElementKey>& InSpaceList);
	FReply HandleAddSpaceClicked();
	FReply OnBakeControlsToNewSpaceButtonClicked();

	//for now picker works off of one ControlRig, this function gets the first control rig with a selection
	UControlRig* GetControlRig();

};

