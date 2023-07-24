// Copyright Epic Games, Inc. All Rights Reserved.

#include "ProxyTableEditor.h"

#include "Framework/Docking/TabManager.h"
#include "Modules/ModuleManager.h"
#include "Styling/AppStyle.h"
#include "Widgets/Docking/SDockTab.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "Editor.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SHyperlink.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "SAssetDropTarget.h"
#include "SClassViewer.h"
#include "SourceCodeNavigation.h"
#include "ProxyTable.h"
#include "ClassViewerFilter.h"
#include "IPropertyAccessEditor.h"
#include "LandscapeRender.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "DragAndDrop/DecoratedDragDropOp.h"
#include "GraphEditorSettings.h"
#include "ObjectChooserClassFilter.h"
#include "ScopedTransaction.h"
#include "ObjectChooserWidgetFactories.h"
#include "ContextPropertyWidget.h"
#include "IObjectChooser.h"
#include "Misc/TransactionObjectEvent.h"

#define LOCTEXT_NAMESPACE "ProxyTableEditor"

namespace UE::ProxyTableEditor
{

const FName FProxyTableEditor::ToolkitFName( TEXT( "GenericAssetEditor" ) );
const FName FProxyTableEditor::PropertiesTabId( TEXT( "ProxyEditor_Properties" ) );
const FName FProxyTableEditor::TableTabId( TEXT( "ProxyEditor_Table" ) );

void FProxyTableEditor::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_GenericAssetEditor", "Asset Editor"));

	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	InTabManager->RegisterTabSpawner( PropertiesTabId, FOnSpawnTab::CreateSP(this, &FProxyTableEditor::SpawnPropertiesTab) )
		.SetDisplayName( LOCTEXT("PropertiesTab", "Details") )
		.SetGroup(WorkspaceMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon("EditorStyle", "LevelEditor.Tabs.Details"));
	
	InTabManager->RegisterTabSpawner( TableTabId, FOnSpawnTab::CreateSP(this, &FProxyTableEditor::SpawnTableTab) )
		.SetDisplayName( LOCTEXT("TableTab", "Proxy Table") )
		.SetGroup(WorkspaceMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon("EditorStyle", "LevelEditor.Tabs.Details"));
}

void FProxyTableEditor::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);

	InTabManager->UnregisterTabSpawner( TableTabId );
	InTabManager->UnregisterTabSpawner( PropertiesTabId );
}

const FName FProxyTableEditor::ProxyEditorAppIdentifier( TEXT( "ProxyEditorApp" ) );

FProxyTableEditor::~FProxyTableEditor()
{
	GEditor->GetEditorSubsystem<UImportSubsystem>()->OnAssetPostImport.RemoveAll(this);
	FCoreUObjectDelegates::OnObjectsReplaced.RemoveAll(this);
	
	DetailsView.Reset();
}


void FProxyTableEditor::InitEditor( const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, const TArray<UObject*>& ObjectsToEdit, FGetDetailsViewObjects GetDetailsViewObjects )
{
	EditingObjects = ObjectsToEdit;
	FCoreUObjectDelegates::OnObjectsReplaced.AddSP(this, &FProxyTableEditor::OnObjectsReplaced);

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>( "PropertyEditor" );
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.NotifyHook = this;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsView = PropertyEditorModule.CreateDetailView( DetailsViewArgs );
	const TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout = FTabManager::NewLayout( "Standalone_ProxyTableEditor_Layout_v1" )
	->AddArea
	(
		FTabManager::NewPrimaryArea() ->SetOrientation(Orient_Vertical)
		->Split
		(
			FTabManager::NewSplitter()
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.7)
				->AddTab( TableTabId, ETabState::OpenedTab )
			)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.3)
				->AddTab( PropertiesTabId, ETabState::OpenedTab )
			)
		)
	);

	const bool bCreateDefaultStandaloneMenu = true;
	const bool bCreateDefaultToolbar = true;
	FAssetEditorToolkit::InitAssetEditor( Mode, InitToolkitHost, FProxyTableEditor::ProxyEditorAppIdentifier, StandaloneDefaultLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, ObjectsToEdit );

	RegenerateMenusAndToolbars();

	SelectRootProperties();
}

