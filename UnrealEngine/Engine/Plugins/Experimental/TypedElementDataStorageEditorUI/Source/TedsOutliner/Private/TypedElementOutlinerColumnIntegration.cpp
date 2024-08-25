// Copyright Epic Games, Inc. All Rights Reserved.

#include "TypedElementOutlinerColumnIntegration.h"

#include "ActorTreeItem.h"
#include "Elements/Columns/TypedElementLabelColumns.h"
#include "Elements/Columns/TypedElementMiscColumns.h"
#include "Elements/Columns/TypedElementPackageColumns.h"
#include "Elements/Columns/TypedElementTransformColumns.h"
#include "Elements/Columns/TypedElementTypeInfoColumns.h"
#include "Elements/Framework/TypedElementRegistry.h"
#include "Elements/Framework/TypedElementQueryBuilder.h"
#include "Elements/Interfaces/TypedElementDataStorageCompatibilityInterface.h"
#include "ILevelEditor.h"
#include "ISceneOutliner.h"
#include "ISceneOutlinerColumn.h"
#include "ISceneOutlinerTreeItem.h"
#include "LevelEditor.h"
#include "MassEntityTypes.h"
#include "SSceneOutliner.h"
#include "SceneOutlinerFwd.h"
#include "SceneOutlinerModule.h"
#include "SceneOutlinerPublicTypes.h"
#include "Elements/Interfaces/Capabilities/TypedElementUiTextCapability.h"
#include "TypedElementOutlinerItem.h"
#include "Columns/UIPropertiesColumns.h"
#include "Elements/Columns/TypedElementCompatibilityColumns.h"
#include "Elements/Columns/TypedElementSlateWidgetColumns.h"
#include "Elements/Framework/TypedElementDataStorageWidget.h"

#define LOCTEXT_NAMESPACE "TypedElementsUI_SceneOutliner"

FAutoConsoleCommand BindColumnsToSceneOutlinerConsoleCommand(
	TEXT("TEDS.UI.BindColumnsToSceneOutliner"),
	TEXT("Bind one or more columns to the most recently used Scene Outliner. Several prebuild configurations are offered as well.")
	TEXT("An example input to show a label column is 'TEDS.UI.BindColumnsToSceneOutliner /Script/TypedElementFramework.TypedElementLabelColumn'."),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			using namespace TypedElementQueryBuilder;
			using DSI = ITypedElementDataStorageInterface;

			UTypedElementRegistry* Registry = UTypedElementRegistry::GetInstance();
			if (ITypedElementDataStorageInterface* DataStorage = Registry->GetMutableDataStorage())
			{
				static TypedElementQueryHandle Queries[] =
				{
					DataStorage->RegisterQuery(Select().ReadWrite<FTypedElementLabelColumn>().Compile()),
					DataStorage->RegisterQuery(Select().ReadOnly<FTypedElementLocalTransformColumn>().Compile()),
					DataStorage->RegisterQuery(Select().ReadOnly<FTypedElementPackagePathColumn>().Compile()),
					DataStorage->RegisterQuery(
						Select()
							.ReadWrite<FTypedElementLabelColumn>()
							.ReadOnly<FTypedElementLocalTransformColumn>()
						.Compile()),
					DataStorage->RegisterQuery(
						Select()
							.ReadOnly<FTypedElementLabelColumn>()
							.ReadOnly<FTypedElementLabelHashColumn>()
						.Compile())
				};

				FTypedElementSceneOutlinerQueryBinder& Binder = FTypedElementSceneOutlinerQueryBinder::GetInstance();
				const TWeakPtr<ILevelEditor> LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor")).GetLevelEditorInstance();
				const TSharedPtr<ISceneOutliner> SceneOutliner = LevelEditor.IsValid() ? LevelEditor.Pin()->GetMostRecentlyUsedSceneOutliner() : nullptr;
				if (SceneOutliner.IsValid())
				{
					if (!Args.IsEmpty())
					{
						if (Args[0].IsNumeric())
						{
							int32 QueryIndex = FCString::Atoi(*Args[0]);
							if (QueryIndex < sizeof(Queries) / sizeof(TypedElementQueryHandle))
							{
								Binder.AssignQuery(Queries[QueryIndex], SceneOutliner);
								return;
							}
						}
						else
						{
							uint32 AdditionCount = 0;
							Select Query;
							for (const FString& Arg : Args)
							{
								FTopLevelAssetPath Path;
								// TrySetPath has an ensure that checks if the path starts with an '/' and otherwise throws
								// an assert.
								if (!Arg.IsEmpty() && Arg[0] == '/' && Path.TrySetPath(Arg))
								{
									const UScriptStruct* ColumnType = TypeOptional(Path);
									if (ColumnType && ColumnType->IsChildOf(FMassFragment::StaticStruct()))
									{
										Query.ReadOnly(ColumnType);
										++AdditionCount;
									}
								}
							}
							if (AdditionCount > 0)
							{
								static TypedElementQueryHandle CustomQuery = TypedElementInvalidQueryHandle;
								if (CustomQuery != TypedElementInvalidQueryHandle)
								{
									DataStorage->UnregisterQuery(CustomQuery);
								}
								CustomQuery = DataStorage->RegisterQuery(Query.Compile());
								Binder.AssignQuery(CustomQuery, SceneOutliner);
								return;
							}
						}
					}
					Binder.AssignQuery(TypedElementInvalidQueryHandle, SceneOutliner);
				}
			}
		}));

