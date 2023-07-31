// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/EnumAsByte.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "Delegates/Delegate.h"
#include "EdGraph/EdGraphNode.h"
#include "EditorUndoClient.h"
#include "Engine/TextureDefines.h"
#include "GraphEditor.h"
#include "Internationalization/Text.h"
#include "Math/Color.h"
#include "Math/UnrealMathSSE.h"
#include "Math/Vector.h"
#include "Math/Vector2D.h"
#include "Misc/NotifyHook.h"
#include "MuCO/CustomizableObjectParameterTypeDefinitions.h"
#include "MuCOE/CustomizableObjectCompiler.h"
#include "MuCOE/ICustomizableObjectEditor.h"
#include "PixelFormat.h"
#include "Stats/Stats2.h"
#include "Templates/SharedPointer.h"
#include "TickableEditorObject.h"
#include "Toolkits/IToolkit.h"
#include "Types/SlateEnums.h"
#include "UObject/GCObject.h"
#include "UObject/NameTypes.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ObjectPtr.h"
#include "UObject/UObjectGlobals.h"

#include "CustomizableObjectEditor.generated.h"

class FCustomizableObjectEditorViewportClient;
class FProperty;
class FSpawnTabArgs;
class FUICommandList;
class IToolkitHost;
class SCustomizableObjectEditorViewportTabBody;
class SDockTab;
class STextComboBox;
class SWidget;
class UCustomizableObject;
class UCustomizableObjectInstance;
class UCustomizableObjectNode;
class UCustomizableObjectNodeMeshClipMorph;
class UCustomizableObjectNodeMeshClipWithMesh;
class UCustomizableObjectNodeObject;
class UCustomizableObjectNodeProjectorConstant;
class UCustomizableObjectNodeProjectorParameter;
class UCustomizableSkeletalComponent;
class UDebugSkelMeshComponent;
class UEdGraph;
class UMaterialInterface;
class UPoseAsset;
class UStaticMeshComponent;
class UTexture;
struct FAssetData;
struct FFrame;
struct FPropertyChangedEvent;
template <typename FuncType> class TFunction;

DECLARE_DELEGATE(FCreatePreviewInstanceFlagDelegate);


/**
* Wrapper UObject class for the UCustomizableObjectInstance::FObjectInstanceUpdatedDelegate dynamic multicast delegate
*/
UCLASS()
class UUpdateClassWrapper : public UObject
{
public:
	GENERATED_BODY()

	/** Method to assign for the callback */
	UFUNCTION()
	void DelegatedCallback(UCustomizableObjectInstance* Instance);

	FCreatePreviewInstanceFlagDelegate Delegate;
};


/** Statistics for the Texture Analyzer */
UCLASS(Transient, MinimalAPI, meta = (DisplayName = "Texture Stats"))
class UCustomizableObjectEditorTextureStats : public UObject
{
	GENERATED_BODY()

public:

	/** Texture - double click to open */
	UPROPERTY(VisibleAnywhere, AssetRegistrySearchable, Category = "Stats", meta = (DisplayName = "Texture", ColumnWidth = "40"))
	FString TextureName;

	/** Texture - double click to open */
	UPROPERTY(VisibleAnywhere, AssetRegistrySearchable, Category = "Stats", meta = (DisplayName = "Name", ColumnWidth = "50"))
	FString TextureParameterName;

	/** Material - double click to open */
	UPROPERTY(VisibleAnywhere, AssetRegistrySearchable, Category = "Stats", meta = (DisplayName = "Material", ColumnWidth = "50"))
	FString MaterialName;

	/** Parent Material - double click to open */
	UPROPERTY(VisibleAnywhere, AssetRegistrySearchable, Category = "Stats", meta = (DisplayName = "Parent", ColumnWidth = "50"))
	FString MaterialParameterName;

	/** Used to open the texture in the editor*/
	UPROPERTY()
	TObjectPtr<UTexture> Texture;

	/** Used to open the material in the editor*/
	UPROPERTY()
	TObjectPtr<UMaterialInterface> Material;

	/** Used to open the parent material in the editor*/
	UPROPERTY()
	TObjectPtr<UMaterialInterface> ParentMaterial;

	/** Resolution of the texture */
	UPROPERTY(VisibleAnywhere, AssetRegistrySearchable, Category = "Stats", meta = (DisplayName = "Resolution X", ColumnWidth = "40", DisplayRight = "true"))
	int32 ResolutionX;
	UPROPERTY(VisibleAnywhere, AssetRegistrySearchable, Category = "Stats", meta = (DisplayName = "Resolution Y", ColumnWidth = "40", DisplayRight = "true"))
	int32 ResolutionY;

