// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BlueprintEditor.h"
#include "Containers/Array.h"
#include "Containers/Set.h"
#include "CoreMinimal.h"
#include "CoreTypes.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2_Actions.h"
#include "Framework/Commands/Commands.h"
#include "Input/Reply.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "Layout/Visibility.h"
#include "SGraphActionMenu.h"
#include "Styling/AppStyle.h"
#include "Templates/SharedPointer.h"
#include "Types/SlateEnums.h"
#include "UObject/NameTypes.h"
#include "UObject/UnrealNames.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SWidget.h"
#include "WorkflowOrientedApp/WorkflowTabManager.h"

class FBlueprintEditor;
class FMenuBuilder;
class FUICommandInfo;
class FUICommandList;
class SComboButton;
class SKismetInspector;
class SSearchBox;
class SWidget;
class UBlueprint;
class UEdGraph;
class UFunction;
class UObject;
class UUserDefinedEnum;
class UUserDefinedStruct;
struct FComponentEventConstructionData;
struct FEdGraphSchemaAction;
struct FEdGraphSchemaAction_K2Struct;
struct FGeometry;
struct FGraphActionListBuilderBase;
struct FGraphActionNode;
struct FGraphActionSort;
struct FKeyEvent;
struct FPointerEvent;
struct FPropertyChangedEvent;
struct FReplaceNodeReferencesHelper;

class FMyBlueprintCommands : public TCommands<FMyBlueprintCommands>
{
public:
	/** Constructor */
	FMyBlueprintCommands() 
		: TCommands<FMyBlueprintCommands>(TEXT("MyBlueprint"), NSLOCTEXT("Contexts", "My Blueprint", "My Blueprint"), NAME_None, FAppStyle::GetAppStyleSetName())
	{
	}

	// Basic operations
	TSharedPtr<FUICommandInfo> OpenGraph;
	TSharedPtr<FUICommandInfo> OpenGraphInNewTab;
	TSharedPtr<FUICommandInfo> OpenExternalGraph;
	TSharedPtr<FUICommandInfo> FocusNode;
	TSharedPtr<FUICommandInfo> FocusNodeInNewTab;
	TSharedPtr<FUICommandInfo> ImplementFunction;
	TSharedPtr<FUICommandInfo> DeleteEntry;
	TSharedPtr<FUICommandInfo> PasteVariable;
	TSharedPtr<FUICommandInfo> PasteLocalVariable;
	TSharedPtr<FUICommandInfo> PasteFunction;
	TSharedPtr<FUICommandInfo> PasteMacro;
	TSharedPtr<FUICommandInfo> GotoNativeVarDefinition;
	TSharedPtr<FUICommandInfo> MoveVariableToParent;
	TSharedPtr<FUICommandInfo> MoveFunctionToParent;
	// Add New Item
	/** Initialize commands */
	virtual void RegisterCommands() override;
};

