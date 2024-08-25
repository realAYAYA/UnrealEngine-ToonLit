// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BlueprintEditorModule.h"
#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "CoreTypes.h"
#include "Delegates/Delegate.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"
#include "EditorUndoClient.h"
#include "Engine/Blueprint.h"
#include "FindInBlueprints.h"
#include "Framework/Application/IMenu.h"
#include "Framework/Commands/InputChord.h"
#include "GraphEditor.h"
#include "Input/Reply.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "Layout/Visibility.h"
#include "Math/Color.h"
#include "Math/Vector2D.h"
#include "Merge.h"
#include "Misc/Guid.h"
#include "Misc/NotifyHook.h"
#include "Misc/Optional.h"
#include "PreviewScene.h"
#include "SKismetInspector.h"
#include "SourceControlOperations.h"
#include "Stats/Stats.h"
#include "Stats/Stats2.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"
#include "Templates/SharedPointer.h"
#include "Templates/SubclassOf.h"
#include "Templates/UnrealTemplate.h"
#include "Tickable.h"
#include "TickableEditorObject.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "Toolkits/IToolkit.h"
#include "Toolkits/IToolkitHost.h"
#include "Types/SlateEnums.h"
#include "UObject/GCObject.h"
#include "UObject/NameTypes.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Widgets/SWidget.h"
#include "Widgets/SWindow.h"
#include "WorkflowOrientedApp/WorkflowTabFactory.h"
#include "WorkflowOrientedApp/WorkflowTabManager.h"

class AActor;
class FBlueprintEditorToolbar;
class FBlueprintNamespaceHelper;
class FProperty;
class FReferenceCollector;
class FSCSEditorTreeNode;
class FSubobjectEditorTreeNode;
class FUICommandList;
class IMessageLogListing;
class INameValidatorInterface;
class ISCSEditorCustomization;
class SBlueprintBookmarks;
class SBlueprintPalette;
class SDockTab;
class SFindInBlueprints;
class SKismetInspector;
class SMyBlueprint;
class SReplaceNodeReferences;
class SSCSEditor;
class SSCSEditorViewport;
class SSubobjectEditor;
class SWidget;
class UActorComponent;
class UBlueprintEditorOptions;
class UBlueprintEditorProjectSettings;
class UBlueprintEditorSettings;
class UClass;
class UEdGraph;
class UEdGraphNode;
class UEdGraphSchema;
class UK2Node_Event;
class UK2Node_FunctionEntry;
class ULevelStreaming;
class UObject;
class USceneComponent;
class UStruct;
class UToolMenu;
class UToolMenu;
class UUserDefinedEnum;
class UUserDefinedStruct;
struct FEdGraphSchemaAction;
struct FPropertyChangedEvent;
struct FSlateBrush;
struct FSlateColor;
struct FSubobjectData;
struct FToolMenuContext;
struct Rect;

/* Enums to use when grouping the blueprint members in the list panel. The order here will determine the order in the list */
namespace NodeSectionID
{
	enum Type
	{
		NONE = 0,
		GRAPH,					// Graph
		ANIMGRAPH,				// Anim Graph
		ANIMLAYER,				// Anim Layer
		FUNCTION,				// Functions
		FUNCTION_OVERRIDABLE,	// Overridable functions
		INTERFACE,				// Interface
		MACRO,					// Macros
		VARIABLE,				// Variables
		COMPONENT,				// Components
		DELEGATE,				// Delegate/Event
		USER_ENUM,				// User defined enums
		LOCAL_VARIABLE,			// Local variables
		USER_STRUCT,			// User defined structs
		USER_SORTED				// User sorted categories
	};
};

/////////////////////////////////////////////////////
// FCustomDebugObjectEntry - Used to pass a custom debug object override around

struct FCustomDebugObject
{
public:
	// Custom object to include, regardless of the current debugging World
	UObject* Object;

	// Override for the object name (if not empty)
	FString NameOverride;

public:
	FCustomDebugObject()
		: Object(nullptr)
	{
	}

	FCustomDebugObject(UObject* InObject, const FString& InLabel)
		: Object(InObject)
		, NameOverride(InLabel)
	{
	}
};

/////////////////////////////////////////////////////
// FSelectionDetailsSummoner

#define LOCTEXT_NAMESPACE "BlueprintEditor"

struct KISMET_API FSelectionDetailsSummoner : public FWorkflowTabFactory
{
public:
	FSelectionDetailsSummoner(TSharedPtr<class FAssetEditorToolkit> InHostingApp);

	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const override;
	virtual TSharedRef<SDockTab> SpawnTab(const FWorkflowTabSpawnInfo& Info) const override;

	virtual FText GetTabToolTipText(const FWorkflowTabSpawnInfo& Info) const override
	{
		return LOCTEXT("SelectionDetailsTooltip", "The Details tab allows you see and edit properties of whatever is selected.");
	}
};

/////////////////////////////////////////////////////
// FComponentEventConstructionData

/** The structure used to construct the "Add Event" menu entries */
struct FComponentEventConstructionData
{
	// The name of the event handler to create.
	FName VariableName;
	// The template component that the handler applies to.
	TWeakObjectPtr<UObject> Component;
};

/** The delegate that the caller must supply to BuildComponentActionsSubMenu that returns the currently selected items */
DECLARE_DELEGATE_OneParam(FGetSelectedObjectsDelegate, TArray<FComponentEventConstructionData>&);

/** Delegate for Node Creation Analytics */
DECLARE_DELEGATE(FNodeCreationAnalytic);

/** Describes user actions that created new node */
namespace ENodeCreateAction
{
	enum Type
	{
		MyBlueprintDragPlacement,
		PaletteDragPlacement,
		GraphContext,
		PinContext,
		Keymap
	};
}

/////////////////////////////////////////////////////
// FBlueprintEditor

/** Main Kismet asset editor */
class KISMET_API FBlueprintEditor : public IBlueprintEditor, public FGCObject, public FNotifyHook, public 
FTickableEditorObject, public FEditorUndoClient, public FNoncopyable
{
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnSetPinVisibility, SGraphEditor::EPinVisibility);

	/** A record of a warning generated by a disallowed pin connection attempt */
	struct FDisallowedPinConnection
	{
		FName PinTypeCategoryA;
		FName PinTypeCategoryB;

		uint8 bPinIsArrayA:1;
		uint8 bPinIsReferenceA:1;
		uint8 bPinIsWeakPointerA:1;

		uint8 bPinIsArrayB:1;
		uint8 bPinIsReferenceB:1;
		uint8 bPinIsWeakPointerB:1;
	};

public:
	//~ Begin IToolkit Interface
	virtual void RegisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;
	//~ End IToolkit Interface

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnModeSet, FName);
	FOnModeSet& OnModeSet() { return OnModeSetData; }
	virtual void SetCurrentMode(FName NewMode) override;

public:
	/**
	 * Edits the specified blueprint
	 *
	 * @param	Mode					Asset editing mode for this editor (standalone or world-centric)
	 * @param	InitToolkitHost			When Mode is WorldCentric, this is the level editor instance to spawn this editor within
	 * @param	InBlueprints			The blueprints to edit
	 * @param	bShouldOpenInDefaultsMode	If true, the editor will open in defaults editing mode
	 */
	void InitBlueprintEditor(const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, const TArray<class UBlueprint*>& InBlueprints, bool bShouldOpenInDefaultsMode);