class FOutlinerColumn : public ISceneOutlinerColumn
{
public:
	FOutlinerColumn(
		TypedElementDataStorage::QueryHandle InQuery,
		ITypedElementDataStorageInterface& InStorage, 
		ITypedElementDataStorageUiInterface& InStorageUi, 
		ITypedElementDataStorageCompatibilityInterface& InStorageCompatibility,
		FName InNameId, 
		TArray<TWeakObjectPtr<const UScriptStruct>> InColumnTypes,
		TSharedPtr<FTypedElementWidgetConstructor> InHeaderWidgetConstructor,
		TSharedPtr<FTypedElementWidgetConstructor> InCellWidgetConstructor,
		FName InFallbackColumnName,
		ISceneOutliner& InOwningOutliner,
		const FTreeItemIDDealiaser& InDealiaser)
		: ColumnTypes(MoveTemp(InColumnTypes))
		, HeaderWidgetConstructor(MoveTemp(InHeaderWidgetConstructor))
		, CellWidgetConstructor(MoveTemp(InCellWidgetConstructor))
		, Storage(InStorage)
		, StorageUi(InStorageUi)
		, StorageCompatibility(InStorageCompatibility)
		, QueryHandle(InQuery)
		, NameId(InNameId)
		, OwningOutliner(InOwningOutliner)
		, Dealiaser(InDealiaser)
	{
		MetaData.AddOrSetMutableData(TEXT("Name"), NameId.ToString());
		ColumnTypes.Shrink();

		// Try to find a fallback column from the regular item, for handling cases like folders which are not in TEDS but want to use TEDS columns
		FSceneOutlinerModule& SceneOutlinerModule = FModuleManager::LoadModuleChecked<FSceneOutlinerModule>("SceneOutliner");
		FallbackColumn = SceneOutlinerModule.FactoryColumn(InFallbackColumnName, OwningOutliner);

		RegisterQueries();
	};
	
	~FOutlinerColumn() override
	{
		UnRegisterQueries();
	}

	FName GetColumnID() override
	{
		return NameId;
	}

	virtual void Tick(double InCurrentTime, float InDeltaTime)
	{
		// Update any rows that could need widget updates
		if(!RowsToUpdate.IsEmpty())
		{
			UpdateWidgets();
			RowsToUpdate.Empty();
		}
	}

	void RegisterQueries()
	{
		using namespace TypedElementQueryBuilder;
		using namespace TypedElementDataStorage;

		// For each TEDS column this column is matched with, we'll add observers to track addition/removal to update any widgets
		for(const TWeakObjectPtr<const UScriptStruct>& ColumnType : ColumnTypes)
		{
			const FName ColumnAddObserverName = *(FString::Printf(TEXT("Column Add Monitor for %s Outliner Column, %s TEDS Column"), *NameId.ToString(), *ColumnType->GetName()));
			FObserver AddObserver(FObserver::EEvent::Add, ColumnType.Get());
			AddObserver.ForceToGameThread(true);

			// TEDS-Outliner TODO: Long term if we move this into TypedElementOutlinerMode or similar we can get access to the exact
			// types the Outliner is looking at and specify them on .Where() to cut down on the things we are observing
			TypedElementDataStorage::QueryHandle AddQueryHandle = Storage.RegisterQuery(
				Select(
					ColumnAddObserverName,
					AddObserver,
					[this](IQueryContext& Context, TypedElementRowHandle Row)
						{
							RowsToUpdate.Add(TPair<TypedElementRowHandle, bool>(Row, true));
						})
				.Compile()
				);
			
			InternalObserverQueries.Add(AddQueryHandle);

			const FName ColumnRemoveObserverName = *(FString::Printf(TEXT("Column Remove Monitor for %s Outliner Column, %s TEDS Column"), *NameId.ToString(), *ColumnType->GetName()));
			FObserver RemoveObserver(FObserver::EEvent::Remove, ColumnType.Get());
			RemoveObserver.ForceToGameThread(true);

			// TEDS-Outliner TODO: Long term if we move this into TypedElementOutlinerMode or similar we can get access to the exact
			// types the Outliner is looking at and specify them on .Where() to cut down on the things we are observing
			TypedElementDataStorage::QueryHandle RemoveQueryHandle = Storage.RegisterQuery(
				Select(
					ColumnRemoveObserverName,
					RemoveObserver,
					[this](IQueryContext& Context, TypedElementRowHandle Row)
						{

							RowsToUpdate.Add(TPair<TypedElementRowHandle, bool>(Row, false));
						})
				.Compile()
			);
			
			InternalObserverQueries.Add(RemoveQueryHandle);
		}

		// We are looking for widgets that have a row reference
		TArray<const UScriptStruct*> SelectionColumns({FTypedElementSlateWidgetReferenceColumn::StaticStruct(), FTypedElementRowReferenceColumn::StaticStruct()});

		// We need to remove duplicates because TEDS/Mass does not handle having the same column in Select() and Where()
		TArray<const UScriptStruct*> AdditionalWidgetColumns = CellWidgetConstructor->GetAdditionalColumnsList().FilterByPredicate([&SelectionColumns](const UScriptStruct* Column)
		{
			return !SelectionColumns.Contains(Column);
		});
		
		// Query to get all widgets that were created by this column
		WidgetQuery = Storage.RegisterQuery(
				Select()
					.ReadOnly(SelectionColumns)
				.Where()
					.All(AdditionalWidgetColumns)
				.Compile());
	}
	
