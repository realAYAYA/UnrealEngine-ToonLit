// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChooserTableEditor.h"

#include "Chooser.h"
#include "ChooserDetails.h"
#include "ChooserEditorWidgets.h"
#include "ChooserFindProperties.h"
#include "ChooserTableEditorCommands.h"
#include "ClassViewerFilter.h"
#include "DetailCategoryBuilder.h"
#include "GraphEditorSettings.h"
#include "IDetailsView.h"
#include "IPropertyAccessEditor.h"
#include "LandscapeRender.h"
#include "ObjectChooserClassFilter.h"
#include "ObjectChooserWidgetFactories.h"
#include "ObjectChooser_Asset.h"
#include "ObjectChooser_Class.h"
#include "PersonaModule.h"
#include "PropertyBag.h"
#include "PropertyCustomizationHelpers.h"
#include "PropertyEditorModule.h"
#include "SAssetDropTarget.h"
#include "SClassViewer.h"
#include "ScopedTransaction.h"
#include "SourceCodeNavigation.h"
#include "StructViewerModule.h"
#include "DragAndDrop/DecoratedDragDropOp.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Modules/ModuleManager.h"
#include "Styling/AppStyle.h"
#include "SChooserTableRow.h"
#include "Elements/Framework/TypedElementQueryBuilder.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SHyperlink.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Views/SListView.h"

#define LOCTEXT_NAMESPACE "ChooserEditor"

namespace UE::ChooserEditor
{

const FName FChooserTableEditor::ToolkitFName( TEXT( "ChooserTableEditor" ) );
const FName FChooserTableEditor::PropertiesTabId( TEXT( "ChooserEditor_Properties" ) );
const FName FChooserTableEditor::FindReplaceTabId( TEXT( "ChooserEditor_FindReplace" ) );
const FName FChooserTableEditor::TableTabId( TEXT( "ChooserEditor_Table" ) );

void FChooserTableEditor::PushChooserTableToEdit(UChooserTable* Chooser)
{
	BreadcrumbTrail->PushCrumb(FText::FromString(Chooser->GetName()), Chooser);
	RefreshAll();
}
	
void FChooserTableEditor::PopChooserTableToEdit()
{
	if (BreadcrumbTrail->HasCrumbs())
	{
		BreadcrumbTrail->PopCrumb();
		RefreshAll();
	}
}
	
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
		.SetIcon(FSlateIcon("ChooserEditorStyle", "ChooserEditor.ChooserTableIconSmall"));

	InTabManager->RegisterTabSpawner( FindReplaceTabId, FOnSpawnTab::CreateSP(this, &FChooserTableEditor::SpawnFindReplaceTab) )
		.SetDisplayName( LOCTEXT("FindReplaceTab", "Find/Replace") )
		.SetGroup(WorkspaceMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Find"));
}
	
void FChooserTableEditor::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);

	InTabManager->UnregisterTabSpawner( TableTabId );
	InTabManager->UnregisterTabSpawner( PropertiesTabId );
	InTabManager->UnregisterTabSpawner( FindReplaceTabId );
}

const FName FChooserTableEditor::ChooserEditorAppIdentifier( TEXT( "ChooserEditorApp" ) );

FChooserTableEditor::~FChooserTableEditor()
{
	if (SelectedColumn)
	{
		SelectedColumn->ClearFlags(RF_Standalone);
	}
	for (UObject* SelectedRow : SelectedRows)
	{
		SelectedRow->ClearFlags(RF_Standalone);
	}
	
	FCoreUObjectDelegates::OnObjectsReplaced.RemoveAll(this);
	FCoreUObjectDelegates::OnObjectTransacted.RemoveAll(this);

	DetailsView.Reset();
	
}


FName FChooserTableEditor::EditorName = "ChooserTableEditor";
	
FName FChooserTableEditor::GetEditorName() const
{
	return EditorName;
}

