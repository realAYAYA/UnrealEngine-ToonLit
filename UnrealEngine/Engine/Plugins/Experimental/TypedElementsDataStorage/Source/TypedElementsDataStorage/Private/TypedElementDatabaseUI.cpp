// Copyright Epic Games, Inc. All Rights Reserved.

#include "TypedElementDatabaseUI.h"

#include "Algo/BinarySearch.h"
#include "Algo/Sort.h"
#include "Algo/Unique.h"
#include "Elements/Columns/TypedElementMiscColumns.h"
#include "Elements/Columns/TypedElementSlateWidgetColumns.h"
#include "Elements/Columns/TypedElementTypeInfoColumns.h"
#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "Elements/Interfaces/TypedElementDataStorageCompatibilityInterface.h"
#include "GenericPlatform/GenericPlatformMemory.h"
#include "Widgets/SlateControlledConstruction.h"

DEFINE_LOG_CATEGORY(LogTypedElementDatabaseUI);

namespace Internal
{
	// Source: https://en.cppreference.com/w/cpp/utility/variant/visit
	template<class... Ts>
	struct TOverloaded : Ts...
	{ 
		using Ts::operator()...; 
	};

	template<class... Ts> TOverloaded(Ts...) -> TOverloaded<Ts...>;
}

void UTypedElementDatabaseUi::Initialize(
	ITypedElementDataStorageInterface* StorageInterface,
	ITypedElementDataStorageCompatibilityInterface* StorageCompatibilityInterface)
{
	checkf(StorageInterface, TEXT("Typed Element's Database compatibility manager is being initialized with an invalid storage target."));

	Storage = StorageInterface;
	StorageCompatibility = StorageCompatibilityInterface;
	CreateStandardArchetypes();
}

void UTypedElementDatabaseUi::Deinitialize()
{
}

void UTypedElementDatabaseUi::RegisterWidgetPurpose(FName Purpose, EPurposeType Type, FText Description)
{
	FPurposeInfo* PurposeInfo = WidgetPurposes.Find(Purpose);
	if (!PurposeInfo)
	{
		FPurposeInfo& NewInfo = WidgetPurposes.Add(Purpose);
		NewInfo.Type = Type;
		NewInfo.Description = MoveTemp(Description);
	}
}

bool UTypedElementDatabaseUi::RegisterWidgetFactory(FName Purpose, const UScriptStruct* Constructor)
{
	checkf(Constructor->IsChildOf(FTypedElementWidgetConstructor::StaticStruct()),
		TEXT("Attempting to register a Typed Elements widget constructor '%s' that isn't derived from FTypedElementWidgetConstructor."),
		*Constructor->GetFullName());
	
	if (FPurposeInfo* PurposeInfo = WidgetPurposes.Find(Purpose))
	{
		switch (PurposeInfo->Type)
		{
		case ITypedElementDataStorageUiInterface::EPurposeType::Generic:
			PurposeInfo->Factories.Emplace(Constructor);
			PurposeInfo->bIsSorted = false;
			return true;
		case ITypedElementDataStorageUiInterface::EPurposeType::UniqueByName:
			if (PurposeInfo->Factories.IsEmpty())
			{
				PurposeInfo->Factories.Emplace(Constructor);
				PurposeInfo->bIsSorted = false;
			}
			else
			{
				PurposeInfo->Factories.EmplaceAt(0, Constructor);
			}
			return true;
		case ITypedElementDataStorageUiInterface::EPurposeType::UniqueByNameAndColumn:
			UE_LOG(LogTypedElementDatabaseUI, Warning,
				TEXT("Unable to register widget factory '%s' as purpose '%s' requires at least one column for matching."), 
				*Constructor->GetName(), *Purpose.ToString());
			return false;
		default:
			checkf(false, TEXT("Unexpected ITypedElementDataStorageUiInterface::EPurposeType found provided when registering widget factory."));
			return false;
		}
	}
	else
	{
		UE_LOG(LogTypedElementDatabaseUI, Warning, 
			TEXT("Unable to register widget factory '%s' as purpose '%s' isn't registered."), *Constructor->GetName(), *Purpose.ToString());
		return false;
	}
}

