// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IControlRigEditor.h"
#include "Editor/ControlRigEditorEditMode.h"
#include "AssetEditorModeManager.h"
#include "DragAndDrop/GraphNodeDragDropOp.h"
#include "ControlRigDefines.h"
#include "Units/RigUnitContext.h"
#include "ControlRigLog.h"
#include "IPersonaViewport.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "RigVMModel/RigVMGraph.h"
#include "RigVMModel/Nodes/RigVMLibraryNode.h"
#include "RigVMCore/RigVM.h"
#include "Graph/SControlRigGraphPinNameListValueWidget.h"
#include "Styling/SlateTypes.h"
#include "AnimPreviewInstance.h"
#include "ScopedTransaction.h"
#include "Graph/ControlRigGraphNode.h"
#include "RigVMModel/RigVMController.h"
#include "Editor/DetailsViewWrapperObject.h"

class UControlRigBlueprint;
class IPersonaToolkit;
class SWidget;
class SBorder;
class USkeletalMesh;
class FStructOnScope;
class UToolMenu;
class FControlRigEditor;

DECLARE_MULTICAST_DELEGATE_TwoParams(FControlRigEditorClosed, const FControlRigEditor*, UControlRigBlueprint*);

struct FControlRigEditorModes
{
	// Mode constants
	static const FName ControlRigEditorMode;
	static FText GetLocalizedMode(const FName InMode)
	{
		static TMap< FName, FText > LocModes;

		if (LocModes.Num() == 0)
		{
			LocModes.Add(ControlRigEditorMode, NSLOCTEXT("ControlRigEditorModes", "ControlRigEditorMode", "Rigging"));
		}

		check(InMode != NAME_None);
		const FText* OutDesc = LocModes.Find(InMode);
		check(OutDesc);
		return *OutDesc;
	}
private:
	FControlRigEditorModes() {}
};

class FControlRigEditor : public IControlRigEditor
{
public:
	/**
	 * Edits the specified character asset(s)
	 *
	 * @param	Mode					Mode that this editor should operate in
	 * @param	InitToolkitHost			When Mode is WorldCentric, this is the level editor instance to spawn this editor within
	 * @param	InControlRigBlueprint	The blueprint object to start editing.
	 */
	void InitControlRigEditor(const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, class UControlRigBlueprint* InControlRigBlueprint);

public:
	FControlRigEditor();
	virtual ~FControlRigEditor();

	// FBlueprintEditor interface
	virtual UBlueprint* GetBlueprintObj() const override;
	virtual TSubclassOf<UEdGraphSchema> GetDefaultSchemaClass() const override;

	int32 GetRigHierarchyTabCount() const { return RigHierarchyTabCount; }

	FControlRigEditorClosed& OnControlRigEditorClosed() { return ControlRigEditorClosedDelegate; }

private:

	FControlRigEditorClosed ControlRigEditorClosedDelegate;

	static bool bAreFunctionReferencesInitialized;
	static void InitFunctionReferences();

public:
	// IToolkit Interface
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FText GetToolkitToolTipText() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;	
	virtual FString GetDocumentationLink() const override
	{
		return FString(TEXT("Engine/Animation/ControlRig"));
	}
	virtual void InitToolMenuContext(FToolMenuContext& MenuContext) override;

	// BlueprintEditor interface
	virtual bool TransactionObjectAffectsBlueprint(UObject* InTransactedObject) override;
	virtual bool CanAddNewLocalVariable() const override;
	virtual void OnAddNewLocalVariable() override;
	virtual void OnPasteNewLocalVariable(const FBPVariableDescription& VariableDescription) override;
	
	virtual void DeleteSelectedNodes() override;
	virtual bool CanDeleteNodes() const override;

	virtual void CopySelectedNodes() override;
	virtual bool CanCopyNodes() const override;

	virtual void PasteNodes() override;
	virtual bool CanPasteNodes() const override;

	virtual FReply OnSpawnGraphNodeByShortcut(FInputChord InChord, const FVector2D& InPosition, UEdGraph* InGraph) override;

	// Control Rig BP does not use regular BP function libraries, no need to load them.
	virtual bool ShouldLoadBPLibrariesFromAssetRegistry() override { return false; }

	// FEditorUndoClient Interface
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override;

	virtual void PostTransaction(bool bSuccess, const FTransaction* Transaction, bool bIsRedo);

	virtual void JumpToHyperlink(const UObject* ObjectReference, bool bRequestRename = false) override;

	void EnsureValidRigElementsInDetailPanel();

	virtual void OnStartWatchingPin();
	virtual bool CanStartWatchingPin() const;

