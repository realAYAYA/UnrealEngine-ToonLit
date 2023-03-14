// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolkits/IToolkitHost.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "Editor/PropertyEditor/Public/PropertyEditorDelegates.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "Widgets/SGroomEditorViewport.h"
#include "HairStrandsInterface.h"

class UGroomAsset; 
class UGroomComponent;
class IDetailsView;
class SDockableTab;
class SGroomEditorViewport;
class USkeletalMesh;
class UStaticMesh;
class USkeletalMeshComponent;
class UGroomAsset;
class UGroomBindingAsset;
class UGroomBindingAssetList;
class UAnimationAsset;

class HAIRSTRANDSEDITOR_API IGroomCustomAssetEditorToolkit : public FAssetEditorToolkit
{
public:
	/** Retrieves the current custom asset. */
	virtual UGroomAsset* GetCustomAsset() const = 0;

	/** Set the current custom asset. */
	virtual void SetCustomAsset(UGroomAsset* InCustomAsset) = 0;

	/** Set preview of a particular binding. */
	virtual void PreviewBinding(int32 BindingIndex) = 0;

	virtual int32 GetActiveBindingIndex() const =0;
};

class HAIRSTRANDSEDITOR_API FGroomCustomAssetEditorToolkit : public IGroomCustomAssetEditorToolkit
{
public:

	virtual void RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager) override;

	/**
	 * Edits the specified asset object
	 *
	 * @param	Mode					Asset editing mode for this editor (standalone or world-centric)
	 * @param	InitToolkitHost			When Mode is WorldCentric, this is the level editor instance to spawn this editor within
	 * @param	InCustomAsset			The Custom Asset to Edit
	 */
	void InitCustomAssetEditor(const EToolkitMode::Type Mode, const TSharedPtr<class IToolkitHost>& InitToolkitHost, UGroomAsset* InCustomAsset);

	FGroomCustomAssetEditorToolkit();

	/** Destructor */
	virtual ~FGroomCustomAssetEditorToolkit();

	/** Begin IToolkit interface */
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FText GetToolkitName() const override;
	virtual FText GetToolkitToolTipText() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;
	virtual bool IsPrimaryEditor() const override { return true; }
	virtual void OnClose() override;
	/** End IToolkit interface */

	/** Retrieves the current custom asset. */
	virtual UGroomAsset* GetCustomAsset() const override;

	/** Set the current custom asset. */
	virtual void SetCustomAsset(UGroomAsset* InCustomAsset) override;	

	/** Set preview of a particular binding. */
	virtual void PreviewBinding(int32 BindingIndex) override;

	/** Return the index of the active binding. */
	virtual int32 GetActiveBindingIndex() const override;

private:

	// called when The Play simulation button is pressed
	void OnPlaySimulation();
	bool CanPlaySimulation() const;
	
	// Called when the pause simulation button is pressed
	void OnPauseSimulation();
	bool CanPauseSimulation() const;

	// Called when the reset simulation button is pressed
	void OnResetSimulation();
	bool CanResetSimulation() const;

	// Called when the play animation button is pressed
	void OnPlayAnimation();
	bool CanPlayAnimation() const;

	// Called when the stop animation button is pressed
	void OnStopAnimation();
	bool CanStopAnimation() const;

	// Add buttons to to the toolbar
	void ExtendToolbar();

	// THis should be called when the properties of the Document are changed
	void DocPropChanged(UObject *, FPropertyChangedEvent &);

	// THis is called when the groom target object changes and needs updating
	void OnSkeletalGroomTargetChanged(USkeletalMesh *NewTarget);

	// Return true if the animation asset should be filtered out (not compatible with the preview skel. mesh)
	bool OnShouldFilterAnimAsset(const FAssetData& AssetData);
	void OnObjectChangedAnimAsset(const FAssetData& AssetData);
	bool OnIsEnabledAnimAsset();
	FString GetCurrentAnimAssetPath() const;

	// create the custom components we need
	void InitPreviewComponents();

	// return a pointer to the groom preview component
	UGroomComponent*		GetPreview_GroomComponent() const;
	USkeletalMeshComponent*	GetPreview_SkeletalMeshComponent() const;

	TSharedRef<SDockTab> SpawnViewportTab(const FSpawnTabArgs& Args);

	TSharedRef<SDockTab> SpawnTab_LODProperties(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_InterpolationProperties(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_RenderingProperties(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_CardsProperties(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_MeshesProperties(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_MaterialProperties(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_PhysicsProperties(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_PreviewGroomComponent(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_BindingProperties(const FSpawnTabArgs& Args);
private:

	/** Dockable tab for properties */
	TSharedPtr< SDockableTab > PropertiesTab;
	TSharedPtr< SGroomEditorViewport > ViewportTab;

	TSharedPtr<class IDetailsView> DetailView_LODProperties;
	TSharedPtr<class IDetailsView> DetailView_InterpolationProperties;
	TSharedPtr<class IDetailsView> DetailView_RenderingProperties;
	TSharedPtr<class IDetailsView> DetailView_CardsProperties;
	TSharedPtr<class IDetailsView> DetailView_MeshesProperties;
	TSharedPtr<class IDetailsView> DetailView_MaterialProperties;
	TSharedPtr<class IDetailsView> DetailView_PhysicsProperties;
	TSharedPtr<class IDetailsView> DetailView_PreviewGroomComponent;
	TSharedPtr<class IDetailsView> DetailView_BindingProperties;

	static const FName ToolkitFName;
	static const FName TabId_Viewport;

	static const FName TabId_LODProperties;
	static const FName TabId_InterpolationProperties;
	static const FName TabId_RenderingProperties;
	static const FName TabId_CardsProperties;
	static const FName TabId_MeshesProperties;
	static const FName TabId_MaterialProperties;
	static const FName TabId_PhysicsProperties;
	static const FName TabId_PreviewGroomComponent;
	static const FName TabId_BindingProperties;


	int32 ActiveGroomBindingIndex = -1;
	FDelegateHandle PropertyListenDelegate;
	TWeakObjectPtr<UGroomAsset> GroomAsset;
	TWeakObjectPtr<UGroomBindingAsset> GroomBindingAsset;
	TWeakObjectPtr<UGroomBindingAssetList> GroomBindingAssetList;

	TWeakObjectPtr<UGroomComponent> PreviewGroomComponent;
	TWeakObjectPtr<USkeletalMeshComponent> PreviewSkeletalMeshComponent;
	TWeakObjectPtr<UAnimationAsset> PreviewSkeletalAnimationAsset;

	TSharedPtr<FAssetThumbnailPool> ThumbnailPool;
};