public:
	//~ Begin FAssetEditorToolkit Interface
	virtual bool OnRequestClose(EAssetEditorCloseReason InCloseReason) override;
	virtual void OnClose() override;
	// End of FAssetEditorToolkit 

	//~ Begin IToolkit Interface
	virtual FName GetToolkitContextFName() const override;
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FText GetToolkitName() const override;
	virtual FText GetToolkitToolTipText() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;
	virtual bool IsBlueprintEditor() const override;
	//~ End IToolkit Interface

	//~ Begin FGCObject Interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override;
	//~ End FGCObject Interface

	//~ Begin IBlueprintEditor Interface
	virtual void RefreshEditors(ERefreshBlueprintEditorReason::Type Reason = ERefreshBlueprintEditorReason::UnknownReason) override;
	virtual void RefreshMyBlueprint();
	virtual void RefreshInspector();
	virtual void AddToSelection(UEdGraphNode* InNode) override;
	virtual void JumpToHyperlink(const UObject* ObjectReference, bool bRequestRename = false) override;
	virtual void JumpToPin(const class UEdGraphPin* Pin) override;
	virtual void SummonSearchUI(bool bSetFindWithinBlueprint, FString NewSearchTerms = FString(), bool bSelectFirstResult = false) override;
	virtual void SummonFindAndReplaceUI() override;
	virtual UEdGraph* GetFocusedGraph() const override;
	virtual TSharedPtr<SGraphEditor> OpenGraphAndBringToFront(UEdGraph* Graph, bool bSetFocus = true) override;

	UE_DEPRECATED(5.0, "GetSelectedSCSEditorTreeNodes has been deprecated. Use GetSelectedSubobjectEditorTreeNodes instead.")
	virtual TArray<TSharedPtr<class FSCSEditorTreeNode>> GetSelectedSCSEditorTreeNodes() const override;
	
	UE_DEPRECATED(5.0, "FindAndSelectSCSEditorTreeNode has been deprecated. Use FindAndSelectSubobjectEditorTreeNode instead.")
	virtual TSharedPtr<class FSCSEditorTreeNode> FindAndSelectSCSEditorTreeNode(const UActorComponent* InComponent, bool IsCntrlDown) override { return nullptr; }

	virtual TArray<TSharedPtr<FSubobjectEditorTreeNode>> GetSelectedSubobjectEditorTreeNodes() const override;
	virtual TSharedPtr<FSubobjectEditorTreeNode> FindAndSelectSubobjectEditorTreeNode(const UActorComponent* InComponent, bool IsCntrlDown) override;
	virtual int32 GetNumberOfSelectedNodes() const override;
	virtual void AnalyticsTrackNodeEvent(UBlueprint* Blueprint, UEdGraphNode *GraphNode, bool bNodeDelete = false) const override;
	void AnalyticsTrackCompileEvent(UBlueprint* Blueprint, int32 NumErrors, int32 NumWarnings) const;
	virtual TSharedPtr<class IClassViewerFilter> GetImportedClassViewerFilter() const override { return ImportedClassViewerFilter; }
	UE_DEPRECATED(5.1, "Please use GetPinTypeSelectorFilters")
	virtual TSharedPtr<class IPinTypeSelectorFilter> GetImportedPinTypeSelectorFilter() const override { return ImportedPinTypeSelectorFilter; }
	virtual void GetPinTypeSelectorFilters(TArray<TSharedPtr<class IPinTypeSelectorFilter>>& OutFilters) const override;
	virtual bool IsNonImportedObject(const UObject* InObject) const;
	virtual bool IsNonImportedObject(const FSoftObjectPath& InObject) const;
	//~ End IBlueprintEditor Interface

	//~ Begin FTickableEditorObject Interface
	virtual void Tick(float DeltaTime) override;
	virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Always; }
	virtual TStatId GetStatId() const override;
	//~ End FTickableEditorObject Interface