FName FProxyTableEditor::GetToolkitFName() const
{
	return ToolkitFName;
}

FText FProxyTableEditor::GetBaseToolkitName() const
{
	return LOCTEXT("AppLabel", "Proxy Table Editor");
}

void FProxyTableEditor::PostUndo(bool bSuccess)
{
	UpdateTableRows();
}
	
void FProxyTableEditor::PostRedo(bool bSuccess)
{
	UpdateTableRows();
}

	
void FProxyTableEditor::NotifyPreChange(FProperty* PropertyAboutToChange)
{
}

void FProxyTableEditor::NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, FProperty* PropertyThatChanged)
{
}

	
FText FProxyTableEditor::GetToolkitName() const
{
	const TArray<UObject*>& EditingObjs = GetEditingObjects();

	check( EditingObjs.Num() > 0 );

	FFormatNamedArguments Args;
	Args.Add( TEXT("ToolkitName"), GetBaseToolkitName() );

	if( EditingObjs.Num() == 1 )
	{
		const UObject* EditingObject = EditingObjs[ 0 ];
		return FText::FromString(EditingObject->GetName());
	}
	else
	{
		UClass* SharedBaseClass = nullptr;
		for( int32 x = 0; x < EditingObjs.Num(); ++x )
		{
			UObject* Obj = EditingObjs[ x ];
			check( Obj );

			UClass* ObjClass = Cast<UClass>(Obj);
			if (ObjClass == nullptr)
			{
				ObjClass = Obj->GetClass();
			}
			check( ObjClass );

			// Initialize with the class of the first object we encounter.
			if( SharedBaseClass == nullptr )
			{
				SharedBaseClass = ObjClass;
			}

			// If we've encountered an object that's not a subclass of the current best baseclass,
			// climb up a step in the class hierarchy.
			while( !ObjClass->IsChildOf( SharedBaseClass ) )
			{
				SharedBaseClass = SharedBaseClass->GetSuperClass();
			}
		}

		check(SharedBaseClass);

		Args.Add( TEXT("NumberOfObjects"), EditingObjs.Num() );
		Args.Add( TEXT("ClassName"), FText::FromString( SharedBaseClass->GetName() ) );
		return FText::Format( LOCTEXT("ToolkitTitle_EditingMultiple", "{NumberOfObjects} {ClassName} - {ToolkitName}"), Args );
	}
}

FText FProxyTableEditor::GetToolkitToolTipText() const
{
	const TArray<UObject*>& EditingObjs = GetEditingObjects();

	check( EditingObjs.Num() > 0 );

	FFormatNamedArguments Args;
	Args.Add( TEXT("ToolkitName"), GetBaseToolkitName() );

	if( EditingObjs.Num() == 1 )
	{
		const UObject* EditingObject = EditingObjs[ 0 ];
		return FAssetEditorToolkit::GetToolTipTextForObject(EditingObject);
	}
	else
	{
		UClass* SharedBaseClass = NULL;
		for( int32 x = 0; x < EditingObjs.Num(); ++x )
		{
			UObject* Obj = EditingObjs[ x ];
			check( Obj );

			UClass* ObjClass = Cast<UClass>(Obj);
			if (ObjClass == nullptr)
			{
				ObjClass = Obj->GetClass();
			}
			check( ObjClass );

			// Initialize with the class of the first object we encounter.
			if( SharedBaseClass == nullptr )
			{
				SharedBaseClass = ObjClass;
			}

			// If we've encountered an object that's not a subclass of the current best baseclass,
			// climb up a step in the class hierarchy.
			while( !ObjClass->IsChildOf( SharedBaseClass ) )
			{
				SharedBaseClass = SharedBaseClass->GetSuperClass();
			}
		}

		check(SharedBaseClass);

		Args.Add( TEXT("NumberOfObjects"), EditingObjs.Num() );
		Args.Add( TEXT("ClassName"), FText::FromString( SharedBaseClass->GetName() ) );
		return FText::Format( LOCTEXT("ToolkitTitle_EditingMultipleToolTip", "{NumberOfObjects} {ClassName} - {ToolkitName}"), Args );
	}
}

FLinearColor FProxyTableEditor::GetWorldCentricTabColorScale() const
{
	return FLinearColor( 0.5f, 0.0f, 0.0f, 0.5f );
}