	/** The memory used in KB */
	UPROPERTY(VisibleAnywhere, AssetRegistrySearchable, Category = "Stats", meta = (DisplayName = "Size Kb", ColumnWidth = "90"))
	FString Size;

	/** The texture format, e.g. PF_DXT1 */
	UPROPERTY(VisibleAnywhere, AssetRegistrySearchable, Category = "Stats", meta = (ColumnWidth = "96"))
	TEnumAsByte<EPixelFormat> Format;

	/** LOD Bias for this texture. (Texture LODBias + Texture group) */
	UPROPERTY(VisibleAnywhere, AssetRegistrySearchable, Category = "Stats", meta = (DisplayName = "LOD Bias", ColumnWidth = "70"))
	int32 LODBias;
	
	/** Says if the texture is being streamed */
	UPROPERTY(VisibleAnywhere, AssetRegistrySearchable, Category = "Stats", meta = (DisplayName = "Streamed", ColumnWidth = "70"))
	FString IsStreamed;

	/** The Level of detail group of the texture */
	UPROPERTY(VisibleAnywhere, AssetRegistrySearchable, Category = "Stats", meta = (DisplayName = "Texture Group", ColumnWidth = "70"))
	TEnumAsByte<enum TextureGroup> LODGroup;
	
	/** The Component of the texture */
	UPROPERTY(VisibleAnywhere, AssetRegistrySearchable, Category = "Stats", meta = (DisplayName = "Component", ColumnWidth = "70"))
	int32 Component;

};


/**
 * CustomizableObject Editor class
 */