	void UnRegisterQueries()
	{
		for(const TypedElementDataStorage::QueryHandle Query : InternalObserverQueries)
		{
			Storage.UnregisterQuery(Query);
		}
		
		Storage.UnregisterQuery(WidgetQuery);
	}

	bool IsRowVisible(const TypedElementDataStorage::RowHandle InRowHandle) const
	{
		// Try to grab the TEDS Outliner item from the row handle
		FSceneOutlinerTreeItemPtr Item = OwningOutliner.GetTreeItem(InRowHandle);

		// If it doesn't exist, this could be a legacy item that uses something other than the row id as the ID, so check if we have a dealiaser
		if(!Item)
		{
			if(Dealiaser.IsBound())
			{
				Item = OwningOutliner.GetTreeItem(Dealiaser.Execute(InRowHandle));
			}
		}

		if(!Item)
		{
			return false;
		}

		// Check if the item is visible in the tree
		return OwningOutliner.GetTree().IsItemVisible(Item);
	}

	void UpdateWidgets()
	{
		// Remove any widget rows that don't actually need an update
		RowsToUpdate = RowsToUpdate.FilterByPredicate([this](const TPair<TypedElementDataStorage::RowHandle, bool>& Pair) -> bool
		{
			// We don't have a widget for this item visible, so there is nothing to update
			if(!IsRowVisible(Pair.Key))
			{
				return false;
			}
			
			// Check if the row now matches the query conditions for this widget
			const bool bMatchesQueryConditions = CellWidgetConstructor->GetQueryConditions() && Storage.MatchesColumns(Pair.Key, *CellWidgetConstructor->GetQueryConditions());
			
			// If we are adding a column that we are monitoring and it now matches, or if we are removing a column that we are monitoring and it now
			// stops matching, there is a potential need for widget update
			return (bMatchesQueryConditions && Pair.Value) || (!bMatchesQueryConditions && !Pair.Value);
		});
		
		using namespace TypedElementQueryBuilder;
		using namespace TypedElementDataStorage;

		// Query to find all widgets that belong to the row handles that need updates
		DirectQueryCallback RowCollector = CreateDirectQueryCallbackBinding(
		[this](const IDirectQueryContext& Context, const FTypedElementSlateWidgetReferenceColumn* ContainerWidgetReferenceColumns, const FTypedElementRowReferenceColumn* RowReferenceColumns)
		{
			const FTypedElementSlateWidgetReferenceColumn* WidgetsIt = ContainerWidgetReferenceColumns;
			const FTypedElementRowReferenceColumn* RowRefsIt = RowReferenceColumns;
			const TConstArrayView<TypedElementRowHandle> Rows = Context.GetRowHandles();

			for(unsigned RowIndex = 0; RowIndex < Context.GetRowCount(); ++RowIndex, ++WidgetsIt, ++RowRefsIt)
			{
				// Check if this widgets owning row is in our rows to update
				bool* bColumnAddedPtr = RowsToUpdate.Find(RowRefsIt->Row);
				// If not, skip it
				if(!bColumnAddedPtr)
				{
					continue;
				}

				// Check if the container TEDSWidget exists, if not we cannot update this widget
				const TSharedPtr<STedsWidget> TedsWidget = WidgetsIt->TedsWidget.Pin();
				if(!TedsWidget)
				{
					continue;
				}

				// A row has numerous widgets, make sure we only update the one that was created by our column by checking the constuctor
				if(TedsWidget->GetWidgetConstructorTypeInfo() != CellWidgetConstructor->GetTypeInfo())
				{
					continue;
				}

				// If a column was added and we are here, we need to re-create the widget
				// TEDS-Outliner TODO: Do we need to create the widget only if it doesn't exist? Or should we also update it to automatically respond
				// to column changes even if it was already created
				if(*bColumnAddedPtr)
				{
					const TSharedPtr<SWidget> RowWidget = CellWidgetConstructor->Construct(Rows[RowIndex], &Storage, &StorageUi,
						FComboMetaDataView(FGenericMetaDataView(MetaData)).Next(FQueryMetaDataView(Storage.GetQueryDescription(QueryHandle))));

					if(RowWidget)
					{
						TedsWidget->SetContent(RowWidget.ToSharedRef());
					}
				}
				// If a column was removed (and we don't match anymore) delete the internal widget
				else
				{
					TedsWidget->SetContent(SNullWidget::NullWidget);
				}
			}
		});

		Storage.RunQuery(WidgetQuery, RowCollector);
	}