	virtual void OnStopWatchingPin();
	virtual bool CanStopWatchingPin() const;

	// IToolkitHost Interface
	virtual void OnToolkitHostingStarted(const TSharedRef<class IToolkit>& Toolkit) override;
	virtual void OnToolkitHostingFinished(const TSharedRef<class IToolkit>& Toolkit) override;

	//  FTickableEditorObject Interface
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	// Gets the Control Rig Blueprint being edited/viewed
	UControlRigBlueprint* GetControlRigBlueprint() const;

	// returns the hierarchy being debugged
	URigHierarchy* GetHierarchyBeingDebugged() const;

	void SetDetailObjects(const TArray<UObject*>& InObjects);
	void SetDetailObjects(const TArray<UObject*>& InObjects, bool bChangeUISelectionState);
	void SetDetailViewForRigElements();
	void SetDetailViewForGraph(URigVMGraph* InGraph);
	void SetDetailViewForFocusedGraph();
	void SetDetailViewForLocalVariable();
	void RefreshDetailView();
	bool DetailViewShowsAnyRigElement() const;
	bool DetailViewShowsAnyRigUnit() const;
	bool DetailViewShowsLocalVariable() const;
	bool DetailViewShowsStruct(UScriptStruct* InStruct) const;
	bool DetailViewShowsRigElement(FRigElementKey InKey) const;

	void ClearDetailObject(bool bChangeUISelectionState = true);

	/** Get the persona toolkit */
	TSharedRef<IPersonaToolkit> GetPersonaToolkit() const { return PersonaToolkit.ToSharedRef(); }

	/** Get the toolbox hosting widget */
	TSharedRef<SBorder> GetToolbox() { return Toolbox.ToSharedRef(); }

	/** Get the edit mode */
	FControlRigEditorEditMode* GetEditMode() const;

	// this changes everytime you compile, so don't cache it expecting it will last. 
	UControlRig* GetInstanceRig() const { return ControlRig;  }

	void OnCurveContainerChanged();

	void OnHierarchyModified(ERigHierarchyNotification InNotif, URigHierarchy* InHierarchy, const FRigBaseElement* InElement);
	void OnHierarchyModified_AnyThread(ERigHierarchyNotification InNotif, URigHierarchy* InHierarchy, const FRigBaseElement* InElement);

	const FName RigHierarchyToGraphDragAndDropMenuName = TEXT("ControlRigEditor.RigHierarchyToGraphDragAndDropMenu");
	void CreateRigHierarchyToGraphDragAndDropMenu() const;
	void OnGraphNodeDropToPerform(TSharedPtr<FGraphNodeDragDropOp> InDragDropOp, UEdGraph* InGraph, const FVector2D& InNodePosition, const FVector2D& InScreenPosition);

	FPersonaViewportKeyDownDelegate& GetKeyDownDelegate() { return OnKeyDownDelegate; }
	FOnGetContextMenu& OnGetViewportContextMenu() { return OnGetViewportContextMenuDelegate; }
	FNewMenuCommandsDelegate& OnViewportContextMenuCommands() { return OnViewportContextMenuCommandsDelegate; }

	DECLARE_EVENT_OneParam(FControlRigEditor, FPreviewControlRigUpdated, FControlRigEditor*);
	FPreviewControlRigUpdated& OnPreviewControlRigUpdated() { return PreviewControlRigUpdated;  }

	virtual bool SelectLocalVariable(const UEdGraph* Graph, const FName& VariableName) override;

private:

	virtual void OnCreateComment() override;

protected:

	void OnHierarchyChanged();
	void OnControlsSettingsChanged();

	void SynchronizeViewportBoneSelection();
	

	// update the cached modification value
	void UpdateBoneModification(FName BoneName, const FTransform& Transform);

	// remove a single bone modification across all instance
	void RemoveBoneModification(FName BoneName);

	// reset all bone modification across all instance
	void ResetAllBoneModification();