void FChooserTableEditor::MakeDebugTargetMenu(UToolMenu* InToolMenu) 
{
	static FName SectionName = "Select Debug Target";
		
	InToolMenu->AddMenuEntry(
			SectionName,
			FToolMenuEntry::InitMenuEntry(
				"None",
				LOCTEXT("None", "None"),
				LOCTEXT("None Tooltip", "Clear selected debug target"),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateLambda([this]()
					{
						UChooserTable* Chooser = GetRootChooser();
						Chooser->ResetDebugTarget();
						if (Chooser->bEnableDebugTesting)
						{
							Chooser->bEnableDebugTesting = false;
							Chooser->bDebugTestValuesValid = false;
							UpdateTableColumns();
						}
					}),
					FCanExecuteAction()
				)
			));
	
	InToolMenu->AddMenuEntry(
			SectionName,
			FToolMenuEntry::InitMenuEntry(
				"Manual",
				LOCTEXT("Manual Testing", "Manual Testing"),
				LOCTEXT("Manual Tooltip", "Test the chooser by manually entering values for each column"),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateLambda([this]()
					{
						UChooserTable* Chooser = GetRootChooser();
						Chooser->ResetDebugTarget();
						if (!Chooser->bEnableDebugTesting)
						{
							Chooser->bEnableDebugTesting = true;
							Chooser->bDebugTestValuesValid = true;
							UpdateTableColumns();
						}
					}),
					FCanExecuteAction()
				)
			));

	const UChooserTable* Chooser = GetChooser();

	Chooser->IterateRecentContextObjects([this, InToolMenu](const FString& ObjectName)
		{
			InToolMenu->AddMenuEntry(
						SectionName,
						FToolMenuEntry::InitMenuEntry(
							FName(ObjectName),
							FText::FromString(ObjectName),
							LOCTEXT("Select Object ToolTip", "Select this object as the debug target"),
							FSlateIcon(),
							FUIAction(
								FExecuteAction::CreateLambda([this, ObjectName]()
								{
									UChooserTable* Chooser = GetRootChooser();
									Chooser->SetDebugTarget(ObjectName);
									Chooser->bDebugTestValuesValid = false;
									if (!Chooser->bEnableDebugTesting)
									{
										Chooser->bEnableDebugTesting = true;
										UpdateTableColumns();
									}
								}),
								FCanExecuteAction()
							)
						));
		}
	);
	

}

void FChooserTableEditor::RegisterToolbar()
{
	UToolMenus* ToolMenus = UToolMenus::Get();
	UToolMenu* ToolBar;
	FName ParentName;
	const FName MenuName = GetToolMenuToolbarName(ParentName);
	if (ToolMenus->IsMenuRegistered(MenuName))
	{
		ToolBar = ToolMenus->ExtendMenu(MenuName);
	}
	else
	{
		ToolBar = UToolMenus::Get()->RegisterMenu(MenuName, ParentName, EMultiBoxType::ToolBar);
	}

	const FChooserTableEditorCommands& Commands = FChooserTableEditorCommands::Get();
	FToolMenuInsert InsertAfterAssetSection("Asset", EToolMenuInsertType::After);
	{
		FToolMenuSection& Section = ToolBar->AddSection("Chooser", TAttribute<FText>(), InsertAfterAssetSection);
		Section.AddEntry(FToolMenuEntry::InitToolBarButton(
			Commands.EditChooserSettings,
			TAttribute<FText>(),
			TAttribute<FText>(),
			FSlateIcon("EditorStyle", "FullBlueprintEditor.EditGlobalOptions")));


		Section.AddDynamicEntry("DebuggingCommands", FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InSection)
		{
			UChooserEditorToolMenuContext* Context = InSection.FindContext<UChooserEditorToolMenuContext>();

			if (Context)
			{
				if (TSharedPtr<FChooserTableEditor> ChooserEditor = Context->ChooserEditor.Pin())
				{
					InSection.AddEntry(FToolMenuEntry::InitComboButton( "SelectDebugTarget",
						FToolUIActionChoice(),
					  FNewToolMenuDelegate::CreateSP(ChooserEditor.Get(), &FChooserTableEditor::MakeDebugTargetMenu),
						TAttribute<FText>::CreateLambda([Chooser = ChooserEditor->GetRootChooser() ]
						{
							if (Chooser->HasDebugTarget())
							{
								return  FText::FromString(Chooser->GetDebugTargetName());
							}
							else
							{
								return Chooser->bEnableDebugTesting ? LOCTEXT("Manual Testing", "Manual Testing") : LOCTEXT("Debug Target", "Debug Target");
							}
						}),
						LOCTEXT("Debug Target Tooltip", "Select an object that has recently been the context object for this chooser to visualize the selection results")));
				}
			}
		}));
	}

}

void FChooserTableEditor::InitToolMenuContext(FToolMenuContext& MenuContext)
{
	FAssetEditorToolkit::InitToolMenuContext(MenuContext);

	UChooserEditorToolMenuContext* Context = NewObject<UChooserEditorToolMenuContext>();
	Context->ChooserEditor = SharedThis(this);
	MenuContext.AddObject(Context);
}

