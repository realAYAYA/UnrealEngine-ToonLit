// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChooserTableEditor.h"

#include "Framework/Docking/TabManager.h"
#include "Modules/ModuleManager.h"
#include "Styling/AppStyle.h"
#include "Widgets/Docking/SDockTab.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SHyperlink.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Input/SComboButton.h"
#include "SAssetDropTarget.h"
#include "SClassViewer.h"
#include "StructViewerModule.h"
#include "SourceCodeNavigation.h"
#include "Chooser.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "ClassViewerFilter.h"
#include "IPropertyAccessEditor.h"
#include "LandscapeRender.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "DragAndDrop/DecoratedDragDropOp.h"
#include "ObjectChooser_Asset.h"
#include "ObjectChooserClassFilter.h"
#include "ObjectChooserWidgetFactories.h"
#include "GraphEditorSettings.h"
#include "IDetailCustomization.h"
#include "PropertyCustomizationHelpers.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "ChooserEditor"

namespace UE::ChooserEditor
{

const FName FChooserTableEditor::ToolkitFName( TEXT( "ChooserTableEditor" ) );
const FName FChooserTableEditor::PropertiesTabId( TEXT( "ChooserEditor_Properties" ) );
const FName FChooserTableEditor::TableTabId( TEXT( "ChooserEditor_Table" ) );

void FChooserTableEditor::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_ChooserTableEditor", "Chooser Table Editor"));

	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	InTabManager->RegisterTabSpawner( PropertiesTabId, FOnSpawnTab::CreateSP(this, &FChooserTableEditor::SpawnPropertiesTab) )
		.SetDisplayName( LOCTEXT("PropertiesTab", "Details") )
		.SetGroup(WorkspaceMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon("EditorStyle", "LevelEditor.Tabs.Details"));
	
	InTabManager->RegisterTabSpawner( TableTabId, FOnSpawnTab::CreateSP(this, &FChooserTableEditor::SpawnTableTab) )
		.SetDisplayName( LOCTEXT("TableTab", "Chooser Table") )
		.SetGroup(WorkspaceMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon("EditorStyle", "LevelEditor.Tabs.Details"));
}

void FChooserTableEditor::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);

	InTabManager->UnregisterTabSpawner( TableTabId );
	InTabManager->UnregisterTabSpawner( PropertiesTabId );
}

const FName FChooserTableEditor::ChooserEditorAppIdentifier( TEXT( "ChooserEditorApp" ) );

FChooserTableEditor::~FChooserTableEditor()
{
	FCoreUObjectDelegates::OnObjectsReplaced.RemoveAll(this);
	
	DetailsView.Reset();
}


void FChooserTableEditor::InitEditor( const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, const TArray<UObject*>& ObjectsToEdit, FGetDetailsViewObjects GetDetailsViewObjects )
{
	EditingObjects = ObjectsToEdit;
	FCoreUObjectDelegates::OnObjectsReplaced.AddSP(this, &FChooserTableEditor::OnObjectsReplaced);

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>( "PropertyEditor" );
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.NotifyHook = this;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsView = PropertyEditorModule.CreateDetailView( DetailsViewArgs );
	const TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout = FTabManager::NewLayout( "Standalone_ChooserTableEditor_Layout_v1" )
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
	FAssetEditorToolkit::InitAssetEditor( Mode, InitToolkitHost, FChooserTableEditor::ChooserEditorAppIdentifier, StandaloneDefaultLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, ObjectsToEdit );

	RegenerateMenusAndToolbars();

	SelectRootProperties();
}

FName FChooserTableEditor::GetToolkitFName() const
{
	return ToolkitFName;
}

FText FChooserTableEditor::GetBaseToolkitName() const
{
	return LOCTEXT("AppLabel", "Chooser Table Editor");
}

void FChooserTableEditor::PostUndo(bool bSuccess)
{
	UpdateTableColumns();
	UpdateTableRows();
}
	
void FChooserTableEditor::PostRedo(bool bSuccess)
{
	UpdateTableColumns();
	UpdateTableRows();
}

	
void FChooserTableEditor::NotifyPreChange(FProperty* PropertyAboutToChange)
{
}