	// FBlueprintEditor Interface
	virtual void CreateDefaultCommands() override;
	virtual void OnCreateGraphEditorCommands(TSharedPtr<FUICommandList> GraphEditorCommandsList);
	virtual TSharedRef<SGraphEditor> CreateGraphEditorWidget(TSharedRef<FTabInfo> InTabInfo, UEdGraph* InGraph) override;
	virtual void Compile() override;
	virtual void SaveAsset_Execute() override;
	virtual void SaveAssetAs_Execute() override;
	virtual bool IsInAScriptingMode() const override { return true; }
	virtual void CreateDefaultTabContents(const TArray<UBlueprint*>& InBlueprints) override;
	virtual void NewDocument_OnClicked(ECreatedDocumentType GraphType) override;
	virtual bool IsSectionVisible(NodeSectionID::Type InSectionID) const override;
	virtual FGraphAppearanceInfo GetGraphAppearance(class UEdGraph* InGraph) const override;
	virtual bool IsEditable(UEdGraph* InGraph) const override;
	virtual bool IsCompilingEnabled() const override;
	virtual FText GetGraphDecorationString(UEdGraph* InGraph) const override;
	virtual void OnActiveTabChanged( TSharedPtr<SDockTab> PreviouslyActive, TSharedPtr<SDockTab> NewlyActivated ) override;
	virtual void OnSelectedNodesChangedImpl(const TSet<class UObject*>& NewSelection) override;
	virtual void OnBlueprintChangedImpl(UBlueprint* InBlueprint, bool bIsJustBeingCompiled) override;
	virtual void RefreshEditors(ERefreshBlueprintEditorReason::Type Reason = ERefreshBlueprintEditorReason::UnknownReason) override;
	virtual void SetupGraphEditorEvents(UEdGraph* InGraph, SGraphEditor::FGraphEditorEvents& InEvents) override;
	virtual void FocusInspectorOnGraphSelection(const TSet<class UObject*>& NewSelection, bool bForceRefresh = false) override;

	void HandleModifiedEvent(ERigVMGraphNotifType InNotifType, URigVMGraph* InGraph, UObject* InSubject);
	void HandleVMCompiledEvent(UObject* InCompiledObject, URigVM* InVM);
	void HandleControlRigExecutedEvent(UControlRig* InControlRig, const EControlRigState InState, const FName& InEventName);
	void HandleControlRigExecutionHalted(const int32 InstructionIndex, UObject* InNode, const FName& InEntryName);
	void SetHaltedNode(URigVMNode* Node);

	// FBaseToolKit overrides
	void CreateEditorModeManager() override;

	// FGCObject Interface
	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override;

	void BindCommands();

protected:
	/** Undo Action**/
	void UndoAction();

	/** Redo Action **/
	void RedoAction();

private:
	/** Extend menu */
	void ExtendMenu();

	/** Extend toolbar */
	void ExtendToolbar();

	/** Fill the toolbar with content */
	void FillToolbar(FToolBarBuilder& ToolbarBuilder);

	TArray<FName> GetEventQueue() const;
	void SetEventQueue(TArray<FName> InEventQueue);
	void SetEventQueue(TArray<FName> InEventQueue, bool bCompile);
	int32 GetEventQueueComboValue() const;
	FText GetEventQueueLabel() const;
	static FSlateIcon GetEventQueueIcon(const TArray<FName>& InEventQueue);
	FSlateIcon GetEventQueueIcon() const;

	enum EControlRigExecutionModeType
	{
		EControlRigExecutionModeType_Release,
		EControlRigExecutionModeType_Debug
	};

	void SetExecutionMode(const EControlRigExecutionModeType InExecutionMode);
	int32 GetExecutionModeComboValue() const;
	FText GetExecutionModeLabel() const;
	static FSlateIcon GetExecutionModeIcon(const EControlRigExecutionModeType InExecutionMode);
	FSlateIcon GetExecutionModeIcon() const;
	
	virtual void GetCustomDebugObjects(TArray<FCustomDebugObject>& DebugList) const override;
	virtual bool OnlyShowCustomDebugObjects() const override { return true; }
	virtual void HandleSetObjectBeingDebugged(UObject* InObject) override;
	virtual FString GetCustomDebugObjectLabel(UObject* ObjectBeingDebugged) const override;

	/** Handle hiding items in the graph */
	void HandleHideItem();
	bool CanHideItem() const;

	/** Handle preview scene setup */
	void HandlePreviewSceneCreated(const TSharedRef<IPersonaPreviewScene>& InPersonaPreviewScene);
	void HandleViewportCreated(const TSharedRef<class IPersonaViewport>& InViewport);
	bool IsToolbarDrawNullsEnabled() const;
	ECheckBoxState GetToolbarDrawNulls() const;
	void OnToolbarDrawNullsChanged(ECheckBoxState InNewValue);
	ECheckBoxState GetToolbarDrawAxesOnSelection() const;
	void OnToolbarDrawAxesOnSelectionChanged(ECheckBoxState InNewValue);
	TOptional<float> GetToolbarAxesScale() const;
	void OnToolbarAxesScaleChanged(float InValue);

		/** Handle switching skeletal meshes */
	void HandlePreviewMeshChanged(USkeletalMesh* InOldSkeletalMesh, USkeletalMesh* InNewSkeletalMesh);