class KISMET_API SMyBlueprint : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SMyBlueprint ) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TWeakPtr<FBlueprintEditor> InBlueprintEditor, const UBlueprint* InBlueprint = nullptr);
	~SMyBlueprint();

	void SetInspector( TSharedPtr<SKismetInspector> InInspector ) { Inspector = InInspector ; }

	/* SWidget interface */
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime);
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent);

	/* Reset the last pin type settings to default. */
	void ResetLastPinType();

	/** Refreshes the graph action menu */
	void Refresh();
	void SetFocusedGraph(UEdGraph* InEdGraph) { EdGraph = InEdGraph; }
	
	/** Accessor for getting the current selection as a K2 graph */
	FEdGraphSchemaAction_K2Graph* SelectionAsGraph() const;

	/** Accessor for getting the current selection as a K2 enum */
	FEdGraphSchemaAction_K2Enum* SelectionAsEnum() const;

	/** Accessor for getting the current selection as a K2 enum */
	FEdGraphSchemaAction_K2Struct* SelectionAsStruct() const;

	/** Accessor for getting the current selection as a K2 var */
	FEdGraphSchemaAction_K2Var* SelectionAsVar() const;
	
	/** Accessor for getting the current selection as a K2 delegate */
	FEdGraphSchemaAction_K2Delegate* SelectionAsDelegate() const;

	/** Accessor for getting the current selection as a K2 event */
	FEdGraphSchemaAction_K2Event* SelectionAsEvent() const;
	
	/** Accessor for getting the current selection as a K2 Input Action */
	FEdGraphSchemaAction_K2InputAction* SelectionAsInputAction() const;

	/** Accessor for getting the current selection as a K2 local var */
	FEdGraphSchemaAction_K2LocalVar* SelectionAsLocalVar() const;

	/** Accessor for getting the current selection as a K2 blueprint base variable */
	FEdGraphSchemaAction_BlueprintVariableBase* SelectionAsBlueprintVariable() const;

	/** Accessor for determining if the current selection is a category*/
	bool SelectionIsCategory() const;
	
	void EnsureLastPinTypeValid();

	/** Gets the last pin type selected by this widget, or by the function editor */
	FEdGraphPinType& GetLastPinTypeUsed() {EnsureLastPinTypeValid(); return LastPinType;}
	FEdGraphPinType& GetLastFunctionPinTypeUsed() {EnsureLastPinTypeValid(); return LastFunctionPinType;}

	/** Accessor the blueprint object from the main editor */
	UBlueprint* GetBlueprintObj() const {return Blueprint;}

	/** Gets whether we are showing user variables only or not */
	bool ShowUserVarsOnly() const { return !IsShowingInheritedVariables(); }

	/** Gets our parent blueprint editor */
	TWeakPtr<FBlueprintEditor> GetBlueprintEditor() {return BlueprintEditorPtr;}

	/**
	 * Fills the supplied array with the currently selected objects
	 * @param OutSelectedItems The array to fill.
	 */
	void GetSelectedItemsForContextMenu(TArray<FComponentEventConstructionData>& OutSelectedItems) const;

	/** Called to reset the search filter */
	void OnResetItemFilter();

	/** Selects an item by name in either the main graph action menu or the local one */
	void SelectItemByName(const FName& ItemName, ESelectInfo::Type SelectInfo = ESelectInfo::Direct, int32 SectionId = INDEX_NONE, bool bIsCategory = false);

	/** Clears the selection in the graph action menus */
	void ClearGraphActionMenuSelection();

	/** Initiates a rename on the selected action node, if possible */
	void OnRequestRenameOnActionNode();

	/** Expands any category with the associated name */
	void ExpandCategory(const FText& CategoryName);

	/** Move the category before the target category */
	bool MoveCategoryBeforeCategory( const FText& CategoryToMove, const FText& TargetCategory );
	
	/** Callbacks for Paste Commands */
	void OnPasteGeneric();
	bool CanPasteGeneric();