void FChooserTableEditor::NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, FProperty* PropertyThatChanged)
{
	if (PropertyThatChanged->GetNameCPP() == GET_MEMBER_NAME_STRING_CHECKED(UChooserTable, OutputObjectType)              // if you change the Output type, we need to update all the Results asset pickers
	   || PropertyChangedEvent.Property->GetNameCPP() == GET_MEMBER_NAME_STRING_CHECKED(UChooserTable, ResultsStructs) )   // if you change the type of a Result, we need to update that widget
	{
		// rebuild all result widgets
		UpdateTableRows();
	}
	if (PropertyThatChanged->GetNameCPP() == GET_MEMBER_NAME_STRING_CHECKED(UChooserTable, ContextObjectType))
	{
		// rebuild all column header widgets
		UpdateTableColumns();
	}
}

	
FText FChooserTableEditor::GetToolkitName() const
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

FText FChooserTableEditor::GetToolkitToolTipText() const
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

FLinearColor FChooserTableEditor::GetWorldCentricTabColorScale() const
{
	return FLinearColor( 0.5f, 0.0f, 0.0f, 0.5f );
}

void FChooserTableEditor::SetPropertyVisibilityDelegate(FIsPropertyVisible InVisibilityDelegate)
{
	DetailsView->SetIsPropertyVisibleDelegate(InVisibilityDelegate);
	DetailsView->ForceRefresh();
}

void FChooserTableEditor::SetPropertyEditingEnabledDelegate(FIsPropertyEditingEnabled InPropertyEditingDelegate)
{
	DetailsView->SetIsPropertyEditingEnabledDelegate(InPropertyEditingDelegate);
	DetailsView->ForceRefresh();
}

TSharedRef<SDockTab> FChooserTableEditor::SpawnPropertiesTab( const FSpawnTabArgs& Args )
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


class FChooserRowDragDropOp : public FDecoratedDragDropOp
{
public:
	DRAG_DROP_OPERATOR_TYPE(FWidgetTemplateDragDropOp, FDecoratedDragDropOp)

	FChooserTableEditor* ChooserEditor;
	uint32 RowIndex;

	/** Constructs the drag drop operation */
	static TSharedRef<FChooserRowDragDropOp> New(FChooserTableEditor* InEditor, uint32 InRowIndex)
	{
		TSharedRef<FChooserRowDragDropOp> Operation = MakeShareable(new FChooserRowDragDropOp());
		Operation->ChooserEditor = InEditor;
		Operation->RowIndex = InRowIndex;
		Operation->DefaultHoverText = LOCTEXT("Chooser Row", "Chooser Row");
		Operation->CurrentHoverText = Operation->DefaultHoverText;
			
		Operation->Construct();
	
		return Operation;
	};
};

class SChooserRowHandle : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SChooserRowHandle)
	{}
	SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_ARGUMENT(FChooserTableEditor*, ChooserEditor)
	SLATE_ARGUMENT(uint32, RowIndex)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		ChooserEditor = InArgs._ChooserEditor;
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
		TSharedRef<FChooserRowDragDropOp> DragDropOp = FChooserRowDragDropOp::New(ChooserEditor, RowIndex);
		return FReply::Handled().BeginDragDrop(DragDropOp);
	}

private:
	FChooserTableEditor* ChooserEditor = nullptr;
	uint32 RowIndex;
};


class SChooserTableRow : public SMultiColumnTableRow<TSharedPtr<FChooserTableEditor::FChooserTableRow>>
{
public:
	SLATE_BEGIN_ARGS(SChooserTableRow) {}
		/** The list item for this row */
		SLATE_ARGUMENT(TSharedPtr<FChooserTableEditor::FChooserTableRow>, Entry)
		SLATE_ARGUMENT(UChooserTable*, Chooser)
		SLATE_ARGUMENT(FChooserTableEditor*, Editor)
	SLATE_END_ARGS()


	void Construct(const FArguments& Args, const TSharedRef<STableViewBase>& OwnerTableView)
	{
		RowIndex = Args._Entry;
		Chooser = Args._Chooser;
		Editor = Args._Editor;

		SMultiColumnTableRow<TSharedPtr<FChooserTableEditor::FChooserTableRow>>::Construct(
			FSuperRowType::FArguments(),
			OwnerTableView
		);
	}

