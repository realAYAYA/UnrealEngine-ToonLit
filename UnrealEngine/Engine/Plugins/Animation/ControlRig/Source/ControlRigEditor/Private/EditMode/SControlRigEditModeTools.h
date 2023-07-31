// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "IDetailKeyframeHandler.h"
#include "RigVMModel/RigVMGraph.h"
#include "Editor/SRigHierarchyTreeView.h"
#include "Editor/SRigSpacePickerWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class SConstraintsEditionWidget;
class FControlRigEditMode;
class IDetailsView;
class ISequencer;
class SControlPicker;
class SExpandableArea;
class SRigHierarchyTreeView;
class UControlRig;
class URigHierarchy;
class FToolBarBuilder;
class FEditorModeTools;
class FControlRigEditModeToolkit;

#define USE_LOCAL_DETAILS 0

class SControlRigEditModeTools : public SCompoundWidget, public IDetailKeyframeHandler
{
public:
	SLATE_BEGIN_ARGS(SControlRigEditModeTools) {}
	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs, TSharedPtr<FControlRigEditModeToolkit> InOwningToolkit, FControlRigEditMode& InEditMode, UWorld* InWorld);
	/** Set the objects to be displayed in the details panel */
	void SetSettingsDetailsObject(const TWeakObjectPtr<>& InObject);
#if USE_LOCAL_DETAILS
	void SetEulerTransformDetailsObjects(const TArray<TWeakObjectPtr<>>& InObjects);
	void SetTransformDetailsObjects(const TArray<TWeakObjectPtr<>>& InObjects);
	void SetTransformNoScaleDetailsObjects(const TArray<TWeakObjectPtr<>>& InObjects);
	void SetFloatDetailsObjects(const TArray<TWeakObjectPtr<>>& InObjects);
	void SetBoolDetailsObjects(const TArray<TWeakObjectPtr<>>& InObjects);
	void SetIntegerDetailsObjects(const TArray<TWeakObjectPtr<>>& InObjects);
	void SetEnumDetailsObjects(const TArray<TWeakObjectPtr<>>& InObjects);
	void SetVectorDetailsObjects(const TArray<TWeakObjectPtr<>>& InObjects);
	void SetVector2DDetailsObjects(const TArray<TWeakObjectPtr<>>& InObjects);
#endif

	/** Set the sequencer we are bound to */
	void SetSequencer(TWeakPtr<ISequencer> InSequencer);

	/** Set The Control Rig we are using*/
	void SetControlRigs(const TArrayView<TWeakObjectPtr<UControlRig>>& InControlRigs);

	/** Returns the hierarchy currently being used */
	const URigHierarchy* GetHierarchy() const;
	// IDetailKeyframeHandler interface
	virtual bool IsPropertyKeyable(const UClass* InObjectClass, const class IPropertyHandle& PropertyHandle) const override;
	virtual bool IsPropertyKeyingEnabled() const override;
	virtual void OnKeyPropertyClicked(const IPropertyHandle& KeyedPropertyHandle) override;
	virtual bool IsPropertyAnimated(const class IPropertyHandle& PropertyHandle, UObject *ParentObject) const override;
private:
	/** Sequencer we are currently bound to */

	TWeakPtr<ISequencer> WeakSequencer;
	TSharedPtr<IDetailsView> SettingsDetailsView;

#if USE_LOCAL_DETAILS
	/** The details views we do most of our work within */
	TSharedPtr<IDetailsView> ControlEulerTransformDetailsView;
	TSharedPtr<IDetailsView> ControlTransformDetailsView;
	TSharedPtr<IDetailsView> ControlTransformNoScaleDetailsView;
	TSharedPtr<IDetailsView> ControlFloatDetailsView;
	TSharedPtr<IDetailsView> ControlBoolDetailsView;
	TSharedPtr<IDetailsView> ControlIntegerDetailsView;
	TSharedPtr<IDetailsView> ControlEnumDetailsView;
	TSharedPtr<IDetailsView> ControlVector2DDetailsView;
	TSharedPtr<IDetailsView> ControlVectorDetailsView;
#endif
	/** Expander to interact with the options of the rig  */
	TSharedPtr<SExpandableArea> RigOptionExpander;
	TSharedPtr<IDetailsView> RigOptionsDetailsView;
#if USE_LOCAL_DETAILS
	/** Hierarchy picker for controls*/
	TSharedPtr<SRigHierarchyTreeView> HierarchyTreeView;
#endif

	/** Space Picker controls*/
	TSharedPtr<SExpandableArea> PickerExpander;
	TSharedPtr<SRigSpacePickerWidget> SpacePickerWidget;

	/** Storage for control rigs */
	TArray<TWeakObjectPtr<UControlRig>> ControlRigs;

	/** Constraint edition widget. */
	TSharedPtr<SExpandableArea> ConstraintPickerExpander = nullptr;
	TSharedPtr<SConstraintsEditionWidget> ConstraintsEditionWidget = nullptr; 

	/** Display or edit set up for property */
	bool ShouldShowPropertyOnDetailCustomization(const struct FPropertyAndParent& InPropertyAndParent) const;
	bool IsReadOnlyPropertyOnDetailCustomization(const struct FPropertyAndParent& InPropertyAndParent) const;

	/** Called when a manipulator is selected in the picker */
	void OnManipulatorsPicked(const TArray<FName>& Manipulators);

	void HandleModifiedEvent(ERigVMGraphNotifType InNotifType, URigVMGraph* InGraph, UObject* InSubject);
	void HandleSelectionChanged(TSharedPtr<FRigTreeElement> Selection, ESelectInfo::Type SelectInfo);
	void OnRigElementSelected(UControlRig* Subject, FRigControlElement* ControlElement, bool bSelected);

	const FRigControlElementCustomization* HandleGetControlElementCustomization(URigHierarchy* InHierarchy, const FRigElementKey& InControlKey);
	void HandleActiveSpaceChanged(URigHierarchy* InHierarchy, const FRigElementKey& InControlKey, const FRigElementKey& InSpaceKey);
	void HandleSpaceListChanged(URigHierarchy* InHierarchy, const FRigElementKey& InControlKey, const TArray<FRigElementKey>& InSpaceList);
	FReply HandleAddSpaceClicked();
	FReply OnBakeControlsToNewSpaceButtonClicked();

	FReply HandleAddConstraintClicked();

	EVisibility GetRigOptionExpanderVisibility() const;

	void OnRigOptionFinishedChange(const FPropertyChangedEvent& PropertyChangedEvent);

private:
	/** Toolbar functions and windows*/
	void ToggleEditPivotMode();

	//TODO may put back void MakeSelectionSetDialog();
	//TWeakPtr<SWindow> SelectionSetWindow;

	FEditorModeTools* ModeTools = nullptr;
	FRigTreeDisplaySettings DisplaySettings;
	const FRigTreeDisplaySettings& GetDisplaySettings() const { return DisplaySettings; }
	bool bIsChangingRigHierarchy = false;

	// The toolkit that created this UI
	TWeakPtr<FControlRigEditModeToolkit> OwningToolkit;

public:
	/** Modes Panel Header Information **/
	void CustomizeToolBarPalette(FToolBarBuilder& ToolBarBuilder);
	FText GetActiveToolName() const;
	FText GetActiveToolMessage() const;
};