public:
	FBlueprintEditor();

	virtual ~FBlueprintEditor();

	/** Add context objects for menus and toolbars */
	virtual void InitToolMenuContext(FToolMenuContext& MenuContext) override;

	/** Check the Node Title is visible */
	bool IsNodeTitleVisible(const UEdGraphNode* Node, bool bRequestRename);

	/** Pan the view to center on a particular node */
	void JumpToNode(const class UEdGraphNode* Node, bool bRequestRename = false);

	/** Returns a pointer to the Blueprint object we are currently editing, as long as we are editing exactly one */
	virtual UBlueprint* GetBlueprintObj() const;

	/**	Returns whether the editor is currently editing a single blueprint object */
	bool IsEditingSingleBlueprint() const;

	/** Ensures the blueprint has keyboard focus. */
	void SetKeyboardFocus();

	/** Getters for the various Kismet2 widgets */
	TSharedRef<SKismetInspector> GetInspector() const { return Inspector.ToSharedRef(); }
	TSharedRef<SKismetInspector> GetDefaultEditor() const { return DefaultEditor.ToSharedRef(); }
	TSharedRef<SBlueprintPalette> GetPalette();
	TSharedRef<SBlueprintBookmarks> GetBookmarksWidget() const { return BookmarksWidget.ToSharedRef(); }
	TSharedRef<SWidget> GetCompilerResults() const { return CompilerResults.ToSharedRef(); }
	TSharedRef<SFindInBlueprints> GetFindResults() const { return FindResults.ToSharedRef(); }

	/** Getters for the various optional Kismet2 widgets */
	UE_DEPRECATED(5.0, "GetSCSEditor has been deprecated. Use GetSubobjectEditor instead.")
	TSharedPtr<SSCSEditor> GetSCSEditor() const { return nullptr; }
	
	TSharedPtr<SSubobjectEditor> GetSubobjectEditor() const { return SubobjectEditor; }
	TSharedPtr<SSCSEditorViewport> GetSubobjectViewport() const { return SubobjectViewport; }
	TSharedPtr<SMyBlueprint> GetMyBlueprintWidget() const { return MyBlueprintWidget; }
	TSharedPtr<SReplaceNodeReferences> GetReplaceReferencesWidget() const { return ReplaceReferencesWidget; }

	/**
	 * Provides access to the preview actor.
	 */
	AActor* GetPreviewActor() const;

	/**
	 * Provides access to the preview scene.
	 */
	FPreviewScene* GetPreviewScene()
	{
		return &PreviewScene;
	}

	/**
	* Creates/updates the preview actor for the given blueprint.
	*
	* @param InBlueprint			The Blueprint to create or update the preview for.
	* @param bInForceFullUpdate	Force a full update to respawn actors.
	*/
	void UpdatePreviewActor(UBlueprint* InBlueprint, bool bInForceFullUpdate = false);

	/**
	* Destroy the Blueprint preview.
	*/
	void DestroyPreview();

	TSharedPtr<class FBlueprintEditorToolbar> GetToolbarBuilder() { return Toolbar; }

	/** @return the documentation location for this editor */
	virtual FString GetDocumentationLink() const override;

	/**	Returns whether the edited blueprint has components */
	bool CanAccessComponentsMode() const;

	/** Returns if we are currently closing the editor */
	bool IsEditorClosing() const;

	// @todo This is a hack for now until we reconcile the default toolbar with application modes
	void RegisterToolbarTab(const TSharedRef<class FTabManager>& TabManager);
	/** Throw a simple message into the log */
	void LogSimpleMessage(const FText& MessageText);

	/** Dumps messages to the compiler log, with an option to force it to display/come to front */
	void DumpMessagesToCompilerLog(const TArray<TSharedRef<class FTokenizedMessage>>& Messages, bool bForceMessageDisplay);

	/** Returns true if PIE is active */
	bool IsPlayInEditorActive() const;

	/** Get the currently selected set of nodes */
	FGraphPanelSelectionSet GetSelectedNodes() const;

	/** Returns the currently selected node if there is a single node selected (if there are multiple nodes selected or none selected, it will return nullptr) */
	UEdGraphNode* GetSingleSelectedNode() const;

	/** Save the current set of edited objects in the LastEditedObjects array so it will be opened next time we open K2 */
	void SaveEditedObjectState();

	/** Create new tab for each element of LastEditedObjects array */
	void RestoreEditedObjectState();

	// Request a save of the edited object state
	// This is used to delay it by one frame when triggered by a tab being closed, so it can finish closing before remembering the new state
	void RequestSaveEditedObjectState();

	/** Get the visible bounds of the given graph node */
	void GetBoundsForNode(const UEdGraphNode* InNode, class FSlateRect& OutRect, float InPadding) const;

	/** Gets the focused graph current view bookmark ID */
	void GetViewBookmark(FGuid& BookmarkId);

	/** Gets the focused graph view location/zoom amount */
	void GetViewLocation(FVector2D& Location, float& ZoomAmount);

	/** Sets the focused graph view location/zoom amount */
	void SetViewLocation(const FVector2D& Location, float ZoomAmount, const FGuid& BookmarkId = FGuid());

	/** Returns whether a graph is editable or not */
	virtual bool IsEditable(UEdGraph* InGraph) const;
	/** Determines if the graph is ReadOnly, this differs from editable in that it is never expected to be edited and is in a read-only state */
	bool IsGraphReadOnly(UEdGraph* InGraph) const;

	/** Used to determine the visibility of the graph's instruction text. */
	float GetInstructionTextOpacity(UEdGraph* InGraph) const;

	/** Returns true if in editing mode */
	virtual bool InEditingMode() const;

	/** Returns true if able to compile */
	virtual bool IsCompilingEnabled() const;

	/** Returns true if the parent class of the Blueprint being edited is also a Blueprint */
	bool IsParentClassABlueprint() const;

	/** Returns true if the parent class of the Blueprint being edited is an editable Blueprint */
	bool IsParentClassAnEditableBlueprint() const;

	/** Returns true if the parent class of the Blueprint being edited is native */
	bool IsParentClassNative() const;

	/** Returns true if the parent class is native and the link to it's header can be shown*/
	virtual bool IsNativeParentClassCodeLinkEnabled() const;

	/** Handles opening the header file of native parent class */
	void OnEditParentClassNativeCodeClicked();

	/** Returns: "(<NativeParentClass>.h)" */
	FText GetTextForNativeParentClassHeaderLink() const;

	/** Determines visibility of the native parent class manipulation buttons on the menu bar overlay */
	EVisibility GetNativeParentClassButtonsVisibility() const;

	/** Determines visibility of the standard parent class label on the menu bar overlay */
	EVisibility GetParentClassNameVisibility() const;

	/** Returns our PIE Status - SIMULATING / SERVER / CLIENT */
	FText GetPIEStatus() const;

	/**
	 * Util for finding a glyph for a graph
	 *
	 * @param Graph - The graph to evaluate
	 * @param bInLargeIcon - if true the icon returned is 22x22 pixels, else it is 16x16
	 * @return An appropriate brush to use to represent the graph, if the graph is an unknown type the function will return the default "function" glyph
	 */
	static const FSlateBrush* GetGlyphForGraph(const UEdGraph* Graph, bool bInLargeIcon = false);

	/**
	 * Util for finding a glyph and color for a variable.
	 *
	 * @param VarScope			Scope to find the variable in
	 * @param VarName			Name of variable
	 * @param IconColorOut		The resulting color for the glyph
	 * @param SecondaryBrushOut The resulting secondary glyph brush (used for Map types)
	 * @param SecondaryColorOut The resulting secondary color for the glyph (used for Map types)
	 * @return					The resulting glyph brush
	 */
	static FSlateBrush const* GetVarIconAndColor(const UStruct* VarScope, FName VarName, FSlateColor& IconColorOut, FSlateBrush const*& SecondaryBrushOut, FSlateColor& SecondaryColorOut);

	/**
	 * Util for finding a glyph and color for a variable.
	 *
	 * @param Property       The variable's property
	 * @param IconColorOut      The resulting color for the glyph
	 * @param SecondaryBrushOut The resulting secondary glyph brush (used for Map types)
	 * @param SecondaryColorOut The resulting secondary color for the glyph (used for Map types)
	 * @return					The resulting glyph brush
	 */
	static FSlateBrush const* GetVarIconAndColorFromProperty(const FProperty* Property, FSlateColor& IconColorOut, FSlateBrush const*& SecondaryBrushOut, FSlateColor& SecondaryColorOut);

	/**
	* Util for finding a glyph and color for a variable.
	*
	* @param PinType       The variable's pin type
	* @param IconColorOut      The resulting color for the glyph
	* @param SecondaryBrushOut The resulting secondary glyph brush (used for Map types)
	* @param SecondaryColorOut The resulting secondary color for the glyph (used for Map types)
	* @return					The resulting glyph brush
	*/
	static FSlateBrush const* GetVarIconAndColorFromPinType(const FEdGraphPinType& PinType, FSlateColor& IconColorOut, FSlateBrush const*& SecondaryBrushOut, FSlateColor& SecondaryColorOut);

	/** Overridable function for determining if the current mode can script */
	virtual bool IsInAScriptingMode() const;

	/** Called when Compile button is clicked */
	virtual void Compile();

	/** Helper functions used to construct/operate-on the "Save on Compile" command */
	bool IsSaveOnCompileEnabled() const;

	/** Calls the above function, but returns an FReply::Handled(). Used in SButtons */
	virtual FReply Compile_OnClickWithReply();

	/** Called when the refresh all nodes button is clicked */
	void RefreshAllNodes_OnClicked();

	EVisibility IsDebuggerVisible() const;

	virtual void OnActiveTabChanged(TSharedPtr<SDockTab> PreviouslyActive, TSharedPtr<SDockTab> NewlyActivated);

	/** Called when a token in a log message is clicked */
	void LogToken_OnClicked(const class IMessageToken& Token);

	virtual void FocusInspectorOnGraphSelection(const TSet<class UObject*>& NewSelection, bool bForceRefresh = false);

	/** Variable list window calls this after it is updated */
	void VariableListWasUpdated();

	/** Virtual override point for editing defaults; allowing more derived editors to edit something else */
	virtual void StartEditingDefaults(bool bAutoFocus = true, bool bForceRefresh = false);

	// Called by the blueprint editing app mode to focus the appropriate tabs, etc...
	void SetupViewForBlueprintEditingMode();

	// Ensures the blueprint is up to date
	void EnsureBlueprintIsUpToDate(UBlueprint* BlueprintObj);

	// Should be called when initializing any editor built off this foundation
	void CommonInitialization(const TArray<UBlueprint*>& InitBlueprints, bool bShouldOpenInDefaultsMode);

	// Other types of BP editors that don't use BP VM/function libs can choose to not load BP function libraries, for example: Control Rig Blueprints
	virtual bool ShouldLoadBPLibrariesFromAssetRegistry() { return true; }

	// Should be called when initializing an editor that has a blueprint, after layout (tab spawning) is done
	void PostLayoutBlueprintEditorInitialization();

	/** Called when graph editor focus is changed */
	virtual void OnGraphEditorFocused(const TSharedRef<class SGraphEditor>& InGraphEditor);

	/** Called when the graph editor tab is backgrounded */
	virtual void OnGraphEditorBackgrounded(const TSharedRef<SGraphEditor>& InGraphEditor);

	/** Enable/disable the subobject editor preview viewport */
	void EnableSubobjectPreview(bool bEnable);

	/** Refresh the preview viewport to reflect changes in the subobject */
	void UpdateSubobjectPreview(bool bUpdateNow = false);

	///////////////////////////////////////////////////////
	// Subobject Editor Callbacks
	/** Delegate invoked when the Subobject editor needs to obtain the object context for editing */
	UObject* GetSubobjectEditorObjectContext() const;

	/** Delegate invoked when the selection is changed in the subobject editor widget */
	virtual void OnSelectionUpdated(const TArray<TSharedPtr<class FSubobjectEditorTreeNode>>& SelectedNodes);

	/** Delegate invoked when an item is double clicked in the subobject editor widget */
	virtual void OnComponentDoubleClicked(TSharedPtr<class FSubobjectEditorTreeNode> Node);

	/** Delegate invoked when a new subobject is added in the subobject editor widget */
	virtual void OnComponentAddedToBlueprint(const FSubobjectData& NewSubobjectData);
	
	/** Delegate invoked when the selection is changed in the subobject editor widget */
	UE_DEPRECATED(5.0, "OnSelectionUpdated(const TArray<TSharedPtr<class FSCSEditorTreeNode>>&) has been deprecated. Use OnSelectionUpdated(const TArray<TSharedPtr<class FSubobjectEditorTreeNode>>&) instead.")
	virtual void OnSelectionUpdated(const TArray<TSharedPtr<class FSCSEditorTreeNode>>& SelectedNodes) {}

	/** Delegate invoked when an item is double clicked in the subobject editor widget */
	UE_DEPRECATED(5.0, "OnComponentDoubleClicked(TSharedPtr<class FSCSEditorTreeNode>) has been deprecated. Use OnComponentDoubleClicked(TSharedPtr<class FSubobjectEditorTreeNode>) instead.")
	virtual void OnComponentDoubleClicked(TSharedPtr<class FSCSEditorTreeNode> Node) {}

	/** Pin visibility accessors */
	void SetPinVisibility(SGraphEditor::EPinVisibility Visibility);
	bool GetPinVisibility(SGraphEditor::EPinVisibility Visibility) const { return PinVisibility == Visibility; }

	/** Reparent the current blueprint */
	void ReparentBlueprint_Clicked();
	virtual bool ReparentBlueprint_IsVisible() const;
	void ReparentBlueprint_NewParentChosen(UClass* ChosenClass);

	/** Utility function to handle all steps required to rename a newly added action */
	void RenameNewlyAddedAction(FName InActionName);

	/** Adds a new variable to this blueprint */
	void OnAddNewVariable();
	FReply OnAddNewVariable_OnClick() { OnAddNewVariable(); return FReply::Handled(); }

	/** Checks if adding a local variable is allowed in the focused graph */
	virtual bool CanAddNewLocalVariable() const;

	/** Adds a new local variable to the focused function graph */
	virtual void OnAddNewLocalVariable();

	/** Pastes a new local variable to the focused function graph */
	virtual void OnPasteNewLocalVariable(const FBPVariableDescription& VariableDescription);

	// Type of new document/graph being created by a menu item
	enum ECreatedDocumentType
	{
		CGT_NewVariable,
		CGT_NewFunctionGraph,
		CGT_NewMacroGraph,
		CGT_NewAnimationLayer,
		CGT_NewEventGraph,
		CGT_NewLocalVariable
	};

	/** Called when New Function button is clicked */
	virtual void NewDocument_OnClicked(ECreatedDocumentType GraphType);
	FReply NewDocument_OnClick(ECreatedDocumentType GraphType) { NewDocument_OnClicked(GraphType); return FReply::Handled(); }

	/** Called when New Delegate button is clicked */
	void OnAddNewDelegate();
	bool AddNewDelegateIsVisible() const;

	// Called to see if the new document menu items is visible for this type
	virtual bool IsSectionVisible(NodeSectionID::Type InSectionID) const { return true; }
	virtual bool NewDocument_IsVisibleForType(ECreatedDocumentType GraphType) const;
	EVisibility NewDocument_GetVisibilityForType(ECreatedDocumentType GraphType) const
	{
		return NewDocument_IsVisibleForType(GraphType) ? EVisibility::Visible : EVisibility::Collapsed;
	}

	static FName SelectionState_MyBlueprint;
	static FName SelectionState_Components;
	static FName SelectionState_Graph;
	static FName SelectionState_ClassSettings;
	static FName SelectionState_ClassDefaults;

	/** Gets or sets the flag for context sensitivity in the graph action menu */
	bool& GetIsContextSensitive() { return bIsActionMenuContextSensitive; }

	/** Gets the UI selection state of this editor */
	FName GetUISelectionState() const { return CurrentUISelection; }
	void SetUISelectionState(FName SelectionOwner);

	virtual void ClearSelectionStateFor(FName SelectionOwner);

	/** Handles spawning a graph node in the current graph using the passed in chord */
	virtual FReply OnSpawnGraphNodeByShortcut(FInputChord InChord, const FVector2D& InPosition, UEdGraph* InGraph);

	/**
	 * Perform the actual promote to variable action on the given pin in the given blueprint.
	 *
	 * @param	InBlueprint				The blueprint in which to create the variable.
	 * @param	InTargetPin				The pin on which to base the variable.
	 * @param	bInToMemberVariable		TRUE if attempting to create a member variable, FALSE if the variable should be local
	 * @param   InOptionalLocation		Where the new node should be placed. If null, a fixed offset from the parent node will be used.
	 */
	void DoPromoteToVariable(UBlueprint* InBlueprint, UEdGraphPin* InTargetPin, bool bInToMemberVariable, const FVector2D* InOptionalLocation = nullptr);

	/** Called when node is spawned by keymap */
	void OnNodeSpawnedByKeymap();

	/** Update Node Creation mechanisms for analytics */
	void UpdateNodeCreationStats(const ENodeCreateAction::Type CreateAction);

	/** Sets customizations for the BP editor details panel. */
	void SetDetailsCustomization(TSharedPtr<class FDetailsViewObjectFilter> DetailsObjectFilter, TSharedPtr<class IDetailRootObjectCustomization> DetailsRootCustomization);

	/** Sets subobject editor UI customization */
	void SetSubobjectEditorUICustomization(TSharedPtr<class ISCSEditorUICustomization> SCSEditorUICustomization);

	UE_DEPRECATED(5.0, "SetSCSEditorUICustomization has been deprecated. Use SetSubobjectEditorUICustomization instead.")
	void SetSCSEditorUICustomization(TSharedPtr<class ISCSEditorUICustomization> SCSEditorUICustomization)
	{
		return SetSubobjectEditorUICustomization(SCSEditorUICustomization);
	}

	/**
	 * Register a customization for interacting with the subobject editor
	 * @param	InComponentName			The name of the component to customize behavior for
	 * @param	InCustomization			The customization instance to use
	 */
	void RegisterSCSEditorCustomization(const FName& InComponentName, TSharedPtr<class ISCSEditorCustomization> InCustomization);

	/**
	 * Unregister a previously registered customization for interacting with the subobject editor
	 * @param	InComponentName			The name of the component to customize behavior for
	 */
	void UnregisterSCSEditorCustomization(const FName& InComponentName);

	/** Forces the merge tool to be shown */
	void CreateMergeToolTab();
	void CreateMergeToolTab(const UBlueprint* BaseBlueprint, const UBlueprint* RemoteBlueprint, const FOnMergeResolved& ResolutionCallback);

	/** Closes the merge tool, rather than simply hiding it */
	void CloseMergeTool();

	/** Dumps the current blueprint search index to a JSON file for debugging purposes */
	void OnGenerateSearchIndexForDebugging();

	/** Dumps the currently-cached index data for the blueprint to a file for debugging */
	void OnDumpCachedIndexDataForBlueprint();

	/**
	 * Check to see if we can customize the subobject editor for the passed-in scene component
	 * @param	InComponentToCustomize	The component to check to see if a customization exists
	 * @return an subobject editor customization instance, if one exists.
	 */
	TSharedPtr<class ISCSEditorCustomization> CustomizeSubobjectEditor(const USceneComponent* InComponentToCustomize) const;

	UE_DEPRECATED(5.0, "CustomizeSCSEditor has been deprecated. Use CustomizeSubobjectEditor instead.")
	TSharedPtr<class ISCSEditorCustomization> CustomizeSCSEditor(USceneComponent* InComponentToCustomize) const
	{
		return CustomizeSubobjectEditor(InComponentToCustomize);
	}

	/** Adds to a list of custom objects for debugging beyond what will automatically be found/used */
	virtual void GetCustomDebugObjects(TArray<FCustomDebugObject>& DebugList) const { }

	/** If returns true only the custom debug list will be used */
	virtual bool OnlyShowCustomDebugObjects() const { return false; }

	/** Can be overloaded to customize the labels in the debug filter */
	virtual FString GetCustomDebugObjectLabel(UObject* ObjectBeingDebugged) const { return FString(); }

	/** Called when a node's title is committed for a rename */
	void OnNodeTitleCommitted(const FText& NewText, ETextCommit::Type CommitInfo, UEdGraphNode* NodeBeingChanged);

	/** Called by a graph title bar to get any extra information the editor would like to display */
	virtual FText GetGraphDecorationString(UEdGraph* InGraph) const;

	/** Gets the display name of a graph */
	static FText GetGraphDisplayName(const UEdGraph* Graph);

	/** Checks to see if the provided graph is contained within the current blueprint */
	bool IsGraphInCurrentBlueprint(const UEdGraph* InGraph) const;

	/** Get the context to use from the Blueprint type */
	static FName GetContextFromBlueprintType(EBlueprintType InType);

	/* Selects an item in "My Blueprint" by name. */
	void SelectGraphActionItemByName(const FName& ItemName, ESelectInfo::Type SelectInfo = ESelectInfo::Direct, int32 SectionId = INDEX_NONE, bool bIsCategory = false);

	/** Handle when the debug object is changed in the UI */
	virtual void HandleSetObjectBeingDebugged(UObject* InObject) {}

	/** Adds a new bookmark node to the Blueprint that's being edited. */
	FBPEditorBookmarkNode* AddBookmark(const FText& DisplayName, const FEditedDocumentInfo& BookmarkInfo, bool bSharedBookmark = false);

	/** Renames the bookmark node with the given ID. */
	void RenameBookmark(const FGuid& BookmarkNodeId, const FText& NewName);

	/** Removes the bookmark node with the given ID. */
	void RemoveBookmark(const FGuid& BookmarkNodeId, bool bRefreshUI = true);

	/** Gets the default schema for this editor */
	TSubclassOf<UEdGraphSchema> GetDefaultSchema() const { return GetDefaultSchemaClass(); }
	
	/** Imports the given namespace into the editor. This may trigger a load event for additional macro and/or function library assets if not already loaded. */
	void ImportNamespace(const FString& InNamespace);

	/** Parameters for the extended ImportNamespaceEx() method */
	struct FImportNamespaceExParameters
	{
		/** Whether this is an automatic or explicit (i.e. user-initiated) action. */
		bool bIsAutoImport;

		/** Callback to use for any post-import actions. Will not be invoked if nothing is imported. */
		FSimpleDelegate OnPostImportCallback;

		/** One or more unique namespace identifiers to be imported as part of the single transaction. */
		TSet<FString> NamespacesToImport;

		/** Default ctor (for initialization). */
		FImportNamespaceExParameters()
		{
			// Treat as auto-import by default (implies that the editor will auto-refresh the details view).
			bIsAutoImport = true;
		}
	};

	/** Imports a set of namespace(s) into the editor. This may trigger a load event for additional macro and/or function library assets if not already loaded. */
	void ImportNamespaceEx(const FImportNamespaceExParameters& InParams);

	/** Removes the given namespace from the editor's current import context. However, this will NOT unload any associated macro and/or function library assets. */
	void RemoveNamespace(const FString& InNamespace);

	/** Selects a local variable to load in the details panel. */
	virtual bool SelectLocalVariable(const UEdGraph* Graph, const FName& VariableName) { return false; }

	/** Duplicates an EdGraphNode using selection and copy / paste functionality */
	void SelectAndDuplicateNode(UEdGraphNode* InNode);