	virtual SHeaderRow::FColumn::FArguments ConstructHeaderRowColumn() override
	{
		using namespace TypedElementDataStorage;

		FString TooltipText = TEXT("Data Storage columns:");
		for (const TWeakObjectPtr<const UScriptStruct>& ColumnType : ColumnTypes)
		{
			if (ColumnType.IsValid())
			{
				TooltipText += TEXT("\n    ");
				ColumnType->AppendName(TooltipText);
			}
		}

		TSharedPtr<SWidget> Widget;
		RowHandle UiRowHandle = InvalidRowHandle;
		if (HeaderWidgetConstructor)
		{
			UiRowHandle = Storage.AddRow(Storage.FindTable(FName(TEXT("Editor_WidgetTable"))));
			Widget = StorageUi.ConstructWidget(UiRowHandle, *HeaderWidgetConstructor, 
				FComboMetaDataView(FGenericMetaDataView(MetaData)).Next(FQueryMetaDataView(Storage.GetQueryDescription(QueryHandle))));
		}
		if (!Widget.IsValid())
		{
			Widget = SNew(STextBlock)
				.Text(FText::FromString(NameId.ToString()));
		}
		
		SHeaderRow::FColumn::FArguments Column = SHeaderRow::Column(NameId)
			.FillWidth(1)
			.HeaderComboVisibility(EHeaderComboVisibility::OnHover)
			.DefaultTooltip(FText::FromString(MoveTemp(TooltipText)))
			.HeaderContent()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				[
					Widget.ToSharedRef()
				]
			];
		if (const FUIHeaderPropertiesColumn* HeaderProperties = Storage.GetColumn<FUIHeaderPropertiesColumn>(UiRowHandle))
		{
			float Width = HeaderProperties->Width;
			switch (HeaderProperties->ColumnSizeMode)
			{
				case EColumnSizeMode::Fill: Column.FillWidth(Width); break;
				case EColumnSizeMode::Fixed: Column.FixedWidth(Width); break;
				case EColumnSizeMode::Manual: Column.ManualWidth(Width); break;
				case EColumnSizeMode::FillSized: Column.FillSized(Width); break;
			}
		}
		return Column;
	}

	// TODO: Sorting is currently handled through the fallback column if it exists because we have no way to sort columns through TEDS
	virtual void SortItems(TArray<FSceneOutlinerTreeItemPtr>& RootItems, const EColumnSortMode::Type SortMode) const override
	{
		if(FallbackColumn)
		{
			FallbackColumn->SortItems(RootItems, SortMode);
		}
	}

	virtual bool SupportsSorting() const override
	{
		return FallbackColumn ? FallbackColumn->SupportsSorting() : false;
	}

	void SetHighlightText(SWidget& Widget)
	{
		if (TSharedPtr<ITypedElementUiTextCapability> TextCapability = Widget.GetMetaData<ITypedElementUiTextCapability>())
		{
			TextCapability->SetHighlightText(OwningOutliner.GetFilterHighlightText());
		}
	
		if (FChildren* ChildWidgets = Widget.GetChildren())
		{
			ChildWidgets->ForEachWidget([this](SWidget& ChildWidget)
				{
					SetHighlightText(ChildWidget);
				});
		}
	}
	
	const TSharedRef<SWidget> ConstructRowWidget(FSceneOutlinerTreeItemRef TreeItem, const STableRow<FSceneOutlinerTreeItemPtr>& Row) override
	{
		using namespace TypedElementDataStorage;
		
		RowHandle RowHandle = InvalidRowHandle;

		TSharedPtr<SWidget> RowWidget;

		if(const FTypedElementOutlinerTreeItem* TEDSItem = TreeItem->CastTo<FTypedElementOutlinerTreeItem>())
		{
			RowHandle = TEDSItem->GetRowHandle();
			
		}
		else if (const FActorTreeItem* ActorItem = TreeItem->CastTo<FActorTreeItem>())
		{
			if (AActor* Actor = ActorItem->Actor.Get())
			{
				RowHandle = StorageCompatibility.FindRowWithCompatibleObject(Actor);
			}
		}
		else if(FallbackColumn)
		{
			RowWidget = FallbackColumn->ConstructRowWidget(TreeItem, Row);
		}

		if(Storage.HasRowBeenAssigned(RowHandle))
		{
			TypedElementRowHandle UiRowHandle = Storage.AddRow(Storage.FindTable(FTypedElementSceneOutlinerQueryBinder::CellWidgetTableName));

			if (FTypedElementRowReferenceColumn* RowReference = Storage.GetColumn<FTypedElementRowReferenceColumn>(UiRowHandle))
			{
				RowReference->Row = RowHandle;
			}
		
			RowWidget = StorageUi.ConstructWidget(UiRowHandle, *CellWidgetConstructor, 
							FComboMetaDataView(FGenericMetaDataView(MetaData)).Next(FQueryMetaDataView(Storage.GetQueryDescription(QueryHandle))));
		}

		if(RowWidget)
		{
			SetHighlightText(*RowWidget);
			return RowWidget.ToSharedRef();
		}

		return SNullWidget::NullWidget;
	}

	virtual void PopulateSearchStrings( const ISceneOutlinerTreeItem& Item, TArray< FString >& OutSearchStrings ) const override
	{
		// TODO: We don't currently have a way to convert TEDS widgets into searchable strings, but we can rely on the fallback column if it exists
		if(FallbackColumn)
		{
			FallbackColumn->PopulateSearchStrings(Item, OutSearchStrings);
		}
	}
	

	TArray<TWeakObjectPtr<const UScriptStruct>> ColumnTypes;
	TSharedPtr<FTypedElementWidgetConstructor> HeaderWidgetConstructor;
	TSharedPtr<FTypedElementWidgetConstructor> CellWidgetConstructor;
	ITypedElementDataStorageInterface& Storage;
	ITypedElementDataStorageUiInterface& StorageUi;
	ITypedElementDataStorageCompatibilityInterface& StorageCompatibility;
	TypedElementDataStorage::QueryHandle QueryHandle;
	TypedElementDataStorage::FMetaData MetaData;
	FName NameId;
	TSharedPtr<ISceneOutlinerColumn> FallbackColumn;
	ISceneOutliner& OwningOutliner;
	FTreeItemIDDealiaser Dealiaser;
	
	TArray<TypedElementDataStorage::QueryHandle> InternalObserverQueries;
	TypedElementDataStorage::QueryHandle WidgetQuery;
	TMap<TypedElementDataStorage::RowHandle, bool> RowsToUpdate;
};




