// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolkits/IToolkitHost.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "Editor/PropertyEditor/Public/PropertyEditorDelegates.h"
#include "Widgets/Views/STableViewBase.h"
#include "Chooser.h"
#include "IDetailCustomization.h"
#include "ChooserTableEditor.generated.h"

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

namespace UE::ChooserEditor
{
	class FChooserTableEditor : public FAssetEditorToolkit
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

		/** FAssetEditorToolkit interface */
		virtual void PostRegenerateMenusAndToolbars() override;

		UChooserTable* GetChooser() { return Cast<UChooserTable>(EditingObjects[0]); }
	
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
	private:

		FReply SelectRootProperties();
	
		/** Create the properties tab and its content */
		TSharedRef<SDockTab> SpawnPropertiesTab( const FSpawnTabArgs& Args );
		/** Create the table tab and its content */
		TSharedRef<SDockTab> SpawnTableTab( const FSpawnTabArgs& Args );
	
		TSharedRef<ITableRow> GenerateTableRow(TSharedPtr<FChooserTableRow> InItem, const TSharedRef<STableViewBase>& OwnerTable);

		/** Handles when an asset is imported */
		void HandleAssetPostImport(class UFactory* InFactory, UObject* InObject);

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

		TArray<TObjectPtr<UChooserRowDetails>> SelectedRows;

		void UpdateTableColumns();
		TArray<TSharedPtr<FChooserTableRow>> TableRows;
	
		TSharedPtr<SComboButton> CreateColumnComboButton;
		TSharedPtr<SHeaderRow> HeaderRow;
		TSharedPtr<SListView<TSharedPtr<FChooserTableRow>>> TableView;

		TArray<TSharedPtr<SEditableText>> ColumnText;

		FName SelectedColumn;
		bool EditColumnName = false;
	public:

		static TMap<const UClass*, TFunction<TSharedRef<SWidget> (UObject* Column, int Row)>> ColumnWidgetCreators;
	
		/** The name given to all instances of this type of editor */
		static const FName ToolkitFName;

		static TSharedRef<FChooserTableEditor> CreateEditor( const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, UObject* ObjectToEdit, FGetDetailsViewObjects GetDetailsViewObjects = FGetDetailsViewObjects() );

		static TSharedRef<FChooserTableEditor> CreateEditor( const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, const TArray<UObject*>& ObjectsToEdit, FGetDetailsViewObjects GetDetailsViewObjects = FGetDetailsViewObjects() );

		static void RegisterWidgets();
	};
}
