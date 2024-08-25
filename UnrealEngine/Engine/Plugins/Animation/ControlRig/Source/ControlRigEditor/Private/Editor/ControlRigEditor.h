// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IControlRigEditor.h"
#include "Editor/ControlRigEditorEditMode.h"
#include "AssetEditorModeManager.h"
#include "DragAndDrop/GraphNodeDragDropOp.h"
#include "ControlRigDefines.h"
#include "Units/RigUnitContext.h"
#include "IPersonaViewport.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "RigVMModel/RigVMGraph.h"
#include "RigVMModel/Nodes/RigVMLibraryNode.h"
#include "RigVMCore/RigVM.h"
#include "Widgets/SRigVMGraphPinNameListValueWidget.h"
#include "Widgets/Input/SComboBox.h"
#include "Styling/SlateTypes.h"
#include "AnimPreviewInstance.h"
#include "ScopedTransaction.h"
#include "Graph/ControlRigGraphNode.h"
#include "RigVMModel/RigVMController.h"
#include "Editor/RigVMDetailsViewWrapperObject.h"
#include "ControlRigTestData.h"
#include "ModularRigController.h"
#include "RigVMHost.h"
#include "SchematicGraphPanel/SSchematicGraphPanel.h"
#include "Units/RigUnit.h"
#include "ControlRigSchematicModel.h"

class UControlRigBlueprint;
class IPersonaToolkit;
class SWidget;
class SBorder;
class USkeletalMesh;
class FStructOnScope;
class UToolMenu;
class FControlRigEditor;

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

	virtual void InitRigVMEditor(const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, class URigVMBlueprint* InRigVMBlueprint) override;

	virtual const FName GetEditorAppName() const override;
	virtual const FName GetEditorModeName() const override;
	virtual TSharedPtr<FApplicationMode> CreateEditorMode() override;

public:
	FControlRigEditor();
	virtual ~FControlRigEditor();

	virtual UObject* GetOuterForHost() const override;

	// FRigVMEditor interface
	virtual UClass* GetDetailWrapperClass() const;
	virtual void Compile() override;
	virtual void HandleModifiedEvent(ERigVMGraphNotifType InNotifType, URigVMGraph* InGraph, UObject* InSubject) override;
	virtual void OnCreateGraphEditorCommands(TSharedPtr<FUICommandList> GraphEditorCommandsList) override;
	UE_DEPRECATED(5.4, "Please use HandleVMCompiledEvent with ExtendedExecuteContext parameter.")
	virtual void HandleVMCompiledEvent(UObject* InCompiledObject, URigVM* InVM) override {}
	virtual void HandleVMCompiledEvent(UObject* InCompiledObject, URigVM* InVM, FRigVMExtendedExecuteContext& InContext) override;
	virtual bool ShouldOpenGraphByDefault() const override { return !IsModularRig(); }
	virtual FReply OnViewportDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;

	// allows the editor to fill an empty graph
	virtual void CreateEmptyGraphContent(URigVMController* InController) override;

	int32 GetRigHierarchyTabCount() const { return RigHierarchyTabCount; }
	int32 GetModularRigHierarchyTabCount() const { return ModularRigHierarchyTabCount; }

	bool IsModularRig() const;