private:
	/** Creates widgets for the graph schema actions */
	TSharedRef<SWidget> OnCreateWidgetForAction(struct FCreateWidgetForActionData* const InCreateData);

	/** Callback used to populate all actions list in SGraphActionMenu */
	void CollectAllActions(FGraphActionListBuilderBase& OutAllActions);
	void CollectStaticSections(TArray<int32>& StaticSectionIDs);
	void AddEventForFunctionGraph(UEdGraph* EdGraph, int32 const SectionId, FGraphActionSort& SortList, const FText& ParentCategory = FText::GetEmpty(), bool bAddChildGraphs = true) const;
	void GetChildGraphs(UEdGraph* EdGraph, int32 const SectionId, FGraphActionSort& SortList, const FText& ParentCategory = FText::GetEmpty()) const;
	void GetChildEvents(UEdGraph const* EdGraph, int32 const SectionId, FGraphActionSort& SortList, const FText& ParentCategory = FText::GetEmpty(), bool bInAddChildGraphs = true) const;
	void GetLocalVariables(FGraphActionSort& SortList) const;
	
	/** Handles the visibility of the local action list */
	EVisibility GetLocalActionsListVisibility() const;

	/** Callbacks for the graph action menu */
	FReply OnActionDragged(const TArray< TSharedPtr<FEdGraphSchemaAction> >& InActions, const FPointerEvent& MouseEvent);
	FReply OnCategoryDragged(const FText& InCategory, const FPointerEvent& MouseEvent);
	void OnActionSelected(const TArray< TSharedPtr<FEdGraphSchemaAction> >& InActions);
	void OnActionSelectedHelper(TSharedPtr<FEdGraphSchemaAction> InAction, TWeakPtr< FBlueprintEditor > InBlueprintEditor, UBlueprint* Blueprint, TSharedRef<SKismetInspector> Inspector);
	void OnGlobalActionSelected(const TArray< TSharedPtr<FEdGraphSchemaAction> >& InActions, ESelectInfo::Type InSelectionType);
	void OnActionDoubleClicked(const TArray< TSharedPtr<FEdGraphSchemaAction> >& InActions);
	void ExecuteAction(TSharedPtr<FEdGraphSchemaAction> InAction);
	TSharedPtr<SWidget> OnContextMenuOpening();

	TSharedRef<SWidget> CreateAddNewMenuWidget();
	void BuildAddNewMenu(FMenuBuilder& MenuBuilder);
	TSharedRef<SWidget> CreateAddToSectionButton(int32 InSectionID, TWeakPtr<SWidget> WeakRowWidget, FText AddNewText, FName MetaDataTag);

	void OnCategoryNameCommitted(const FText& InNewText, ETextCommit::Type InTextCommit, TWeakPtr< struct FGraphActionNode > InAction );
	bool CanRequestRenameOnActionNode(TWeakPtr<struct FGraphActionNode> InSelectedNode) const;
	FText OnGetSectionTitle( int32 InSectionID );
	TSharedRef<SWidget> OnGetSectionWidget( TSharedRef<SWidget> RowWidget, int32 InSectionID );
	EVisibility OnGetSectionTextVisibility(TWeakPtr<SWidget> RowWidget, int32 InSectionID) const;
	TSharedRef<SWidget> OnGetFunctionListMenu();
	void BuildOverridableFunctionsMenu(FMenuBuilder& MenuBuilder);
	FReply OnAddButtonClickedOnSection(int32 InSectionID);
	bool CanAddNewElementToSection(int32 InSectionID) const;

	bool HandleActionMatchesName(FEdGraphSchemaAction* InAction, const FName& InName) const;

	/** Support functions for checkbox to manage displaying user variables only */
	bool IsShowingInheritedVariables() const;
	void OnToggleShowInheritedVariables();

	/** Support functions for view options for Show Empty Sections */
	void OnToggleShowEmptySections();
	bool IsShowingEmptySections() const;
	
	/** Support functions for view options for Show Replicated Variables only */
	void OnToggleShowReplicatedVariablesOnly();
	bool IsShowingReplicatedVariablesOnly() const;

	/** Support functions for view options for bAlwaysShowInterfacesInOverrides blueprint editor setting */
	void OnToggleAlwaysShowInterfacesInOverrides();
	bool GetAlwaysShowInterfacesInOverrides() const;

	/** Support functions for view options for bShowParentClassInOverrides blueprint editor setting */
	void OnToggleShowParentClassInOverrides();
	bool GetShowParentClassInOverrides() const;

	/** Support functions for view options for bShowAccessSpecifier blueprint editor setting */
	void OnToggleShowAccessSpecifier();
	bool GetShowAccessSpecifier() const;

	/** Helper function to open the selected graph */
	void OpenGraph(FDocumentTracker::EOpenDocumentCause InCause, bool bOpenExternalGraphInNewEditor = false);

	/**
	* Check if the override of a given function is most likely desired as a blueprint function 
	* or as an event. 
	* 
	* @param OverrideFunc	Desired function to override
	* 
	* @return	True if the function is desired as a function, false if desired as an event
	*/
	bool IsImplementationDesiredAsFunction(const UFunction* OverrideFunc) const;

	/** Callbacks for commands */
	void OnOpenGraph();
	void OnOpenGraphInNewTab();
	void OnOpenExternalGraph();
	bool CanOpenGraph() const;
	bool CanOpenExternalGraph() const;
	bool CanFocusOnNode() const;
	void OnFocusNode();
	void OnFocusNodeInNewTab();
	void OnImplementFunction();
	void ImplementFunction(TSharedPtr<FEdGraphSchemaAction_K2Graph> GraphAction);
	void ImplementFunction(FEdGraphSchemaAction_K2Graph* GraphAction);
	bool CanImplementFunction() const;
	void OnFindReference(bool bSearchAllBlueprints, const EGetFindReferenceSearchStringFlags Flags);
	bool CanFindReference() const;
	void OnFindAndReplaceReference();
	bool CanFindAndReplaceReference() const;
	void OnDeleteEntry();
	bool CanDeleteEntry() const;
	FReply OnAddNewLocalVariable();
	bool CanRequestRenameOnActionNode() const;
	bool IsDuplicateActionVisible() const;
	bool CanDuplicateAction() const;
	void OnDuplicateAction();
	void GotoNativeCodeVarDefinition();
	bool IsNativeVariable() const;
	void OnMoveToParent();
	bool CanMoveVariableToParent() const;
	bool CanMoveFunctionToParent() const;
	void OnCopy();
	bool CanCopy() const;
	void OnCut();
	bool CanCut() const;
	void OnPasteVariable();
	void OnPasteLocalVariable();
	bool CanPasteVariable() const;
	bool CanPasteLocalVariable() const;
	void OnPasteFunction();
	bool CanPasteFunction() const;
	void OnPasteMacro();
	bool CanPasteMacro() const;

	/** Gets the currently selected Category or returns default category name */
	FText GetPasteCategory() const;

	/** Callback when the filter is changed, forces the action tree(s) to filter */
	void OnFilterTextChanged( const FText& InFilterText );

	/** Callback for the action trees to get the filter text */
	FText GetFilterText() const;

	/** Checks if the selected action has context menu */
	bool SelectionHasContextMenu() const;

	/** Update Node Create Analytic */
	void UpdateNodeCreation();

	/** Returns the displayed category, if any, of a graph */
	FText GetGraphCategory(UEdGraph* InGraph) const;

	/** Helper function to delete a graph in the MyBlueprint window */
	void OnDeleteGraph(UEdGraph* InGraph, EEdGraphSchemaAction_K2Graph::Type);

	/** Helper function to delete a delegate in the MyBlueprint window */
	void OnDeleteDelegate(FEdGraphSchemaAction_K2Delegate* InDelegateAction);

	UEdGraph* GetFocusedGraph() const;

	/** Delegate to hook us into non-structural Blueprint object post-change events */
	void OnObjectPropertyChanged(UObject* InObject, FPropertyChangedEvent& InPropertyChangedEvent);

	/** Helper function indicating whehter we're in editing mode, and can modify the target blueprint */
	bool IsEditingMode() const;

	/** Determine whether an FEdGraphSchemaAction is associated with an event */
	static bool IsAnInterfaceEvent(FEdGraphSchemaAction_K2Graph* InAction);

