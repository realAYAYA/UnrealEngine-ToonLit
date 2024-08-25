// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "AdvancedPreviewScene.h" // IWYU pragma: keep
#include "Tools/BaseAssetToolkit.h"
#include "DragAndDrop/DecoratedDragDropOp.h"
#include "EditorUndoClient.h"
#include "Widgets/Views/STreeView.h"

class FAdvancedPreviewScene;
class FSpawnTabArgs;
class FEditorViewportClient;
class UAssetEditor;
class IStructureDetailsView;
class FSmartObjectViewModel;
class USmartObjectDetailsWrapper;
class USmartObjectDefinition;
struct FSmartObjectDefinitionPreviewData;
enum class EItemDropZone;
struct FSmartObjectOutlinerItem;

class SMARTOBJECTSEDITORMODULE_API FSmartObjectAssetToolkit : public FBaseAssetToolkit, public FSelfRegisteringEditorUndoClient, public FGCObject
{
public:
	explicit FSmartObjectAssetToolkit(UAssetEditor* InOwningAssetEditor);
	virtual ~FSmartObjectAssetToolkit();

	virtual TSharedPtr<FEditorViewportClient> CreateEditorViewportClient() const override;

protected:
	virtual void PostInitAssetEditor() override;
	virtual void RegisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;
	virtual void OnClose() override;
	virtual void SetEditingObject(class UObject* InObject) override;
	
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override
	{
		return TEXT("FSmartObjectAssetToolkit");
	}

	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override;
	
private:

	void OnParametersChanged(const USmartObjectDefinition& SmartObjectDefinition);

	void UpdatePreviewActor();
	void UpdateCachedPreviewDataFromDefinition();

	void UpdateItemList();
	
	/** Callback to detect changes in number of slot to keep gizmos in sync. */
	void OnPropertyChanged(UObject* ObjectBeingModified, FPropertyChangedEvent& PropertyChangedEvent);

	/** Creates a tab allowing the user to select a mesh or actor template to spawn in the preview scene. */
	TSharedRef<SDockTab> SpawnTab_PreviewSettings(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_Outliner(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_SelectionDetails(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_SceneViewport(const FSpawnTabArgs& Args);

	TSharedRef<ITableRow> OnGenerateRow(TSharedPtr<FSmartObjectOutlinerItem> InItem, const TSharedRef<STableViewBase>& OwnerTable);
	void OnGetChildren(TSharedPtr<FSmartObjectOutlinerItem> InItem, TArray<TSharedPtr<FSmartObjectOutlinerItem>>& OutChildren) const;
	void OnOutlinerSelectionChanged(TSharedPtr<FSmartObjectOutlinerItem> SelectedItem, ESelectInfo::Type SelectType);
	TSharedPtr<SWidget> OnOutlinerContextMenu();
	void HandleSelectionChanged(TConstArrayView<FGuid> Selection);
	void HandleSlotsChanged(USmartObjectDefinition* Definition);

	FReply OnOutlinerDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) const;
	TOptional<EItemDropZone> OnOutlinerCanAcceptDrop(const FDragDropEvent& DragDropEvent, EItemDropZone DropZone, const TSharedPtr<FSmartObjectOutlinerItem> TargetItem) const;
	FReply OnOutlinerAcceptDrop(const FDragDropEvent& DragDropEvent, EItemDropZone DropZone,  const TSharedPtr<FSmartObjectOutlinerItem> TargetItem);

	void UpdateDetailsSelection();

	FText GetOutlinerItemDescription(TSharedPtr<FSmartObjectOutlinerItem> Item) const;
	FSlateColor GetOutlinerItemColor(TSharedPtr<FSmartObjectOutlinerItem> Item) const;
	
	/** Additional Tab to select mesh/actor to add a 3D preview in the scene. */
	static const FName PreviewSettingsTabID;
	static const FName OutlinerTabID;
	static const FName SceneViewportTabID;
	static const FName DetailsTabID;

	/** Scene in which the 3D preview of the asset lives. */
	TUniquePtr<FAdvancedPreviewScene> AdvancedPreviewScene;

	/** Details view for the preview settings. */
	TSharedPtr<IStructureDetailsView> PreviewDetailsView;

	TSharedPtr<FStructOnScope> CachedPreviewData;
	
	TSharedPtr<STreeView<TSharedPtr<FSmartObjectOutlinerItem>>> ItemTreeWidget;
	TArray<TSharedPtr<FSmartObjectOutlinerItem>> ItemList;
	bool bUpdatingOutlinerSelection = false;
	bool bUpdatingViewSelection = false;

	TSharedPtr<SDockTab> DetailsTab;
	TSharedPtr<IDetailsView> DetailsAssetView;
	
    /** Typed pointer to the custom ViewportClient created by the toolkit. */
	mutable TSharedPtr<class FSmartObjectAssetEditorViewportClient> SmartObjectViewportClient;

	TSharedPtr<FSmartObjectViewModel> ViewModel;
	
	FDelegateHandle SelectionChangedHandle;
	FDelegateHandle SlotsChangedHandle;
};