bool UTypedElementDatabaseUi::RegisterWidgetFactory(
	FName Purpose, const UScriptStruct* Constructor, TypedElementDataStorage::FQueryConditions Columns)
{
	if (!Columns.IsEmpty())
	{
		checkf(Constructor->IsChildOf(FTypedElementWidgetConstructor::StaticStruct()),
			TEXT("Attempting to register a Typed Elements widget constructor '%s' that isn't deriving from FTypedElementWidgetConstructor."),
			*Constructor->GetFullName());

		if (FPurposeInfo* PurposeInfo = WidgetPurposes.Find(Purpose))
		{
			switch (PurposeInfo->Type)
			{
			case ITypedElementDataStorageUiInterface::EPurposeType::Generic:
				PurposeInfo->Factories.Emplace(Constructor);
				PurposeInfo->bIsSorted = false;
				return true;
			case ITypedElementDataStorageUiInterface::EPurposeType::UniqueByName:
				if (!Columns.IsEmpty())
				{
					if (PurposeInfo->Factories.IsEmpty())
					{
						PurposeInfo->Factories.Emplace(Constructor);
						PurposeInfo->bIsSorted = false;
					}
					else
					{
						PurposeInfo->Factories.EmplaceAt(0, Constructor);
					}
					return true;
				}
				else
				{
					return false;
				}
			case ITypedElementDataStorageUiInterface::EPurposeType::UniqueByNameAndColumn:
				if (!Columns.IsEmpty())
				{
					PurposeInfo->Factories.Emplace(Constructor, MoveTemp(Columns));
					PurposeInfo->bIsSorted = false;
					return true;
				}
				else
				{
					return false;
				}
			default:
				checkf(false, TEXT("Unexpected ITypedElementDataStorageUiInterface::EPurposeType found provided when registering widget factory."));
				return false;
			}
		}
		else
		{
			UE_LOG(LogTypedElementDatabaseUI, Warning,
				TEXT("Unable to register widget factory '%s' as purpose '%s' isn't registered."), *Constructor->GetName(), *Purpose.ToString());
			return false;
		}
	}
	else
	{
		return RegisterWidgetFactory(Purpose, Constructor);
	}
}

bool UTypedElementDatabaseUi::RegisterWidgetFactory(FName Purpose, TUniquePtr<FTypedElementWidgetConstructor>&& Constructor)
{
	checkf(Constructor->GetTypeInfo(), TEXT("Widget constructor being registered that doesn't have valid type information."));
	
	if (FPurposeInfo* PurposeInfo = WidgetPurposes.Find(Purpose))
	{
		switch (PurposeInfo->Type)
		{
		case ITypedElementDataStorageUiInterface::EPurposeType::Generic:
			PurposeInfo->Factories.Emplace(MoveTemp(Constructor));
			PurposeInfo->bIsSorted = false;
			return true;
		case ITypedElementDataStorageUiInterface::EPurposeType::UniqueByName:
			if (PurposeInfo->Factories.IsEmpty())
			{
				PurposeInfo->Factories.Emplace(MoveTemp(Constructor));
				PurposeInfo->bIsSorted = false;
			}
			else
			{
				PurposeInfo->Factories.EmplaceAt(0, MoveTemp(Constructor));
			}
			return true;
		case ITypedElementDataStorageUiInterface::EPurposeType::UniqueByNameAndColumn:
			UE_LOG(LogTypedElementDatabaseUI, Warning,
				TEXT("Unable to register widget factory '%s' as purpose '%s' requires at least one column for matching."),
				*Constructor->GetTypeInfo()->GetName(), *Purpose.ToString());
			return false;
		default:
			checkf(false, TEXT("Unexpected ITypedElementDataStorageUiInterface::EPurposeType found provided when registering widget factory."));
			return false;
		}
	}
	else
	{
		UE_LOG(LogTypedElementDatabaseUI, Warning, 
			TEXT("Unable to register widget factory as purpose '%s' isn't registered."), *Purpose.ToString());
		return false;
	}
}