//
// UTypedElementSceneOutlinerFactory
// 
void UTypedElementSceneOutlinerFactory::RegisterTables(ITypedElementDataStorageInterface& DataStorage)
{
	TypedElementTableHandle BaseWidgetTable = DataStorage.FindTable(FName(TEXT("Editor_WidgetTable")));
	if (BaseWidgetTable != TypedElementInvalidTableHandle)
	{
		DataStorage.RegisterTable(
			BaseWidgetTable,
			{
				FTypedElementRowReferenceColumn::StaticStruct()
			},
			FTypedElementSceneOutlinerQueryBinder::CellWidgetTableName);
	}
}

void UTypedElementSceneOutlinerFactory::RegisterWidgetPurposes(ITypedElementDataStorageUiInterface& DataStorageUi) const
{
	using PurposeType = ITypedElementDataStorageUiInterface::EPurposeType;

	DataStorageUi.RegisterWidgetPurpose(FTypedElementSceneOutlinerQueryBinder::HeaderWidgetPurpose, PurposeType::UniqueByNameAndColumn,
		LOCTEXT("HeaderWidgetPurpose", "Widgets for headers in any Scene Outliner for specific columns or column combinations."));
	DataStorageUi.RegisterWidgetPurpose(FTypedElementSceneOutlinerQueryBinder::DefaultHeaderWidgetPurpose, PurposeType::UniqueByName,
		LOCTEXT("DefaultHeaderWidgetPurpose", "The default widget to use in headers for the Scene Outliner."));
	
	DataStorageUi.RegisterWidgetPurpose(FTypedElementSceneOutlinerQueryBinder::CellWidgetPurpose, PurposeType::UniqueByNameAndColumn,
		LOCTEXT("CellWidgetPurpose", "Widgets for cells in any Scene Outliner for specific columns or column combinations."));
	DataStorageUi.RegisterWidgetPurpose(FTypedElementSceneOutlinerQueryBinder::DefaultCellWidgetPurpose, PurposeType::UniqueByName,
		LOCTEXT("DefaultCellWidgetPurpose", "The default widget to use in cells for the Scene Outliner."));

	DataStorageUi.RegisterWidgetPurpose(FTypedElementSceneOutlinerQueryBinder::ItemLabelCellWidgetPurpose, PurposeType::UniqueByNameAndColumn,
		LOCTEXT("ItemCellWidgetPurpose", "Widgets for cells in any Scene Outliner that are specific to the Item label column."));
	DataStorageUi.RegisterWidgetPurpose(FTypedElementSceneOutlinerQueryBinder::DefaultItemLabelCellWidgetPurpose, PurposeType::UniqueByName,
		LOCTEXT("DefaultItemCellWidgetPurpose", "The default widget to use in cells for the Scene Outliner specific to the Item label column."));


}



//
// FTypedElementSceneOutlinerQueryBinder
// 

const FName FTypedElementSceneOutlinerQueryBinder::CellWidgetTableName(TEXT("Editor_SceneOutlinerCellWidgetTable"));
const FName FTypedElementSceneOutlinerQueryBinder::HeaderWidgetPurpose(TEXT("SceneOutliner.Header"));
const FName FTypedElementSceneOutlinerQueryBinder::DefaultHeaderWidgetPurpose(TEXT("SceneOutliner.Header.Default"));
const FName FTypedElementSceneOutlinerQueryBinder::CellWidgetPurpose(TEXT("SceneOutliner.Cell"));
const FName FTypedElementSceneOutlinerQueryBinder::DefaultCellWidgetPurpose(TEXT("SceneOutliner.Cell.Default"));
const FName FTypedElementSceneOutlinerQueryBinder::ItemLabelCellWidgetPurpose(TEXT("SceneOutliner.ItemLabel.Cell"));
const FName FTypedElementSceneOutlinerQueryBinder::DefaultItemLabelCellWidgetPurpose(TEXT("SceneOutliner.ItemLabel.Cell.Default"));

FTypedElementSceneOutlinerQueryBinder::FTypedElementSceneOutlinerQueryBinder()
{
	UTypedElementRegistry* Registry = UTypedElementRegistry::GetInstance();
	checkf(Registry, TEXT("Unable to bind a Scene Outliner to a query before the Typed Elements are available."));
	if (Registry)
	{
		Storage = Registry->GetMutableDataStorage();
		StorageUi = Registry->GetMutableDataStorageUi();
		StorageCompatibility = Registry->GetMutableDataStorageCompatibility();
	}

	SetupDefaultColumnMapping();
}

