// Copyright Epic Games, Inc. All Rights Reserved.

#include "TypedElementDatabase.h"

#include "Editor.h"
#include "Engine/World.h"
#include "MassEntityEditorSubsystem.h"
#include "Stats/Stats2.h"
#include "TickTaskManagerInterface.h"
#include "MassSubsystemAccess.h"

void UTypedElementDatabase::Initialize()
{
	check(GEditor);
	UMassEntityEditorSubsystem* Mass = GEditor->GetEditorSubsystem<UMassEntityEditorSubsystem>();
	check(Mass);
	Mass->GetOnPreTickDelegate().AddUObject(this, &UTypedElementDatabase::OnPreMassTick);

	ActiveEditorEntityManager = Mass->GetMutableEntityManager().AsShared();
}

void UTypedElementDatabase::Deinitialize()
{
	Reset();
}

void UTypedElementDatabase::OnPreMassTick(float DeltaTime)
{
	checkf(IsAvailable(), TEXT("Typed Element Database was ticked while it's not ready."));
	OnUpdateDelegate.Broadcast();
}

TSharedPtr<FMassEntityManager> UTypedElementDatabase::GetActiveMutableEditorEntityManager()
{
	return ActiveEditorEntityManager;
}

TSharedPtr<const FMassEntityManager> UTypedElementDatabase::GetActiveEditorEntityManager() const
{
	return ActiveEditorEntityManager;
}

TypedElementTableHandle UTypedElementDatabase::RegisterTable(TConstArrayView<const UScriptStruct*> ColumnList)
{
	return RegisterTable(ColumnList, {});
}

TypedElementTableHandle UTypedElementDatabase::RegisterTable(TConstArrayView<const UScriptStruct*> ColumnList, const FName Name)
{
	if (ActiveEditorEntityManager && (!Name.IsValid() || !TableNameLookup.Contains(Name)))
	{
		TypedElementTableHandle Result = Tables.Num();
		Tables.Add(ActiveEditorEntityManager->CreateArchetype(ColumnList, Name));
		if (Name.IsValid())
		{
			TableNameLookup.Add(Name, Result);
		}
		return Result;
	}
	return TypedElementInvalidTableHandle;
}

TypedElementTableHandle UTypedElementDatabase::RegisterTable(TypedElementTableHandle SourceTable,
	TConstArrayView<const UScriptStruct*> ColumnList)
{
	return RegisterTable(SourceTable, ColumnList, {});
}

TypedElementTableHandle UTypedElementDatabase::RegisterTable(TypedElementTableHandle SourceTable, 
	TConstArrayView<const UScriptStruct*> ColumnList, const FName Name)
{
	if (ActiveEditorEntityManager && (!Name.IsValid() || !TableNameLookup.Contains(Name)) && SourceTable < Tables.Num())
	{
		TypedElementTableHandle Result = Tables.Num();
		Tables.Add(ActiveEditorEntityManager->CreateArchetype(Tables[SourceTable], ColumnList, Name));
		if (Name.IsValid())
		{
			TableNameLookup.Add(Name, Result);
		}
		return Result;
	}
	return TypedElementInvalidTableHandle;
}

TypedElementTableHandle UTypedElementDatabase::FindTable(const FName Name)
{
	TypedElementTableHandle* TableHandle = TableNameLookup.Find(Name);
	return TableHandle ? *TableHandle : TypedElementInvalidTableHandle;
}

TypedElementRowHandle UTypedElementDatabase::AddRow(TypedElementTableHandle Table)
{
	checkf(Table < Tables.Num(), TEXT("Attempting to add a row to a non-existing table."));
	return ActiveEditorEntityManager ? 
		ActiveEditorEntityManager->CreateEntity(Tables[Table]).AsNumber() :
		TypedElementInvalidRowHandle;
}

TypedElementRowHandle UTypedElementDatabase::AddRow(FName TableName)
{
	TypedElementTableHandle* Table = TableNameLookup.Find(TableName);
	return Table ? AddRow(*Table) : TypedElementInvalidRowHandle;
}