	/** Overridden from SMultiColumnTableRow.  Generates a widget for this column of the list view. */
	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
	{
		static FName Result = "Result";
		static FName Handles = "Handles";
		
		if (RowIndex->RowIndex < Chooser->ResultsStructs.Num())
		{
			if (ColumnName == Handles && RowIndex->RowIndex < Chooser->ResultsStructs.Num())
			{
				// row drag handle
				
				return SNew(SChooserRowHandle).ChooserEditor(Editor).RowIndex(RowIndex->RowIndex);
			}
			else if (ColumnName == Result) 
			{
				TSharedPtr<SWidget> ResultWidget = FObjectChooserWidgetFactories::CreateWidget(Chooser, FObjectChooserBase::StaticStruct(), Chooser->ResultsStructs[RowIndex->RowIndex].GetMutableMemory(), Chooser->ResultsStructs[RowIndex->RowIndex].GetScriptStruct(),Chooser->ContextObjectType,
				FOnStructPicked::CreateLambda([this, RowIndex=RowIndex->RowIndex](const UScriptStruct* ChosenStruct)
				{
					const FScopedTransaction Transaction(LOCTEXT("Change Row Result Type", "Change Row Result Type"));
					Chooser->ResultsStructs[RowIndex].InitializeAs(ChosenStruct);
					Chooser->Modify(true);
					FObjectChooserWidgetFactories::CreateWidget(Chooser, FObjectChooserBase::StaticStruct(), Chooser->ResultsStructs[RowIndex].GetMutableMemory(), ChosenStruct, Chooser->ContextObjectType, FOnStructPicked(), &CacheBorder);
				}),
				&CacheBorder
				);
				return ResultWidget.ToSharedRef();
			}
			else
			{
				const int ColumnIndex = ColumnName.GetNumber() - 1;
				if (ColumnIndex < Chooser->ColumnsStructs.Num() && ColumnIndex >=0)
				{
					FChooserColumnBase* Column = &Chooser->ColumnsStructs[ColumnIndex].GetMutable<FChooserColumnBase>();
					const UStruct * ColumnStruct = Chooser->ColumnsStructs[ColumnIndex].GetScriptStruct();
					TSharedPtr<SWidget> ColumnWidget;
					while (ColumnStruct && !ColumnWidget.IsValid())
					{
						if (auto Creator = FChooserTableEditor::ColumnWidgetCreators.Find(ColumnStruct))
						{
							ColumnWidget = (*Creator)(Chooser, Column, RowIndex->RowIndex);
							break;
						}
						ColumnStruct = ColumnStruct->GetSuperStruct();
					}
					
					if (ColumnWidget.IsValid())
					{
						return ColumnWidget.ToSharedRef();
					}
				}
			}
		}
		else if (RowIndex->RowIndex == Chooser->ResultsStructs.Num())
        {
			// on the row past the end, show an Add button in the result column
			if (ColumnName == Result)
			{
				return Editor->GetCreateRowComboButton().ToSharedRef();
			}
		}
		return SNullWidget::NullWidget;
	}

private:
	TSharedPtr<FChooserTableEditor::FChooserTableRow> RowIndex;
	UChooserTable* Chooser;
	FChooserTableEditor* Editor;
	TSharedPtr<SBorder> CacheBorder;
};


TSharedRef<ITableRow> FChooserTableEditor::GenerateTableRow(TSharedPtr<FChooserTableRow> InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	UChooserTable* Chooser = Cast<UChooserTable>(EditingObjects[0]);
	
	return SNew(SChooserTableRow, OwnerTable)
		.Entry(InItem).Chooser(Chooser).Editor(this);
}

FReply FChooserTableEditor::SelectRootProperties()
{
	if( DetailsView.IsValid() )
	{
		// Make sure details window is pointing to our object
		DetailsView->SetObjects( EditingObjects );
	}

	return FReply::Handled();
}