bool UTypedElementDatabaseUi::RegisterWidgetFactory(FName Purpose, TUniquePtr<FTypedElementWidgetConstructor>&& Constructor, 
	TypedElementDataStorage::FQueryConditions Columns)
{
	if (!Columns.IsEmpty())
	{
		checkf(Constructor->GetTypeInfo(), TEXT("Widget constructor being registered that doesn't have valid type information."));

		if (FPurposeInfo* PurposeInfo = WidgetPurposes.Find(Purpose))
		{
			switch (PurposeInfo->Type)
			{
			case ITypedElementDataStorageUiInterface::EPurposeType::Generic:
				PurposeInfo->Factories.Emplace(MoveTemp(Constructor));
				PurposeInfo->bIsSorted = false;
				return true;
			case ITypedElementDataStorageUiInterface::EPurposeType::UniqueByName:
				if (!Columns.IsEmpty())
				{
					if (PurposeInfo->Factories.IsEmpty())
					{
						PurposeInfo->Factories.Emplace(MoveTemp(Constructor));
						PurposeInfo->bIsSorted = false;
					}
					else
					{
						PurposeInfo->Factories.EmplaceAt(0, MoveTemp(Constructor));
					}
					return true;
				}
				else
				{
					return false;
				}
			case ITypedElementDataStorageUiInterface::EPurposeType::UniqueByNameAndColumn:
				if (!Columns.IsEmpty())
				{
					PurposeInfo->Factories.Emplace(MoveTemp(Constructor), MoveTemp(Columns));
					PurposeInfo->bIsSorted = false;
					return true;
				}
				else
				{
					return false;
				}
			default:
				checkf(false, TEXT("Unexpected ITypedElementDataStorageUiInterface::EPurposeType found provided when registering widget factory."));
				return false;
			}
		}
		else
		{
			UE_LOG(LogTypedElementDatabaseUI, Warning, TEXT("Unable to register widget factory '%s' as purpose '%s' isn't registered."), 
				*Constructor->GetTypeInfo()->GetName(), *Purpose.ToString());
			return false;
		}
	}
	else
	{
		return RegisterWidgetFactory(Purpose, MoveTemp(Constructor));
	}
}

void UTypedElementDatabaseUi::CreateWidgetConstructors(FName Purpose,
	const TypedElementDataStorage::FMetaDataView& Arguments, const WidgetConstructorCallback& Callback)
{
	if (FPurposeInfo* PurposeInfo = WidgetPurposes.Find(Purpose))
	{
		for (const FWidgetFactory& Factory : PurposeInfo->Factories)
		{
			if (!CreateSingleWidgetConstructor(Factory.Constructor, Arguments, {}, Factory.Columns, Callback))
			{
				return;
			}
		}
	}
}

void UTypedElementDatabaseUi::CreateWidgetConstructors(FName Purpose, EMatchApproach MatchApproach, 
	TArray<TWeakObjectPtr<const UScriptStruct>>& Columns, const TypedElementDataStorage::FMetaDataView& Arguments,
	const WidgetConstructorCallback& Callback)
{
	if (FPurposeInfo* PurposeInfo = WidgetPurposes.Find(Purpose))
	{
		// Sort so searching can be done in a single pass. This would also allow for binary searching, but the number of columns
		// is typically small enough for a binary search to end up being more expensive than a linear search. This may change
		// if/when there are a sufficient enough number of widgets that are bound to a large number of columns.
		Columns.Sort(
			[](const TWeakObjectPtr<const UScriptStruct>& Lhs, const TWeakObjectPtr<const UScriptStruct>& Rhs)
			{
				return Lhs.Get() < Rhs.Get();
			});

		if (!PurposeInfo->bIsSorted)
		{
			// This is the only call that requires the array of factories to be sorted from largest to smallest number
			// of columns, so lazily sort only when needed.
			PurposeInfo->Factories.StableSort(
				[](const FWidgetFactory& Lhs, const FWidgetFactory& Rhs)
				{
					int32 LeftSize = Lhs.Columns.MinimumColumnMatchRequired();
					int32 RightSize = Rhs.Columns.MinimumColumnMatchRequired();
					return LeftSize > RightSize;
				});
			PurposeInfo->bIsSorted = true;
		}

		switch (MatchApproach)
		{
		case EMatchApproach::LongestMatch:
			CreateWidgetConstructors_LongestMatch(PurposeInfo->Factories, Columns, Arguments, Callback);
			break;
		case EMatchApproach::ExactMatch:
			CreateWidgetConstructors_ExactMatch(PurposeInfo->Factories, Columns, Arguments, Callback);
			break;
		case EMatchApproach::SingleMatch:
			CreateWidgetConstructors_SingleMatch(PurposeInfo->Factories, Columns, Arguments, Callback);
			break;
		default:
			checkf(false, TEXT("Unsupported match type (%i) for CreateWidgetConstructors."), 
				static_cast<std::underlying_type_t<EMatchApproach>>(MatchApproach));
		}
	}
}