bool UTypedElementDatabase::BatchAddRow(TypedElementTableHandle Table, int32 Count, TypedElementDataStorageCreationCallbackRef OnCreated)
{
	OnCreated.CheckCallable();
	checkf(Table < Tables.Num(), TEXT("Attempting to add multiple rows to a non-existing table."));
	if (ActiveEditorEntityManager)
	{
		TArray<FMassEntityHandle> Entities;
		Entities.Reserve(Count);
		TSharedRef<FMassEntityManager::FEntityCreationContext> Context = 
			ActiveEditorEntityManager->BatchCreateEntities(Tables[Table], Count, Entities);
		
		for (FMassEntityHandle Entity : Entities)
		{
			OnCreated(Entity.AsNumber());
		}

		return true;
	}
	return false;
}

bool UTypedElementDatabase::BatchAddRow(FName TableName, int32 Count, TypedElementDataStorageCreationCallbackRef OnCreated)
{
	TypedElementTableHandle* Table = TableNameLookup.Find(TableName);
	return Table ? BatchAddRow(*Table, Count, OnCreated) : false;
}

void UTypedElementDatabase::RemoveRow(TypedElementRowHandle Row)
{
	if (ActiveEditorEntityManager)
	{
		ActiveEditorEntityManager->DestroyEntity(FMassEntityHandle::FromNumber(Row));
	}
}

void UTypedElementDatabase::AddTag(TypedElementRowHandle Row, const UScriptStruct* TagType)
{
	checkf(TagType && TagType->IsChildOf(FMassTag::StaticStruct()),
		TEXT("Tag type '%s' is invalid as it needs to be set or derived from FMassTag."), *GetPathNameSafe(TagType));

	FMassEntityHandle Entity = FMassEntityHandle::FromNumber(Row);
	if (ActiveEditorEntityManager && ActiveEditorEntityManager->IsEntityValid(Entity))
	{
		ActiveEditorEntityManager->AddTagToEntity(Entity, TagType);
	}
}

void UTypedElementDatabase::AddTag(TypedElementRowHandle Row, FTopLevelAssetPath TagName)
{
	bool bExactMatch = true;
	UScriptStruct* TagStructInfo = Cast<UScriptStruct>(StaticFindObject(UScriptStruct::StaticClass(), TagName, bExactMatch));
	if (TagStructInfo)
	{
		AddTag(Row, TagStructInfo);
	}
}

void* UTypedElementDatabase::AddOrGetColumnData(TypedElementRowHandle Row, const UScriptStruct* ColumnType)
{
	checkf(ColumnType && ColumnType->IsChildOf(FMassFragment::StaticStruct()), 
		TEXT("Colum type '%s' is invalid as it needs to be set or derived from FMassFragment."), *GetPathNameSafe(ColumnType));

	FMassEntityHandle Entity = FMassEntityHandle::FromNumber(Row);
	if (ActiveEditorEntityManager && ActiveEditorEntityManager->IsEntityValid(Entity))
	{
		FStructView Column = ActiveEditorEntityManager->GetFragmentDataStruct(Entity, ColumnType);
		if (!Column.IsValid())
		{
			ActiveEditorEntityManager->AddFragmentToEntity(Entity, ColumnType);
			Column = ActiveEditorEntityManager->GetFragmentDataStruct(Entity, ColumnType);
			checkf(Column.IsValid(), TEXT("Added a new column to the Typed Element's data storae, but it couldn't be retrieved."));

		}
		return Column.GetMutableMemory();
	}
	return nullptr;
}

ColumnDataResult UTypedElementDatabase::AddOrGetColumnData(TypedElementRowHandle Row, FTopLevelAssetPath ColumnName)
{
	constexpr bool bExactMatch = true;
	UScriptStruct* FragmentStructInfo = Cast<UScriptStruct>(StaticFindObject(UScriptStruct::StaticClass(), ColumnName, bExactMatch));
	return FragmentStructInfo ?
		ColumnDataResult{ FragmentStructInfo, AddOrGetColumnData(Row, FragmentStructInfo) }:
		ColumnDataResult{ nullptr, nullptr };
}

ColumnDataResult UTypedElementDatabase::AddOrGetColumnData(TypedElementRowHandle Row, FTopLevelAssetPath ColumnName,
	TConstArrayView<TypedElement::ColumnUtils::Argument> Arguments)
{
	ColumnDataResult Result = AddOrGetColumnData(Row, ColumnName);
	if (Result.Description && Result.Data)
	{
		TypedElement::ColumnUtils::SetColumnValues(Result.Data, Result.Description, Arguments);
		return Result;
	}
	else
	{
		return ColumnDataResult{ nullptr, nullptr };
	}
}