void FProxyTableEditor::SetPropertyVisibilityDelegate(FIsPropertyVisible InVisibilityDelegate)
{
	DetailsView->SetIsPropertyVisibleDelegate(InVisibilityDelegate);
	DetailsView->ForceRefresh();
}

void FProxyTableEditor::SetPropertyEditingEnabledDelegate(FIsPropertyEditingEnabled InPropertyEditingDelegate)
{
	DetailsView->SetIsPropertyEditingEnabledDelegate(InPropertyEditingDelegate);
	DetailsView->ForceRefresh();
}

TSharedRef<SDockTab> FProxyTableEditor::SpawnPropertiesTab( const FSpawnTabArgs& Args )
{
	check( Args.GetTabId() == PropertiesTabId );

	return SNew(SDockTab)
		.Label( LOCTEXT("GenericDetailsTitle", "Details") )
		.TabColorScale( GetTabColorScale() )
		.OnCanCloseTab_Lambda([]() { return false; })
		[
			DetailsView.ToSharedRef()
		];
}


class FProxyRowDragDropOp : public FDecoratedDragDropOp
{
public:
	DRAG_DROP_OPERATOR_TYPE(FWidgetTemplateDragDropOp, FDecoratedDragDropOp)

	FProxyTableEditor* ChooserEditor;
	uint32 RowIndex;

	/** Constructs the drag drop operation */
	static TSharedRef<FProxyRowDragDropOp> New(FProxyTableEditor* InEditor, uint32 InRowIndex)
	{
		TSharedRef<FProxyRowDragDropOp> Operation = MakeShareable(new FProxyRowDragDropOp());
		Operation->ChooserEditor = InEditor;
		Operation->RowIndex = InRowIndex;
		Operation->DefaultHoverText = LOCTEXT("Proxy Row", "Proxy Row");
		// UE::ChooserEditor::FObjectChooserWidgetFactories::ConvertToText(InEditor->GetProxyTable()->Entries[InRowIndex].Value.GetObject(), Operation->DefaultHoverText);
		Operation->CurrentHoverText = Operation->DefaultHoverText;
			
		Operation->Construct();
	
		return Operation;
	};
};

class SProxyRowHandle : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SProxyRowHandle)
	{}
	SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_ARGUMENT(FProxyTableEditor*, ProxyEditor)
	SLATE_ARGUMENT(uint32, RowIndex)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		ProxyEditor = InArgs._ProxyEditor;
		RowIndex = InArgs._RowIndex;

		ChildSlot
		[
			SNew(SBox) .Padding(0.0f) .HAlign(HAlign_Center) .VAlign(VAlign_Center) .WidthOverride(16.0f)
			[
				SNew(SImage)
				.Image(FCoreStyle::Get().GetBrush("VerticalBoxDragIndicatorShort"))
			]
		];
	}

	FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		return FReply::Handled().DetectDrag(SharedThis(this), EKeys::LeftMouseButton);
	};

	FReply OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		TSharedRef<FProxyRowDragDropOp> DragDropOp = FProxyRowDragDropOp::New(ProxyEditor, RowIndex);
		return FReply::Handled().BeginDragDrop(DragDropOp);
	}

private:
	FProxyTableEditor* ProxyEditor = nullptr;
	uint32 RowIndex;
};


class SProxyTableRow : public SMultiColumnTableRow<TSharedPtr<FProxyTableEditor::FProxyTableRow>>
{
public:
	SLATE_BEGIN_ARGS(SProxyTableRow) {}
		/** The list item for this row */
		SLATE_ARGUMENT(TSharedPtr<FProxyTableEditor::FProxyTableRow>, Entry)
		SLATE_ARGUMENT(UProxyTable*, ProxyTable)
		SLATE_ARGUMENT(FProxyTableEditor*, Editor)
	SLATE_END_ARGS()


	void Construct(const FArguments& Args, const TSharedRef<STableViewBase>& OwnerTableView)
	{
		RowIndex = Args._Entry;
		ProxyTable = Args._ProxyTable;
		Editor = Args._Editor;

		SMultiColumnTableRow<TSharedPtr<FProxyTableEditor::FProxyTableRow>>::Construct(
			FSuperRowType::FArguments(),
			OwnerTableView
		);
	}