void UTypedElementDatabaseUi::ConstructWidgets(FName Purpose, const TypedElementDataStorage::FMetaDataView& Arguments,
	const WidgetCreatedCallback& ConstructionCallback)
{
	if (FPurposeInfo* PurposeInfo = WidgetPurposes.Find(Purpose))
	{
		for (const FWidgetFactory& Factory : PurposeInfo->Factories)
		{
			std::visit(Internal::TOverloaded
				{
					[this, &Arguments, &ConstructionCallback](const UScriptStruct* ConstructorType)
					{ 
						FTypedElementWidgetConstructor* Constructor = reinterpret_cast<FTypedElementWidgetConstructor*>(
							FMemory_Alloca_Aligned(ConstructorType->GetStructureSize(), ConstructorType->GetMinAlignment()));
						if (Constructor)
						{
							ConstructorType->InitializeStruct(Constructor);
							CreateWidgetInstance(*Constructor, Arguments, ConstructionCallback);
							ConstructorType->DestroyStruct(&Constructor);
						}
						else
						{
							checkf(false, TEXT("Remaining stack space is too small to create a Typed Elements widget constructor from a description."));
						}
					},
					[this, &Arguments, &ConstructionCallback](const TUniquePtr<FTypedElementWidgetConstructor>& Constructor)
					{
						CreateWidgetInstance(*Constructor, Arguments, ConstructionCallback);
					}
				}, Factory.Constructor);
		}
	}
}

bool UTypedElementDatabaseUi::CreateSingleWidgetConstructor(
	const FWidgetFactory::ConstructorType& Constructor,
	const TypedElementDataStorage::FMetaDataView& Arguments,
	TArray<TWeakObjectPtr<const UScriptStruct>> MatchedColumnTypes,
	const TypedElementDataStorage::FQueryConditions& QueryConditions,
	const WidgetConstructorCallback& Callback)
{
	struct Visitor
	{
		Visitor(
			TArray<TWeakObjectPtr<const UScriptStruct>>&& InMatchedColumnTypes,
			const TypedElementDataStorage::FQueryConditions& InQueryConditions,
			const TypedElementDataStorage::FMetaDataView& InArguments,
			const WidgetConstructorCallback& InCallback) 
			: MatchedColumnTypes(MoveTemp(InMatchedColumnTypes))
			, QueryConditions(InQueryConditions)
			, Arguments(InArguments)
			, Callback(InCallback)
		{}

		TArray<TWeakObjectPtr<const UScriptStruct>> MatchedColumnTypes;
		const TypedElementDataStorage::FQueryConditions& QueryConditions;
		const TypedElementDataStorage::FMetaDataView& Arguments;
		const WidgetConstructorCallback& Callback;

		bool operator()(const UScriptStruct* Target)
		{
			TUniquePtr<FTypedElementWidgetConstructor> Result(reinterpret_cast<FTypedElementWidgetConstructor*>(
				FMemory::Malloc(Target->GetStructureSize(), Target->GetMinAlignment())));
			if (Result)
			{
				Target->InitializeStruct(Result.Get());
				Result->Initialize(Arguments, MoveTemp(MatchedColumnTypes), QueryConditions);
				return Callback(MoveTemp(Result), Result->GetMatchedColumns());
			}
			return true;
		}

		bool operator()(const TUniquePtr<FTypedElementWidgetConstructor>& Target)
		{
			const UScriptStruct* TargetType = Target->GetTypeInfo();
			checkf(TargetType, TEXT("Expected valid type information from a widget constructor."));
			TUniquePtr<FTypedElementWidgetConstructor> Result(reinterpret_cast<FTypedElementWidgetConstructor*>(
				FMemory::Malloc(TargetType->GetStructureSize(), TargetType->GetMinAlignment())));
			if (Result)
			{
				TargetType->InitializeStruct(Result.Get());
				TargetType->CopyScriptStruct(Result.Get(), Target.Get());
				Result->Initialize(Arguments, MoveTemp(MatchedColumnTypes), QueryConditions);
				return Callback(MoveTemp(Result), Result->GetMatchedColumns());
			}
			return true;
		}
	};
	return std::visit(Visitor(MoveTemp(MatchedColumnTypes), QueryConditions, Arguments, Callback), Constructor);
}