void* UTypedElementDatabase::GetColumnData(TypedElementRowHandle Row, const UScriptStruct* ColumnType)
{
	checkf(ColumnType && ColumnType->IsChildOf(FMassFragment::StaticStruct()),
		TEXT("Colum type '%s' is invalid as it needs to be set or derived from FMassFragment."), *GetPathNameSafe(ColumnType));

	FMassEntityHandle Entity = FMassEntityHandle::FromNumber(Row);
	if (ActiveEditorEntityManager && ActiveEditorEntityManager->IsEntityValid(Entity))
	{
		FStructView Column = ActiveEditorEntityManager->GetFragmentDataStruct(Entity, ColumnType);
		if (Column.IsValid())
		{
			return Column.GetMutableMemory();
		}
	}
	return nullptr;
}

ColumnDataResult UTypedElementDatabase::GetColumnData(TypedElementRowHandle Row, FTopLevelAssetPath ColumnName)
{
	FMassEntityHandle Entity = FMassEntityHandle::FromNumber(Row);
	if (ActiveEditorEntityManager && ActiveEditorEntityManager->IsEntityValid(Entity))
	{
		const UScriptStruct* FragmentType = nullptr;
		FMassArchetypeHandle Archetype = ActiveEditorEntityManager->GetArchetypeForEntityUnsafe(Entity);
		ActiveEditorEntityManager->ForEachArchetypeFragmentType(Archetype, 
			[ColumnName, &FragmentType](const UScriptStruct* Fragment)
			{
				if (Fragment->GetStructPathName() == ColumnName)
				{
					FragmentType = Fragment;
				}
			});

		if (FragmentType != nullptr)
		{
			FStructView Column = ActiveEditorEntityManager->GetFragmentDataStruct(Entity, FragmentType);
			if (Column.IsValid())
			{
				return ColumnDataResult{ FragmentType, Column.GetMutableMemory() };
			}
		}
	}
	return ColumnDataResult{ nullptr, nullptr };
}

TypedElementQueryHandle UTypedElementDatabase::RegisterQuery(const FQueryDescription& Query)
{
	auto LocalToNativeAccess = [](FQueryDescription::EAccessType Access) -> EMassFragmentAccess
	{
		switch (Access)
		{
		case FQueryDescription::EAccessType::ReadOnly:
			return EMassFragmentAccess::ReadOnly;
		case FQueryDescription::EAccessType::ReadWrite:
			return EMassFragmentAccess::ReadWrite;
		default:
			checkf(false, TEXT("Invalid query access type: %i."), static_cast<uint32>(Access));
			return EMassFragmentAccess::MAX;
		}
	};

	QueryStore::Handle Result = Queries.Emplace();
	FExtendedQuery& StoredQuery = Queries.GetMutable(Result);
	StoredQuery.Action = Query.Action;
	StoredQuery.bSimpleQuery = Query.bSimpleQuery;

	FMassEntityQuery& NativeQuery = StoredQuery.NativeQuery;

	if (Query.Action == FQueryDescription::EActionType::Count)
	{
		checkf(Query.Selection.IsEmpty(), TEXT("Count queries for the Typed Elements Data Storage can't have entries for selection."));
	}
	else
	{
		for (const FQueryDescription::FAccessControlledStruct& SelectEntry : Query.Selection)
		{
			checkf(SelectEntry.Type, TEXT("Provided query selection type can not be null."));
			checkf(SelectEntry.Type->IsChildOf(FTypedElementDataStorageColumn::StaticStruct()),
				TEXT("Provided query selection type '%s' is not based on FTypedElementDataStorageColumn."), *SelectEntry.Type->GetStructPathName().ToString());
			NativeQuery.AddRequirement(SelectEntry.Type, LocalToNativeAccess(SelectEntry.Access));
		}
	}

	if (Query.bSimpleQuery) // This backend currently only supports simple queries.
	{
		checkf(Query.ConditionTypes.Num() == Query.ConditionOperators.Num(),
			TEXT("The types and operators for a typed element query have gone out of sync."));
		
		const FQueryDescription::FOperator* Operand = Query.ConditionOperators.GetData();
		for (FQueryDescription::EOperatorType Type : Query.ConditionTypes)
		{
			EMassFragmentPresence Presence;
			switch (Type)
			{
			case FQueryDescription::EOperatorType::SimpleAll:
				Presence = EMassFragmentPresence::All;
				break;
			case FQueryDescription::EOperatorType::SimpleAny:
				Presence = EMassFragmentPresence::Any;
				break;
			case FQueryDescription::EOperatorType::SimpleNone:
				Presence = EMassFragmentPresence::None;
				break;
			default:
				continue;
			}

			if (Operand->Type->IsChildOf(FMassTag::StaticStruct()))
			{
				NativeQuery.AddTagRequirement(*(Operand->Type), Presence);
			}
			else if (Operand->Type->IsChildOf(FMassFragment::StaticStruct()))
			{
				NativeQuery.AddRequirement(Operand->Type, EMassFragmentAccess::None, Presence);
			}

			++Operand;
		}
	}

	for (const FQueryDescription::FAccessControlledClass& DependencyEntry : Query.Dependencies)
	{
		checkf(DependencyEntry.Type, TEXT("Provided query dependcy type can not be null."));
		checkf(DependencyEntry.Type->IsChildOf<UWorldSubsystem>(),
			TEXT("Provided query dependency type '%s' is not based on UWorldSubSystem."), *DependencyEntry.Type->GetStructPathName().ToString());
		
		constexpr bool bGameThreadOnly = true;
		NativeQuery.AddSubsystemRequirement(const_cast<UClass*>(DependencyEntry.Type), LocalToNativeAccess(DependencyEntry.Access), bGameThreadOnly);
	}

	return Result.Handle;
}

