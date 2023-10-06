// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "AdvancedPreviewScene.h" // IWYU pragma: keep
#include "Tools/BaseAssetToolkit.h"
#include "EditorUndoClient.h"

class FAdvancedPreviewScene;
class FSpawnTabArgs;
class FEditorViewportClient;
class UAssetEditor;
class IStructureDetailsView;

struct FSmartObjectDefinitionPreviewData;

class SMARTOBJECTSEDITORMODULE_API FSmartObjectAssetToolkit : public FBaseAssetToolkit, public FSelfRegisteringEditorUndoClient, public FGCObject
{
public:
	explicit FSmartObjectAssetToolkit(UAssetEditor* InOwningAssetEditor);

	virtual TSharedPtr<FEditorViewportClient> CreateEditorViewportClient() const override;

protected:
	virtual void PostInitAssetEditor() override;
	virtual void RegisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;
	virtual void OnClose() override;

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override
	{
		return TEXT("FSmartObjectAssetToolkit");
	}

	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override;
	
private:

	void UpdatePreviewActor();
	void UpdateCachedPreviewDataFromDefinition();
	
	/** Callback to detect changes in number of slot to keep gizmos in sync. */
	void OnPropertyChanged(UObject* ObjectBeingModified, FPropertyChangedEvent& PropertyChangedEvent) const;

	/** Creates a tab allowing the user to select a mesh or actor template to spawn in the preview scene. */
	TSharedRef<SDockTab> SpawnTab_PreviewSettings(const FSpawnTabArgs& Args);

	TSharedRef<SDockTab> SpawnTab_SceneViewport(const FSpawnTabArgs& Args);
	
	/** Additional Tab to select mesh/actor to add a 3D preview in the scene. */
	static const FName PreviewSettingsTabID;
	static const FName SceneViewportTabID;

	/** Scene in which the 3D preview of the asset lives. */
	TUniquePtr<FAdvancedPreviewScene> AdvancedPreviewScene;

	/** Details view for the preview settings. */
	TSharedPtr<IStructureDetailsView> PreviewDetailsView;

	TSharedPtr<TStructOnScope<FSmartObjectDefinitionPreviewData>> CachedPreviewData;
	
    /** Typed pointer to the custom ViewportClient created by the toolkit. */
	mutable TSharedPtr<class FSmartObjectAssetEditorViewportClient> SmartObjectViewportClient;
};