void UTypedElementDatabaseUi::CreateWidgetInstance(
	FTypedElementWidgetConstructor& Constructor, 
	const TypedElementDataStorage::FMetaDataView& Arguments,
	const WidgetCreatedCallback& ConstructionCallback)
{
	TypedElementRowHandle Row = Storage->AddRow(WidgetTable);
	Storage->AddColumns(Row, Constructor.GetAdditionalColumnsList());
	TSharedPtr<SWidget> Widget = Constructor.ConstructFinalWidget(Row, Storage, this, Arguments);
	if (Widget)
	{
		ConstructionCallback(Widget.ToSharedRef(), Row);
	}
	else
	{
		Storage->RemoveRow(Row);
	}
}

TSharedPtr<SWidget> UTypedElementDatabaseUi::ConstructWidget(TypedElementRowHandle Row, FTypedElementWidgetConstructor& Constructor,
	const TypedElementDataStorage::FMetaDataView& Arguments)
{
	const TArray<TWeakObjectPtr<const UScriptStruct>>& ColumnTypes = Constructor.GetMatchedColumns();

	if (ColumnTypes.Num() == 1)
	{
		if (FTypedElementScriptStructTypeInfoColumn* TypeInfo = Storage->GetColumn<FTypedElementScriptStructTypeInfoColumn>(Row))
		{
			TypeInfo->TypeInfo = *ColumnTypes.begin();
		}
	}
	return Constructor.ConstructFinalWidget(Row, Storage, this, Arguments);
}

void UTypedElementDatabaseUi::ListWidgetPurposes(const WidgetPurposeCallback& Callback) const
{
	for (auto&& It : WidgetPurposes)
	{
		Callback(It.Key, It.Value.Type, It.Value.Description);
	}
}

void UTypedElementDatabaseUi::CreateStandardArchetypes()
{
	WidgetTable = Storage->RegisterTable(MakeArrayView(
		{
			FTypedElementSlateWidgetReferenceColumn::StaticStruct(),
			FTypedElementSlateWidgetReferenceDeletesRowTag::StaticStruct()
		}), FName(TEXT("Editor_WidgetTable")));
}

void UTypedElementDatabaseUi::CreateWidgetConstructors_LongestMatch(const TArray<FWidgetFactory>& WidgetFactories,
	TArray<TWeakObjectPtr<const UScriptStruct>>& Columns, const TypedElementDataStorage::FMetaDataView& Arguments,
	const WidgetConstructorCallback& Callback)
{
	TArray<TWeakObjectPtr<const UScriptStruct>> MatchedColumns;
	for (auto FactoryIt = WidgetFactories.CreateConstIterator(); FactoryIt && !Columns.IsEmpty(); ++FactoryIt)
	{
		if (FactoryIt->Columns.MinimumColumnMatchRequired() > Columns.Num())
		{
			// There are more columns required for this factory than there are in the requested columns list so skip this
			// factory.
			continue;
		}

		MatchedColumns.Reset();

		if (FactoryIt->Columns.Verify(MatchedColumns, Columns, true))
		{
			// Remove the found columns from the requested list.
			Algo::SortBy(MatchedColumns, [](const TWeakObjectPtr<const UScriptStruct>& Column) { return Column.Get(); });
			MatchedColumns.SetNum(Algo::Unique(MatchedColumns), EAllowShrinking::No);
			
			TWeakObjectPtr<const UScriptStruct>* ColumnsIt = Columns.GetData();
			TWeakObjectPtr<const UScriptStruct>* ColumnsEnd = ColumnsIt + Columns.Num();
			int32 ColumnIndex = 0;
			for (const TWeakObjectPtr<const UScriptStruct>& MatchedColumn : MatchedColumns)
			{
				// Remove all the columns that were matched from the provided column list.
				while (*ColumnsIt != MatchedColumn)
				{
					++ColumnIndex;
					++ColumnsIt;
					if (ColumnsIt == ColumnsEnd)
					{
						ensureMsgf(false, TEXT("A previously found matching column can't be found in the original array."));
						return;
					}
				}
				Columns.RemoveAt(ColumnIndex, 1, EAllowShrinking::No);
				--ColumnsEnd;
			}
			
			if (!CreateSingleWidgetConstructor(FactoryIt->Constructor, Arguments, MoveTemp(MatchedColumns), FactoryIt->Columns, Callback))
			{
				return;
			}
		}
	}
}