	/** Overridden from SMultiColumnTableRow.  Generates a widget for this column of the list view. */
	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
	{
		static FName Handles = "Handles";
		static FName Key = "Key";
		static FName Value = "Value";
		
		if (RowIndex->RowIndex < ProxyTable->Entries.Num())
		{
			if (ColumnName == Handles)
			{
				// row drag handle
				return SNew(SProxyRowHandle).ProxyEditor(Editor).RowIndex(RowIndex->RowIndex);
			}
			else if (ColumnName == Value) 
			{
				TSharedPtr<SWidget> ResultWidget = ChooserEditor::FObjectChooserWidgetFactories::CreateWidget(ProxyTable, FObjectChooserBase::StaticStruct(),
					ProxyTable->Entries[RowIndex->RowIndex].ValueStruct.GetMutableMemory(),
					ProxyTable->Entries[RowIndex->RowIndex].ValueStruct.GetScriptStruct(),
					nullptr/*ProxyTable->ContextObjectType*/,
					FOnStructPicked::CreateLambda([this, RowIndex=RowIndex->RowIndex](const UScriptStruct* ChosenStruct)
					{
						const FScopedTransaction Transaction(LOCTEXT("Change Value Type", "Change Value Type"));
						ProxyTable->Entries[RowIndex].ValueStruct.InitializeAs(ChosenStruct);
						ProxyTable->Modify(true);
						ChooserEditor::FObjectChooserWidgetFactories::CreateWidget(ProxyTable, FObjectChooserBase::StaticStruct(),
								ProxyTable->Entries[RowIndex].ValueStruct.GetMutableMemory(),
								ProxyTable->Entries[RowIndex].ValueStruct.GetScriptStruct(),
								nullptr/*ProxyTable->ContextObjectType*/, FOnStructPicked(), &CacheBorder);
					}),
					&CacheBorder
					);
				return ResultWidget.ToSharedRef();
			}
			else if (ColumnName == Key)
			{
				return SNew(SEditableTextBox)
					.Text_Lambda([this](){ return ProxyTable->Entries.Num() > RowIndex->RowIndex ?  FText::FromName(ProxyTable->Entries[RowIndex->RowIndex].Key) : FText();})
					.OnTextCommitted_Lambda([this](const FText& Text, ETextCommit::Type CommitType)
					{
						if (ProxyTable->Entries.Num() > RowIndex->RowIndex)
						{
							ProxyTable->Entries[RowIndex->RowIndex].Key = FName(Text.ToString());
						}
					});
			}
		}
		else if (RowIndex->RowIndex == ProxyTable->Entries.Num())
        {
			// on the row past the end, show an Add button in the result column
			if (ColumnName == Key)
			{
				return Editor->GetCreateRowComboButton().ToSharedRef();
			}
		}
		return SNullWidget::NullWidget;
	}

private:
	TSharedPtr<FProxyTableEditor::FProxyTableRow> RowIndex;
	UProxyTable* ProxyTable;
	FProxyTableEditor* Editor;
	TSharedPtr<SBorder> CacheBorder;
};


TSharedRef<ITableRow> FProxyTableEditor::GenerateTableRow(TSharedPtr<FProxyTableRow> InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	UProxyTable* ProxyTable = Cast<UProxyTable>(EditingObjects[0]);
	
	return SNew(SProxyTableRow, OwnerTable)
		.Entry(InItem).ProxyTable(ProxyTable).Editor(this);
}

FReply FProxyTableEditor::SelectRootProperties()
{
	if( DetailsView.IsValid() )
	{
		// Make sure details window is pointing to our object
		DetailsView->SetObjects( EditingObjects );
	}

	return FReply::Handled();
}


void FProxyTableEditor::UpdateTableColumns()
{
	HeaderRow->ClearColumns();
	HeaderRow->AddColumn(SHeaderRow::Column("Handles")
					.ManualWidth(30)
					.HeaderContent()
					[					
						SNew(SButton).OnClicked_Raw(this, &FProxyTableEditor::SelectRootProperties)
					]);
	
	HeaderRow->AddColumn(SHeaderRow::Column("Key")
					.DefaultLabel(LOCTEXT("KeyColumnName", "Key"))
					.ManualWidth(500));

	HeaderRow->AddColumn(SHeaderRow::Column("Value")
					.DefaultLabel(LOCTEXT("ValueColumnName", "Value"))
					.ManualWidth(500));

}