protected:

	/** Called during initialization of the blueprint editor to register commands and extenders. */
	virtual void InitalizeExtenders();

	/** Called during initialization of the blueprint editor to register any application modes. */
	virtual void RegisterApplicationModes(const TArray<UBlueprint*>& InBlueprints, bool bShouldOpenInDefaultsMode, bool bNewlyCreated = false);

	// Updates the selected object used by the stand lone defaults editor widget.
	void RefreshStandAloneDefaultsEditor();

	// Zooming to fit the entire graph
	void ZoomToWindow_Clicked();
	bool CanZoomToWindow() const;

	// Zooming to fit the current selection
	void ZoomToSelection_Clicked();
	bool CanZoomToSelection() const;

	// Navigating into/out of graphs
	void NavigateToParentGraph();
	void NavigateToParentGraphByDoubleClick();
	bool CanNavigateToParentGraph() const;
	void NavigateToChildGraph();
	bool CanNavigateToChildGraph() const;

	/** Determines visibility of the find parent class in content browser button on the menu bar overlay */
	EVisibility GetFindParentClassVisibility() const;

	/** Determines visibility of the edit parent class button on the menu bar overlay */
	EVisibility GetEditParentClassVisibility() const;

	/** Recreates the overlay on the menu bar */
	virtual void PostRegenerateMenusAndToolbars() override;

	/** Returns the name of the Blueprint's parent class */
	FText GetParentClassNameText() const;

	/** Handler for "Find parent class in CB" button */
	FReply OnFindParentClassInContentBrowserClicked();

	/** Handler for "Edit parent class" button */
	FReply OnEditParentClassClicked();

	/** Called to start a quick find (focus the search box in the explorer tab) */
	void FindInBlueprint_Clicked();

	// Is the main details panel currently showing 'Global options' (e.g., class metadata)?
	bool IsDetailsPanelEditingGlobalOptions() const;

	/** Edit the class settings aka Blueprint global options */
	void EditGlobalOptions_Clicked();

	// Is the main details panel currently showing 'Class defaults' (Note: Has nothing to do with the standalone class defaults panel)?
	bool IsDetailsPanelEditingClassDefaults() const;

	/** Edit the class defaults */
	void EditClassDefaults_Clicked();

	/** Called to undo the last action */
	void UndoGraphAction();

	/** Whether or not we can perform an undo of the last transacted action */
	bool CanUndoGraphAction() const;

	/** Called to redo the last undone action */
	void RedoGraphAction();

	/** Whether or not we can redo an undone action */
	bool CanRedoGraphAction() const;
	
	/** Called when the selection changes in the GraphEditor */
	virtual void OnSelectedNodesChangedImpl(const TSet<class UObject*>& NewSelection);

	/** Called when the selection changes in the GraphEditor */
	void OnSelectedNodesChanged(const TSet<class UObject*>& NewSelection) { OnSelectedNodesChangedImpl(NewSelection); }

	/** Called when an actor is dropped onto the graph editor */
	void OnGraphEditorDropActor(const TArray< TWeakObjectPtr<AActor> >& Actors, UEdGraph* Graph, const FVector2D& DropLocation);

	/** Called when a streaming level is dropped onto the graph editor */
	void OnGraphEditorDropStreamingLevel(const TArray< TWeakObjectPtr<ULevelStreaming> >& Levels, UEdGraph* Graph, const FVector2D& DropLocation);

	/** Called to create context menu when right-clicking on graph */
	FActionMenuContent OnCreateGraphActionMenu(UEdGraph* InGraph, const FVector2D& InNodePosition, const TArray<UEdGraphPin*>& InDraggedPins, bool bAutoExpand, SGraphEditor::FActionMenuClosed InOnMenuClosed);

	/** Called from graph context menus when they close to tell the editor why they closed */
	void OnGraphActionMenuClosed(bool bActionExecuted, bool bContextSensitiveChecked, bool bGraphPinContext);

	/** Called when the Blueprint we are editing has changed */
	virtual void OnBlueprintChangedImpl(UBlueprint* InBlueprint, bool bIsJustBeingCompiled = false);

	/** Called when the Blueprint we are editing has changed, forwards to impl */
	void OnBlueprintChanged(UBlueprint* InBlueprint) { return OnBlueprintChangedImpl(InBlueprint); }

	void OnBlueprintCompiled(UBlueprint* InBlueprint);

	/** Handles the unloading of Blueprints (by closing the editor, if it operating on the Blueprint being unloaded)*/
	void OnBlueprintUnloaded(UBlueprint* InBlueprint);

	/** Called when a property change is about to be propagated to instances of the Blueprint */
	void OnPreObjectPropertyChanged(UObject* InObject, const FEditPropertyChain& EditPropertyChain);

	/** Called after a property change has been propagated to instances of the Blueprint */
	void OnPostObjectPropertyChanged(UObject* InObject, FPropertyChangedEvent& PropertyChangedEvent);

	//@TODO: Should the breakpoint/watch modification operations be whole-blueprint, or current-graph?

	/** Deletes all breakpoints for the blueprint being edited */
	void ClearAllBreakpoints();

	/** Disables all breakpoints for the blueprint being edited */
	void DisableAllBreakpoints();

	/** Enables all breakpoints for the blueprint being edited */
	void EnableAllBreakpoints();

	/** Clears all watches associated with the blueprint being edited */
	void ClearAllWatches();

	bool HasAnyBreakpoints() const;
	bool HasAnyEnabledBreakpoints() const;
	bool HasAnyDisabledBreakpoints() const;
	bool HasAnyWatches() const;

	/** Spawns a new blueprint debugger tab */
	void OpenBlueprintDebugger();
	bool CanOpenBlueprintDebugger() const;

	// Utility helper to get the currently hovered pin in the currently visible graph, or nullptr if there isn't one
	UEdGraphPin* GetCurrentlySelectedPin() const;

	// UI Action functionality
	void OnPromoteToVariable(bool bInToMemberVariable);
	bool CanPromoteToVariable(bool bInToMemberVariable) const;

	void OnSplitStructPin();
	bool CanSplitStructPin() const;

	void OnRecombineStructPin();
	bool CanRecombineStructPin() const;

	void OnAddExecutionPin();
	bool CanAddExecutionPin() const;

	void OnInsertExecutionPinBefore();
	void OnInsertExecutionPinAfter();
	void OnInsertExecutionPin(EPinInsertPosition Position);
	bool CanInsertExecutionPin() const;

	void OnRemoveExecutionPin();
	bool CanRemoveExecutionPin() const;

	void OnRemoveThisStructVarPin();
	bool CanRemoveThisStructVarPin() const;

	void OnRemoveOtherStructVarPins();
	bool CanRemoveOtherStructVarPins() const;

	void OnRestoreAllStructVarPins();
	bool CanRestoreAllStructVarPins() const;

	void OnResetPinToDefaultValue();
	bool CanResetPinToDefaultValue() const;

	void OnAddOptionPin();
	bool CanAddOptionPin() const;

	void OnRemoveOptionPin();
	bool CanRemoveOptionPin() const;

	/** Functions for handling the changing of the pin's type (PinCategory, PinSubCategory, etc) */
	FEdGraphPinType OnGetPinType(UEdGraphPin* SelectedPin) const;
	void OnChangePinType();
	void OnChangePinTypeFinished(const FEdGraphPinType& PinType, UEdGraphPin* SelectedPin);
	bool CanChangePinType() const;

	void OnAddParentNode();
	bool CanAddParentNode() const;
	
	void OnCreateMatchingFunction();
	bool CanCreateMatchingFunction() const;

	void OnEnableBreakpoint();
	bool CanEnableBreakpoint() const;

	void OnToggleBreakpoint();
	bool CanToggleBreakpoint() const;

	void OnDisableBreakpoint();
	bool CanDisableBreakpoint() const;

	void OnAddBreakpoint();
	bool CanAddBreakpoint() const;

	void OnRemoveBreakpoint();
	bool CanRemoveBreakpoint() const;

	void OnCollapseNodes();
	bool CanCollapseNodes() const;

	void OnCollapseSelectionToFunction();
	bool CanCollapseSelectionToFunction() const;

	void OnCollapseSelectionToMacro();
	bool CanCollapseSelectionToMacro() const;

	void OnPromoteSelectionToFunction();
	bool CanPromoteSelectionToFunction() const;

	void OnPromoteSelectionToMacro();
	bool CanPromoteSelectionToMacro() const;

	void OnExpandNodes();
	bool CanExpandNodes() const;

	/**
	* Move the given set of nodes to an average spot near the Source position
	* 
	* @param AverageNodes					The nodes to move
	* @param SourcePos						The source position used to average the nodes around
	* @param bExpandedNodesNeedUniqueGuid	If true then a new Guid will be generated for each node in the set
	*/
	void MoveNodesToAveragePos(TSet<UEdGraphNode*>& AverageNodes, FVector2D SourcePos, bool bExpandedNodesNeedUniqueGuid = false) const;

	void OnConvertFunctionToEvent();