void FChooserTableEditor::UpdateTableColumns()
{
	UChooserTable* Chooser = Cast<UChooserTable>(EditingObjects[0]);
	
	HeaderRow->ClearColumns();
	
	HeaderRow->AddColumn(SHeaderRow::Column("Handles")
					.ManualWidth(30)
					.HeaderContent()
					[					
						SNew(SButton).OnClicked_Raw(this, &FChooserTableEditor::SelectRootProperties)
					]);
	
	HeaderRow->AddColumn(SHeaderRow::Column("Result")
					.DefaultLabel(LOCTEXT("ResultColumnName", "Result"))
					.ManualWidth(300));

	FName ColumnId("ChooserColumn", 1);
	int NumColumns = Chooser->ColumnsStructs.Num();	
	for(int ColumnIndex = 0; ColumnIndex < NumColumns; ColumnIndex++)
	{
		FChooserColumnBase& Column = Chooser->ColumnsStructs[ColumnIndex].GetMutable<FChooserColumnBase>();
		TSharedPtr<SWidget> HeaderWidget = nullptr;
		if (FChooserParameterBase* InputValue = Column.GetInputValue())
		{
			HeaderWidget = FObjectChooserWidgetFactories::CreateWidget(Chooser, InputValue, Column.GetInputType(), Chooser->ContextObjectType);
		}

		HeaderRow->AddColumn(SHeaderRow::FColumn::FArguments()
			.ColumnId(ColumnId)
			.ManualWidth(200)
			.OnGetMenuContent_Lambda([this, &Column, Chooser, ColumnIndex, ColumnId]()
			{
				UChooserColumnMenuContext* MenuContext = NewObject<UChooserColumnMenuContext>();
				MenuContext->Editor = this;
                MenuContext->Chooser = Chooser;
                MenuContext->ColumnIndex = ColumnIndex;

				FMenuBuilder MenuBuilder(true,nullptr);

				MenuBuilder.AddMenuEntry(LOCTEXT("Column Properties","Properties"),LOCTEXT("Delete Column ToolTip", "Remove this column and all its data from the table"),FSlateIcon(),
					FUIAction(
						FExecuteAction::CreateLambda([this,Chooser,ColumnIndex, ColumnId, &Column]()
						{
							SelectColumn(ColumnId.GetNumber() - 1);
						})
						)
					);

				MenuBuilder.AddMenuEntry(LOCTEXT("Delete Column","Delete"),LOCTEXT("Delete Column ToolTip", "Remove this column and all its data from the table"),FSlateIcon(),
					FUIAction(
						FExecuteAction::CreateLambda([this,Chooser,ColumnIndex, &Column]()
						{
							const FScopedTransaction Transaction(LOCTEXT("Delete Column Transaction", "Delete Column"));
							Chooser->Modify(true);
							Chooser->ColumnsStructs.RemoveAt(ColumnIndex);
							UpdateTableColumns();
						})
						));
				
				MenuBuilder.AddSubMenu(LOCTEXT("Input Type", "Input Type"),
					LOCTEXT("InputTypeToolTip", "Change input parameter type"),
					FNewMenuDelegate::CreateLambda([this, Chooser, &ColumnIndex](FMenuBuilder& Builder)
					{
						FStructViewerInitializationOptions Options;
						Options.StructFilter = MakeShared<FStructFilter>(Chooser->ColumnsStructs[ColumnIndex].Get<FChooserColumnBase>().GetInputBaseType());
						Options.NameTypeToDisplay = EStructViewerNameTypeToDisplay::DisplayName;
						
						// Add class filter for columns here
						TSharedRef<SWidget> Widget = FModuleManager::LoadModuleChecked<FStructViewerModule>("StructViewer").CreateStructViewer(Options, FOnStructPicked::CreateLambda([this, ColumnIndex](const UScriptStruct* ChosenStruct)
						{
							const FScopedTransaction Transaction(LOCTEXT("SetColumnInputType", "Set Column Input Type"));
							UChooserTable* ChooserTable = Cast<UChooserTable>(EditingObjects[0]);
							ChooserTable->ColumnsStructs[ColumnIndex].GetMutable<FChooserColumnBase>().SetInputType(ChosenStruct);
							ChooserTable->Modify(true);
							UpdateTableColumns();
							UpdateTableRows();
						}));
					
						Builder.AddWidget(Widget, FText());
					}));
				
				return MenuBuilder.MakeWidget();
				
			})
			.HeaderContent()
			[
				SNew(SBorder)
				.VAlign(VAlign_Center)
				.Padding(3)
				.BorderBackgroundColor_Lambda([this, ColumnId] ()
				{
					// unclear why this color is coming out much darker
					return (SelectedColumn && SelectedColumn->Column == ColumnId.GetNumber() - 1) ? FSlateColor(FColor(0x00, 0x70, 0xe0, 0xFF)) : FSlateColor(FLinearColor(0.05f,0.05f,0.05f));
				})
				.OnMouseButtonDown_Lambda([this, ColumnIndex, ColumnId](	const FGeometry&, const FPointerEvent& PointerEvent)
				{
					UChooserTable* Chooser = Cast<UChooserTable>(EditingObjects[0]);
					TableView->ClearSelection();
					
					SelectColumn(ColumnId.GetNumber() - 1);
					return FReply::Handled();
				})
				[
					HeaderWidget ? HeaderWidget.ToSharedRef() : SNullWidget::NullWidget
				]
				
			]);
		
		ColumnId.SetNumber(ColumnId.GetNumber() + 1);
	}

	HeaderRow->AddColumn( SHeaderRow::FColumn::FArguments()
		.ColumnId("Add")
		.FillWidth(1.0)
		.HeaderContent( )
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().MaxWidth(120)
			[
				CreateColumnComboButton.ToSharedRef()
			]
		]
		);

}

