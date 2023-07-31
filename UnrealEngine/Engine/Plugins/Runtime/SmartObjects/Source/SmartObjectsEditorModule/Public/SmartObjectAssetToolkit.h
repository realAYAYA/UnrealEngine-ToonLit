// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Tools/BaseAssetToolkit.h"
#include "AdvancedPreviewScene.h"
#include "EdModeInteractiveToolsContext.h"
#include "SmartObjectAssetToolkit.generated.h"

class FEditorViewportClient;
class UAssetEditor;
class UTransformProxy;
class UCombinedTransformGizmo;
class UInteractiveToolsContext;
class USmartObjectComponent;
class USmartObjectDefinition;

USTRUCT()
struct FSmartObjectSlotEditorTarget
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UTransformProxy> TransformProxy = nullptr;

	UPROPERTY()
	TObjectPtr<UCombinedTransformGizmo> TransformGizmo = nullptr;
};

UCLASS(Transient)
class USmartObjectAssetEditorTool : public UObject
{
	GENERATED_BODY()

public:
	/** Setup required context and create gizmos to modify slot within the provided definition through the associated component. */
	void Initialize(UInteractiveToolsContext* InteractiveToolsContext, USmartObjectDefinition* SmartObjectDefinition, USmartObjectComponent* SmartObjectComponent);

	/** Unregister context and destroy gizmos */
	void Cleanup();

	/** Recreate gizmos (e.g. slots being added/removed) */
	void RebuildGizmos();

	/** Refresh gizmos (e.g. slots offset/rotation modified) */
	void RefreshGizmos();

private:
	/** Creates Gizmos for each slot definition. */
	void CreateGizmos();

	/** Destroys all Gizmos. */
	void DestroyGizmos();

	UPROPERTY(Transient)
	TObjectPtr<UInteractiveToolsContext> ToolsContext;

	UPROPERTY(Transient)
	TObjectPtr<USmartObjectDefinition> Definition;

	UPROPERTY(Transient)
	TObjectPtr<USmartObjectComponent> PreviewComponent;

	/** List of Gizmos created for each slot of the definition. */
	UPROPERTY(Transient)
	TArray<FSmartObjectSlotEditorTarget> ActiveGizmos;
};

class SMARTOBJECTSEDITORMODULE_API FSmartObjectAssetToolkit : public FBaseAssetToolkit, public FGCObject
{
public:
	explicit FSmartObjectAssetToolkit(UAssetEditor* InOwningAssetEditor);

protected:
	virtual TSharedPtr<FEditorViewportClient> CreateEditorViewportClient() const override;
	virtual void PostInitAssetEditor() override;
	virtual void RegisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;
	virtual void OnClose() override;

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override
	{
		return TEXT("FSmartObjectAssetToolkit");
	}

private:
	/** Callback to detect changes in number of slot to keep gizmos in sync. */
	void OnPropertyChanged(UObject* ObjectBeingModified, FPropertyChangedEvent& PropertyChangedEvent) const;

	/** Creates a tab allowing the user to select a mesh or actor template to spawn in the preview scene. */
	TSharedRef<SDockTab> SpawnTab_PreviewSettings(const FSpawnTabArgs& Args);

	/** Additional Tab to select mesh/actor to add a 3D preview in the scene. */
	static const FName PreviewSettingsTabID;

	/** Scene in which the 3D preview of the asset lives. */
	TUniquePtr<FAdvancedPreviewScene> AdvancedPreviewScene;

	/** Tool to edit slot location and orientation through interactive gizmos. */
	TObjectPtr<USmartObjectAssetEditorTool> Tool;

    /** Typed pointer to the custom ViewportClient created by the toolkit. */
	mutable TSharedPtr<class FSmartObjectAssetEditorViewportClient> SmartObjectViewportClient;

	/** Object path of an actor picked from the current level Editor to spawn a preview in the scene. */
	FString PreviewActorObjectPath;

	/** Object path of a mesh selected from the content to spawn a preview in the scene. */
	FString PreviewMeshObjectPath;

	/** Class of the template actor to spawn a preview in the scene. */
	TWeakObjectPtr<const UClass> PreviewActorClass;
};
