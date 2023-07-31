// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ComponentVisualizerManager.h"
#include "EditorViewportClient.h"

class FSmartObjectAssetToolkit;
class USmartObjectComponent;
class UStaticMeshComponent;
class FPreviewScene;
class SEditorViewport;
class AActor;

class FSmartObjectAssetEditorViewportClient	: public FEditorViewportClient
{
public:
	explicit FSmartObjectAssetEditorViewportClient(const TSharedRef<const FSmartObjectAssetToolkit>& InAssetEditorToolkit, FPreviewScene* InPreviewScene = nullptr, const TWeakPtr<SEditorViewport>& InEditorViewportWidget = nullptr);

	void SetPreviewComponent(USmartObjectComponent* InPreviewComponent);
	void SetPreviewMesh(UStaticMesh* InStaticMesh);
	void SetPreviewActor(AActor* InActor);
	void SetPreviewActorClass(const UClass* ActorClass);
	USmartObjectComponent* GetPreviewComponent() const { return PreviewSmartObjectComponent.Get(); }

protected:
	virtual void Draw(const FSceneView* View, FPrimitiveDrawInterface* PDI) override;

	/** Computes and returns a bounding box that encapsulated preview actor or mesh */
	FBox GetPreviewBounds() const;

private:

	/** Weak pointer to the preview SmartObject component added to the preview scene */
	TWeakObjectPtr<USmartObjectComponent> PreviewSmartObjectComponent = nullptr;

	/** Weak pointer to the preview Mesh component added to the preview scene */
	TWeakObjectPtr<UStaticMeshComponent> PreviewMeshComponent = nullptr;

	/** Weak pointer to the preview actor added to the preview scene */
	TWeakObjectPtr<AActor> PreviewActor = nullptr;

	/** Weak pointer to the preview actor added to the preview scene */
	TWeakObjectPtr<AActor> PreviewActorFromClass = nullptr;

	/** Weak pointer back to asset editor we are embedded in */
	TWeakPtr<const FSmartObjectAssetToolkit> AssetEditorToolkit;
};