public:
	/** Converts the given function entry node to an event if it passes validation */
	bool ConvertFunctionIfValid(UK2Node_FunctionEntry* FuncEntryNode);

	/** Converts the given event node to a function graph on this blueprint if it passes validation */
	bool ConvertEventIfValid(UK2Node_Event* EventToConv);

protected:
	/** 
	* Callback function for the context menu on a node to determine if a function 
	* could possibly be converted to an event
	*/
	bool CanConvertFunctionToEvent() const;

	/*
	* Given a function node, move all nodes from the function out of the function graph,
	* create an event with the same function name, and connect nodes to that event. 
	* 
	* @param SelectedCallFunctionNode	The function node to convert to an event
	*/
	void ConvertFunctionToEvent(UK2Node_FunctionEntry* SelectedCallFunctionNode);

	void OnConvertEventToFunction();
	void ConvertEventToFunction(UK2Node_Event* SelectedEventNode);
	bool CanConvertEventToFunction() const;

	/**
	* Get all connected nodes and add them to a set of out nodes
	* 
	* @param Node		The node to get all connects to 
	* @return	TArray of all connected nodes
	*/
	TArray<UEdGraphNode*> GetAllConnectedNodes(UEdGraphNode* const Node) const;

	void OnAlignTop();
	void OnAlignMiddle();
	void OnAlignBottom();
	void OnAlignLeft();
	void OnAlignCenter();
	void OnAlignRight();

	void OnStraightenConnections();

	void OnDistributeNodesH();
	void OnDistributeNodesV();

	void SelectAllNodes();
	bool CanSelectAllNodes() const;

	virtual void DeleteSelectedNodes();
	virtual bool CanDeleteNodes() const;

	/**
	* Given a node, make connections from anything connected to it's input pin to
	* anything connected to it's "then" pins. Only works on impure nodes with single exec/then pins.
	* 
	* @param Node	The node to reconnect
	*/
	void ReconnectExecPins(class UK2Node* Node);

	void DeleteSelectedDuplicatableNodes();

	virtual void CutSelectedNodes();
	virtual bool CanCutNodes() const;

	virtual void CopySelectedNodes();
	virtual bool CanCopyNodes() const;

	/** Paste on graph at specific location */
	virtual void PasteNodesHere(class UEdGraph* DestinationGraph, const FVector2D& GraphLocation) override;

	/** Paste Variable Definition or Nodes */
	virtual void PasteGeneric();
	virtual bool CanPasteGeneric() const;

	virtual void PasteNodes();
	virtual bool CanPasteNodes() const override;

	void DuplicateNodes();
	bool CanDuplicateNodes() const;

	void OnSelectReferenceInLevel();
	bool CanSelectReferenceInLevel() const;

	void OnAssignReferencedActor();
	bool CanAssignReferencedActor() const;

	virtual void OnStartWatchingPin();
	virtual bool CanStartWatchingPin() const;

	virtual void OnStopWatchingPin();
	virtual bool CanStopWatchingPin() const;

	void ToggleSaveIntermediateBuildProducts();
	bool GetSaveIntermediateBuildProducts() const;

	void OnListObjectsReferencedByClass();
	void OnListObjectsReferencedByBlueprint();
	void OnRepairCorruptedBlueprint();

	void OnNodeDoubleClicked(UEdGraphNode* Node);

	virtual void OnEditTabClosed(TSharedRef<SDockTab> Tab);

	virtual bool GetBoundsForSelectedNodes(class FSlateRect& Rect, float Padding) override;

	/**
	 * Pulls out the pins to use as a template when collapsing a selection to a function with a custom event involved.
	 *
	 * @param InCustomEvent				The custom event used as a template
	 * @param InGatewayNode				The node replacing the selection of nodes
	 * @param InEntryNode				The entry node in the graph
	 * @param InResultNode				The result node in the graph
	 * @param InCollapsableNodes		The selection of nodes being collapsed
	 */
	void ExtractEventTemplateForFunction(class UK2Node_CustomEvent* InCustomEvent, UEdGraphNode* InGatewayNode, class UK2Node_EditablePinBase* InEntryNode, class UK2Node_EditablePinBase* InResultNode, TSet<UEdGraphNode*>& InCollapsableNodes);

	/**
	 * Collapses a selection of nodes into a graph for composite, function, or macro nodes.
	 *
	 * @param InGatewayNode				The node replacing the selection of nodes
	 * @param InEntryNode				The entry node in the graph
	 * @param InResultNode				The result node in the graph
	 * @param InSourceGraph				The graph the selection is from
	 * @param InDestinationGraph		The destination graph to move the selected nodes to
	 * @param InCollapsableNodes		The selection of nodes being collapsed
	 */
	void CollapseNodesIntoGraph(UEdGraphNode* InGatewayNode, class UK2Node_EditablePinBase* InEntryNode, class UK2Node_EditablePinBase* InResultNode, UEdGraph* InSourceGraph, UEdGraph* InDestinationGraph, TSet<UEdGraphNode*>& InCollapsableNodes, bool bCanDiscardEmptyReturnNode, bool bCanHaveWeakObjPtrParam);

	/** Called when a selection of nodes are being collapsed into a sub-graph */
	void CollapseNodes(TSet<class UEdGraphNode*>& InCollapsableNodes);

	/** Called when a selection of nodes are being collapsed into a function */
	UEdGraph* CollapseSelectionToFunction(TSharedPtr<SGraphEditor> InRootGraph, TSet<class UEdGraphNode*>& InCollapsableNodes, UEdGraphNode*& OutFunctionNode);

	/** Called when a selection of nodes are being collapsed into a macro */
	UEdGraph* CollapseSelectionToMacro(TSharedPtr<SGraphEditor> InRootGraph, TSet<class UEdGraphNode*>& InCollapsableNodes, UEdGraphNode*& OutMacroNode);

	/**
	 * Called when a selection of nodes is being collapsed into a function
	 *
	 * @param InSelection		The selection to check
	 *
	 * @return					Returns TRUE if the selection can be promoted to a function
	 */
	bool CanCollapseSelectionToFunction(TSet<class UEdGraphNode*>& InSelection) const;

	/**
	 * Called when a selection of nodes is being collapsed into a macro
	 *
	 * @param InSelection		The selection to check
	 *
	 * @return					Returns TRUE if the selection can be promoted to a macro
	 */
	bool CanCollapseSelectionToMacro(TSet<class UEdGraphNode*>& InSelection) const;

	/**
	 * Expands passed in node */
	static void ExpandNode(UEdGraphNode* InNodeToExpand, UEdGraph* InSourceGraph, TSet<UEdGraphNode*>& OutExpandedNodes);

	/**
	* Move every node from the source graph to the destination graph. Add Each node that is moved to the OutExpandedNodes set.
	* If the source graph is a function graph, keep track of the entry and result nodes in the given Out Parameters. 
	*
	* @param SourceNodes		Nodes to move
	* @param DestinationGraph	Graph to move nodes to
	* @param OutExpandedNodes	Set of each node that was moved from the source to destination graph
	* @param OutEntry			Pointer to the function entry node
	* @param OutResult			Pointer to the function result node
	* @param bIsCollapsedGraph	Whether or not the source graph is collapsed
	**/
	static void MoveNodesToGraph(TArray<UEdGraphNode*>& SourceNodes, UEdGraph* DestinationGraph, TSet<UEdGraphNode*>& OutExpandedNodes, UEdGraphNode** OutEntry, UEdGraphNode** OutResult, const bool bIsCollapsedGraph = false);

	/** Start editing the defaults for this blueprint */
	void OnStartEditingDefaultsClicked();

	/** Creates the widgets that go into the tabs (note: does not create the tabs themselves) **/
	virtual void CreateDefaultTabContents(const TArray<UBlueprint*>& InBlueprints);

	/** Create Default Commands **/
	virtual void CreateDefaultCommands();

	/** Called when DeleteUnusedVariables button is clicked */
	void DeleteUnusedVariables_OnClicked();

	/** Called when Find In Blueprints menu is opened is clicked */
	void FindInBlueprints_OnClicked();

	//~ Begin FNotifyHook Interface
	virtual void NotifyPreChange( FProperty* PropertyAboutToChange ) override;
	virtual void NotifyPostChange( const FPropertyChangedEvent& PropertyChangedEvent, FProperty* PropertyThatChanged) override;
	//~ End FNotifyHook Interface

	/** Callback to determine visibility of the public view checkbox in the Defaults editor */
	bool ShouldShowPublicViewControl() const;

	/** Callback when properties have finished being handled */
	virtual void OnFinishedChangingProperties(const FPropertyChangedEvent& PropertyChangedEvent);

	/** On starting to rename node */
	void OnRenameNode();
	bool CanRenameNodes() const;

	/** Called when a node's title is being committed for a rename so it can be verified */
	bool OnNodeVerifyTitleCommit(const FText& NewText, UEdGraphNode* NodeBeingChanged, FText& OutErrorMessage);

	/**Load macro & function blueprint libraries from asset registry*/
	void LoadLibrariesFromAssetRegistry();

	//~ Begin FEditorUndoClient Interface
	virtual void	PostUndo(bool bSuccess) override;
	virtual void	PostRedo(bool bSuccess) override;
	// End of FEditorUndoClient

	/** Setup all the events that the graph editor can handle */
	virtual void SetupGraphEditorEvents(UEdGraph* InGraph, SGraphEditor::FGraphEditorEvents& InEvents);

	/** Get the graph appearance of the currently focused graph */
	FGraphAppearanceInfo GetCurrentGraphAppearance() const;

	/** Get the graph appearance of a specific graph, GetCurrentGraphAppearance() uses the currently focused graph. */
	virtual FGraphAppearanceInfo GetGraphAppearance(class UEdGraph* InGraph) const;

	/** Whenever new graphs need to be created it will use this schema by default. */
	virtual TSubclassOf<UEdGraphSchema> GetDefaultSchemaClass() const;

	/** Attempts to invoke the details tab if it's currently possible to. */
	void TryInvokingDetailsTab(bool bFlash = true);

	/** Handles activation of a graph editor "quick jump" command */
	void OnGraphEditorQuickJump(int32 BookmarkIndex);

	/** Binds a graph editor "quick jump" to the command at the given index */
	void SetGraphEditorQuickJump(int32 BookmarkIndex);

	/** Unbinds a graph editor "quick jump" from the command at the given index */
	void ClearGraphEditorQuickJump(int32 BookmarkIndex);

	/** Unbinds all graph editor "quick jump" commands */
	void ClearAllGraphEditorQuickJumps();

	/** Create a graph title bar widget */
	TSharedRef<SWidget> CreateGraphTitleBarWidget(TSharedRef<FTabInfo> InTabInfo, UEdGraph* InGraph);