void FTypedElementSceneOutlinerQueryBinder::SetupDefaultColumnMapping()
{
	// Map the type column from the TEDS to the default Outliner type column, so we can show type info for objects not in TEDS
	TEDSToOutlinerDefaultColumnMapping.Add(FTypedElementClassTypeInfoColumn::StaticStruct(), FSceneOutlinerBuiltInColumnTypes::ActorInfo());
}

FName FTypedElementSceneOutlinerQueryBinder::FindOutlinerColumnFromTEDSColumns(TConstArrayView<TWeakObjectPtr<const UScriptStruct>> TEDSColumns) const
{
	// Currently, the algorithm naively looks through the mapping and returns the first match
	for(const TWeakObjectPtr<const UScriptStruct>& Column : TEDSColumns)
	{
		if(const FName* FoundDefaultColumn = TEDSToOutlinerDefaultColumnMapping.Find(Column))
		{
			return *FoundDefaultColumn;
		}
	}

	return FName();
}

FTypedElementSceneOutlinerQueryBinder& FTypedElementSceneOutlinerQueryBinder::GetInstance()
{
	static FTypedElementSceneOutlinerQueryBinder Binder;
	return Binder;
}

TSharedPtr<FTypedElementSceneOutliner>* FTypedElementSceneOutlinerQueryBinder::FindOrAddQueryMapping(const TSharedPtr<ISceneOutliner>& Outliner)
{
	TSharedPtr<FTypedElementSceneOutliner>* QueryMapping = SceneOutliners.Find(Outliner);
	if (QueryMapping == nullptr)
	{
		QueryMapping = &SceneOutliners.Add(Outliner, MakeShared<FTypedElementSceneOutliner>());
		(*QueryMapping)->Initialize(*Storage, *StorageUi, *StorageCompatibility, Outliner);
	}

	return QueryMapping;
}

void FTypedElementSceneOutlinerQueryBinder::AssignQuery(TypedElementQueryHandle Query, const TSharedPtr<ISceneOutliner>& Outliner)
{
	CleanupStaleOutliners();

	TSharedPtr<FTypedElementSceneOutliner>* QueryMapping = FindOrAddQueryMapping(Outliner);
	(*QueryMapping)->AssignQuery(Query);
}

void FTypedElementSceneOutlinerQueryBinder::RegisterTreeItemIDDealiaser(const TSharedPtr<ISceneOutliner>& Outliner, const FTreeItemIDDealiaser& InDealiaser)
{
	TSharedPtr<FTypedElementSceneOutliner>* QueryMapping = FindOrAddQueryMapping(Outliner);
	(*QueryMapping)->RegisterDealiaser(InDealiaser);
}

