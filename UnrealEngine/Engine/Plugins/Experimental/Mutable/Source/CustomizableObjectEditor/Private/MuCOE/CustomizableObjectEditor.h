// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CustomizableObjectInstanceEditor.h"
#include "EditorUndoClient.h"
#include "GraphEditor.h"
#include "Misc/NotifyHook.h"
#include "MuCOE/CustomizableObjectCompiler.h"
#include "MuCOE/ICustomizableObjectEditor.h"
#include "TickableEditorObject.h"
#include "MuCO/CustomizableObjectPrivate.h"
#include "Widgets/Input/SNumericDropDown.h"

#include "CustomizableObjectEditor.generated.h"

enum class ECustomizableObjectProjectorType : uint8;
namespace ESelectInfo { enum Type : int; }

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

extern void RemoveRestrictedChars(FString& String);


enum class EGizmoType : uint8
{
	Hidden,
	ProjectorParameter,
	NodeProjectorConstant,
	NodeProjectorParameter,
	ClipMorph,
	ClipMesh,
	Light,
};


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
	explicit FCustomizableObjectEditor(UCustomizableObject& ObjectToEdit);

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
	virtual TSharedPtr<SCustomizableObjectEditorViewportTabBody> GetViewport() override;
	virtual UCustomizableObjectInstance* GetPreviewInstance() override;
	virtual bool CanPasteNodes() const override;
	virtual void PasteNodesHere(const FVector2D& Location) override;
	virtual void SelectNode(const UEdGraphNode* Node) override;
	virtual void ReconstructAllChildNodes(UCustomizableObjectNode& StartNode, const UClass& NodeType) override;
	virtual UProjectorParameter* GetProjectorParameter() override;
	virtual UCustomSettings* GetCustomSettings() override;
	virtual void HideGizmo() override;
	virtual void ShowGizmoProjectorNodeProjectorConstant(UCustomizableObjectNodeProjectorConstant& Node) override;
	virtual void HideGizmoProjectorNodeProjectorConstant() override;
	virtual void ShowGizmoProjectorNodeProjectorParameter(UCustomizableObjectNodeProjectorParameter& Node) override;
	virtual void HideGizmoProjectorNodeProjectorParameter() override;
	virtual void ShowGizmoProjectorParameter(const FString& ParamName, int32 RangeIndex = -1) override;
	virtual void HideGizmoProjectorParameter() override;
	virtual void ShowGizmoClipMorph(UCustomizableObjectNodeMeshClipMorph& Node) override;
	virtual void HideGizmoClipMorph() override;
	virtual void ShowGizmoClipMesh(UCustomizableObjectNodeMeshClipWithMesh& Node) override;
	virtual void HideGizmoClipMesh() override;
	virtual void ShowGizmoLight(ULightComponent& SelectedLight) override;
	virtual void HideGizmoLight() override;

	/** Select only this node only. Do nothing if already was only selected. */
	void SelectSingleNode(UCustomizableObjectNode& Node);
	
	/** Called to undo the last action */
	void UndoGraphAction();

	/** Called to redo the last undone action */
	void RedoGraphAction();

	/** Utility method: Test whether the CO Node Object given as parameter is linked to any of the CO Node Object Group nodes
	* in the Test CO given as parameter */
	static bool GroupNodeIsLinkedToParentByName(UCustomizableObjectNodeObject* Node, UCustomizableObject* Test, const FString& ParentGroupName);

	// FEditorUndoClient Interface
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override { PostUndo(bSuccess); }

	// FNotifyHook interface
	virtual void NotifyPostChange( const FPropertyChangedEvent& PropertyChangedEvent, FProperty* PropertyThatChanged ) override;

	// FTickableGameObject interface
	virtual bool IsTickable() const override;
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

	/** Callback to notify the editor when the PreviewInstance has been updated */
	void OnUpdatePreviewInstance();

	/** Called when the Object Properties needs to be updated */
	virtual void UpdateObjectProperties() override;

	/** Getter of CustomizableObjectEditorAdvancedPreviewSettings */
	TSharedPtr<class SCustomizableObjectEditorAdvancedPreviewSettings> GetCustomizableObjectEditorAdvancedPreviewSettings();

	TSharedPtr<class SCustomizableObjectNodeLayoutBlocksEditor> GetLayoutBlocksEditor() { return LayoutBlocksEditor; }

	/** Debug the object as a raw mutable data in the internal tools. */
	void DebugObject() const;
	