public:
	
	// IToolkit Interface
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual FString GetDocumentationLink() const override
	{
		return FString(TEXT("Engine/Animation/ControlRig"));
	}

	// BlueprintEditor interface
	virtual FReply OnSpawnGraphNodeByShortcut(FInputChord InChord, const FVector2D& InPosition, UEdGraph* InGraph) override;
	virtual bool IsSectionVisible(NodeSectionID::Type InSectionID) const override;
	virtual bool NewDocument_IsVisibleForType(ECreatedDocumentType GraphType) const override;

	virtual void PostTransaction(bool bSuccess, const FTransaction* Transaction, bool bIsRedo) override;

	void EnsureValidRigElementsInDetailPanel();

	//  FTickableEditorObject Interface
	virtual void Tick(float DeltaTime) override;

	// Gets the Control Rig Blueprint being edited/viewed
	UControlRigBlueprint* GetControlRigBlueprint() const;

	// returns the hierarchy being debugged
	UControlRig* GetControlRig() const;

	// returns the hierarchy being debugged
	URigHierarchy* GetHierarchyBeingDebugged() const;

	void SetDetailViewForRigElements();
	void SetDetailViewForRigElements(const TArray<FRigElementKey>& InKeys);
	bool DetailViewShowsAnyRigElement() const;
	bool DetailViewShowsRigElement(FRigElementKey InKey) const;
	TArray<FRigElementKey> GetSelectedRigElementsFromDetailView() const;

	void SetDetailViewForRigModules();
	void SetDetailViewForRigModules(const TArray<FString> InKeys);
	bool DetailViewShowsAnyRigModule() const;
	bool DetailViewShowsRigModule(FString InKey) const;
	TArray<FString> ModulesSelected;

	virtual void SetDetailObjects(const TArray<UObject*>& InObjects) override;
	virtual void RefreshDetailView() override;

	void CreatePersonaToolKitIfRequired();

public:

	/** Get the persona toolkit */
	TSharedRef<IPersonaToolkit> GetPersonaToolkit() const { return PersonaToolkit.ToSharedRef(); }

	/** Get the edit mode */
	FControlRigEditorEditMode* GetEditMode() const;

	// this changes everytime you compile, so don't cache it expecting it will last. 
	UControlRig* GetInstanceRig() const { return GetControlRig();  }

	void OnCurveContainerChanged();

	void OnHierarchyModified(ERigHierarchyNotification InNotif, URigHierarchy* InHierarchy, const FRigBaseElement* InElement);
	void OnHierarchyModified_AnyThread(ERigHierarchyNotification InNotif, URigHierarchy* InHierarchy, const FRigBaseElement* InElement);

	void HandleRigTypeChanged(UControlRigBlueprint* InBlueprint);

	void HandleModularRigModified(EModularRigNotification InNotification, const FRigModuleReference* InModule);
	void HandlePostCompileModularRigs(URigVMBlueprint* InBlueprint);

	const FName RigHierarchyToGraphDragAndDropMenuName = TEXT("ControlRigEditor.RigHierarchyToGraphDragAndDropMenu");
	void CreateRigHierarchyToGraphDragAndDropMenu() const;
	void OnGraphNodeDropToPerform(TSharedPtr<FGraphNodeDragDropOp> InDragDropOp, UEdGraph* InGraph, const FVector2D& InNodePosition, const FVector2D& InScreenPosition);

	FPersonaViewportKeyDownDelegate& GetKeyDownDelegate() { return OnKeyDownDelegate; }

	FOnGetContextMenu& OnGetViewportContextMenu() { return OnGetViewportContextMenuDelegate; }
	FNewMenuCommandsDelegate& OnViewportContextMenuCommands() { return OnViewportContextMenuCommandsDelegate; }

	// DirectManipulation functionality
	void HandleRequestDirectManipulationPosition() const { (void)HandleRequestDirectManipulation(ERigControlType::Position); }
	void HandleRequestDirectManipulationRotation() const { (void)HandleRequestDirectManipulation(ERigControlType::Rotator); }
	void HandleRequestDirectManipulationScale() const { (void)HandleRequestDirectManipulation(ERigControlType::Scale); }
	bool HandleRequestDirectManipulation(ERigControlType InControlType) const;
	bool SetDirectionManipulationSubject(const URigVMUnitNode* InNode);
	bool IsDirectManipulationEnabled() const;
	EVisibility GetDirectManipulationVisibility() const;
	FText GetDirectionManipulationText() const;
	void OnDirectManipulationChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo);
	const TArray<FRigDirectManipulationTarget> GetDirectManipulationTargets() const;
	const TArray<TSharedPtr<FString>>& GetDirectManipulationTargetTextList() const;
	bool ClearDirectManipulationSubject() { return SetDirectionManipulationSubject(nullptr); }
	void RefreshDirectManipulationTextList();

	// Rig connector functionality
	EVisibility GetConnectorWarningVisibility() const;
	FText GetConnectorWarningText() const;
	FReply OnNavigateToConnectorWarning() const;
	FSimpleMulticastDelegate& OnRequestNavigateToConnectorWarning() { return RequestNavigateToConnectorWarningDelegate; }

	FVector2D ComputePersonaProjectedScreenPos(const FVector& InWorldPos, bool bClampToScreenRectangle = false);
	