TSharedRef<SDockTab> FChooserTableEditor::SpawnTableTab( const FSpawnTabArgs& Args )
{
	check( Args.GetTabId() == TableTabId );

	UChooserTable* Chooser = Cast<UChooserTable>(EditingObjects[0]);
	
	// + button to create new columns
	
	CreateColumnComboButton = SNew(SComboButton).OnGetMenuContent_Lambda([this]()
	{
		FStructViewerInitializationOptions Options;
		Options.StructFilter = MakeShared<FStructFilter>(FChooserColumnBase::StaticStruct());
		Options.NameTypeToDisplay = EStructViewerNameTypeToDisplay::DisplayName;
		
		// Add class filter for columns here
		TSharedRef<SWidget> Widget = FModuleManager::LoadModuleChecked<FStructViewerModule>("StructViewer").CreateStructViewer(Options, FOnStructPicked::CreateLambda([this](const UScriptStruct* ChosenStruct)
		{
			CreateColumnComboButton->SetIsOpen(false);
			UChooserTable* Chooser = Cast<UChooserTable>(EditingObjects[0]);
			const FScopedTransaction Transaction(LOCTEXT("Add Column Transaction", "Add Column"));
			Chooser->Modify(true);

			Chooser->ColumnsStructs.SetNum(Chooser->ColumnsStructs.Num()+1);
			Chooser->ColumnsStructs.Last().InitializeAs(ChosenStruct);
			
			UpdateTableColumns();
			UpdateTableRows();

			SelectColumn(Chooser->ColumnsStructs.Num() - 1);
		}));
		return Widget;
	})
	.ComboButtonStyle(FAppStyle::Get(), "SimpleComboButton")
	.ButtonContent()
	[
		SNew(STextBlock).Text(LOCTEXT("AddColumn", "+ Add Column"))
	];


	CreateRowComboButton = SNew(SComboButton)
		.ComboButtonStyle(FAppStyle::Get(), "SimpleComboButton")
		.ButtonContent()
		[
			SNew(STextBlock).Text(LOCTEXT("AddRow", "+ Add Row"))
		]
		.OnGetMenuContent_Lambda([this]()
		{
			FStructViewerInitializationOptions Options;
			Options.StructFilter = MakeShared<FStructFilter>(FObjectChooserBase::StaticStruct());
			Options.NameTypeToDisplay = EStructViewerNameTypeToDisplay::DisplayName;
			
			TSharedRef<SWidget> Widget = FModuleManager::LoadModuleChecked<FStructViewerModule>("StructViewer").CreateStructViewer(Options, FOnStructPicked::CreateLambda([this](const UScriptStruct* ChosenStruct)
			{
				CreateRowComboButton->SetIsOpen(false);
				UChooserTable* Chooser = Cast<UChooserTable>(EditingObjects[0]);
				const FScopedTransaction Transaction(LOCTEXT("Add Row Transaction", "Add Row"));
				Chooser->Modify(true);

				Chooser->ResultsStructs.SetNum(Chooser->ResultsStructs.Num()+1);
				Chooser->ResultsStructs.Last().InitializeAs(ChosenStruct);
				UpdateTableRows();
			}));
			
			return Widget;
		});

	HeaderRow = SNew(SHeaderRow);

	UpdateTableRows();
	UpdateTableColumns();

	TableView = SNew(SListView<TSharedPtr<FChooserTableRow>>)
    			.ListItemsSource(&TableRows)
				.OnKeyDownHandler_Lambda([this](const FGeometry&, const FKeyEvent& Event)
				{
					if (Event.GetKey() == EKeys::Delete)
					{
						const FScopedTransaction Transaction(LOCTEXT("Delete Row Transaction", "Delete Row"));
						UChooserTable* Chooser = Cast<UChooserTable>(EditingObjects[0]);
						Chooser->Modify(true);
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
							Chooser->ResultsStructs.RemoveAt(RowIndex);
						}

						for(FInstancedStruct& ColumnData : Chooser->ColumnsStructs)
						{
							FChooserColumnBase& Column = ColumnData.GetMutable<FChooserColumnBase>();
							Column.DeleteRows(RowsToDelete);
						}

						UpdateTableRows();
						
						return FReply::Handled();
					}
					return FReply::Unhandled();
				}
				)
				.OnSelectionChanged_Lambda([this](TSharedPtr<FChooserTableRow> SelectedItem,  ESelectInfo::Type SelectInfo)
				{
					SelectedColumn = nullptr;
					if (SelectedItem)
					{
						SelectedRows.SetNum(0);
						UChooserTable* Chooser = Cast<UChooserTable>(EditingObjects[0]);
						// Get the list of objects to edit the details of
						TObjectPtr<UChooserRowDetails> Selection = NewObject<UChooserRowDetails>();
						Selection->Chooser = Chooser;
						Selection->Row = SelectedItem->RowIndex;
						Selection->SetFlags(RF_Transactional);
						SelectedRows.Add(Selection);
											
						TArray<UObject*> DetailsObjects;
						for(auto& Item : SelectedRows)
						{
							DetailsObjects.Add(Item.Get());
						}

						if( DetailsView.IsValid() )
						{
							// Make sure details window is pointing to our object
							DetailsView->SetObjects( DetailsObjects );
						}
					}
				})
    			.OnGenerateRow_Raw(this, &FChooserTableEditor::GenerateTableRow)
				.HeaderRow(HeaderRow);
	
	return SNew(SDockTab)
		.Label( LOCTEXT("ChooserTableTitle", "Chooser Table") )
		.TabColorScale( GetTabColorScale() )
		.OnCanCloseTab_Lambda([]() { return false; })
		[
			TableView.ToSharedRef()
		];
}