	/** Push a newly compiled/opened control rig to the edit mode */
	void UpdateControlRig();

	/** Update the name lists for use in name combo boxes */
	void CacheNameLists();

	/** Rebind our anim instance to the preview's skeletal mesh component */
	void RebindToSkeletalMeshComponent();

	/** Update the skeletal mesh componens so that the anim instance has the correct skeletal mesh */
	void UpdateMeshInAnimInstance(USkeletalMesh* InNewSkeletalMesh);

	/** Update stale watch pins */
	void UpdateStaleWatchedPins();

	/** Wraps the normal blueprint editor's action menu creation callback */
	FActionMenuContent HandleCreateGraphActionMenu(UEdGraph* InGraph, const FVector2D& InNodePosition, const TArray<UEdGraphPin*>& InDraggedPins, bool bAutoExpand, SGraphEditor::FActionMenuClosed InOnMenuClosed);
	void OnNodeTitleCommitted(const FText& NewText, ETextCommit::Type CommitInfo, UEdGraphNode* NodeBeingChanged);

	void ToggleExecuteGraph();
	bool IsExecuteGraphOn() const;
	void ToggleAutoCompileGraph();
	bool IsAutoCompileGraphOn() const;
	bool CanAutoCompileGraph() const { return true; }
	void ToggleEventQueue();
	void ToggleExecutionMode();
	TSharedRef<SWidget> GenerateEventQueueMenuContent();
	TSharedRef<SWidget> GenerateExecutionModeMenuContent();

	enum ERigElementGetterSetterType
	{
		ERigElementGetterSetterType_Transform,
		ERigElementGetterSetterType_Rotation,
		ERigElementGetterSetterType_Translation,
		ERigElementGetterSetterType_Initial,
		ERigElementGetterSetterType_Relative,
		ERigElementGetterSetterType_Offset,
		ERigElementGetterSetterType_Name
	};

	 void HandleMakeElementGetterSetter(ERigElementGetterSetterType Type, bool bIsGetter, TArray<FRigElementKey> Keys, UEdGraph* Graph, FVector2D NodePosition);

	 void HandleOnControlModified(UControlRig* Subject, FRigControlElement* ControlElement, const FRigControlModifiedContext& Context);

	 void HandleRefreshEditorFromBlueprint(UControlRigBlueprint* InBlueprint);

	 void HandleVariableDroppedFromBlueprint(UObject* InSubject, FProperty* InVariableToDrop, const FVector2D& InDropPosition, const FVector2D& InScreenPosition);

	 void HandleBreakpointAdded();

	 void OnGraphNodeClicked(UControlRigGraphNode* InNode);

	 void OnNodeDoubleClicked(UControlRigBlueprint* InBlueprint, URigVMNode* InNode);

	 void OnGraphImported(UEdGraph* InEdGraph);

	 virtual bool OnActionMatchesName(FEdGraphSchemaAction* InAction, const FName& InName) const override;

	void HandleShowCurrentStatement();
	
	void HandleBreakpointActionRequested(const ERigVMBreakpointAction BreakpointAction);

	bool IsHaltedAtBreakpoint() const;

	void FrameSelection();

protected:

	/** Toolbox hosting widget */
	TSharedPtr<SBorder> Toolbox;

	/** Persona toolkit used to support skeletal mesh preview */
	TSharedPtr<IPersonaToolkit> PersonaToolkit;

	/** The extender to pass to the level editor to extend it's window menu */
	TSharedPtr<FExtender> MenuExtender;

	/** Toolbar extender */
	TSharedPtr<FExtender> ToolbarExtender;

	/** Preview instance inspector widget */
	TSharedPtr<SWidget> PreviewEditor;

	/** Our currently running control rig instance */
	UControlRig* ControlRig;

	/** preview scene */
	TSharedPtr<IPersonaPreviewScene> PreviewScene;

	/** preview animation instance */
	UAnimPreviewInstance* PreviewInstance;

	/** Delegate to deal with key down evens in the viewport / editor */
	FPersonaViewportKeyDownDelegate OnKeyDownDelegate;

	/** Delgate to build the context menu for the viewport */
	FOnGetContextMenu OnGetViewportContextMenuDelegate;
	UToolMenu* HandleOnGetViewportContextMenuDelegate();
	FNewMenuCommandsDelegate OnViewportContextMenuCommandsDelegate;
	TSharedPtr<FUICommandList> HandleOnViewportContextMenuCommandsDelegate();

	/** Bone Selection related */
	FTransform GetRigElementTransform(const FRigElementKey& InElement, bool bLocal, bool bOnDebugInstance) const;
	void SetRigElementTransform(const FRigElementKey& InElement, const FTransform& InTransform, bool bLocal);
	
