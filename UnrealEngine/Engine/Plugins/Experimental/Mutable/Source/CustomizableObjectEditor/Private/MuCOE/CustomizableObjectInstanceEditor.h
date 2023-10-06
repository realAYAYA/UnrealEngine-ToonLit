// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCOE/ICustomizableObjectInstanceEditor.h"
#include "TickableEditorObject.h"

#include "CustomizableObjectInstanceEditor.generated.h"

class FProperty;
class FSpawnTabArgs;
class SDockTab;
class SWidget;
class UCustomizableObject;
class UCustomizableObjectInstance;
class UPoseAsset;
struct FFrame;

DECLARE_DELEGATE(FCreatePreviewInstanceFlagDelegate);

/**
* Wrapper UObject class for the UCustomizableObjectInstance::FObjectInstanceUpdatedDelegate dynamic multicast delegate
*/
UCLASS()
class UUpdateClassWrapperClass : public UObject
{
public:
	GENERATED_BODY()

	/** Method to assign for the callback */
	UFUNCTION()
	void DelegatedCallback(UCustomizableObjectInstance* Instance);

	FCreatePreviewInstanceFlagDelegate Delegate;
};


/**
 * CustomizableObject Editor class
 */
class FCustomizableObjectInstanceEditor : 
	public ICustomizableObjectInstanceEditor,
	public FGCObject,
	public FTickableEditorObject
{
public:
	FCustomizableObjectInstanceEditor();
	virtual ~FCustomizableObjectInstanceEditor() override;

	virtual void RegisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;

	/**
	 * Edits the specified object
	 *
	 * @param	Mode					Asset editing mode for this editor (standalone or world-centric)
	 * @param	InitToolkitHost			When Mode is WorldCentric, this is the level editor instance to spawn this editor within
	 * @param	ObjectToEdit			The object to edit
	 */
	void InitCustomizableObjectInstanceEditor( const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UCustomizableObjectInstance* ObjectToEdit );

	// FSerializableObject interface
	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override;
	virtual FString GetReferencerName() const override
	{
		return TEXT("FCustomizableObjectInstanceEditor");
	}
	// End of FSerializableObject interface

	/** IToolkit interface */
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FText GetToolkitName() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;

	/** ICustomizableObjectInstanceEditor interface */
	UCustomizableObjectInstance* GetPreviewInstance() override;
	virtual void RefreshTool() override;
	virtual void RefreshViewport() override;
	virtual void SetPoseAsset(class UPoseAsset* PoseAssetParameter) override;

	/** Callback for the object modified event */
	void OnObjectModified(UObject* Object);

	/** Getter of AssetRegistryLoaded */
	bool GetAssetRegistryLoaded();

	/** Callback to notify the editor when the PreviewInstance has been updated */
	void OnUpdatePreviewInstance();

	/** Compile the customizable object. */
	void CompileObject(UCustomizableObject* Object);

	/** FTickableGameObject interface */
	virtual bool IsTickable(void) const override;
	virtual void Tick(float InDeltaTime) override;
	virtual TStatId GetStatId() const override;

private:

	TSharedRef<SDockTab> SpawnTab_Viewport(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_InstanceProperties(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_AdvancedPreviewSettings(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_TextureAnalyzer(const FSpawnTabArgs& Args);

private:

	void CreatePreviewInstance();

	/** Binds commands associated with the Static Mesh Editor. */
	void BindCommands();
		
	/** Callback when selection changes in the Property Tree. */
	void OnInstancePropertySelectionChanged(FProperty* InProperty);

	/** The instance notifies the editor of projector states changes through this. */
	void OnProjectorStateChanged();

	/** Callback for the asset registry initial load */
	void OnAssetRegistryLoadComplete();

	/** Updates the visibility of PreviewSkeletalMeshComponent */
	void UpdatePreviewVisibility();

	/** Save Customizable Object Instance open in the editor */
	void SaveAsset_Execute() override;

	/** Says if the customizable Object can be shown or be oppened in the editor */
	bool CanOpenOrShowParent();

	/** Show Customizable Object Intance's Parent In Content Browser */
	void ShowParentInContentBrowser();

	/** Open Customizable Object Intance's Parent In Editor */
	void OpenParentInEditor();

	/** Open the Texture Analyzer tab */
	void OpenTextureAnalyzerTab();

private:

	/** The currently viewed object. */
	TObjectPtr<class UCustomizableObjectInstance> CustomizableObjectInstance;
	TArray<TObjectPtr<class UCustomizableSkeletalComponent>> PreviewCustomizableSkeletalComponents;
	TObjectPtr<class UStaticMeshComponent> PreviewStaticMeshComponent;
	TArray<TObjectPtr<class UDebugSkelMeshComponent>> PreviewSkeletalMeshComponents;

	/** List of open tool panels; used to ensure only one exists at any one time */
	TMap< FName, TWeakPtr<class SDockableTab> > SpawnedToolPanels;

	/** Preview Viewport widget */
	TSharedPtr<class SCustomizableObjectEditorViewportTabBody> Viewport;
	TSharedPtr<class IDetailsView> CustomizableInstanceDetailsView;

	/** Level of Details Settings widget. */
	TSharedPtr< class SLevelOfDetailSettings> LevelOfDetailSettings;

	/** Widget for displaying the available UV Channels. */
	TSharedPtr< class STextComboBox > UVChannelCombo;

	/** List of available UV Channels. */
	TArray< TSharedPtr< FString > > UVChannels;

	/** Widget for displaying the available LOD. */
	TSharedPtr< class STextComboBox > LODLevelCombo;

	/** List of LODs. */
	TArray< TSharedPtr< FString > > LODLevels;

	/**	The tab ids for all the tabs used */
	static const FName ViewportTabId;
	static const FName InstancePropertiesTabId;
	static const FName AdvancedPreviewSettingsTabId;
	static const FName TextureAnalyzerTabId;

	/** Handle for the OnObjectModified event */
	FDelegateHandle OnObjectModifiedHandle;

	/** Flag to know when the asset registry initial loading has completed */
	bool AssetRegistryLoaded = true;

	/** UObject class to be able to use the update callback */
	TObjectPtr<UUpdateClassWrapperClass> HelperCallback;

	/** Scene preview settings widget */
	TSharedPtr<SWidget> AdvancedPreviewSettingsWidget;

	/** Object compiler */
	TUniquePtr<class FCustomizableObjectCompiler> Compiler;

	/** Pose asset when doing drag and drop of an UPoseAsset to the viewport */
	UPoseAsset* PoseAsset;

	/** Texture Analyzer table widget which shows the information of the transient textures used in the customizable object instance */
	TSharedPtr<class SCustomizableObjecEditorTextureAnalyzer> TextureAnalyzer;

	/** Variables used to force the refresh of the details view widget. These are needed because sometimes the scrollbar of the window doesn't appear
	    until we force the refresh */
	bool bOnlyRuntimeParameters;
	bool bOnlyRelevantParameters;

	/** Adds the customizable Object Instance Editor commands to the default toolbar */
	void ExtendToolbar();
};