private:

	/** Returns true if modules can be recompiled */
	static bool CanRecompileModules();

	/* User wants to edit tunnel via function editor */
	void OnEditTunnel();

protected:
	
	/* Create comment node on graph */
	virtual void OnCreateComment();
	
	/** Create custom event node on graph */
	virtual void OnCreateCustomEvent();

	// Create new graph editor widget for the supplied document container
	virtual TSharedRef<SGraphEditor> CreateGraphEditorWidget(TSharedRef<class FTabInfo> InTabInfo, class UEdGraph* InGraph);

private:

	/** Helper to move focused graph when clicking on graph breadcrumb */
	void OnChangeBreadCrumbGraph( class UEdGraph* InGraph);

	/** Function to check whether the give graph is a subgraph */
	static bool IsASubGraph( const class UEdGraph* GraphPtr );

	/** Creates the Subobject Editor tree component view and the Subobject Viewport. */
	void CreateSubobjectEditors();

	/** Callback when a token is clicked on in the compiler results log */
	void OnLogTokenClicked(const TSharedRef<class IMessageToken>& Token);

	/** Helper function to navigate the current tab */
	void NavigateTab(FDocumentTracker::EOpenDocumentCause InCause);

	/** Find all references of the selected node. */
	void OnFindReferences(bool bSearchAllBlueprints, const EGetFindReferenceSearchStringFlags Flags);

	/** Checks if we can currently find all references of the node selection. */
	bool CanFindReferences();

	/** Called when the user generates a warning tooltip because a connection was invalid */
	void OnDisallowedPinConnection(const class UEdGraphPin* PinA, const class UEdGraphPin* PinB);

	/**
	 * Checks to see if the focused Graph-Editor is focused on a animation graph or not.
	 * 
	 * @return True if GraphEdPtr's current graph is an animation graph, false if not.
	 */
	bool IsEditingAnimGraph() const;

	/** Returns whether the currently focused graph is editable or not */
	bool IsFocusedGraphEditable() const;

	/** Called when the user wants to jump to a node's definition */
	void OnGoToDefinition();

	/** Checks to see if it is possible to jump to the selected node's definition. */
	bool CanGoToDefinition() const;

	/** Open documentation for the selected node class */
	void OnGoToDocumentation();

	/** Can we open documentation for the selected node */
	bool CanGoToDocumentation();

	/** Util to try and get doc link for the currently selected node */
	FString GetDocLinkForSelectedNode();

	/** Util to try and get the base URL for the doc link for the currently selected node */
	FString GetDocLinkBaseUrlForSelectedNode();

	/** Set the enabled state for currently-selected nodes */
	void OnSetEnabledStateForSelectedNodes(ENodeEnabledState NewState);

	/** Returns the appropriate check box state representing whether or not the selected nodes are enabled */
	ECheckBoxState GetEnabledCheckBoxStateForSelectedNodes();

	/**
	 * Load editor settings from disk (docking state, window pos/size, option state, etc).
	 */
	virtual void LoadEditorSettings();

	/**
	 * Saves editor settings to disk (docking state, window pos/size, option state, etc).
	 */
	virtual void SaveEditorSettings();

	/** Attempt to match the given enabled state for currently-selected nodes */
	ECheckBoxState CheckEnabledStateForSelectedNodes(ENodeEnabledState CheckState);

	/** Handle undo/redo */
	void HandleUndoTransaction(const class FTransaction* Transaction);