	// FNotifyHook Interface
	virtual void NotifyPreChange(FProperty* PropertyAboutToChange) override;
	virtual void NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, FProperty* PropertyThatChanged) override;
	/** delegate for changing property */
	virtual void OnFinishedChangingProperties(const FPropertyChangedEvent& PropertyChangedEvent) override;
	void OnPropertyChanged(UObject* InObject, FPropertyChangedEvent& InEvent);
	void OnWrappedPropertyChangedChainEvent(UDetailsViewWrapperObject* InWrapperObject, const FString& InPropertyPath, FPropertyChangedChainEvent& InPropertyChangedChainEvent);
	void OnRequestLocalizeFunctionDialog(URigVMLibraryNode* InFunction, UControlRigBlueprint* InTargetBlueprint, bool bForce);
	FRigVMController_BulkEditResult OnRequestBulkEditDialog(UControlRigBlueprint* InBlueprint, URigVMController* InController, URigVMLibraryNode* InFunction, ERigVMControllerBulkEditType InEditType);
	bool OnRequestBreakLinksDialog(TArray<URigVMLink*> InLinks);
	void HandleJumpToHyperlink(const UObject* InSubject);
	bool UpdateDefaultValueForVariable(FBPVariableDescription& InVariable, bool bUseCDO);

	URigVMGraph* GetFocusedModel() const;
	URigVMController* GetFocusedController() const;
	TSharedPtr<SGraphEditor> GetGraphEditor(UEdGraph* InEdGraph) const;
	
	// stores a node snippet into the setting
	void StoreNodeSnippet(int32 InSnippetIndex);
	// restores a node snippet from the setting
	void RestoreNodeSnippet(int32 InSnippetIndex);
	// returns the right setting storage for a given snippet index
	static FString* GetSnippetStorage(int32 InSnippetIndex);

	URigVMController* ActiveController;
	bool bControlRigEditorInitialized;
	bool bIsSettingObjectBeingDebugged;

	/** Currently executing ControlRig or not - later maybe this will change to enum for whatever different mode*/
	bool bExecutionControlRig;

	/** The log to use for errors resulting from the init phase of the units */
	FControlRigLog ControlRigLog;
	/** Once the log is collected update the graph */
	void UpdateGraphCompilerErrors();

	void OnAnimInitialized();

	/** Are we currently compiling through the user interface */
	bool bIsCompilingThroughUI;

	FPreviewControlRigUpdated PreviewControlRigUpdated;

	TSharedPtr<SControlRigGraphPinNameListValueWidget> PinControlNameList;
	bool IsPinControlNameListEnabled() const;
	TSharedRef<SWidget> MakePinControlNameListItemWidget(TSharedPtr<FString> InItem);
	FText GetPinControlNameListText() const;
	TSharedPtr<FString> GetPinControlCurrentlySelectedItem(const TArray<TSharedPtr<FString>>* InNameList) const;
	void SetPinControlNameListText(const FText& NewTypeInValue, ETextCommit::Type /*CommitInfo*/);
	void OnPinControlNameListChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
	void OnPinControlNameListComboBox(const TArray<TSharedPtr<FString>>* InNameList);
	bool IsConstructionModeEnabled() const;

	bool bAnyErrorsLeft;

	TArray<FName> LastEventQueue;
	EControlRigExecutionModeType ExecutionMode;
	FString LastDebuggedRig;
	int32 RigHierarchyTabCount;
	TMap<FString, FString> KnownInstructionLimitWarnings;

	URigVMNode* HaltedAtNode;

	bool bSuspendDetailsPanelRefresh;

	TArray<TStrongObjectPtr<UDetailsViewWrapperObject>> WrapperObjects;
	TWeakObjectPtr<AStaticMeshActor> WeakGroundActorPtr;

	FDelegateHandle PropertyChangedHandle;

	void OnPreConstruction_AnyThread(UControlRig* InRig, const EControlRigState InState, const FName& InEventName);
	void OnPostConstruction_AnyThread(UControlRig* InRig, const EControlRigState InState, const FName& InEventName);

	bool bIsConstructionEventRunning;
	uint32 LastHierarchyHash;

	static const TArray<FName> ForwardsSolveEventQueue;
	static const TArray<FName> BackwardsSolveEventQueue;
	static const TArray<FName> ConstructionEventQueue;
	static const TArray<FName> BackwardsAndForwardsSolveEventQueue;

	friend class FControlRigEditorMode;
	friend class SControlRigStackView;
	friend class SRigHierarchy;
	friend struct FRigHierarchyTabSummoner;
};