TSharedRef<SDockTab> FProxyTableEditor::SpawnTableTab( const FSpawnTabArgs& Args )
{
	check( Args.GetTabId() == TableTabId );

	UProxyTable* ProxyTable = Cast<UProxyTable>(EditingObjects[0]);
	
	CreateRowComboButton = SNew(SComboButton)
		.ComboButtonStyle(FAppStyle::Get(), "SimpleComboButton")
		.ButtonContent()
		[
			SNew(STextBlock).Text(LOCTEXT("AddRow", "+ Add Row"))
		]
		.OnGetMenuContent_Lambda([this]()
		{
			FStructViewerInitializationOptions Options;
			Options.StructFilter = MakeShared<ChooserEditor::FStructFilter>(FObjectChooserBase::StaticStruct());
			Options.NameTypeToDisplay = EStructViewerNameTypeToDisplay::DisplayName;
			
			TSharedRef<SWidget> Widget = FModuleManager::LoadModuleChecked<FStructViewerModule>("StructViewer").CreateStructViewer(Options, FOnStructPicked::CreateLambda([this](const UScriptStruct* ChosenStruct)
			{
				CreateRowComboButton->SetIsOpen(false);
				UProxyTable* ProxyTable = Cast<UProxyTable>(EditingObjects[0]);
				const FScopedTransaction Transaction(LOCTEXT("Add Row Transaction", "Add Row"));
				ProxyTable->Modify(true);

				ProxyTable->Entries.SetNum(ProxyTable->Entries.Num()+1);
				ProxyTable->Entries.Last().ValueStruct.InitializeAs(ChosenStruct);
				UpdateTableRows();
			}));
			
			return Widget;
		});

	HeaderRow = SNew(SHeaderRow);

	UpdateTableColumns();
	UpdateTableRows();

	TableView = SNew(SListView<TSharedPtr<FProxyTableRow>>)
    			.ListItemsSource(&TableRows)
				.OnKeyDownHandler_Lambda([this](const FGeometry&, const FKeyEvent& Event)
				{
					if (Event.GetKey() == EKeys::Delete)
					{
						const FScopedTransaction Transaction(LOCTEXT("Delete Row Transaction", "Delete Row"));
						UProxyTable* ProxyTable = Cast<UProxyTable>(EditingObjects[0]);
						ProxyTable->Modify(true);
						// delete selected rows.
						TArray<uint32> RowsToDelete;
						for(auto& SelectedRow:SelectedRows)
						{
							RowsToDelete.Add(SelectedRow->Row);
						}
						// sort indices in reverse
						RowsToDelete.Sort([](int32 A, int32 B){ return A>B; });
						for(uint32 RowIndex : RowsToDelete)
						{
							ProxyTable->Entries.RemoveAt(RowIndex);
						}

						UpdateTableRows();
						
						return FReply::Handled();
					}
					return FReply::Unhandled();
				}
				)
				.OnSelectionChanged_Lambda([this](TSharedPtr<FProxyTableRow> SelectedItem,  ESelectInfo::Type SelectInfo)
				{
					if (SelectedItem)
					{
						SelectedRows.SetNum(0);
						UProxyTable* ProxyTable = Cast<UProxyTable>(EditingObjects[0]);
						// Get the list of objects to edit the details of
						TObjectPtr<UProxyRowDetails> Selection = NewObject<UProxyRowDetails>();
						Selection->ProxyTable = ProxyTable;
						Selection->Row = SelectedItem->RowIndex;
						Selection->SetFlags(RF_Transactional); 
						SelectedRows.Add(Selection);
											
						if( DetailsView.IsValid() )
						{
							// Make sure details window is pointing to our object
							//DetailsView->SetObjects( ... )
						}
					}
				})
    			.OnGenerateRow_Raw(this, &FProxyTableEditor::GenerateTableRow)
				.HeaderRow(HeaderRow);
	
	return SNew(SDockTab)
		.Label( LOCTEXT("ProxtTableTitle", "Proxy Table") )
		.TabColorScale( GetTabColorScale() )
		.OnCanCloseTab_Lambda([]() { return false; })
		[
			TableView.ToSharedRef()
		];
}