void FChooserTableEditor::UpdateTableRows()
{
	UChooserTable* Chooser = Cast<UChooserTable>(EditingObjects[0]);
	int32 OldNum = TableRows.Num();
	int32 NewNum = Chooser->ResultsStructs.Num();

	// Sync the TableRows array which drives the ui table to match the number of results.
	// Add 1 at the end, for the "Add Row" control
	TableRows.SetNum(NewNum + 1);
	
	for(int32 i = OldNum; i < NewNum+1; i++)
	{
		TableRows[i] = MakeShared<FChooserTableRow>(i);
	}

	// Make sure each column has the same number of row datas as there are results
	for(FInstancedStruct& ColumnData : Chooser->ColumnsStructs)
	{
		FChooserColumnBase& Column = ColumnData.GetMutable<FChooserColumnBase>();
		Column.SetNumRows(NewNum);
	}

	if (TableView.IsValid())
	{
		TableView->RebuildList();
	}
}

void FChooserTableEditor::OnObjectsReplaced(const TMap<UObject*, UObject*>& ReplacementMap)
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

FString FChooserTableEditor::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix", "Chooser Table Asset ").ToString();
}

TSharedRef<FChooserTableEditor> FChooserTableEditor::CreateEditor( const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, UObject* ObjectToEdit, FGetDetailsViewObjects GetDetailsViewObjects )
{
	TSharedRef< FChooserTableEditor > NewEditor( new FChooserTableEditor() );

	TArray<UObject*> ObjectsToEdit;
	ObjectsToEdit.Add( ObjectToEdit );
	NewEditor->InitEditor( Mode, InitToolkitHost, ObjectsToEdit, GetDetailsViewObjects );

	return NewEditor;
}

TSharedRef<FChooserTableEditor> FChooserTableEditor::CreateEditor( const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, const TArray<UObject*>& ObjectsToEdit, FGetDetailsViewObjects GetDetailsViewObjects )
{
	TSharedRef< FChooserTableEditor > NewEditor( new FChooserTableEditor() );
	NewEditor->InitEditor( Mode, InitToolkitHost, ObjectsToEdit, GetDetailsViewObjects );
	return NewEditor;
}
	
void FChooserTableEditor::SelectColumn(int Index)
{
	UChooserTable* Chooser = GetChooser();
	if (Index < Chooser->ColumnsStructs.Num())
	{
		if (SelectedColumn == nullptr)
		{
			SelectedColumn = NewObject<UChooserColumnDetails>();
			SelectedColumn->Chooser = Chooser;
		}
		SelectedColumn->Column = Index;
		DetailsView->SetObject(SelectedColumn, true);
	}
}
	
void FChooserTableEditor::DeleteColumn(int Index)
{
	UChooserTable* Chooser = GetChooser();
	if (Index < Chooser->ColumnsStructs.Num())
	{
		Chooser->ColumnsStructs.RemoveAt(Index);
		UpdateTableColumns();
	}
}

/// Result widgets

TMap<const UStruct*, TFunction<TSharedRef<SWidget> (UChooserTable* Chooser, FChooserColumnBase* Column, int Row)>> FChooserTableEditor::ColumnWidgetCreators;