private:
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
	
	// Compile options menu callbacks
	TSharedRef<SWidget> GenerateCompileOptionsMenuContent(TSharedRef<FUICommandList> InCommandList);
	void ResetCompileOptions();
	TSharedPtr<STextComboBox> CompileOptimizationCombo;
	TArray< TSharedPtr<FString> > CompileOptimizationStrings;
	TSharedPtr<STextComboBox> CompileTextureCompressionCombo;
	TArray< TSharedPtr<FString> > CompileTextureCompressionStrings;
	TSharedPtr<SNumericDropDown<float>> CompileTilingCombo;
	TSharedPtr<SNumericDropDown<float>> EmbeddedDataLimitCombo;
	TSharedPtr<SNumericDropDown<float>> PackagedDataLimitCombo;

	void CompileOptions_UseDiskCompilation_Toggled();
	bool CompileOptions_UseDiskCompilation_IsChecked();
	void OnChangeCompileOptimizationLevel(TSharedPtr<FString> NewSelection, ESelectInfo::Type);
	void OnChangeCompileTextureCompressionType(TSharedPtr<FString> NewSelection, ESelectInfo::Type);

	/** Save Customizable Object open in editor */
	void SaveAsset_Execute() override;

	/** Callback when selection changes in the Property Tree. */
	void OnObjectPropertySelectionChanged(FProperty* InProperty);

	/** Callback when selection changes in the Property Tree. */
	void OnInstancePropertySelectionChanged(FProperty* InProperty);

	/** Callback for the object modified event */
	void OnObjectModified(UObject* Object);
		
	void CreateGraphEditorWidget(UEdGraph* InGraph);

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

	/** Updates the visibility of PreviewSkeletalMeshComponent */
	void UpdatePreviewVisibility();

	/** Searches a node that contains the inserted word */
	void OnEnterText(const FText& NewText, ETextCommit::Type TextType);

	/** Logs the search results of the search
	 * @param Context The UObject we have found to be related with the searched string.
	 * @param Type The type of relation with the searched word. It is a node, a value or maybe a variable?
	 * @param bIsFirst Is this the first time we encountered something during our search?
	 * @param Result The string containing the search word we are looking for in Node
	 */
	void LogSearchResult(const UObject& Context, const FString& Type, bool bIsFirst, const FString& Result) const;

	/** Open the Texture Analyzer tab */
	void OpenTextureAnalyzerTab();

	/** Open the Performance Report tab */
	void OpenPerformanceReportTab();

	/** Creates the necessary components for the preview of the CO instance */
	void CreatePreviewComponents();

	/** Recursively find any property that its name or value contains the given string.
	  * @param Property Root property.
	  * @param Container Root property container (address of the property value).
	  * @param FindString String to find for.
	  * @param Context UObject Context where this string has been found.
	  * @param bFound Mark as true if any property has been found. */
	void FindProperty(const FProperty* Property, const void* Container, const FString& FindString, const UObject& Context, bool& bFound);
	
public:
	void OnCustomizableObjectStatusChanged(FCustomizableObjectStatus::EState PreviousState, FCustomizableObjectStatus::EState CurrentState);

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
	TObjectPtr<UCustomizableObject> CustomizableObject;
	TObjectPtr<UCustomizableObjectInstance> PreviewInstance = nullptr;
	TArray<TObjectPtr<UCustomizableSkeletalComponent>> PreviewCustomizableSkeletalComponents;
	TArray<TObjectPtr<UDebugSkelMeshComponent>> PreviewSkeletalMeshComponents;

	/** Object compiler */
	FCustomizableObjectCompiler Compiler;

	/** List of open tool panels; used to ensure only one exists at any one time */
	TMap< FName, TWeakPtr<class SDockableTab> > SpawnedToolPanels;

	/** Preview Viewport widget */
	TSharedPtr<SCustomizableObjectEditorViewportTabBody> Viewport;
	TSharedPtr<FCustomizableObjectEditorViewportClient> ViewportClient;

	TSharedPtr<IDetailsView> CustomizableInstanceDetailsView;


	/** Property View */
	TSharedPtr<class IDetailsView> CustomizableObjectDetailsView;

	/** */
	TSharedPtr<SGraphEditor> GraphEditor;
	TSharedPtr<class IDetailsView> GraphNodeDetailsView;

	/** This widget shows the layout blocks and they can be edited. It is kept alive for some problems re-registering toolbar commands.*/
	TSharedPtr<class SCustomizableObjectNodeLayoutBlocksEditor> LayoutBlocksEditor;
	TSharedPtr<class SCustomizableObjectNodeLayoutBlocksSelector> LayoutBlocksSelector;

	/** Widget to select which node pins are visible. */
	TSharedPtr<class SCustomizableObjectNodePinViewer> NodePinViewer;
	
	/** UObject class to be able to use the update callback */
	TObjectPtr<UUpdateClassWrapper> HelperCallback;
	
	/** Scene preview settings widget, upcast of CustomizableObjectEditorAdvancedPreviewSettings */
	TSharedPtr<SWidget> AdvancedPreviewSettingsWidget;

	/** Scene preview settings widget */
	TSharedPtr<class SCustomizableObjectEditorAdvancedPreviewSettings> CustomizableObjectEditorAdvancedPreviewSettings;
	
	/** Texture Analyzer table widget which shows the information of the transient textures used in the customizable object instance */
	TSharedPtr<class SCustomizableObjecEditorTextureAnalyzer> TextureAnalyzer;

	/** Performance report widget to test and analyze the current customizable object resource demands */
	TSharedPtr<class SCustomizableObjecEditorPerformanceReport> PerformanceReport;

	/** Widget to explore all the tags related with the Customizable Object open in the editor */
	TSharedPtr<class SCustomizableObjectEditorTagExplorer> TagExplorer;

	/** Adds the customizable Object Editor commands to the default toolbar */
	void ExtendToolbar();

	/** URL to open when pressing the documentation button generated by UE */
	const FString DocumentationURL{ TEXT("https://github.com/anticto/Mutable-Documentation/wiki") };

	/** Postponed work to do when OnUpdatePreviewInstance is called. Emptied at the end on each OnUpdatePreviewInstance call. */
	TArray<TFunction<void()>> OnUpdatePreviewInstanceWork;

	TObjectPtr<UProjectorParameter> ProjectorParameter = nullptr;

	TObjectPtr<UCustomSettings> CustomSettings = nullptr;

	bool bRecursionGuard = false;
	
	EGizmoType GizmoType = EGizmoType::Hidden;

protected:
	/** @return the documentation location for this editor */
	virtual FString GetDocumentationLink() const override;
};