void FProxyTableEditor::UpdateTableRows()
{
	UProxyTable* ProxyTable = Cast<UProxyTable>(EditingObjects[0]);
	int32 OldNum = TableRows.Num();
	int32 NewNum = ProxyTable->Entries.Num();

	// Sync the TableRows array which drives the ui table to match the number of results.
	// Add 1 at the end, for the "Add Row" control
	TableRows.SetNum(NewNum + 1);
	
	for(int32 i = OldNum; i < NewNum+1; i++)
	{
		TableRows[i] = MakeShared<FProxyTableRow>(i);
	}

	if (TableView.IsValid())
	{
		TableView->RebuildList();
	}
}

void FProxyTableEditor::OnObjectsReplaced(const TMap<UObject*, UObject*>& ReplacementMap)
{
	bool bChangedAny = false;

	// Refresh our details view if one of the objects replaced was in the map. This gets called before the reinstance GC fixup, so we might as well fixup EditingObjects now too
	for (int32 i = 0; i < EditingObjects.Num(); i++)
	{
		UObject* SourceObject = EditingObjects[i];
		UObject* ReplacedObject = ReplacementMap.FindRef(SourceObject);

		if (ReplacedObject && ReplacedObject != SourceObject)
		{
			EditingObjects[i] = ReplacedObject;
			bChangedAny = true;
		}
	}

	if (bChangedAny)
	{
		DetailsView->SetObjects(EditingObjects);
	}
}

FString FProxyTableEditor::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix", "Generic Asset ").ToString();
}
	
TSharedRef<FProxyTableEditor> FProxyTableEditor::CreateEditor( const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, UObject* ObjectToEdit, FGetDetailsViewObjects GetDetailsViewObjects )
{
	TSharedRef< FProxyTableEditor > NewEditor( new FProxyTableEditor() );

	TArray<UObject*> ObjectsToEdit;
	ObjectsToEdit.Add( ObjectToEdit );
	NewEditor->InitEditor( Mode, InitToolkitHost, ObjectsToEdit, GetDetailsViewObjects );

	return NewEditor;
}

TSharedRef<FProxyTableEditor> FProxyTableEditor::CreateEditor( const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, const TArray<UObject*>& ObjectsToEdit, FGetDetailsViewObjects GetDetailsViewObjects )
{
	TSharedRef< FProxyTableEditor > NewEditor( new FProxyTableEditor() );
	NewEditor->InitEditor( Mode, InitToolkitHost, ObjectsToEdit, GetDetailsViewObjects );
	return NewEditor;
}

/// Result widgets

TSharedRef<SWidget> CreateLookupProxyWidget(UObject* TransactionObject, void* Value, UClass* ContextObject)
{
	FLookupProxy* LookupProxy = static_cast<FLookupProxy*>(Value);
	
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		[
			UE::ChooserEditor::CreatePropertyWidget<FProxyTableContextProperty>(TransactionObject, LookupProxy->ProxyTable.GetMutablePtr<FProxyTableContextProperty>(), ContextObject, GetDefault<UGraphEditorSettings>()->ObjectPinTypeColor)
		]
		+ SHorizontalBox::Slot()
		[
			SNew(SEditableTextBox)
			.Text_Lambda([LookupProxy]() { return FText::FromName(LookupProxy->Key);})
			.OnTextChanged_Lambda([TransactionObject,LookupProxy](const FText& NewText)
			{
				FScopedTransaction ScopedTransaction(LOCTEXT("Change LookupProxy Key Name", "Change LookupProxy Key Name"));
				TransactionObject->Modify(true);
				LookupProxy->Key = FName(NewText.ToString());
			})
		];
}

void FProxyTableEditor::RegisterWidgets()
{
	UE::ChooserEditor::FObjectChooserWidgetFactories::ChooserWidgetCreators.Add(FLookupProxy::StaticStruct(), CreateLookupProxyWidget);
}
	
}

#undef LOCTEXT_NAMESPACE
