// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorUndoClient.h"
#include "Editor/SModularRigTreeView.h"
#include "ControlRigBlueprint.h"
#include "DragAndDrop/GraphNodeDragDropOp.h"
#include "Editor/RigVMEditor.h"
#include "ControlRigDragOps.h"

class SModularRigModel;
class FControlRigEditor;
class SSearchBox;
class FUICommandList;
class URigVMBlueprint;
class UControlRig;
struct FAssetData;
class FMenuBuilder;
class UToolMenu;
struct FToolMenuContext;

/** Widget allowing editing of a control rig's structure */
class SModularRigModel : public SCompoundWidget, public FEditorUndoClient
{
public:
	SLATE_BEGIN_ARGS(SModularRigModel) {}
	SLATE_END_ARGS()

	~SModularRigModel();

	void Construct(const FArguments& InArgs, TSharedRef<FControlRigEditor> InControlRigEditor);

	FControlRigEditor* GetControlRigEditor() const
	{
		if(ControlRigEditor.IsValid())
		{
			return ControlRigEditor.Pin().Get();
		}
		return nullptr;
	}

private:

	void OnEditorClose(const FRigVMEditor* InEditor, URigVMBlueprint* InBlueprint);

	/** Bind commands that this widget handles */
	void BindCommands();

	/** SWidget interface */
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent);

	/** Rebuild the tree view */
	void RefreshTreeView(bool bRebuildContent = true);

	/** Return all selected keys */
	TArray<FString> GetSelectedKeys() const;

	/** Create a new item */
	void HandleNewItem();
	void HandleNewItem(UClass* InClass, const FString& InParentPath);

	/** Rename item */
	bool CanRenameModule() const;
	void HandleRenameModule();
	FName HandleRenameModule(const FString& InOldPath, const FName& InNewName);
	bool HandleVerifyNameChanged(const FString& InOldPath, const FName& InNewName, FText& OutErrorMessage);

	/** Delete items */
	void HandleDeleteModules();
	void HandleDeleteModules(const TArray<FString>& InPaths);

	/** Reparent items */
	void HandleReparentModules(const TArray<FString>& InPaths, const FString& InParentPath);

	/** Mirror items */
	void HandleMirrorModules();
	void HandleMirrorModules(const TArray<FString>& InPaths);

	/** Reresolve items */
	void HandleReresolveModules();
	void HandleReresolveModules(const TArray<FString>& InPaths);

	/** Resolve connector */
	void HandleConnectorResolved(const FRigElementKey& InConnector, const FRigElementKey& InTarget);

	/** UnResolve connector */
	void HandleConnectorDisconnect(const FRigElementKey& InConnector);

	/** Set Selection Changed */
	void OnSelectionChanged(TSharedPtr<FModularRigTreeElement> Selection, ESelectInfo::Type SelectInfo);

	TSharedPtr< SWidget > CreateContextMenuWidget();
	void OnItemClicked(TSharedPtr<FModularRigTreeElement> InItem);
	void OnItemDoubleClicked(TSharedPtr<FModularRigTreeElement> InItem);
	
	// FEditorUndoClient
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override;

	// reply to a drag operation
	FReply OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);

	// reply to a drop operation on item
	TOptional<EItemDropZone> OnCanAcceptDrop(const FDragDropEvent& DragDropEvent, EItemDropZone DropZone, TSharedPtr<FModularRigTreeElement> TargetItem);
	FReply OnAcceptDrop(const FDragDropEvent& DragDropEvent, EItemDropZone DropZone, TSharedPtr<FModularRigTreeElement> TargetItem);

	// SWidget Overrides
	virtual FReply OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;

	static const FName ContextMenuName;
	static void CreateContextMenu();
	UToolMenu* GetContextMenu();
	TSharedPtr<FUICommandList> GetContextMenuCommands() const;
	
	/** Our owning control rig editor */
	TWeakPtr<FControlRigEditor> ControlRigEditor;

	/** Tree view widget */
	TSharedPtr<SModularRigTreeView> TreeView;
	TSharedPtr<SHeaderRow> HeaderRowWidget;

	TWeakObjectPtr<UControlRigBlueprint> ControlRigBlueprint;
	TWeakObjectPtr<UModularRig> ControlRigBeingDebuggedPtr;
	
	/** Command list we bind to */
	TSharedPtr<FUICommandList> CommandList;

	bool IsSingleSelected() const;
	
	UModularRig* GetModularRig() const;
	UModularRig* GetDefaultModularRig() const;
	const UModularRig* GetModularRigForTreeView() const { return GetModularRig(); }
	void OnRequestDetailsInspection(const FString& InKey);
	void HandlePreCompileModularRigs(URigVMBlueprint* InBlueprint);
	void HandlePostCompileModularRigs(URigVMBlueprint* InBlueprint);
	void OnHierarchyModified(ERigHierarchyNotification InNotif, URigHierarchy* InHierarchy, const FRigBaseElement* InElement);

	void HandleRefreshEditorFromBlueprint(URigVMBlueprint* InBlueprint);
	void HandleSetObjectBeingDebugged(UObject* InObject);

public:

	friend class FModularRigTreeElement;
	friend class SModularRigModelItem;
	friend class UControlRigBlueprintEditorLibrary;
};