class FCustomizableObjectEditor : 
	public ICustomizableObjectEditor, 
	public FGCObject,
	public FNotifyHook,
	public FTickableEditorObject,
	public FEditorUndoClient
{
public:
	/**
	 * Create a new Customizable Object editor. Called immediately after construction.
	 *
	 * @param	Mode					Asset editing mode for this editor (standalone or world-centric)
	 * @param	InitToolkitHost			When Mode is WorldCentric, this is the level editor instance to spawn this editor within
	 * @param	ObjectToEdit			The object to edit
	 */
	static TSharedRef<FCustomizableObjectEditor> Create(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, UCustomizableObject* ObjectToEdit);

	virtual ~FCustomizableObjectEditor() override;

	virtual void RegisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;
	void CreateCommentBoxFromKey();

	/**
	 * Initialize a new Customizable Object editor. Called immediately after construction.
	 * Required due to being unable to use SharedThis in the constructor.
	 *
	 * See static Create(...) function.
	 */
	void InitCustomizableObjectEditor(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost);

	// FGCObject interface
	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override; 
	virtual FString GetReferencerName() const override
	{
		return TEXT("FCustomizableObjectEditor");
	}

	// IToolkit interface
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FText GetToolkitName() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;

	// ICustomizableObjectEditor interface
	virtual UCustomizableObject* GetCustomizableObject() override;
	virtual void RefreshTool() override;
	virtual void RefreshViewport() override;
	virtual UCustomizableObjectInstance* GetPreviewInstance() override;
	virtual bool CanPasteNodes() const override;
	virtual void PasteNodesHere(const FVector2D& Location) override;
	virtual void SelectNode(const UCustomizableObjectNode* Node) override;
	virtual void SetPoseAsset(class UPoseAsset* PoseAssetParameter) override;
	
	/** Called to undo the last action */
	void UndoGraphAction();

	/** Called to redo the last undone action */
	void RedoGraphAction();

	/** Utility method: Test whether the CO Node Object given as parameter is linked to any of the CO Node Object Group nodes
	* in the Test CO given as parameter */
	static bool GroupNodeIsLinkedToParentByName(UCustomizableObjectNodeObject* Node, UCustomizableObject* Test, const FString& ParentGroupName);

	/** Callback to flag LaunchRefreshMaterialInAllChildren and start the timing for the Editor notification before actually calling
	* the method that does the task, RefreshMaterialNodesInAllChildren */
	void RefreshMaterialNodesInAllChildrenCallback();

	/** Called to refresh all the Customizable Object Material Node nodes children of a Customizalbe Object Node Object Group */
	void RefreshMaterialNodesInAllChildren();

	// FEditorUndoClient Interface
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override { PostUndo(bSuccess); }

	/** FNotifyHook interface */
	virtual void NotifyPostChange( const FPropertyChangedEvent& PropertyChangedEvent, FProperty* PropertyThatChanged ) override;

	/** FTickableGameObject interface */
	virtual bool IsTickable(void) const override;
	virtual void Tick( float InDeltaTime ) override;
	virtual TStatId GetStatId() const override;

	// Delegates
	void DeleteSelectedNodes();
	bool CanDeleteNodes() const;
	void DuplicateSelectedNodes();
	bool CanDuplicateSelectedNodes() const;
	void OnSelectedGraphNodesChanged(const FGraphPanelSelectionSet& NewSelection);
	/**
	 * Called when a node's title is committed for a rename
	 *
	 * @param	NewText				New title text
	 * @param	CommitInfo			How text was committed
	 * @param	NodeBeingChanged	The node being changed
	 */
	void OnNodeTitleCommitted(const FText& NewText, ETextCommit::Type CommitInfo, UEdGraphNode* NodeBeingChanged);

	virtual void UpdateGraphNodeProperties();

	/** Getter of AssetRegistryLoaded */
	bool GetAssetRegistryLoaded();

	/** Callback to notify the editor when the PreviewInstance has been updated */
	void OnUpdatePreviewInstance();

	/** Called when the Object Properties needs to be updated */
	virtual void UpdateObjectProperties() override;

	/** Getter of CustomizableObjectEditorAdvancedPreviewSettings */
	TSharedPtr<class SCustomizableObjectEditorAdvancedPreviewSettings> GetCustomizableObjectEditorAdvancedPreviewSettings();

	/** Add a node to be reconstructed at the end of the tick. */
	void MarkForReconstruct(UEdGraphNode* Node);

	TSharedPtr<class SCustomizableObjectNodeLayoutBlocksEditor> GetLayoutBlocksEditor() { return LayoutBlocksEditor; }

private:
	explicit FCustomizableObjectEditor(UCustomizableObject& ObjectToEdit);

	TSharedRef<SDockTab> SpawnTab_Viewport(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_ObjectProperties(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_InstanceProperties(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_Graph(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_GraphNodeProperties(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_AdvancedPreviewSettings(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_TextureAnalyzer(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_PerformanceReport(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_TagExplorer(const FSpawnTabArgs& Args);
	
	void CreatePreviewInstance();

	/** Returns true if successful */
	bool CreatePreviewComponent(int32 ComponentIndex);

	/** Binds commands associated with the Static Mesh Editor. */
	void BindCommands();
	
	/** Command list for the graph editor */
	TSharedPtr<FUICommandList> GraphEditorCommands;

	// Compile the customizable object.
	void CompileObject();
	void CompileObjectUserPressedButton();
	void CompileOnlySelectedObjectUserPressedButton();
	
	/** Debug the object as a raw mutable data in the internal tools. */
	void DebugObject();

	// Compile options menu callbacks
	TSharedRef<SWidget> GenerateCompileOptionsMenuContent(TSharedRef<FUICommandList> InCommandList);
	void ResetCompileOptions();
	TSharedPtr<STextComboBox> CompileOptimizationCombo;
	TArray< TSharedPtr<FString> > CompileOptimizationStrings;
	void CompileOptions_UseParallelCompilation_Toggled();
	bool CompileOptions_UseParallelCompilation_IsChecked();
	void CompileOptions_UseDiskCompilation_Toggled();
	bool CompileOptions_UseDiskCompilation_IsChecked();
	void CompileOptions_TextureCompression_Toggled();
	bool CompileOptions_TextureCompression_IsChecked();
	void OnChangeCompileOptimizationLevel(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);

	/** Save Customizable Object open in editor */
	void SaveAsset_Execute() override;

	/** Callback when selection changes in the Property Tree. */
	void OnObjectPropertySelectionChanged(FProperty* InProperty);

	/** Callback when selection changes in the Property Tree. */
	void OnInstancePropertySelectionChanged(FProperty* InProperty);

	/** Callback for the object modified event */
	void OnObjectModified(UObject* Object);
	
	/** */
	void OnPreviewInstanceUpdated();
	
	/** */
	TSharedRef<SGraphEditor> CreateGraphEditorWidget(UEdGraph* InGraph);

	/** Copy the currently selected nodes */
	void CopySelectedNodes();
	/** Whether we are able to copy the currently selected nodes */
	bool CanCopyNodes() const;

	/** Paste the contents of the clipboard */
	void PasteNodes();

	/** Cut the currently selected nodes */
	void CutSelectedNodes();
	/** Whether we are able to cut the currently selected nodes */
	bool CanCutNodes() const;

	virtual UEdGraphNode* CreateCommentBox(const FVector2D& NodePos) override;

	/** Callback for the asset registry initial load */
	void OnAssetRegistryLoadComplete();

	/** Updates the visibility of PreviewSkeletalMeshComponent */
	void UpdatePreviewVisibility();

	/** Handler for when an asset's property has changed */
	void OnObjectPropertyChanged(UObject* Object, FPropertyChangedEvent& PropertyChangedEvent);

	/** Utility method to reset current's projector visibility with no Skeletal Mesh Update */
	void ResetProjectorVisibilityNoUpdate();

	/** Searches a node that contains the inserted word */
	void OnEnterText(const FText& NewText, ETextCommit::Type TextType);

	/** Logs the search results of the search
	 * @param Node The Customizable Object Node we have found to be related with the searched string.
	 * @param Type The type of relation with the searched word. It is a node, a value or maybe a variable?
	 * @param bIsFirst Is this the first time we encountered something during our search?
	 * @param Result The string containing the search word we are looking for in Node
	 */
	void LogSearchResult(UCustomizableObjectNode* Node, FString Type, bool bIsFirst, FString Result) const;

	/** Open the Texture Analyzer tab */
	void OpenTextureAnalyzerTab();

	/** Open the Performance Report tab */
	void OpenPerformanceReportTab();

	/** Creates the necessary components for the preview of the CO instance */
	void CreatePreviewComponents();

public:

	// Helpers to get the absolute parent of a Customizable Object
	static UCustomizableObject* GetAbsoluteCOParent(const UCustomizableObjectNodeObject* const Root);
	static void AddCachedReferencers(const FName& PathName, TArray<FName>& ArrayReferenceNames, TArray<FAssetData>& ArrayAssetData);
	static void GetExternalChildObjects(const UCustomizableObject* const Object, TArray<UCustomizableObject*>& ExternalChildren, const bool bRecursively = true, const EObjectFlags ExcludeFlags = EObjectFlags::RF_Transient);


	/**	The tab ids for all the tabs used */
	static const FName ViewportTabId;
	static const FName ObjectPropertiesTabId;
	static const FName InstancePropertiesTabId;
	static const FName GraphTabId;
	static const FName GraphNodePropertiesTabId;
	static const FName SystemPropertiesTabId;
	static const FName AdvancedPreviewSettingsTabId;
	static const FName TextureAnalyzerTabId;
	static const FName PerformanceReportTabId;
	static const FName TagExplorerTabId;
	static const FName ObjectDebuggerTabId;
	static const FName PopulationClassTagManagerTabId;

private:
	/** The currently viewed object. */
	UCustomizableObject* CustomizableObject;
	UCustomizableObjectInstance* PreviewInstance = nullptr;
	TArray<UCustomizableSkeletalComponent*> PreviewCustomizableSkeletalComponents;
	UStaticMeshComponent* PreviewStaticMeshComponent = nullptr;
	TArray<UDebugSkelMeshComponent*> PreviewSkeletalMeshComponents;

	/** Object compiler */
	FCustomizableObjectCompiler Compiler;

	/** List of open tool panels; used to ensure only one exists at any one time */
	TMap< FName, TWeakPtr<class SDockableTab> > SpawnedToolPanels;

	/** Preview Viewport widget */
	TSharedPtr<SCustomizableObjectEditorViewportTabBody> Viewport;
	TSharedPtr<FCustomizableObjectEditorViewportClient> ViewportClient;

	TSharedPtr<class IDetailsView> CustomizableInstanceDetailsView;


	/** Property View */
	TSharedPtr<class IDetailsView> CustomizableObjectDetailsView;

	/** */
	TSharedPtr<SGraphEditor> GraphEditor;
	TSharedPtr<class IDetailsView> GraphNodeDetailsView;

	/** This splitter can be used to add more widgets outside the property editor of the currently selected node. */
	TSharedPtr<class SSplitter> NodeDetailsSplitter;

	/** This widget shows the layout blocks and they can be edited. It is kept alive for some problems re-registering toolbar commands.*/
	TSharedPtr<class SCustomizableObjectNodeLayoutBlocksEditor> LayoutBlocksEditor;
	TSharedPtr<class SCustomizableObjectNodeLayoutBlocksSelector> LayoutBlocksSelector;

	/** Widget to select which node pins are visible. */
	TSharedPtr<class SCustomizableObjectNodePinViewer> NodePinViewer;

	UCustomizableObjectNodeMeshClipMorph* SelectedMeshClipMorphNode = nullptr;

	UCustomizableObjectNodeMeshClipWithMesh* SelectedMeshClipWithMeshNode = nullptr;

	UCustomizableObjectNodeProjectorConstant* SelectedProjectorNode = nullptr;

	UCustomizableObjectNodeProjectorParameter* SelectedProjectorParameterNode = nullptr;

	/** Handle for the OnObjectModified event */
	FDelegateHandle OnObjectModifiedHandle;

	bool ProjectorConstantNodeSelected = false;
	bool ProjectorParameterNodeSelected = false;
	bool SelectedGraphNodesChanged = false;
	bool SelectedProjectorParameterNotNode = false;
	bool ResetProjectorVisibilityForNonNode = false;
	bool SetProjectorVisibilityForParameter = false;
	bool SetProjectorTypeForParameter = false;
	FString ProjectorParameterName;
	FString ProjectorParameterNameWithIndex;
	int32 ProjectorRangeIndex;
	int32 ProjectorParameterIndex = -1;
	FVector3f ProjectorParameterPosition;
	FVector3f ProjectorParameterDirection;
	FVector3f ProjectorParameterUp;
	FVector3f ProjectorParameterScale;
	ECustomizableObjectProjectorType ProjectorParameterProjectionType;
	float ProjectionAngle;
	bool ManagingProjector = false;

	/** Flag to know when the asset registry initial loading has completed */
	bool AssetRegistryLoaded = false;

	/** Flag to know whether the lasts steps when making a new preview instance need to be done when asset registry completes asset loading */
	bool UpdateSkeletalMeshAfterAssetLoaded = false;
	
	/** UObject class to be able to use the update callback */
	UUpdateClassWrapper* HelperCallback;
	
	/** Scene preview settings widget, upcast of CustomizableObjectEditorAdvancedPreviewSettings */
	TSharedPtr<SWidget> AdvancedPreviewSettingsWidget;

	/** Scene preview settings widget */
	TSharedPtr<class SCustomizableObjectEditorAdvancedPreviewSettings> CustomizableObjectEditorAdvancedPreviewSettings;

	/** Advanced scene preview settings */
	class UCustomizableObjectEmptyClassForSettings* AdditionalSettings;
	
	/** When launching the refresh material in all children command from Customizable Object Node Object Group, if the COs are not loaded, the
	* Editor will remaing stuck since the command is executed on the game thread. Use this flag to launch the Editor notification */
	bool LaunchRefreshMaterialInAllChildren = false;

	/** Used together with LaunchRefreshMaterialInAllChildren as a timer to launch the RefreshMaterialNodesInAllChildren method
	* which will perform the task */
	float PendingTimeRefreshMaterialInAllChildren = 2.0;

	/** Pose asset when doing drag and drop of an UPoseAsset to the viewport */
	UPoseAsset* PoseAsset;

	/** Texture Analyzer table widget which shows the information of the transient textures used in the customizable object instance */
	TSharedPtr<class SCustomizableObjecEditorTextureAnalyzer> TextureAnalyzer;

	/** Performance report widget to test and analyze the current cusomizable object resource demands */
	TSharedPtr<class SCustomizableObjecEditorPerformanceReport> PerformanceReport;

	/** Widget to explore all the tags related with the Customizable Object open in the editor */
	TSharedPtr<class SCustomizableObjectEditorTagExplorer> TagExplorer;

	/** Adds the customizable Object Editor commands to the default toolbar */
	void ExtendToolbar();

	/** List of ndes to be reconstructed at the end of the tick. */
	TArray<UEdGraphNode*> ReconstructNodes;

	/** URL to open when pressing the documentation button generated by UE */
	const FString DocumentationURL{ TEXT("https://work.anticto.com/w/mutable/unreal-engine-4/") };

	/** Postponed work to do when OnUpdatePreviewInstance is called. Emptied at the end on each OnUpdatePreviewInstance call. */
	TArray<TFunction<void()>> OnUpdatePreviewInstanceWork;
	
protected:
	/** @return the documentation location for this editor */
	virtual FString GetDocumentationLink() const override;
};
