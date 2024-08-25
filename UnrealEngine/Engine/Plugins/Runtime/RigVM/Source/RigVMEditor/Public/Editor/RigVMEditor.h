// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BlueprintEditor.h"
#include "RigVMHost.h"
#include "RigVMBlueprint.h"
#include "Editor/RigVMDetailsViewWrapperObject.h"

class FRigVMEditor;

DECLARE_MULTICAST_DELEGATE_TwoParams(FRigVMEditorClosed, const FRigVMEditor*, URigVMBlueprint*);

struct FRigVMEditorModes
{
	// Mode constants
	static const FName RigVMEditorMode;
	static FText GetLocalizedMode(const FName InMode)
	{
		static TMap< FName, FText > LocModes;

		if (LocModes.Num() == 0)
		{
			LocModes.Add(RigVMEditorMode, NSLOCTEXT("RigVMEditorModes", "RigVMEditorMode", "RigVM"));
		}

		check(InMode != NAME_None);
		const FText* OutDesc = LocModes.Find(InMode);
		check(OutDesc);
		return *OutDesc;
	}
private:
	FRigVMEditorModes() {}
};

class RIGVMEDITOR_API FRigVMEditor : public FBlueprintEditor
{
public:

	FRigVMEditor();
	virtual ~FRigVMEditor();

	/**
	 * Edits the specified asset(s)
	 *
	 * @param	Mode					Mode that this editor should operate in
	 * @param	InitToolkitHost			When Mode is WorldCentric, this is the level editor instance to spawn this editor within
	 * @param	InRigVMBlueprint	The blueprint object to start editing.
	 */
	virtual void InitRigVMEditor(const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, class URigVMBlueprint* InRigVMBlueprint);

	virtual void HandleAssetRequestedOpen(UObject* InObject);
	virtual void HandleAssetRequestClose(UObject* InObject, EAssetEditorCloseReason InReason);
	bool bRequestedReopen = false;

	virtual const FName GetEditorAppName() const;
	virtual const FName GetEditorModeName() const;
	virtual TSharedPtr<FApplicationMode> CreateEditorMode();

	// FBlueprintEditor interface
	virtual UBlueprint* GetBlueprintObj() const override;
	virtual TSubclassOf<UEdGraphSchema> GetDefaultSchemaClass() const override;
	virtual bool InEditingMode() const override;

	//  FTickableEditorObject Interface
	virtual void Tick(float DeltaTime) override;

	// IToolkit Interface
	virtual void BringToolkitToFront() override;
	virtual FName GetToolkitFName() const override;
	virtual FName GetToolkitContextFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FText GetToolkitToolTipText() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;	
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
	virtual bool IsNativeParentClassCodeLinkEnabled() const override { return false; }
	virtual bool ReparentBlueprint_IsVisible() const override { return false; }
	virtual FReply OnSpawnGraphNodeByShortcut(FInputChord InChord, const FVector2D& InPosition, UEdGraph* InGraph) override;
	virtual bool ShouldLoadBPLibrariesFromAssetRegistry() override { return false; }
	virtual void JumpToHyperlink(const UObject* ObjectReference, bool bRequestRename = false) override;
	virtual bool ShouldOpenGraphByDefault() const { return true; }

	// FEditorUndoClient Interface
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override;

	virtual void PostTransaction(bool bSuccess, const FTransaction* Transaction, bool bIsRedo) {};

	virtual void OnStartWatchingPin();
	virtual bool CanStartWatchingPin() const;

	virtual void OnStopWatchingPin();
	virtual bool CanStopWatchingPin() const;

	// IToolkitHost Interface
	virtual void OnToolkitHostingStarted(const TSharedRef<class IToolkit>& Toolkit) override;
	virtual void OnToolkitHostingFinished(const TSharedRef<class IToolkit>& Toolkit) override;

	//  FTickableEditorObject Interface
	virtual TStatId GetStatId() const override;

	// returns the blueprint being edited
	URigVMBlueprint* GetRigVMBlueprint() const;

	// returns the currently debugged / viewed host
	URigVMHost* GetRigVMHost() const;

	virtual UObject* GetOuterForHost() const;

	// returns the class to use for detail wrapper objects (UI shim layer)
	virtual UClass* GetDetailWrapperClass() const;

	// allows the editor to fill an empty graph
	virtual void CreateEmptyGraphContent(URigVMController* InController) {}

	DECLARE_EVENT_OneParam(FRigVMEditor, FPreviewHostUpdated, FRigVMEditor*);
	FPreviewHostUpdated& OnPreviewHostUpdated() { return PreviewHostUpdated;  }

	FRigVMEditorClosed& OnEditorClosed() { return RigVMEditorClosedDelegate; }

	/** Get the toolbox hosting widget */
	TSharedRef<SBorder> GetToolbox() { return Toolbox.ToSharedRef(); }

	virtual bool SelectLocalVariable(const UEdGraph* Graph, const FName& VariableName) override;

protected:

	enum ERigVMEditorExecutionModeType
	{
		ERigVMEditorExecutionModeType_Release,
		ERigVMEditorExecutionModeType_Debug
	};

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
	virtual bool AreEventGraphsAllowed() const override;
	virtual bool AreMacrosAllowed() const override;
	virtual bool AreDelegatesAllowed() const override;
	virtual bool NewDocument_IsVisibleForType(ECreatedDocumentType GraphType) const;
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

	virtual void HandleModifiedEvent(ERigVMGraphNotifType InNotifType, URigVMGraph* InGraph, UObject* InSubject);
	UE_DEPRECATED(5.4, "Please use HandleVMCompiledEvent with ExtendedExecuteContext param.")
	virtual void HandleVMCompiledEvent(UObject* InCompiledObject, URigVM* InVM) {}
	virtual void HandleVMCompiledEvent(UObject* InCompiledObject, URigVM* InVM, FRigVMExtendedExecuteContext& InContext);
	virtual void HandleVMExecutedEvent(URigVMHost* InHost, const FName& InEventName);
	virtual void HandleVMExecutionHalted(const int32 InstructionIndex, UObject* InNode, const FName& InEntryName);
	void SetHaltedNode(URigVMNode* Node);
	
	// FNotifyHook Interface
	virtual void NotifyPreChange(FProperty* PropertyAboutToChange) override;
	virtual void NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, FProperty* PropertyThatChanged) override;
	/** delegate for changing property */
	virtual void OnFinishedChangingProperties(const FPropertyChangedEvent& PropertyChangedEvent) override;
	void OnPropertyChanged(UObject* InObject, FPropertyChangedEvent& InEvent);
	virtual void OnWrappedPropertyChangedChainEvent(URigVMDetailsViewWrapperObject* InWrapperObject, const FString& InPropertyPath, FPropertyChangedChainEvent& InPropertyChangedChainEvent);
	void OnRequestLocalizeFunctionDialog(FRigVMGraphFunctionIdentifier& InFunction, URigVMBlueprint* InTargetBlueprint, bool bForce);
	FRigVMController_BulkEditResult OnRequestBulkEditDialog(URigVMBlueprint* InBlueprint, URigVMController* InController, URigVMLibraryNode* InFunction, ERigVMControllerBulkEditType InEditType);
	bool OnRequestBreakLinksDialog(TArray<URigVMLink*> InLinks);
	TRigVMTypeIndex OnRequestPinTypeSelectionDialog(const TArray<TRigVMTypeIndex>& InTypes);
	void HandleJumpToHyperlink(const UObject* InSubject);
	bool UpdateDefaultValueForVariable(FBPVariableDescription& InVariable, bool bUseCDO);

	URigVMController* ActiveController;

	/** Push a newly compiled/opened host to the editor */
	virtual void UpdateRigVMHost();
	virtual void UpdateRigVMHost_PreClearOldHost(URigVMHost* InPreviousHost) {};

	/** Update the name lists for use in name combo boxes */
	virtual void CacheNameLists();

	// FGCObject Interface
	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override;

	virtual void BindCommands();

	void ToggleAutoCompileGraph();
	bool IsAutoCompileGraphOn() const;
	bool CanAutoCompileGraph() const { return true; }
	void ToggleEventQueue();
	void ToggleExecutionMode();
	TSharedRef<SWidget> GenerateEventQueueMenuContent();
	TSharedRef<SWidget> GenerateExecutionModeMenuContent();
	virtual void GenerateEventQueueMenuContent(FMenuBuilder& MenuBuilder);

	/** Wraps the normal blueprint editor's action menu creation callback */
	FActionMenuContent HandleCreateGraphActionMenu(UEdGraph* InGraph, const FVector2D& InNodePosition, const TArray<UEdGraphPin*>& InDraggedPins, bool bAutoExpand, SGraphEditor::FActionMenuClosed InOnMenuClosed);
	void OnNodeTitleCommitted(const FText& NewText, ETextCommit::Type CommitInfo, UEdGraphNode* NodeBeingChanged);

	/** Undo Action**/
	void UndoAction();

	/** Redo Action **/
	void RedoAction();
	
	virtual void OnCreateComment() override;

	bool IsDetailsPanelRefreshSuspended() const { return bSuspendDetailsPanelRefresh; }
	bool& GetSuspendDetailsPanelRefreshFlag() { return bSuspendDetailsPanelRefresh; }
	virtual void SetDetailObjects(const TArray<UObject*>& InObjects);
	virtual void SetDetailObjects(const TArray<UObject*>& InObjects, bool bChangeUISelectionState);
	virtual void SetMemoryStorageDetails(const TArray<FRigVMMemoryStorageStruct*>& InStructs);
	virtual void SetDetailViewForGraph(URigVMGraph* InGraph);
	virtual void SetDetailViewForFocusedGraph();
	virtual void SetDetailViewForLocalVariable();
	virtual void RefreshDetailView();
	virtual bool DetailViewShowsAnyRigUnit() const;
	virtual bool DetailViewShowsLocalVariable() const;
	virtual bool DetailViewShowsStruct(UScriptStruct* InStruct) const;
	virtual void ClearDetailObject(bool bChangeUISelectionState = true);
	virtual void ClearDetailsViewWrapperObjects();
	const TArray<TStrongObjectPtr<URigVMDetailsViewWrapperObject>>& GetWrapperObjects() const { return WrapperObjects; }

	void SetHost(URigVMHost* InHost);

	URigVMGraph* GetFocusedModel() const;
	URigVMController* GetFocusedController() const;
	TSharedPtr<SGraphEditor> GetGraphEditor(UEdGraph* InEdGraph) const;

	/** Extend menu */
	void ExtendMenu();

	/** Extend toolbar */
	void ExtendToolbar();
	
	/** Fill the toolbar with content */
	virtual void FillToolbar(FToolBarBuilder& ToolbarBuilder, bool bEndSection = true);