void UTypedElementDatabaseUi::CreateWidgetConstructors_ExactMatch(const TArray<FWidgetFactory>& WidgetFactories,
	TArray<TWeakObjectPtr<const UScriptStruct>>& Columns, const TypedElementDataStorage::FMetaDataView& Arguments,
	const WidgetConstructorCallback& Callback)
{
	int32 ColumnCount = Columns.Num();
	TArray<TWeakObjectPtr<const UScriptStruct>> MatchedColumns;
	for (const FWidgetFactory& Factory : WidgetFactories)
	{
		// If there are more matches required that there are columns, then there will never be an exact match.
		// Less than the column count can still result in a match that covers all columns.
		if (Factory.Columns.MinimumColumnMatchRequired() > ColumnCount)
		{
			continue;
		}

		MatchedColumns.Reset();

		if (Factory.Columns.Verify(MatchedColumns, Columns, true))
		{
			Algo::SortBy(MatchedColumns, [](const TWeakObjectPtr<const UScriptStruct>& Column) { return Column.Get(); });
			MatchedColumns.SetNum(Algo::Unique(MatchedColumns), EAllowShrinking::No);
			if (MatchedColumns.Num() == Columns.Num())
			{
				Columns.Reset();
				CreateSingleWidgetConstructor(Factory.Constructor, Arguments, MoveTemp(MatchedColumns), Factory.Columns, Callback);
				return;
			}
		}
	}
}

void UTypedElementDatabaseUi::CreateWidgetConstructors_SingleMatch(const TArray<FWidgetFactory>& WidgetFactories,
	TArray<TWeakObjectPtr<const UScriptStruct>>& Columns, const TypedElementDataStorage::FMetaDataView& Arguments,
	const WidgetConstructorCallback& Callback)
{
	auto FactoryIt = WidgetFactories.rbegin();
	auto FactoryEnd = WidgetFactories.rend();

	// Start from the back as the widgets with lower counts will be last.
	for (int32 ColumnIndex = Columns.Num() - 1; ColumnIndex >= 0; --ColumnIndex)
	{
		for (; FactoryIt != FactoryEnd; ++FactoryIt)
		{
			TConstArrayView<TWeakObjectPtr<const UScriptStruct>> ColumnData = (*FactoryIt).Columns.GetColumns();
			if (ColumnData.Num() > 1)
			{
				// Moved passed the point where factories only have a single column.
				return;
			}
			else if (ColumnData.Num() == 0)
			{
				// Need to move further to find factories with exactly one column.
				continue;
			}

			if (ColumnData[0] == Columns[ColumnIndex])
			{
				Columns.RemoveAt(ColumnIndex);
				CreateSingleWidgetConstructor((*FactoryIt).Constructor, Arguments, 
					TArray<TWeakObjectPtr<const UScriptStruct>>(ColumnData), (*FactoryIt).Columns, Callback);
				// Match was found so move on to the next column in the column.
				break;
			}
		}
	}
}



//
// FWidgetFactory
//

UTypedElementDatabaseUi::FWidgetFactory::FWidgetFactory(const UScriptStruct* InConstructor)
	: Constructor(InConstructor)
{
}

UTypedElementDatabaseUi::FWidgetFactory::FWidgetFactory(TUniquePtr<FTypedElementWidgetConstructor>&& InConstructor)
	: Constructor(MoveTemp(InConstructor))
{
	checkf(std::get<TUniquePtr<FTypedElementWidgetConstructor>>(Constructor)->GetTypeInfo(), 
		TEXT("Widget constructor registered that didn't contain valid type information."));
}

UTypedElementDatabaseUi::FWidgetFactory::FWidgetFactory(const UScriptStruct* InConstructor, 
	TypedElementDataStorage::FQueryConditions&& InColumns)
	: Columns(MoveTemp(InColumns))
	, Constructor(InConstructor)
{
}

UTypedElementDatabaseUi::FWidgetFactory::FWidgetFactory(TUniquePtr<FTypedElementWidgetConstructor>&& InConstructor, 
	TypedElementDataStorage::FQueryConditions&& InColumns)
	: Columns(MoveTemp(InColumns))
	, Constructor(MoveTemp(InConstructor))
{
	checkf(std::get<TUniquePtr<FTypedElementWidgetConstructor>>(Constructor)->GetTypeInfo(),
		TEXT("Widget constructor registered that didn't contain valid type information."));
}
