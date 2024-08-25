// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdMode.h"

#include "SkeletalMeshModelingToolsEditorMode.generated.h"

//class FStylusStateTracker;
class FSkeletalMeshModelingToolsEditorModeToolkit;
class UEdModeInteractiveToolsContext;
class ISkeletalMeshNotifier;
class ISkeletalMeshEditorBinding;
class ISkeletalMeshEditingInterface;
class HHitProxy;
class UDebugSkelMeshComponent;
class ISkeletalMeshEditor;

UCLASS()
class USkeletalMeshModelingToolsEditorMode : 
	public UBaseLegacyWidgetEdMode
{
	GENERATED_BODY()
public:
	const static FEditorModeID Id;	

	USkeletalMeshModelingToolsEditorMode();
	explicit USkeletalMeshModelingToolsEditorMode(FVTableHelper& Helper);
	virtual ~USkeletalMeshModelingToolsEditorMode() override;

	// UEdMode overrides
	virtual void Initialize() override;
	virtual void Enter() override;
	virtual void Exit() override;
	virtual void CreateToolkit() override;
	virtual bool IsCompatibleWith(FEditorModeID OtherModeID) const override;
	virtual bool UsesTransformWidget() const override { return false; };
	virtual bool UsesPropertyWidgets() const override { return false; };
	virtual void Tick(FEditorViewportClient* InViewportClient, float InDeltaTime) override;
	virtual bool HandleClick(FEditorViewportClient* InViewportClient, HHitProxy *HitProxy, const FViewportClick &Click) override;
	virtual bool ComputeBoundingBoxForViewportFocus(AActor* Actor, UPrimitiveComponent* PrimitiveComponent, FBox& InOutBox) const override;
	virtual bool UsesToolkits() const override { return true; }

	// binding
	void SetEditorBinding(const TWeakPtr<ISkeletalMeshEditor>& InSkeletalMeshEditor);
		
protected:
	virtual void OnToolStarted(UInteractiveToolManager* Manager, UInteractiveTool* Tool) override;
	virtual void OnToolEnded(UInteractiveToolManager* Manager, UInteractiveTool* Tool) override;
	
private:
	// Stylus support is currently disabled; this is left in for reference if/when it is brought back
	//TUniquePtr<FStylusStateTracker> StylusStateTracker;

	static ISkeletalMeshEditingInterface* GetSkeletonInterface(UInteractiveTool* InTool);

	UDebugSkelMeshComponent* GetSkelMeshComponent() const;

	bool NeedsTransformGizmo() const;

	FDelegateHandle ToToolNotifierHandle;
	FDelegateHandle FromToolNotifierHandle;
	
	TWeakPtr<ISkeletalMeshEditorBinding> Binding;
};