void FChooserTableEditor::BindCommands()
{
	const FChooserTableEditorCommands& Commands = FChooserTableEditorCommands::Get();

	ToolkitCommands->MapAction(
		Commands.EditChooserSettings,
		FExecuteAction::CreateSP(this, &FChooserTableEditor::SelectRootProperties));
}

void FChooserTableEditor::OnObjectsTransacted(UObject* Object, const FTransactionObjectEvent& Event)
{
	if (UChooserTable* ChooserTable = Cast<UChooserTable>(Object))
	{
		// if this is the chooser we're editing
		if (GetChooser() == ChooserTable)
		{
			if (CurrentSelectionType == ESelectionType::Rows)
			{
				// refresh details if we have rows selected
				RefreshRowSelectionDetails();
			}
		}
	}
	
	if (UChooserRowDetails* RowDetails = Cast<UChooserRowDetails>(Object))
	{
		// if this is for the chooser we're editing
		if (GetChooser() == RowDetails->Chooser)
		{
			// copy all the values over
			TValueOrError<FStructView, EPropertyBagResult> Result = RowDetails->Properties.GetValueStruct("Result", FInstancedStruct::StaticStruct());
			if (Result.IsValid())
			{
				RowDetails->Chooser->ResultsStructs[RowDetails->Row] = Result.GetValue().Get<FInstancedStruct>();
			}

			int ColumnIndex = 0;
			for (FInstancedStruct& ColumnData : RowDetails->Chooser->ColumnsStructs)
			{
				FChooserColumnBase& Column = ColumnData.GetMutable<FChooserColumnBase>();
				Column.SetFromDetails(RowDetails->Properties, ColumnIndex, RowDetails->Row);
				ColumnIndex++;
			}
		}
	}
}

void FChooserTableEditor::InitEditor( const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, const TArray<UObject*>& ObjectsToEdit, FGetDetailsViewObjects GetDetailsViewObjects )
{
	EditingObjects = ObjectsToEdit;

	BreadcrumbTrail = SNew(SBreadcrumbTrail<UChooserTable*>)
		.ButtonStyle(FAppStyle::Get(), "GraphBreadcrumbButton")
		.TextStyle(FAppStyle::Get(), "GraphBreadcrumbButtonText")
		.ButtonContentPadding( FMargin(4.f, 2.f) )
		.DelimiterImage( FAppStyle::GetBrush("BreadcrumbTrail.Delimiter") )
		.OnCrumbPushed_Lambda([this](UChooserTable* Table)
		{
			RefreshAll();
		})
		.OnCrumbPopped_Lambda([this](UChooserTable* Table)
		{
			RefreshAll();
		});
		
	UChooserTable* RootTable = GetRootChooser();
	BreadcrumbTrail->PushCrumb(FText::FromString(RootTable->GetName()), RootTable);
	
	
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

	BindCommands();
	RegenerateMenusAndToolbars();
	RegisterToolbar();

	SelectRootProperties();

		
	FAnimAssetFindReplaceConfig FindReplaceConfig;
	FindReplaceConfig.InitialProcessorClass = UChooserFindProperties::StaticClass();
	
	FCoreUObjectDelegates::OnObjectTransacted.AddSP(this, &FChooserTableEditor::OnObjectsTransacted);
}

FName FChooserTableEditor::GetToolkitFName() const
{
	return ToolkitFName;
}

FText FChooserTableEditor::GetBaseToolkitName() const
{
	return LOCTEXT("AppLabel", "Chooser Table Editor");
}

void FChooserTableEditor::RefreshAll()
{
	if (HeaderRow)
	{
		// Cache Selection state
		ESelectionType CachedSelectionType = CurrentSelectionType;
		int SelectedColumnIndex = -1;
		UChooserTable* SelectedChooser = nullptr;
		TArray<int> CachedSelectedRows;

		if (CachedSelectionType == ESelectionType::Column)
		{
			SelectedColumnIndex = SelectedColumn->Column;
			SelectedChooser = SelectedColumn->Chooser;
		}
		else if (CachedSelectionType == ESelectionType::Rows)
		{
			if (!SelectedRows.IsEmpty())
			{
				SelectedChooser = SelectedRows[0]->Chooser;
			}
			for(const TObjectPtr<UChooserRowDetails>& SelectedRow : SelectedRows)
			{
				CachedSelectedRows.Add(SelectedRow->Row);
			}
		}
		
		UpdateTableColumns();
		UpdateTableRows();

		// reapply cached selection state
		if (CachedSelectionType == ESelectionType::Root)
		{
			SelectRootProperties();
		}
		else if (CachedSelectionType == ESelectionType::Column)
		{
			SelectColumn(SelectedChooser, SelectedColumnIndex);
		}
		else if (CachedSelectionType == ESelectionType::Rows)
		{
			ClearSelectedRows();
			for(int Row : CachedSelectedRows)
			{
				SelectRow(Row, false);
			}
		}
		
	}
}