void FTypedElementSceneOutlinerQueryBinder::CleanupStaleOutliners()
{
	for (TMap<TWeakPtr<ISceneOutliner>, TSharedPtr<FTypedElementSceneOutliner>>::TIterator It(SceneOutliners); It; ++It)
	{
		// Remove any query mappings where the target Outliner doesn't exist anymore
		if(!It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}
}

//
// FTypedElementSceneOutliner
//

FTypedElementSceneOutliner::~FTypedElementSceneOutliner()
{
	TSharedPtr<ISceneOutliner> OutlinerPinned = Outliner.Pin();
	if (OutlinerPinned)
	{
		ClearColumns(*OutlinerPinned);
	}
}

void FTypedElementSceneOutliner::Initialize(
	ITypedElementDataStorageInterface& InStorage,
	ITypedElementDataStorageUiInterface& InStorageUi,
	ITypedElementDataStorageCompatibilityInterface& InStorageCompatibility,
	const TSharedPtr<ISceneOutliner>& InOutliner)
{
	Storage = &InStorage;
	StorageUi = &InStorageUi;
	StorageCompatibility = &InStorageCompatibility;
	Outliner = InOutliner;
}

TArray<TWeakObjectPtr<const UScriptStruct>> FTypedElementSceneOutliner::CreateVerifiedColumnTypeAray(
	TConstArrayView<TWeakObjectPtr<const UScriptStruct>> ColumnTypes)
{
	TArray<TWeakObjectPtr<const UScriptStruct>> VerifiedColumnTypes;
	VerifiedColumnTypes.Reserve(ColumnTypes.Num());
	for (const TWeakObjectPtr<const UScriptStruct>& ColumnType : ColumnTypes)
	{
		if (ColumnType.IsValid())
		{
			VerifiedColumnTypes.Add(ColumnType.Get());
		}
	}
	return VerifiedColumnTypes;
}

TSharedPtr<FTypedElementWidgetConstructor> FTypedElementSceneOutliner::CreateHeaderWidgetConstructor(
	ITypedElementDataStorageInterface& Storage, ITypedElementDataStorageUiInterface& StorageUi, 
	TypedElementQueryHandle Query, TConstArrayView<TWeakObjectPtr<const UScriptStruct>> ColumnTypes)
{
	using MatchApproach = ITypedElementDataStorageUiInterface::EMatchApproach;

	const ITypedElementDataStorageInterface::FQueryDescription& Description = Storage.GetQueryDescription(Query);
	TypedElementDataStorage::FQueryMetaDataView MetaDataView(Description);

	TArray<TWeakObjectPtr<const UScriptStruct>> VerifiedColumnTypes = CreateVerifiedColumnTypeAray(ColumnTypes);
	TSharedPtr<FTypedElementWidgetConstructor> Constructor;

	FName HeaderNames[] = { TEXT("SceneOutliner.Header"), TEXT("General.Header") };
	for (const FName& Purpose : HeaderNames)
	{
		StorageUi.CreateWidgetConstructors(Purpose, MatchApproach::ExactMatch, VerifiedColumnTypes, MetaDataView,
			[&Constructor, ColumnTypes](
				TUniquePtr<FTypedElementWidgetConstructor> CreatedConstructor, 
				TConstArrayView<TWeakObjectPtr<const UScriptStruct>> MatchedColumnTypes)
			{
				if (ColumnTypes.Num() == MatchedColumnTypes.Num())
				{
					Constructor = TSharedPtr<FTypedElementWidgetConstructor>(CreatedConstructor.Release());
				}
				// Either this was the exact match so no need to search further or the longest possible chain didn't match so the next ones will 
				// always be shorter in both cases just return.
				return false;
			});
		if (Constructor)
		{
			return Constructor;
		}
	}
	FName HeaderDefaultNames[] = { TEXT("SceneOutliner.Header.Default"), TEXT("General.Header.Default") };
	for (const FName& Purpose : HeaderDefaultNames)
	{
		StorageUi.CreateWidgetConstructors(Purpose, MetaDataView,
			[&Constructor, ColumnTypes](
				TUniquePtr<FTypedElementWidgetConstructor> CreatedConstructor,
				TConstArrayView<TWeakObjectPtr<const UScriptStruct>> MatchedColumnTypes)
			{
				Constructor = TSharedPtr<FTypedElementWidgetConstructor>(CreatedConstructor.Release());
				return false;
			});
		if (Constructor)
		{
			return Constructor;
		}
	}
	return nullptr;
}

void FTypedElementSceneOutliner::RegisterDealiaser(const FTreeItemIDDealiaser& InDealiaser)
{
	Dealiaser = InDealiaser;
}

void FTypedElementSceneOutliner::AssignQuery(TypedElementQueryHandle Query)
{
	using MatchApproach = ITypedElementDataStorageUiInterface::EMatchApproach;
	constexpr int32 DefaultPriorityIndex = 100;
	FTypedElementSceneOutlinerQueryBinder& Binder = FTypedElementSceneOutlinerQueryBinder::GetInstance();

	if (TSharedPtr<ISceneOutliner> OutlinerPinned = Outliner.Pin())
	{
		const ITypedElementDataStorageInterface::FQueryDescription& Description = Storage->GetQueryDescription(Query);
		TypedElementDataStorage::FQueryMetaDataView MetaDataView(Description);

		ClearColumns(*OutlinerPinned);

		if (Description.Action == ITypedElementDataStorageInterface::FQueryDescription::EActionType::Select)
		{
			int32 SelectionCount = Description.SelectionTypes.Num();
			AddedColumns.Reset(SelectionCount);

			TArray<TWeakObjectPtr<const UScriptStruct>> ColumnTypes = CreateVerifiedColumnTypeAray(Description.SelectionTypes);

			int32 IndexOffset = 0;
			auto ColumnConstructor = [this, Query, &IndexOffset, &OutlinerPinned, Binder](
				TUniquePtr<FTypedElementWidgetConstructor> Constructor, TConstArrayView<TWeakObjectPtr<const UScriptStruct>> ColumnTypes)
				{
					TSharedPtr<FTypedElementWidgetConstructor> CellConstructor(Constructor.Release());

					/* If we have a fallback column for this query, remove it, take over it's priority and 
					 * replace it with the TEDS column. But also allow the TEDS-Outliner column to fallback to it for
					 * data not in TEDS yet.
				 	 */
					FName FallbackColumn = Binder.FindOutlinerColumnFromTEDSColumns(ColumnTypes);
					const FSceneOutlinerColumnInfo* FallbackColumnInfo = OutlinerPinned->GetSharedData().ColumnMap.Find(FallbackColumn);
					int32 ColumnPriority = FallbackColumnInfo ? FallbackColumnInfo->PriorityIndex : DefaultPriorityIndex + IndexOffset;

					OutlinerPinned->RemoveColumn(FallbackColumn);

					FName NameId = FindLongestMatchingName(ColumnTypes, IndexOffset);
					AddedColumns.Add(NameId);
					OutlinerPinned->AddColumn(NameId,
						FSceneOutlinerColumnInfo(ESceneOutlinerColumnVisibility::Visible, ColumnPriority,
							FCreateSceneOutlinerColumn::CreateLambda(
								[this, Query, NameId, &ColumnTypes, CellConstructor, &OutlinerPinned, FallbackColumn](ISceneOutliner&)
								{
									TSharedPtr<FTypedElementWidgetConstructor> HeaderConstructor = 
										CreateHeaderWidgetConstructor(*Storage, *StorageUi, Query, ColumnTypes);
									return MakeShared<FOutlinerColumn>(
										Query, *Storage, *StorageUi, *StorageCompatibility, NameId,
										TArray<TWeakObjectPtr<const UScriptStruct>>(ColumnTypes.GetData(), ColumnTypes.Num()), 
										MoveTemp(HeaderConstructor), CellConstructor, FallbackColumn, *OutlinerPinned.Get(), Dealiaser);

								})
						)
					);
					++IndexOffset;
					return true;
				};
			StorageUi->CreateWidgetConstructors(FName(TEXT("SceneOutliner.Cell")), MatchApproach::LongestMatch, ColumnTypes, 
				MetaDataView, ColumnConstructor);
			StorageUi->CreateWidgetConstructors(FName(TEXT("General.Cell")), MatchApproach::LongestMatch, ColumnTypes, 
				MetaDataView, ColumnConstructor);

			for (TWeakObjectPtr<const UScriptStruct>& ColumnType : ColumnTypes)
			{
				FName FallbackColumn = Binder.FindOutlinerColumnFromTEDSColumns({ColumnType});

				auto AssignWidgetToColumn = [this, Query, ColumnType, &IndexOffset, &OutlinerPinned, FallbackColumn](
					TUniquePtr<FTypedElementWidgetConstructor> Constructor, TConstArrayView<TWeakObjectPtr<const UScriptStruct>>)
				{
					TSharedPtr<FTypedElementWidgetConstructor> CellConstructor(Constructor.Release());
					FName NameId = FName(ColumnType->GetDisplayNameText().ToString());
					AddedColumns.Add(NameId);
					OutlinerPinned->AddColumn(NameId,
						FSceneOutlinerColumnInfo(ESceneOutlinerColumnVisibility::Visible, DefaultPriorityIndex + IndexOffset,
							FCreateSceneOutlinerColumn::CreateLambda(
								[this, Query, NameId, ColumnType, CellConstructor, &OutlinerPinned, FallbackColumn](ISceneOutliner&)
								{
									TArray<TWeakObjectPtr<const UScriptStruct>> ColumnTypesStored;
									ColumnTypesStored.Add(ColumnType);
									TSharedPtr<FTypedElementWidgetConstructor> HeaderConstructor =
										CreateHeaderWidgetConstructor(*Storage, *StorageUi, Query, { ColumnType });
									return MakeShared<FOutlinerColumn>(
										Query, *Storage, *StorageUi, *StorageCompatibility, NameId, MoveTemp(ColumnTypesStored),
										HeaderConstructor, CellConstructor, FallbackColumn, *OutlinerPinned.Get(), Dealiaser);

								})
						)
					);
					++IndexOffset;
					return false;
				};
				int32 BeforeIndexOffset = IndexOffset;
				StorageUi->CreateWidgetConstructors(FName(TEXT("SceneOutliner.Cell.Default")), MetaDataView, AssignWidgetToColumn);
				if (BeforeIndexOffset == IndexOffset)
				{
					StorageUi->CreateWidgetConstructors(FName(TEXT("General.Cell.Default")), MetaDataView, AssignWidgetToColumn);
					if (BeforeIndexOffset == IndexOffset)
					{
						++IndexOffset;
					}
				}
			}
		}
	}
}

FName FTypedElementSceneOutliner::FindLongestMatchingName(TConstArrayView<TWeakObjectPtr<const UScriptStruct>> ColumnTypes, int32 DefaultNameIndex)
{
	switch (ColumnTypes.Num())
	{
	case 0:
		return FName(TEXT("Column"), DefaultNameIndex);
	case 1:
		return FName(ColumnTypes[0]->GetDisplayNameText().ToString());
	default:
	{
		FText LongestMatchText = ColumnTypes[0]->GetDisplayNameText();
		FStringView LongestMatch = LongestMatchText.ToString();
		const TWeakObjectPtr<const UScriptStruct>* ItEnd = ColumnTypes.end();
		const TWeakObjectPtr<const UScriptStruct>* It = ColumnTypes.begin();
		++It; // Skip the first entry as that's already set.
		for (; It != ItEnd; ++It)
		{
			FText NextMatchText = (*It)->GetDisplayNameText();
			FStringView NextMatch = NextMatchText.ToString();

			int32 MatchSize = 0;
			auto ItLeft = LongestMatch.begin();
			auto ItLeftEnd = LongestMatch.end();
			auto ItRight = NextMatch.begin();
			auto ItRightEnd = NextMatch.end();
			while (
				ItLeft != ItLeftEnd &&
				ItRight != ItRightEnd &&
				*ItLeft == *ItRight)
			{
				++MatchSize;
				++ItLeft;
				++ItRight;
			}

			// At least 3 letters have to match to avoid single or double letter names which typically mean nothing.
			if (MatchSize > 2)
			{
				LongestMatch.LeftInline(MatchSize);
			}
			else
			{
				// There are not enough characters in the string that match.
				return FName(TEXT("Column"), DefaultNameIndex);
			}
		}
		return FName(LongestMatch);
	}
	};
}

void FTypedElementSceneOutliner::ClearColumns(ISceneOutliner& InOutliner)
{
	for (FName ColumnName : AddedColumns)
	{
		InOutliner.RemoveColumn(ColumnName);
	}
}

#undef LOCTEXT_NAMESPACE