TSharedRef<SWidget> CreateAssetWidget(UObject* TransactionObject, void* Value, UClass* ContextClass)
{
	FAssetChooser* DIAsset = static_cast<FAssetChooser*>(Value);

	UObject* Asset = DIAsset->Asset;

	UChooserTable* Chooser = Cast<UChooserTable>(TransactionObject);
	
	return SNew(SObjectPropertyEntryBox)
		.AllowedClass((Chooser!=nullptr && Chooser->OutputObjectType!=nullptr) ? Chooser->OutputObjectType.Get() : UObject::StaticClass())
		.ObjectPath_Lambda([DIAsset](){ return DIAsset->Asset ? DIAsset->Asset.GetPath() : "";})
		.OnObjectChanged_Lambda([TransactionObject, DIAsset](const FAssetData& AssetData)
		{
			const FScopedTransaction Transaction(LOCTEXT("Edit Asset", "Edit Asset"));
			TransactionObject->Modify(true);
			DIAsset->Asset = AssetData.GetAsset();
		});
}

TSharedRef<SWidget> CreateEvaluateChooserWidget(UObject* TransactionObject, void* Value, UClass* ContextObject)
{
	FEvaluateChooser* EvaluateChooser = static_cast<FEvaluateChooser*>(Value);
	
	return SNew(SObjectPropertyEntryBox)
		.AllowedClass(UChooserTable::StaticClass())
		.ObjectPath_Lambda([EvaluateChooser](){ return EvaluateChooser->Chooser ? EvaluateChooser->Chooser.GetPath() : "";})
		.OnObjectChanged_Lambda([TransactionObject, EvaluateChooser](const FAssetData& AssetData)
		{
			const FScopedTransaction Transaction(LOCTEXT("Edit Chooser", "Edit Chooser"));
			TransactionObject->Modify(true);
			EvaluateChooser->Chooser = Cast<UChooserTable>(AssetData.GetAsset());
		});
}
	
class FChooserDetails : public IDetailCustomization
{
public:
	FChooserDetails() {};
	virtual ~FChooserDetails() override {};

	static TSharedRef<IDetailCustomization> MakeInstance()
	{
		return MakeShareable( new FChooserDetails() );
	}

	// IDetailCustomization interface
	virtual void CustomizeDetails(class IDetailLayoutBuilder& DetailBuilder) override;
};
		
void FChooserDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailBuilder.GetObjectsBeingCustomized(Objects);

	UChooserTable* Chooser = Cast<UChooserTable>(Objects[0]);
	
	IDetailCategoryBuilder& HiddenCategory = DetailBuilder.EditCategory(TEXT("Hidden"));

	TArray<TSharedRef<IPropertyHandle>> HiddenProperties;
	HiddenCategory.GetDefaultProperties(HiddenProperties);
	for(TSharedRef<IPropertyHandle>& PropertyHandle :  HiddenProperties)
	{
		// these (Results and Columns arrays) need to be hidden when showing the root ChooserTable properties
		// but still need to be EditAnywhere so that the Properties exist for displaying when you select a row or column (eg by FChooserRowDetails below)
		PropertyHandle->MarkHiddenByCustomization();
	}
}

class FChooserRowDetails : public IDetailCustomization
{
public:
	FChooserRowDetails() {};
	virtual ~FChooserRowDetails() override {};

	static TSharedRef<IDetailCustomization> MakeInstance()
	{
		return MakeShareable( new FChooserRowDetails() );
	}

	// IDetailCustomization interface
	virtual void CustomizeDetails(class IDetailLayoutBuilder& DetailBuilder) override;
};