protected:

	virtual void BindCommands() override;

	void OnHierarchyChanged();

	void SynchronizeViewportBoneSelection();

	virtual void SaveAsset_Execute() override;
	virtual void SaveAssetAs_Execute() override;

	// update the cached modification value
	void UpdateBoneModification(FName BoneName, const FTransform& Transform);

	// remove a single bone modification across all instance
	void RemoveBoneModification(FName BoneName);

	// reset all bone modification across all instance
	void ResetAllBoneModification();

	virtual void HandleVMExecutedEvent(URigVMHost* InHost, const FName& InEventName) override;

	// FBaseToolKit overrides
	virtual void CreateEditorModeManager() override;

private:
	/** Fill the toolbar with content */
	virtual void FillToolbar(FToolBarBuilder& ToolbarBuilder, bool bEndSection = true) override;

	virtual TArray<FName> GetDefaultEventQueue() const override;
	virtual void SetEventQueue(TArray<FName> InEventQueue, bool bCompile) override;
	virtual int32 GetEventQueueComboValue() const override;
	virtual FText GetEventQueueLabel() const override;
	virtual FSlateIcon GetEventQueueIcon(const TArray<FName>& InEventQueue) const override;
	virtual void HandleSetObjectBeingDebugged(UObject* InObject) override;

	/** Handle preview scene setup */
	void HandlePreviewSceneCreated(const TSharedRef<IPersonaPreviewScene>& InPersonaPreviewScene);
	void HandleViewportCreated(const TSharedRef<class IPersonaViewport>& InViewport);
	bool IsToolbarDrawNullsEnabled() const;
	ECheckBoxState GetToolbarDrawNulls() const;
	void OnToolbarDrawNullsChanged(ECheckBoxState InNewValue);
	bool IsToolbarDrawSocketsEnabled() const;
	ECheckBoxState GetToolbarDrawSockets() const;
	void OnToolbarDrawSocketsChanged(ECheckBoxState InNewValue);
	ECheckBoxState GetToolbarDrawAxesOnSelection() const;
	void OnToolbarDrawAxesOnSelectionChanged(ECheckBoxState InNewValue);
	TOptional<float> GetToolbarAxesScale() const;
	void OnToolbarAxesScaleChanged(float InValue);
	void HandleToggleSchematicViewport();
	bool IsSchematicViewportActive() const;

		/** Handle switching skeletal meshes */
	void HandlePreviewMeshChanged(USkeletalMesh* InOldSkeletalMesh, USkeletalMesh* InNewSkeletalMesh);

	/** Push a newly compiled/opened control rig to the edit mode */
	virtual void UpdateRigVMHost() override;
	virtual void UpdateRigVMHost_PreClearOldHost(URigVMHost* InPreviousHost) override;

	/** Update the name lists for use in name combo boxes */
	virtual void CacheNameLists() override;

	/** Rebind our anim instance to the preview's skeletal mesh component */
	void RebindToSkeletalMeshComponent();

	virtual void GenerateEventQueueMenuContent(FMenuBuilder& MenuBuilder) override;

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

	void FilterDraggedKeys(TArray<FRigElementKey>& Keys, bool bRemoveNameSpace);
	void HandleMakeElementGetterSetter(ERigElementGetterSetterType Type, bool bIsGetter, TArray<FRigElementKey> Keys, UEdGraph* Graph, FVector2D NodePosition);

	void HandleOnControlModified(UControlRig* Subject, FRigControlElement* ControlElement, const FRigControlModifiedContext& Context);

	virtual void HandleRefreshEditorFromBlueprint(URigVMBlueprint* InBlueprint) override;

	FText GetTestAssetName() const;
	FText GetTestAssetTooltip() const;
	bool SetTestAssetPath(const FString& InAssetPath);
	TSharedRef<SWidget> GenerateTestAssetModeMenuContent();
	TSharedRef<SWidget> GenerateTestAssetRecordMenuContent();
	bool RecordTestData(double InRecordingDuration);
	void ToggleTestData();