public:
	
	virtual TArray<FName> GetDefaultEventQueue() const;
	TArray<FName> GetEventQueue() const;
	void SetEventQueue(TArray<FName> InEventQueue);
	virtual void SetEventQueue(TArray<FName> InEventQueue, bool bCompile);
	virtual int32 GetEventQueueComboValue() const { return INDEX_NONE; }
	virtual FText GetEventQueueLabel() const { return FText(); }
	virtual FSlateIcon GetEventQueueIcon(const TArray<FName>& InEventQueue) const;
	FSlateIcon GetEventQueueIcon() const;

protected:

	void SetExecutionMode(const ERigVMEditorExecutionModeType InExecutionMode);
	int32 GetExecutionModeComboValue() const;
	FText GetExecutionModeLabel() const;
	static FSlateIcon GetExecutionModeIcon(const ERigVMEditorExecutionModeType InExecutionMode);
	FSlateIcon GetExecutionModeIcon() const;
	
	virtual void GetCustomDebugObjects(TArray<FCustomDebugObject>& DebugList) const override;
	virtual bool OnlyShowCustomDebugObjects() const override { return true; }
	virtual void HandleSetObjectBeingDebugged(UObject* InObject) override;
	virtual FString GetCustomDebugObjectLabel(UObject* ObjectBeingDebugged) const override;

	/** Handle hiding items in the graph */
	void HandleHideItem();
	bool CanHideItem() const;

	/** Update stale watch pins */
	void UpdateStaleWatchedPins();
	
	virtual void HandleRefreshEditorFromBlueprint(URigVMBlueprint* InBlueprint);
	virtual void HandleVariableDroppedFromBlueprint(UObject* InSubject, FProperty* InVariableToDrop, const FVector2D& InDropPosition, const FVector2D& InScreenPosition);
	virtual void HandleBreakpointAdded();
	virtual void OnGraphNodeClicked(URigVMEdGraphNode* InNode);
	virtual void OnNodeDoubleClicked(URigVMBlueprint* InBlueprint, URigVMNode* InNode);
	virtual void OnGraphImported(UEdGraph* InEdGraph);
	virtual bool OnActionMatchesName(FEdGraphSchemaAction* InAction, const FName& InName) const override;
	virtual void HandleShowCurrentStatement();
	virtual void HandleBreakpointActionRequested(const ERigVMBreakpointAction BreakpointAction);
	virtual bool IsHaltedAtBreakpoint() const;
	virtual void FrameSelection();

	/** Once the log is collected update the graph */
	void UpdateGraphCompilerErrors();

	/** Returns true if PIE is currently running */
	static bool IsPIERunning();

private:

	void OnPIEStopped(bool bSimulation);

	/** Our currently running rig vm instance */
	//TObjectPtr<URigVMHost> Host;

	FPreviewHostUpdated PreviewHostUpdated;

	FRigVMEditorClosed RigVMEditorClosedDelegate;

	/** Toolbox hosting widget */
	TSharedPtr<SBorder> Toolbox;

	bool bAnyErrorsLeft;
	TMap<FString, FString> KnownInstructionLimitWarnings;
	URigVMNode* HaltedAtNode;
	FString LastDebuggedHost;

	bool bSuspendDetailsPanelRefresh;
	bool bAllowBulkEdits;
	bool bIsSettingObjectBeingDebugged;

protected:
	bool bRigVMEditorInitialized;

private:
	/** Are we currently compiling through the user interface */
	bool bIsCompilingThroughUI;

	TArray<TStrongObjectPtr<URigVMDetailsViewWrapperObject>> WrapperObjects;

	ERigVMEditorExecutionModeType ExecutionMode;

	/** The log to use for errors resulting from the init phase of the units */
	FRigVMLog RigVMLog;
	

protected:
	TArray<FName> LastEventQueue;

	/** The extender to pass to the level editor to extend it's window menu */
	TSharedPtr<FExtender> MenuExtender;

	/** Toolbar extender */
	TSharedPtr<FExtender> ToolbarExtender;

	FDelegateHandle PropertyChangedHandle;

	friend class SRigVMExecutionStackView;
};