public://@TODO

	virtual bool TransactionObjectAffectsBlueprint(UObject* InTransactedObject);

	TSharedPtr<FDocumentTracker> DocumentManager;
	
	/** Update all nodes' unrelated states when the graph has changed */
	void UpdateNodesUnrelatedStatesAfterGraphChange();

	/** Whether event graphs are allowed to be displayed/created in a blueprint */
	virtual bool AreEventGraphsAllowed() const;

	/** Whether macros are allowed  to be displayed/created in a blueprint */
	virtual bool AreMacrosAllowed() const;

	/** Whether delegates are allowed  to be displayed/created in a blueprint */
	virtual bool AreDelegatesAllowed() const;

protected:

	/** Should intermediate build products be saved when recompiling? */
	bool bSaveIntermediateBuildProducts;

	/** True if the current blueprint is in the process of being reparented */
	bool bIsReparentingBlueprint;

	/** Flags if this blueprint editor should close on its next tick. */
	bool bPendingDeferredClose;

	/** Currently focused graph editor */
	TWeakPtr<class SGraphEditor> FocusedGraphEdPtr;
	
	/** Factory that spawns graph editors; used to look up all tabs spawned by it. */
	TWeakPtr<FDocumentTabFactory> GraphEditorTabFactoryPtr;

	/** User-defined enumerators to keep loaded */
	TSet<TWeakObjectPtr<UUserDefinedEnum>> UserDefinedEnumerators;

	/** User-defined structures to keep loaded */
	TSet<TWeakObjectPtr<UUserDefinedStruct>> UserDefinedStructures;
	
	/** Macro/function libraries to keep loaded */
	TArray<TObjectPtr<UBlueprint>> StandardLibraries;

	/** Subobject Editor */
	TSharedPtr<SSubobjectEditor> SubobjectEditor;

	/** Viewport widget */
	TSharedPtr<SSCSEditorViewport> SubobjectViewport;

	/** Node inspector widget */
	TSharedPtr<class SKismetInspector> Inspector;

	/** defaults inspector widget */
	TSharedPtr<class SKismetInspector> DefaultEditor;

	/** Palette of all classes with funcs/vars */
	TSharedPtr<class SBlueprintPalette> Palette;

	/** Blueprint bookmark editing widget */
	TSharedPtr<class SBlueprintBookmarks> BookmarksWidget;

	/** All of this blueprints' functions and variables */
	TSharedPtr<class SMyBlueprint> MyBlueprintWidget;
	
	/** Widget for replacing node references */
	TSharedPtr<class SReplaceNodeReferences> ReplaceReferencesWidget;

	/** Compiler results log, with the log listing that it reflects */
	TSharedPtr<class SWidget> CompilerResults;
	TSharedPtr<class IMessageLogListing> CompilerResultsListing;
	
	/** Limited cache of last compile results */
	int32 CachedNumWarnings;
	int32 CachedNumErrors;
	
	/** Find results log as well as the search filter */
	TSharedPtr<class SFindInBlueprints> FindResults;

	/** Merge tool - WeakPtr because it's owned by the GlobalTabManager */
	TWeakPtr<class SDockTab> MergeTool;
	/** Merge tool - Delegate to call when the merge tool is closed. */
	FOnMergeResolved OnMergeResolved;

	/** Reference to owner of the current popup */
	TWeakPtr<class SWindow> NameEntryPopupWindow;

	/** Reference to helper object to validate names in popup */
	TSharedPtr<class INameValidatorInterface> NameEntryValidator;

	/** Reference to owner of the pin type change popup */
	TWeakPtr<class IMenu> PinTypeChangeMenu;

	/** The toolbar builder class */
	TSharedPtr<class FBlueprintEditorToolbar> Toolbar;

	/** Imported namespace helper utility object */
	TSharedPtr<FBlueprintNamespaceHelper> ImportedNamespaceHelper;

	/** Filter used to restrict class viewer widgets in the editor context to imported namespaces only */
	TSharedPtr<class IClassViewerFilter> ImportedClassViewerFilter;

	/** Filter used to restrict pin type selector widgets in the editor context to imported namespaces only */
	TSharedPtr<class IPinTypeSelectorFilter> ImportedPinTypeSelectorFilter;

	/** Filter used to restrict pin type selector widgets to those which permissions allow */
	TSharedPtr<class IPinTypeSelectorFilter> PermissionsPinTypeSelectorFilter;

	/** Filter used to compose multiple pin type filters together */
	TSharedPtr<class IPinTypeSelectorFilter> CompositePinTypeSelectorFilter;

	/** Pending set of namespaces to be auto-imported on the next tick */
	TSet<FString> DeferredNamespaceImports;
	
	FOnSetPinVisibility OnSetPinVisibility;

	/** Has someone requested a deferred update of the saved document state? */
	bool bRequestedSavingOpenDocumentState;

	/** Did we update the blueprint when it opened */
	bool bBlueprintModifiedOnOpen;

	/** Whether to hide unused pins or not */
	SGraphEditor::EPinVisibility PinVisibility;

	/** Whether the graph action menu should be sensitive to the pins dragged off of */
	bool bIsActionMenuContextSensitive;
	
	/** The current UI selection state of this editor */
	FName CurrentUISelection;

	/** Whether we are already in the process of closing this editor */
	bool bEditorMarkedAsClosed;

	/** Blueprint preview scene */
	FPreviewScene PreviewScene;

	/** The preview actor representing the current preview */
	mutable TWeakObjectPtr<AActor> PreviewActorPtr;

	/** If true, fade out nodes which are unrelated to the selected nodes automatically. */
	bool bHideUnrelatedNodes;

	/** Lock the current fade state of each node */
	bool bLockNodeFadeState;

	/** If a regular node (not a comment node) has been selected */
	bool bSelectRegularNode;

	/** True if the editor was opened in defaults mode */
	bool bWasOpenedInDefaultsMode;

	/** Focus nodes which are related to the selected nodes */
	void ResetAllNodesUnrelatedStates();
	void CollectExecUpstreamNodes(UEdGraphNode* CurrentNode, TArray<UEdGraphNode*>& CollectedNodes);
	void CollectExecDownstreamNodes(UEdGraphNode* CurrentNode, TArray<UEdGraphNode*>& CollectedNodes);
	void CollectPureDownstreamNodes(UEdGraphNode* CurrentNode, TArray<UEdGraphNode*>& CollectedNodes);
	void CollectPureUpstreamNodes(UEdGraphNode* CurrentNode, TArray<UEdGraphNode*>& CollectedNodes);
	void HideUnrelatedNodes();

	/** Register Menus */
	void RegisterMenus();