void UTypedElementDatabase::UnregisterQuery(TypedElementQueryHandle Query)
{
	QueryStore::Handle Handle;
	Handle.Handle = Query;

	if (Queries.IsAlive(Handle))
	{
		Queries.Get(Handle).NativeQuery.Clear();
	}

	Queries.Remove(Handle);
}

ITypedElementDataStorageInterface::FQueryResult UTypedElementDatabase::RunQuery(TypedElementQueryHandle Query)
{
	FQueryResult Result;

	QueryStore::Handle Handle;
	Handle.Handle = Query;

	if (Queries.IsAlive(Handle))
	{
		FExtendedQuery& QueryData = Queries.Get(Handle);
		if (QueryData.bSimpleQuery)
		{
			switch (QueryData.Action)
			{
			case FQueryDescription::EActionType::None:
				Result.Completed = FQueryResult::ECompletion::Fully;
				break;
			case FQueryDescription::EActionType::Select:
				checkf(false, TEXT("Support for this option will be coming in a future update."));
				Result.Completed = FQueryResult::ECompletion::Unsupported;
				break;
			case FQueryDescription::EActionType::Count:
				checkf(ActiveEditorEntityManager, 
					TEXT("Unable to run Typed Element Data Storage query before a MASS Entity Manager has been assigned."));
				Result.Count = QueryData.NativeQuery.GetNumMatchingEntities(*ActiveEditorEntityManager);
				Result.Completed = FQueryResult::ECompletion::Fully;
				break;
			default:
				Result.Completed = FQueryResult::ECompletion::Unsupported;
				break;
			}
		}
		else
		{
			checkf(false, TEXT("Support for this option will be coming in a future update."));
			Result.Completed = FQueryResult::ECompletion::Unsupported;
		}
	}
	else
	{
		Result.Completed = FQueryResult::ECompletion::Unavailable;
	}

	return Result;
}

FTypedElementOnDataStorageUpdate& UTypedElementDatabase::OnUpdate()
{
	return OnUpdateDelegate;
}

bool UTypedElementDatabase::IsAvailable() const
{
	return bool(ActiveEditorEntityManager);
}

void* UTypedElementDatabase::GetExternalSystemAddress(UClass* Target)
{
	if (Target && Target->IsChildOf<USubsystem>())
	{
		return FMassSubsystemAccess::FetchSubsystemInstance(/*World=*/nullptr, Target);
	}
	return nullptr;
}

void UTypedElementDatabase::Reset()
{
	Tables.Reset();
	TableNameLookup.Reset();
	ActiveEditorEntityManager.Reset();
}