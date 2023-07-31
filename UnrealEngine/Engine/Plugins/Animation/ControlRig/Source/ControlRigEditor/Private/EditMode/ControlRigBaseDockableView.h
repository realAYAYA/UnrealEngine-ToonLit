// Copyright Epic Games, Inc. All Rights Reserved.
/**
* Base View for Dockable Control Rig Animation widgets Details/Outliner
*/
#pragma once

#include "CoreMinimal.h"
#include "EditorModeManager.h"


class UControlRig;
class ISequencer;
class FControlRigEditMode;
struct FRigControlElement;
struct FRigElementKey;

class FControlRigBaseDockableView 
{
public:
	FControlRigBaseDockableView();
	virtual ~FControlRigBaseDockableView();
	TArray<UControlRig*> GetControlRigs() const;

	virtual void SetEditMode(FControlRigEditMode& InEditMode);

protected:
	virtual void HandleControlSelected(UControlRig* Subject, FRigControlElement* InControl, bool bSelected);
	virtual void HandleControlAdded(UControlRig* ControlRig, bool bIsAdded);

	void HandlElementSelected(UControlRig* Subject, const FRigElementKey& Key, bool bSelected);

	ISequencer* GetSequencer() const;

	FEditorModeTools* ModeTools = nullptr;

};

