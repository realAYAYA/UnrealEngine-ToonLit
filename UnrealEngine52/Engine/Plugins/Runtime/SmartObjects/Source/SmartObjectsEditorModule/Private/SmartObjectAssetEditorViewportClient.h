﻿// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditorViewportClient.h"

class FSmartObjectAssetToolkit;
class USmartObjectComponent;
class UStaticMeshComponent;
class FPreviewScene;
class SEditorViewport;
class AActor;
class USmartObjectDefinition;
class FScopedTransaction;
namespace UE::SmartObjects::Editor
{
	struct FSelectedItem;
};

class FSmartObjectAssetEditorViewportClient	: public FEditorViewportClient
{
public:
	explicit FSmartObjectAssetEditorViewportClient(const TSharedRef<const FSmartObjectAssetToolkit>& InAssetEditorToolkit, FPreviewScene* InPreviewScene = nullptr, const TWeakPtr<SEditorViewport>& InEditorViewportWidget = nullptr);
	virtual ~FSmartObjectAssetEditorViewportClient() override;

	void SetSmartObjectDefinition(USmartObjectDefinition& Definition);
	void SetPreviewMesh(UStaticMesh* InStaticMesh);
	void SetPreviewActor(AActor* InActor);
	void SetPreviewActorClass(const UClass* ActorClass);

	USmartObjectDefinition* GetSmartObjectDefinition() const { return SmartObjectDefinition.Get(); }
	
protected:
	virtual void Draw(const FSceneView* View, FPrimitiveDrawInterface* PDI) override;
	virtual void DrawCanvas( FViewport& InViewport, FSceneView& View, FCanvas& Canvas ) override;
	virtual void ProcessClick(FSceneView& View, HHitProxy* HitProxy, FKey Key, EInputEvent Event, uint32 HitX, uint32 HitY) override;
	virtual void TrackingStarted(const struct FInputEventState& InInputState, bool bIsDraggingWidget, bool bNudge) override;
	virtual void TrackingStopped() override;
	virtual bool InputWidgetDelta(FViewport* InViewport, EAxisList::Type CurrentAxis, FVector& Drag, FRotator& Rot, FVector& Scale) override;

	virtual FVector GetWidgetLocation() const override;
	virtual FMatrix GetWidgetCoordSystem() const override;
	virtual ECoordSystem GetWidgetCoordSystemSpace() const override;
	virtual UE::Widget::EWidgetMode GetWidgetMode() const override;
	virtual bool CanSetWidgetMode(UE::Widget::EWidgetMode NewMode) const override;
	virtual void SetWidgetMode(UE::Widget::EWidgetMode NewMode) override;
	virtual void SetWidgetCoordSystemSpace(ECoordSystem NewCoordSystem) override;

	/** Computes and returns a bounding box that encapsulated preview actor or mesh */
	FBox GetPreviewBounds() const;

private:

	void BeginTransaction(FText Text);
	void EndTransaction();

	/** Weak pointer to the Smart Object definition that is edited */
	TWeakObjectPtr<USmartObjectDefinition> SmartObjectDefinition = nullptr;
	
	/** Weak pointer to the preview Mesh component added to the preview scene */
	TWeakObjectPtr<UStaticMeshComponent> PreviewMeshComponent = nullptr;

	/** Weak pointer to the preview actor added to the preview scene */
	TWeakObjectPtr<AActor> PreviewActor = nullptr;

	/** Weak pointer to the preview actor added to the preview scene */
	TWeakObjectPtr<AActor> PreviewActorFromClass = nullptr;

	/** Weak pointer back to asset editor we are embedded in */
	TWeakPtr<const FSmartObjectAssetToolkit> AssetEditorToolkit;

	/** Pointer to currently active transaction */
	FScopedTransaction* ScopedTransaction = nullptr;

	/** True if currently using the transform widget */
	bool bIsManipulating = false;

	/** Currently active transform widget type. */
	UE::Widget::EWidgetMode WidgetMode = UE::Widget::WM_Translate;

	/** Currently active transform widget coord system. */
	ECoordSystem WidgetCoordSystemSpace = COORD_World;

	/** Cached widget location (updated from slots and annotations before manipulating the gizmo) */
	mutable FVector CachedWidgetLocation = FVector::ZeroVector;
	
	/** Currently selected slots. @todo: Make view model and move this there. */
	TArray<UE::SmartObjects::Editor::FSelectedItem> Selection;
};