private:
	/** List of UI Commands for this scope */
	TSharedPtr<FUICommandList> CommandList;

	/** Pointer back to the blueprint editor that owns us */
	TWeakPtr<FBlueprintEditor> BlueprintEditorPtr;
	
	/** Graph Action Menu for displaying all our variables and functions */
	TSharedPtr<class SGraphActionMenu> GraphActionMenu;

	/** The +Function button in the function section */
	TSharedPtr<SComboButton> FunctionSectionButton;

	/** When we rebuild the view of members, we cache (but don't display) any overridable functions for user in popup menus. */
	TArray< TSharedPtr<FEdGraphSchemaAction_K2Graph> > OverridableFunctionActions;

	/** When we refresh the list of functions we cache off the implemented ones to ask questions for overridable functions. */
	TSet<FName> ImplementedFunctionCache;

	/** The last pin type used (including the function editor last pin type) */
	FEdGraphPinType LastPinType;
	FEdGraphPinType LastFunctionPinType;

	/** Enums created from 'blueprint' level */
	TArray<TWeakObjectPtr<UUserDefinedEnum>> EnumsAddedToBlueprint;

	/** The filter box that handles filtering for both graph action menus. */
	TSharedPtr< SSearchBox > FilterBox;

	/** Enums created from 'blueprint' level */
	TArray<TWeakObjectPtr<UUserDefinedStruct>> StructsAddedToBlueprint;

	/** The blueprint being displayed: */
	UBlueprint* Blueprint;

	/** The Ed Graph being displayed: */
	UEdGraph* EdGraph;

	/** The Kismet Inspector used to display properties: */
	TWeakPtr<SKismetInspector> Inspector;

	/** Flag to indicate whether or not we need to refresh the panel */
	bool bNeedsRefresh;

	/** If set we'll show only replicated variables (local to a particular blueprint view). */
	bool bShowReplicatedVariablesOnly;
};