protected:

	/** Persona toolkit used to support skeletal mesh preview */
	TSharedPtr<IPersonaToolkit> PersonaToolkit;

	/** Preview instance inspector widget */
	TSharedPtr<IPersonaViewport> PreviewViewport;

	/** preview scene */
	TSharedPtr<IPersonaPreviewScene> PreviewScene;

	/** preview animation instance */
	UAnimPreviewInstance* PreviewInstance;

	/** Model for the schematic views */
	FControlRigSchematicModel SchematicModel;

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
	
	/** delegate for changing property */
	virtual void OnFinishedChangingProperties(const FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void OnWrappedPropertyChangedChainEvent(URigVMDetailsViewWrapperObject* InWrapperObject, const FString& InPropertyPath, FPropertyChangedChainEvent& InPropertyChangedChainEvent) override;

	URigVMController* ActiveController;

	/** Currently executing ControlRig or not - later maybe this will change to enum for whatever different mode*/
	bool bExecutionControlRig;

	void OnAnimInitialized();

	bool IsConstructionModeEnabled() const;
	bool IsDebuggingExternalControlRig(const UControlRig* InControlRig = nullptr) const;
	bool ShouldExecuteControlRig(const UControlRig* InControlRig = nullptr) const;

	int32 RigHierarchyTabCount;
	int32 ModularRigHierarchyTabCount;
	TWeakObjectPtr<AStaticMeshActor> WeakGroundActorPtr;

	void OnPreForwardsSolve_AnyThread(UControlRig* InRig, const FName& InEventName);
	void OnPreConstructionForUI_AnyThread(UControlRig* InRig, const FName& InEventName);
	void OnPreConstruction_AnyThread(UControlRig* InRig, const FName& InEventName);
	void OnPostConstruction_AnyThread(UControlRig* InRig, const FName& InEventName);
	FRigPose PreConstructionPose;
	TArray<FRigSocketState> SocketStates;
	TArray<FRigConnectorState> ConnectorStates;

	bool bIsConstructionEventRunning;
	uint32 LastHierarchyHash;

	TStrongObjectPtr<UControlRigTestData> TestDataStrongPtr;

	TWeakObjectPtr<const URigVMUnitNode> DirectManipulationSubject;
	mutable TArray<TSharedPtr<FString>> DirectManipulationTextList;
	TSharedPtr<SComboBox<TSharedPtr<FString>>> DirectManipulationCombo;
	bool bRefreshDirectionManipulationTargetsRequired;
	FSimpleMulticastDelegate RequestNavigateToConnectorWarningDelegate;
	TSharedPtr<SSchematicGraphPanel> SchematicViewport;

	static const TArray<FName> ForwardsSolveEventQueue;
	static const TArray<FName> BackwardsSolveEventQueue;
	static const TArray<FName> ConstructionEventQueue;
	static const TArray<FName> BackwardsAndForwardsSolveEventQueue;

	friend class FControlRigEditorMode;
	friend class FModularRigEditorMode;
	friend class SControlRigStackView;
	friend class SRigHierarchy;
	friend class SModularRigModel;
	friend struct FRigHierarchyTabSummoner;
	friend struct FModularRigModelTabSummoner;
};