public:
	/** Make nodes which are unrelated to the selected nodes fade out */
	void ToggleHideUnrelatedNodes();
	bool IsToggleHideUnrelatedNodesChecked() const;
	
	UE_DEPRECATED(5.0, "The Toggle Hide Unrelated Nodes button is always shown now")
	bool ShouldShowToggleHideUnrelatedNodes(bool bIsToolbar) const;

	/** Make a drop down menu to control the opacity of unrelated nodes */
	TSharedRef<SWidget> MakeHideUnrelatedNodesOptionsMenu();
	TOptional<float> HandleUnrelatedNodesOpacityBoxValue() const;
	void HandleUnrelatedNodesOpacityBoxChanged(float NewOpacity);
	void OnLockNodeStateCheckStateChanged(ECheckBoxState NewCheckedState);


public:
	//@TODO: To be moved/merged
	TSharedPtr<SDockTab> OpenDocument(const UObject* DocumentID, FDocumentTracker::EOpenDocumentCause Cause);

	/** Finds the tab associated with the specified asset, and closes if it is open */
	void CloseDocumentTab(const UObject* DocumentID);

	// Finds any open tabs containing the specified document and adds them to the specified array; returns true if at least one is found
	bool FindOpenTabsContainingDocument(const UObject* DocumentID, /*inout*/ TArray< TSharedPtr<SDockTab> >& Results);

public:
	/** Broadcasts a notification whenever the editor needs associated controls to refresh */
	DECLARE_EVENT ( FBlueprintEditor, FOnRefreshEvent );
	FOnRefreshEvent& OnRefresh() { return RefreshEvent; }

	/** function used by the SMyBlueprint to determine if an action matches a name.
	  * This happens during selection of a custom action ( a graph, a variable etc )
	  */
	virtual bool OnActionMatchesName(FEdGraphSchemaAction* InAction, const FName& InName) const { return false; }

private:
	/** Notification used whenever the editor wants associated controls to refresh. */
	FOnRefreshEvent RefreshEvent;

	/** Broadcast notification for associated controls to update */
	void BroadcastRefresh() { RefreshEvent.Broadcast(); }

	/** Command list for the graph editor */
	TSharedPtr<FUICommandList> GraphEditorCommands;

	/** Structure to contain editor usage analytics */
	struct FAnalyticsStatistics
	{
		/** Stats collected about graph action menu usage */
		int32 GraphActionMenusNonCtxtSensitiveExecCount;
		int32 GraphActionMenusCtxtSensitiveExecCount;
		int32 GraphActionMenusCancelledCount;

		/** Stats collection about user node creation actions */
		int32 MyBlueprintNodeDragPlacementCount;
		int32 PaletteNodeDragPlacementCount;
		int32 NodeGraphContextCreateCount;
		int32 NodePinContextCreateCount;
		int32 NodeKeymapCreateCount;
		int32 NodePasteCreateCount;

		/** New node instance information */
		struct FNodeDetails
		{
			FName NodeClass;
			int32 Instances;
		};

		/** Stats collected about warning tooltips */
		TArray<FDisallowedPinConnection> GraphDisallowedPinConnections;
	};

	/** analytics statistics for the Editor */ 
	FAnalyticsStatistics AnalyticsStats;

	/** Customizations for the Subobject editor */
	TMap<FName, TSharedPtr<ISCSEditorCustomization>> SubobjectEditorCustomizations;

	/** Whether the current project is C++ or blueprint based */
	bool bCodeBasedProject;

	/** Delegates that are fired when the blueprint editor changes modes */
	FOnModeSet OnModeSetData;

	/** When set, flags which graph has a action menu currently open (if null, no graphs do). */
	UEdGraph* HasOpenActionMenu;
	/** Used to nicely fade instruction text, when the context menu is opened. */
	float InstructionsFadeCountdown;

	/** Handle to the registered OnActiveTabChanged delegate */
	FDelegateHandle OnActiveTabChangedDelegateHandle;

	/** Delegate handle registered for when settings change */
	FDelegateHandle BlueprintEditorSettingsChangedHandle;
	FDelegateHandle BlueprintProjectSettingsChangedHandle;

	enum class ESafeToModifyDuringPIEStatus
	{
		Unknown,
		Safe,
		NotSafe
	};

	mutable ESafeToModifyDuringPIEStatus ModifyDuringPIEStatus = ESafeToModifyDuringPIEStatus::Unknown;

	// Allow derived editors to add command mappings 
	virtual void OnCreateGraphEditorCommands(TSharedPtr<FUICommandList> GraphEditorCommandsList) {}

	virtual void OnBlueprintProjectSettingsChanged(UObject*, struct FPropertyChangedEvent&);
	virtual void OnBlueprintEditorPreferencesChanged(UObject*, struct FPropertyChangedEvent&);
};

#undef LOCTEXT_NAMESPACE
