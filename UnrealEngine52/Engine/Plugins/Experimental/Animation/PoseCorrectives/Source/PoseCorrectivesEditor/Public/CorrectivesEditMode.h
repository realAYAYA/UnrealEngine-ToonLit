// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "ControlRigEditor/Private/EditMode/ControlRigEditMode.h"

class FPoseCorrectivesEditorController;

class FCorrectivesEditMode: public FControlRigEditMode
{
public:
	static FName ModeName;

	virtual bool HandleClick(FEditorViewportClient* InViewportClient, HHitProxy *HitProxy, const FViewportClick &Click) override;

	void ClearSelection();
};
