// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditMode/ControlRigEditMode.h"

class UPersonaOptions;

class FControlRigEditorEditMode : public FControlRigEditMode
{
public:
	static FName ModeName;
	
	virtual bool IsInLevelEditor() const override { return false; }
	virtual bool AreEditingControlRigDirectly() const override { return true; }

	// FEdMode interface
	virtual void Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI) override;

	/* IPersonaEditMode interface */
	virtual bool GetCameraTarget(FSphere& OutTarget) const override;
	virtual class IPersonaPreviewScene& GetAnimPreviewScene() const override;
	virtual void GetOnScreenDebugInfo(TArray<FText>& OutDebugInfo) const override;

	/** If set to true the edit mode will additionally render all bones */
	bool bDrawHierarchyBones = false;

	/** Drawing options */
	UPersonaOptions* ConfigOption = nullptr;
};

class FModularRigEditorEditMode : public FControlRigEditorEditMode
{
public:
	static FName ModeName;
};