// Make the details panel show the values for the selected row, showing each column value
void FChooserRowDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailBuilder.GetObjectsBeingCustomized(Objects);

	UChooserRowDetails* Row = Cast<UChooserRowDetails>(Objects[0]);
	UChooserTable* Chooser = Row->Chooser;
	
	if (Chooser->ResultsStructs.Num() > Row->Row)
	{
		IDetailCategoryBuilder& PropertiesCategory = DetailBuilder.EditCategory("Row Properties");

		TSharedPtr<IPropertyHandle> ChooserProperty = DetailBuilder.GetProperty("Chooser", Row->StaticClass());
		DetailBuilder.HideProperty(ChooserProperty);
	
		TSharedPtr<IPropertyHandle> ResultsArrayProperty = ChooserProperty->GetChildHandle("ResultsStructs");
		TSharedPtr<IPropertyHandle> CurrentResultProperty = ResultsArrayProperty->AsArray()->GetElement(Row->Row);
		IDetailPropertyRow& NewResultProperty = PropertiesCategory.AddProperty(CurrentResultProperty);
		NewResultProperty.DisplayName(LOCTEXT("ResultColumnName","Result"));
		NewResultProperty.ShowPropertyButtons(false); // hide array add button
		NewResultProperty.ShouldAutoExpand(true);
	
		for(int ColumnIndex=0; ColumnIndex<Chooser->ColumnsStructs.Num(); ColumnIndex++)
		{
			FChooserColumnBase& Column = Chooser->ColumnsStructs[ColumnIndex].GetMutable<FChooserColumnBase>();
			TSharedRef<FStructOnScope> StructOnScope = MakeShared<FStructOnScope>(Chooser->ColumnsStructs[ColumnIndex].GetScriptStruct(), reinterpret_cast<uint8*>(&Column));
			TSharedPtr<IPropertyHandle> ColumnDataProperty = DetailBuilder.AddStructurePropertyData({StructOnScope}, "RowValues");
			uint32 NumElements = 0;
			ColumnDataProperty->AsArray()->GetNumElements(NumElements);
			if (Row->Row < (int)NumElements)
			{
				TSharedRef<IPropertyHandle> CellData = ColumnDataProperty->AsArray()->GetElement(Row->Row);
	
				IDetailPropertyRow& NewColumnProperty = PropertiesCategory.AddProperty(CellData);
				FText DisplayName = LOCTEXT("No Input Value", "No Input Value");
				if (FChooserParameterBase* InputValue = Column.GetInputValue())
				{
					InputValue->GetDisplayName(DisplayName); 
				}
				NewColumnProperty.DisplayName(DisplayName);
				NewColumnProperty.ShowPropertyButtons(false); // hide array add button
				NewColumnProperty.ShouldAutoExpand(true);
			}
		}
	}
}

class FChooserColumnDetails : public IDetailCustomization
{
public:
	FChooserColumnDetails() {};
	virtual ~FChooserColumnDetails() override {};

	static TSharedRef<IDetailCustomization> MakeInstance()
	{
		return MakeShareable( new FChooserColumnDetails() );
	}

	// IDetailCustomization interface
	virtual void CustomizeDetails(class IDetailLayoutBuilder& DetailBuilder) override;
};

// Make the details panel show the values for the selected row, showing each column value
void FChooserColumnDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailBuilder.GetObjectsBeingCustomized(Objects);

	UChooserColumnDetails* Column = Cast<UChooserColumnDetails>(Objects[0]);
	UChooserTable* Chooser = Column->Chooser;
	
	if (Chooser->ColumnsStructs.Num() > Column->Column)
	{
		IDetailCategoryBuilder& PropertiesCategory = DetailBuilder.EditCategory("Row Properties");

		TSharedPtr<IPropertyHandle> ChooserProperty = DetailBuilder.GetProperty("Chooser", Column->StaticClass());
		DetailBuilder.HideProperty(ChooserProperty);
	
		TSharedPtr<IPropertyHandle> ColumnsArrayProperty = ChooserProperty->GetChildHandle("ColumnsStructs");
		TSharedPtr<IPropertyHandle> CurrentColumnProperty = ColumnsArrayProperty->AsArray()->GetElement(Column->Column);
		IDetailPropertyRow& NewResultProperty = PropertiesCategory.AddProperty(CurrentColumnProperty);
		NewResultProperty.DisplayName(LOCTEXT("Selected Column","Selected Column"));
		NewResultProperty.ShowPropertyButtons(false); // hide array add button
		NewResultProperty.ShouldAutoExpand(true);
	}
}

void FChooserTableEditor::RegisterWidgets()
{
	// todo: fallback widget
	FObjectChooserWidgetFactories::ChooserWidgetCreators.Add(FAssetChooser::StaticStruct(), CreateAssetWidget);
	FObjectChooserWidgetFactories::ChooserWidgetCreators.Add(FEvaluateChooser::StaticStruct(), CreateEvaluateChooserWidget);
	

	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	
	PropertyModule.RegisterCustomClassLayout("ChooserTable", FOnGetDetailCustomizationInstance::CreateStatic(&FChooserDetails::MakeInstance));	
	PropertyModule.RegisterCustomClassLayout("ChooserRowDetails", FOnGetDetailCustomizationInstance::CreateStatic(&FChooserRowDetails::MakeInstance));	
	PropertyModule.RegisterCustomClassLayout("ChooserColumnDetails", FOnGetDetailCustomizationInstance::CreateStatic(&FChooserColumnDetails::MakeInstance));	
}
}

#undef LOCTEXT_NAMESPACE