void FChooserTableEditor::PostUndo(bool bSuccess)
{
	RefreshAll();
}

void FChooserTableEditor::PostRedo(bool bSuccess)
{
	RefreshAll();
}


void FChooserTableEditor::NotifyPreChange(FProperty* PropertyAboutToChange)
{
}

void FChooserTableEditor::NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, FProperty* PropertyThatChanged)
{
	// Called on details panel edits
	
	if (CurrentSelectionType==ESelectionType::Root)
	{
		// Editing the root in the details panel can change ContextData that means all wigets need to be refreshed
		UpdateTableColumns();
		UpdateTableRows();
		SelectRootProperties();
	}
	if (CurrentSelectionType==ESelectionType::Column)
	{
		check(SelectedColumn);
		int SelectedColumnIndex = SelectedColumn->Column;
		UChooserTable* SelectedColumnChooser = SelectedColumn->Chooser;
		// Editing column properties can change the column type, which requires refreshing everything
		UpdateTableColumns();
		UpdateTableRows();
		SelectColumn(SelectedColumnChooser, SelectedColumnIndex);
	}
	// editing row data should not require any refreshing
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
	
TSharedRef<SDockTab> FChooserTableEditor::SpawnFindReplaceTab( const FSpawnTabArgs& Args )
{
	check( Args.GetTabId() == FindReplaceTabId );

	FPersonaModule& PersonaModule = FModuleManager::LoadModuleChecked<FPersonaModule>("Persona");
	FAnimAssetFindReplaceConfig Config;
	Config.InitialProcessorClass = UChooserFindProperties::StaticClass();
	return SNew(SDockTab)
		.Label( LOCTEXT("FindReplaceTitle", "Find/Replace") )
		.TabColorScale( GetTabColorScale() )
	[
		PersonaModule.CreateFindReplaceWidget(Config)
	];
}

TSharedRef<ITableRow> FChooserTableEditor::GenerateTableRow(TSharedPtr<FChooserTableRow> InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	UChooserTable* Chooser = GetChooser();

	return SNew(SChooserTableRow, OwnerTable)
		.Entry(InItem).Chooser(Chooser).Editor(this);
}

void FChooserTableEditor::SelectRootProperties()
{
	if( DetailsView.IsValid() )
	{
		// point the details view to the main table
		DetailsView->SetObject( GetRootChooser() );
		CurrentSelectionType = ESelectionType::Root;
	}
}

int FChooserTableEditor::MoveRow(int SourceRowIndex, int TargetRowIndex)
{
	UChooserTable* Chooser = GetChooser();
	TargetRowIndex = FMath::Min(TargetRowIndex,Chooser->ResultsStructs.Num());

	const FScopedTransaction Transaction(LOCTEXT("Move Row", "Move Row"));

	Chooser->Modify(true);

	for (FInstancedStruct& ColStruct : Chooser->ColumnsStructs)
	{
		FChooserColumnBase& Column = ColStruct.GetMutable<FChooserColumnBase>();
		Column.MoveRow(SourceRowIndex, TargetRowIndex);
	}

	FInstancedStruct Result = Chooser->ResultsStructs[SourceRowIndex];
	Chooser->ResultsStructs.RemoveAt(SourceRowIndex);
	if (SourceRowIndex < TargetRowIndex)
	{
		TargetRowIndex--;
	}
	Chooser->ResultsStructs.Insert(Result, TargetRowIndex);

	UpdateTableRows();

	return TargetRowIndex;
}
	
void FChooserTableEditor::SelectRow(int32 RowIndex, bool bClear)
{
	if (TableRows.IsValidIndex(RowIndex))
	{
		if (!TableView->IsItemSelected(TableRows[RowIndex]))
		{
			if (bClear)
			{
				TableView->ClearSelection();
			}
			TableView->SetItemSelection(TableRows[RowIndex], true, ESelectInfo::OnMouseClick);
		}
	}
}
	
void FChooserTableEditor::ClearSelectedRows() 
{
	SelectedRows.SetNum(0);
	TableView->ClearSelection();
	SelectRootProperties();
}

bool FChooserTableEditor::IsRowSelected(int32 RowIndex)
{
	for(auto& SelectedRow:SelectedRows)
 	{
 		if (SelectedRow->Row == RowIndex)
 		{
 			return true;
 		}
 	}
	return false;
}

void FChooserTableEditor::UpdateTableColumns()
{
	UChooserTable* Chooser = GetChooser();

	HeaderRow->ClearColumns();

	HeaderRow->AddColumn(SHeaderRow::Column("Handles")
					.DefaultLabel(FText())
					.ManualWidth(30));

	HeaderRow->AddColumn(SHeaderRow::Column("Result")
					.DefaultLabel(LOCTEXT("ResultColumnName", "Result"))
					.ManualWidth(300));

	FName ColumnId("ChooserColumn", 1);
	int NumColumns = Chooser->ColumnsStructs.Num();	
	for(int ColumnIndex = 0; ColumnIndex < NumColumns; ColumnIndex++)
	{
		FChooserColumnBase& Column = Chooser->ColumnsStructs[ColumnIndex].GetMutable<FChooserColumnBase>();

		TSharedPtr<SWidget> HeaderWidget = FObjectChooserWidgetFactories::CreateColumnWidget(&Column, Chooser->ColumnsStructs[ColumnIndex].GetScriptStruct(), Chooser->GetContextOwner(), -1);
		if (!HeaderWidget.IsValid())
		{
			HeaderWidget = SNullWidget::NullWidget;
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

				MenuBuilder.AddMenuEntry(LOCTEXT("Column Properties","Properties"),LOCTEXT("Select Column ToolTip", "Select this Column, and show its properties in the Details panel"),FSlateIcon(),
					FUIAction(
						FExecuteAction::CreateLambda([this,Chooser,ColumnIndex, ColumnId, &Column]()
						{
							SelectColumn(Chooser, ColumnId.GetNumber() - 1);
						})
						)
					);

				if (ColumnIndex > 0 && !Chooser->ColumnsStructs[ColumnIndex].Get<FChooserColumnBase>().IsRandomizeColumn())
				{
					MenuBuilder.AddMenuEntry(LOCTEXT("Move Left","Move Left"),LOCTEXT("Move Left ToolTip", "Move this column to the left."),FSlateIcon(),
						FUIAction(
							FExecuteAction::CreateLambda([this,Chooser,ColumnIndex, &Column]()
							{
								const FScopedTransaction Transaction(LOCTEXT("Move Column Left Transaction", "Move Column Left"));
								Chooser->Modify(true);
								Chooser->ColumnsStructs.Swap(ColumnIndex, ColumnIndex - 1);
								UpdateTableColumns();
							})
							));
				}
				if (ColumnIndex < Chooser->ColumnsStructs.Num() - 1 && !Chooser->ColumnsStructs[ColumnIndex+1].Get<FChooserColumnBase>().IsRandomizeColumn())
				{
					MenuBuilder.AddMenuEntry(LOCTEXT("Move Right","Move Right"),LOCTEXT("Move Right ToolTip", "Move this column to the right."),FSlateIcon(),
						FUIAction(
							FExecuteAction::CreateLambda([this,Chooser,ColumnIndex, &Column]()
							{
								const FScopedTransaction Transaction(LOCTEXT("Move Column Right Transaction", "Move Column Right"));
								Chooser->Modify(true);
								Chooser->ColumnsStructs.Swap(ColumnIndex, ColumnIndex + 1);
								UpdateTableColumns();
							})
							));
				}


				MenuBuilder.AddMenuEntry(LOCTEXT("Delete Column", "Delete"), LOCTEXT("Delete Column ToolTip", "Remove this column and all its data from the table"), FSlateIcon(),
					FUIAction(
						FExecuteAction::CreateLambda([this, Chooser, ColumnIndex, &Column]()
						{
							DeleteColumn(ColumnIndex);
						})
						));
			
				MenuBuilder.AddSubMenu(LOCTEXT("Input Type", "Input Type"),
					LOCTEXT("InputTypeToolTip", "Change input parameter type"),
					FNewMenuDelegate::CreateLambda([this, Chooser, &ColumnIndex](FMenuBuilder& Builder)
					{
						FStructViewerInitializationOptions Options;
						Options.StructFilter = MakeShared<FStructFilter>(Chooser->ColumnsStructs[ColumnIndex].Get<FChooserColumnBase>().GetInputBaseType());
						Options.bAllowViewOptions = false;
						Options.bShowNoneOption = false;
						Options.NameTypeToDisplay = EStructViewerNameTypeToDisplay::DisplayName;
					
						// Add class filter for columns here
						TSharedRef<SWidget> Widget = FModuleManager::LoadModuleChecked<FStructViewerModule>("StructViewer").CreateStructViewer(Options, FOnStructPicked::CreateLambda([this, ColumnIndex](const UScriptStruct* ChosenStruct)
						{
							const FScopedTransaction Transaction(LOCTEXT("SetColumnInputType", "Set Column Input Type"));
							UChooserTable* ChooserTable = GetChooser();
							ChooserTable->ColumnsStructs[ColumnIndex].GetMutable<FChooserColumnBase>().SetInputType(ChosenStruct);
							ChooserTable->Modify(true);
							UpdateTableColumns();
							UpdateTableRows();
						}));
				
						Builder.AddWidget(Widget, FText());
					}));
			
				return MenuBuilder.MakeWidget();
			})
			.HeaderComboVisibility(EHeaderComboVisibility::Ghosted)
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
				.OnMouseButtonDown_Lambda([this, Chooser, ColumnIndex, ColumnId](	const FGeometry&, const FPointerEvent& PointerEvent)
				{
					TableView->ClearSelection();
				
					SelectColumn(Chooser, ColumnId.GetNumber() - 1);
					return FReply::Handled();
				})
				[
					HeaderWidget.ToSharedRef()
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

void FChooserTableEditor::AddColumn(const UScriptStruct* ColumnType)
{
	CreateColumnComboButton->SetIsOpen(false);
	UChooserTable* Chooser = GetChooser();
	const FScopedTransaction Transaction(LOCTEXT("Add Column Transaction", "Add Column"));
	Chooser->Modify(true);

	FInstancedStruct NewColumn;
	NewColumn.InitializeAs(ColumnType);
	const FChooserColumnBase& NewColumnRef = NewColumn.Get<FChooserColumnBase>();
	int InsertIndex = 0;
	if (NewColumnRef.IsRandomizeColumn())
	{
		// add randomization column at the end (do nothing if there already is one)
		InsertIndex = Chooser->ColumnsStructs.Num();
		if (InsertIndex == 0 || !Chooser->ColumnsStructs[InsertIndex - 1].Get<FChooserColumnBase>().IsRandomizeColumn())
		{
			Chooser->ColumnsStructs.Add(NewColumn);
		}
	}
	else if (NewColumnRef.HasOutputs())
	{
		// add output columns at the end (but before any randomization column)
		InsertIndex = Chooser->ColumnsStructs.Num();
		if (InsertIndex > 0 && Chooser->ColumnsStructs[InsertIndex - 1].Get<FChooserColumnBase>().IsRandomizeColumn())
		{
			InsertIndex--;
		}
		Chooser->ColumnsStructs.Insert(NewColumn, InsertIndex);
	}
	else
	{
		// add other columns after the last non-output, non-randomization column
		while(InsertIndex < Chooser->ColumnsStructs.Num())
		{
			const FChooserColumnBase& Column = Chooser->ColumnsStructs[InsertIndex].Get<FChooserColumnBase>();
			if (Column.HasOutputs() || Column.IsRandomizeColumn())
			{
				break;
			}
			InsertIndex++;
		}
		Chooser->ColumnsStructs.Insert(NewColumn, InsertIndex);
	}

	UpdateTableColumns();
	UpdateTableRows();

	SelectColumn(Chooser, InsertIndex);
}

void FChooserTableEditor::RefreshRowSelectionDetails()
{
	for (UObject* SelectedRow : SelectedRows)
	{
		SelectedRow->ClearFlags(RF_Standalone);
	}
	SelectedRows.SetNum(0);
	UChooserTable* Chooser = GetChooser();
	
	// Get the list of objects to edit the details of
	TArray<TSharedPtr<FChooserTableRow>> SelectedItems = TableView->GetSelectedItems();
	for(TSharedPtr<FChooserTableRow>& SelectedItem : SelectedItems)
	{
		if (Chooser->ResultsStructs.IsValidIndex(SelectedItem->RowIndex))
		{
			TObjectPtr<UChooserRowDetails> Selection = NewObject<UChooserRowDetails>();
			Selection->Chooser = Chooser;
			Selection->Row = SelectedItem->RowIndex;
			Selection->SetFlags(RF_Standalone | RF_Transactional);

			FInstancedStruct& Result = Chooser->ResultsStructs[SelectedItem->RowIndex];
			Selection->Properties.AddProperty("Result", EPropertyBagPropertyType::Struct, FInstancedStruct::StaticStruct());
			Selection->Properties.SetValueStruct("Result", FConstStructView(FInstancedStruct::StaticStruct(), reinterpret_cast<uint8*>(&Result)));

			int ColumnIndex = 0;
			for (FInstancedStruct& ColumnData : Chooser->ColumnsStructs)
			{
				FChooserColumnBase& Column = ColumnData.GetMutable<FChooserColumnBase>();
				Column.AddToDetails(Selection->Properties, ColumnIndex, SelectedItem->RowIndex);
				ColumnIndex++;
			}
		
			SelectedRows.Add(Selection);
			
		}
	}
	
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
    										

TSharedRef<SDockTab> FChooserTableEditor::SpawnTableTab( const FSpawnTabArgs& Args )
{
	check( Args.GetTabId() == TableTabId );

	UChooserTable* Chooser = GetChooser();

	// + button to create new columns
	
	CreateColumnComboButton = SNew(SComboButton).OnGetMenuContent_Lambda([this]()
	{
		FStructViewerInitializationOptions Options;
		Options.StructFilter = MakeShared<FStructFilter>(FChooserColumnBase::StaticStruct());
		Options.bAllowViewOptions = false;
		Options.bShowNoneOption = false;
		Options.NameTypeToDisplay = EStructViewerNameTypeToDisplay::DisplayName;
		
		// Add class filter for columns here
		FStructViewerModule& StructViewerModule = FModuleManager::LoadModuleChecked<FStructViewerModule>("StructViewer");
		TSharedRef<SWidget> Widget = StructViewerModule.CreateStructViewer(Options, FOnStructPicked::CreateRaw(this, &FChooserTableEditor::AddColumn));
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
			Options.bAllowViewOptions = false;
			Options.bShowNoneOption = false;
			Options.NameTypeToDisplay = EStructViewerNameTypeToDisplay::DisplayName;
			
			TSharedRef<SWidget> Widget = FModuleManager::LoadModuleChecked<FStructViewerModule>("StructViewer").CreateStructViewer(Options, FOnStructPicked::CreateLambda([this](const UScriptStruct* ChosenStruct)
			{
				CreateRowComboButton->SetIsOpen(false);
				UChooserTable* Chooser = GetChooser();
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
						UChooserTable* Chooser = GetChooser();
						Chooser->Modify(true);
						// delete selected rows.
						TArray<uint32> RowsToDelete;
						for(auto& SelectedRow:SelectedRows)
						{
							RowsToDelete.Add(SelectedRow->Row);
						}

						SelectedRows.SetNum(0);
						SelectRootProperties();

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
				.OnSelectionChanged_Lambda([this](TSharedPtr<FChooserTableRow>,  ESelectInfo::Type SelectInfo)
				{
					// deselect any selected column
					ClearSelectedColumn();

					CurrentSelectionType = ESelectionType::Rows;

					RefreshRowSelectionDetails();
				})
    			.OnGenerateRow_Raw(this, &FChooserTableEditor::GenerateTableRow)
				.HeaderRow(HeaderRow);


	TSharedRef<SComboButton> EditChooserTableButton = SNew(SComboButton)
		.ButtonStyle(FAppStyle::Get(), "GraphBreadcrumbButton");
	
	EditChooserTableButton->SetOnGetMenuContent(
    		FOnGetContent::CreateLambda(
    			[ EditChooserTableButton, this]()
                			{
    							FMenuBuilder MenuBuilder(true, nullptr);
                            
								UObject* RootChooser = GetRootChooser();
								TArray<UObject*> ObjectsInPackage;
								GetObjectsWithOuter(RootChooser->GetPackage(), ObjectsInPackage);

								for (UObject* Object : ObjectsInPackage)
								{
									if (UChooserTable* Chooser = Cast<UChooserTable>(Object))
									{
										MenuBuilder.AddMenuEntry( FText::FromString(Chooser->GetName()), LOCTEXT("AddExistingObjectTooltip", "Add a reference to this existing Chooser Table."), FSlateIcon(),
											FUIAction(FExecuteAction::CreateLambda([this, EditChooserTableButton, Chooser, RootChooser]()
											{
												while(GetChooser() != RootChooser)
												{
													PopChooserTableToEdit();
												}
												if (Chooser != RootChooser)
												{
													PushChooserTableToEdit(Chooser);
												}
												EditChooserTableButton->SetIsOpen(false);
											})));
									}
                            	}
    
    							return MenuBuilder.MakeWidget();
    
                			})
    		);

	return SNew(SDockTab)
		.Label( LOCTEXT("ChooserTableTitle", "Chooser Table") )
		.TabColorScale( GetTabColorScale() )
		.OnCanCloseTab_Lambda([]() { return false; })
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(3)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()
				[
					EditChooserTableButton
				]
				+ SHorizontalBox::Slot().FillWidth(1)
				[
					BreadcrumbTrail.ToSharedRef()
				]
			]
			+ SVerticalBox::Slot().FillHeight(1)
			[
				SNew(SScrollBox).Orientation(Orient_Horizontal)
				+ SScrollBox::Slot()
				[
					TableView.ToSharedRef()
				]
			]
		];
}

void FChooserTableEditor::UpdateTableRows()
{
	UChooserTable* Chooser = GetChooser();
	int32 NewNum = Chooser->ResultsStructs.Num();

	// Sync the TableRows array which drives the ui table to match the number of results.
	TableRows.SetNum(0, EAllowShrinking::No);
	for(int i =0; i < NewNum; i++)
	{
		TableRows.Add(MakeShared<FChooserTableRow>(i));
	}

	// Add one at the end, for the Fallback result
	TableRows.Add(MakeShared<FChooserTableRow>(SChooserTableRow::SpecialIndex_Fallback));
	// Add one at the end, for the "Add Row" control
	TableRows.Add(MakeShared<FChooserTableRow>(SChooserTableRow::SpecialIndex_AddRow));

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
		SelectRootProperties();
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
	
void FChooserTableEditor::SelectColumn(UChooserTable* ChooserEditor, int Index)
{
	UChooserTable* Chooser = GetChooser();
   	if (Index < Chooser->ColumnsStructs.Num())
   	{
   		if (SelectedColumn == nullptr)
   		{
   			SelectedColumn = NewObject<UChooserColumnDetails>();
   			SelectedColumn->SetFlags(RF_Standalone);
   		}
   
   		SelectedColumn->Chooser = Chooser;
   		SelectedColumn->Column = Index;
   		DetailsView->SetObject(SelectedColumn, true);
   		CurrentSelectionType = ESelectionType::Column;
   	}
   	else
   	{
   		SelectRootProperties();
   	}
}
	
void FChooserTableEditor::ClearSelectedColumn()
{
	UChooserTable* Chooser = GetChooser();
	if (SelectedColumn != nullptr)
	{
		SelectedColumn->Column = -1;
		if (DetailsView->GetSelectedObjects().Contains(SelectedColumn))
		{
			SelectRootProperties();
		}
	}
}
	
void FChooserTableEditor::DeleteColumn(int Index)
{
	const FScopedTransaction Transaction(LOCTEXT("Delete Column Transaction", "Delete Column"));
	ClearSelectedColumn();
	SelectRootProperties();
	UChooserTable* Chooser = GetChooser();

	if (Index < Chooser->ColumnsStructs.Num())
	{
		Chooser->Modify(true);
		Chooser->ColumnsStructs.RemoveAt(Index);
		UpdateTableColumns();
	}
}

void FChooserTableEditor::RegisterWidgets()
{
	FObjectChooserWidgetFactories::RegisterWidgetCreator(FAssetChooser::StaticStruct(), CreateAssetWidget);
	FObjectChooserWidgetFactories::RegisterWidgetCreator(FClassChooser::StaticStruct(), CreateClassWidget);
	FObjectChooserWidgetFactories::RegisterWidgetCreator(FEvaluateChooser::StaticStruct(), CreateEvaluateChooserWidget);
	FObjectChooserWidgetFactories::RegisterWidgetCreator(FNestedChooser::StaticStruct(), CreateNestedChooserWidget);

	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	
	PropertyModule.RegisterCustomClassLayout("ChooserTable", FOnGetDetailCustomizationInstance::CreateStatic(&FChooserDetails::MakeInstance));	
	PropertyModule.RegisterCustomClassLayout("ChooserRowDetails", FOnGetDetailCustomizationInstance::CreateStatic(&FChooserRowDetails::MakeInstance));	
	PropertyModule.RegisterCustomClassLayout("ChooserColumnDetails", FOnGetDetailCustomizationInstance::CreateStatic(&FChooserColumnDetails::MakeInstance));	
}
}

#undef LOCTEXT_NAMESPACE
