// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolkits/IToolkitHost.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "PropertyEditorDelegates.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableViewBase.h"
#include "Chooser.h"
#include "EditorUndoClient.h"
#include "Misc/NotifyHook.h"
#include "ChooserTableEditor.generated.h"

class SComboButton;
class SEditableText;
class IDetailsView;

// Class used for chooser editor details customization
UCLASS()
class UChooserRowDetails : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Instanced, Category="Hidden")
	TObjectPtr<UChooserTable> Chooser;
	int Row;
};

// Class used for chooser editor details customization
UCLASS()
class UChooserColumnDetails : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Instanced, Category="Hidden")
	TObjectPtr<UChooserTable> Chooser;
	int Column = -1;
};


namespace UE::ChooserEditor
{
	class FChooserTableEditor : public FAssetEditorToolkit, public FSelfRegisteringEditorUndoClient, public FNotifyHook
	{
	public:
		/** Delegate that, given an array of assets, returns an array of objects to use in the details view of an FSimpleAssetEditor */
		DECLARE_DELEGATE_RetVal_OneParam(TArray<UObject*>, FGetDetailsViewObjects, const TArray<UObject*>&);

		virtual void RegisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;
		virtual void UnregisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;


		/**
		* Edits the specified asset object
		*
		* @param	Mode					Asset editing mode for this editor (standalone or world-centric)
		* @param	InitToolkitHost			When Mode is WorldCentric, this is the level editor instance to spawn this editor within
		* @param	ObjectsToEdit			The object to edit
		* @param	GetDetailsViewObjects	If bound, a delegate to get the array of objects to use in the details view; uses ObjectsToEdit if not bound
		*/
		void InitEditor( const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, const TArray<UObject*>& ObjectsToEdit, FGetDetailsViewObjects GetDetailsViewObjects );

		/** Destructor */
		virtual ~FChooserTableEditor();

		/** IToolkit interface */
		virtual FName GetToolkitFName() const override;
		virtual FText GetBaseToolkitName() const override;
		virtual FText GetToolkitName() const override;
		virtual FText GetToolkitToolTipText() const override;
		virtual FString GetWorldCentricTabPrefix() const override;
		virtual FLinearColor GetWorldCentricTabColorScale() const override;
		virtual bool IsPrimaryEditor() const override { return true; }
		virtual bool IsSimpleAssetEditor() const override { return false; }
		virtual void InitToolMenuContext(FToolMenuContext& MenuContext) override;
		
		/** FEditorUndoClient Interface */
		virtual void PostUndo(bool bSuccess) override;
		virtual void PostRedo(bool bSuccess) override;
		
		/** Begin FNotifyHook Interface */
		virtual void NotifyPreChange( FProperty* PropertyAboutToChange ) override;
		virtual void NotifyPostChange( const FPropertyChangedEvent& PropertyChangedEvent, FProperty* PropertyThatChanged) override;

		UChooserTable* GetChooser() { return Cast<UChooserTable>(EditingObjects[0]); }
		const UChooserTable* GetChooser() const { return Cast<UChooserTable>(EditingObjects[0]); }
	
		/** Used to show or hide certain properties */
		void SetPropertyVisibilityDelegate(FIsPropertyVisible InVisibilityDelegate);
		/** Can be used to disable the details view making it read-only */
		void SetPropertyEditingEnabledDelegate(FIsPropertyEditingEnabled InPropertyEditingDelegate);
	
		struct FChooserTableRow
		{
			FChooserTableRow(int32 i) { RowIndex = i; }
			int32 RowIndex;
		};

		void UpdateTableRows();
		void SelectColumn(int Index);
		void ClearSelectedColumn();
		void DeleteColumn(int Index);
		void AddColumn(const UScriptStruct* ColumnType);
		void MoveRow(int SourceRowIndex, int TargetIndex);
	private:

		void SelectRootProperties();
		void RegisterToolbar();
		void BindCommands();
		void MakeDebugTargetMenu(UToolMenu* InToolMenu);
	
		/** Create the properties tab and its content */
		TSharedRef<SDockTab> SpawnPropertiesTab( const FSpawnTabArgs& Args );
		/** Create the table tab and its content */
		TSharedRef<SDockTab> SpawnTableTab( const FSpawnTabArgs& Args );
	
		TSharedRef<ITableRow> GenerateTableRow(TSharedPtr<FChooserTableRow> InItem, const TSharedRef<STableViewBase>& OwnerTable);

		/** Called when objects need to be swapped out for new versions, like after a blueprint recompile. */
		void OnObjectsReplaced(const TMap<UObject*, UObject*>& ReplacementMap);

		/** Details view */
		TSharedPtr< class IDetailsView > DetailsView;

		/** App Identifier. */
		static const FName ChooserEditorAppIdentifier;

		/**	The tab ids for all the tabs used */
		static const FName PropertiesTabId;
		static const FName TableTabId;

		/** The objects open within this editor */
		TArray<UObject*> EditingObjects;

		UChooserColumnDetails* SelectedColumn = nullptr;
		TArray<TObjectPtr<UChooserRowDetails>> SelectedRows;

		void UpdateTableColumns();
		TArray<TSharedPtr<FChooserTableRow>> TableRows;
	
		TSharedPtr<SComboButton> CreateColumnComboButton;
		TSharedPtr<SComboButton> CreateRowComboButton;
		
		TSharedPtr<SHeaderRow> HeaderRow;
		TSharedPtr<SListView<TSharedPtr<FChooserTableRow>>> TableView;
	public:
		TSharedPtr<SComboButton>& GetCreateRowComboButton() { return CreateRowComboButton; };

		/** The name given to all instances of this type of editor */
		static const FName ToolkitFName;

		static TSharedRef<FChooserTableEditor> CreateEditor( const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, UObject* ObjectToEdit, FGetDetailsViewObjects GetDetailsViewObjects = FGetDetailsViewObjects() );

		static TSharedRef<FChooserTableEditor> CreateEditor( const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, const TArray<UObject*>& ObjectsToEdit, FGetDetailsViewObjects GetDetailsViewObjects = FGetDetailsViewObjects() );

		static void RegisterWidgets();
	};
}

UCLASS()
class UChooserEditorToolMenuContext : public UObject
{
	GENERATED_BODY()

public:
	TWeakPtr<UE::ChooserEditor::FChooserTableEditor> ChooserEditor;
};
