// Copyright Epic Games, Inc. All Rights Reserved.

#include "TypedElementDatabase.h"

#include "Editor.h"
#include "Elements/Interfaces/TypedElementDataStorageFactory.h"
#include "Elements/Framework/TypedElementRegistry.h"
#include "Engine/World.h"
#include "MassCommonTypes.h"
#include "MassEntityEditorSubsystem.h"
#include "MassEntityTypes.h"
#include "MassProcessor.h"
#include "MassSimulationSubsystem.h"
#include "MassSubsystemAccess.h"
#include "Processors/TypedElementProcessorAdaptors.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Stats/Stats2.h"
#include "TickTaskManagerInterface.h"
#include "UObject/UObjectIterator.h"

const FName UTypedElementDatabase::TickGroupName_SyncWidget(TEXT("SyncWidgets"));

FAutoConsoleCommandWithOutputDevice PrintQueryCallbacksConsoleCommand(
	TEXT("TEDS.PrintQueryCallbacks"),
	TEXT("Prints out a list of all processors."),
	FConsoleCommandWithOutputDeviceDelegate::CreateLambda([](FOutputDevice& Output)
		{
			if (UTypedElementRegistry* Registry = UTypedElementRegistry::GetInstance())
			{
				if (UTypedElementDatabase* DataStorage = Cast<UTypedElementDatabase>(Registry->GetMutableDataStorage()))
				{
					DataStorage->DebugPrintQueryCallbacks(Output);
				}
			}
		}));

FAutoConsoleCommandWithOutputDevice PrintSupportedColumnsConsoleCommand(
	TEXT("TEDS.PrintSupportedColumns"),
	TEXT("Prints out a list of available Data Storage columns."),
	FConsoleCommandWithOutputDeviceDelegate::CreateLambda([](FOutputDevice& Output)
		{
			Output.Log(TEXT("The Typed Elements Data Storage supports the following columns:"));
			
			UScriptStruct* FragmentTypeInfo = FMassFragment::StaticStruct();
			UScriptStruct* TagTypeInfo = FMassTag::StaticStruct();
			for (TObjectIterator<UScriptStruct> It; It; ++It)
			{
				if (It->IsChildOf(FragmentTypeInfo) || It->IsChildOf(TagTypeInfo))
				{
					int32 StructureSize = It->GetStructureSize();
					if (StructureSize >= 1024 * 1024)
					{
						Output.Logf(TEXT("    [%6.2f mib] %s"), StructureSize / (1024.0f * 1024.0f), *It->GetFullName());
					}
					else if (StructureSize >= 1024)
					{
						Output.Logf(TEXT("    [%6.2f kib] %s"), StructureSize / 1024.0f, *It->GetFullName());
					}
					else
					{
						Output.Logf(TEXT("    [%6i b  ] %s"), StructureSize, *It->GetFullName());
					}
				}
			}
			Output.Log(TEXT("End of Typed Elements Data Storage supported column list."));
		}));

namespace TypedElementDatabasePrivate
{
	struct ColumnsToBitSetsResult
	{
		bool bMustUpdateFragments = false;
		bool bMustUpdateTags = false;
		
		bool MustUpdate() const { return bMustUpdateFragments || bMustUpdateTags; }
	};
	ColumnsToBitSetsResult ColumnsToBitSets(TConstArrayView<const UScriptStruct*> Columns, FMassFragmentBitSet& Fragments, FMassTagBitSet& Tags)
	{
		ColumnsToBitSetsResult Result;

		for (const UScriptStruct* ColumnType : Columns)
		{
			if (ColumnType->IsChildOf(FMassFragment::StaticStruct()))
			{
				Fragments.Add(*ColumnType);
				Result.bMustUpdateFragments = true;
			}
			else if (ColumnType->IsChildOf(FMassTag::StaticStruct()))
			{
				Tags.Add(*ColumnType);
				Result.bMustUpdateTags = true;
			}
		}
		return Result;
	}
}

void UTypedElementDatabase::Initialize()
{
	check(GEditor);
	UMassEntityEditorSubsystem* Mass = GEditor->GetEditorSubsystem<UMassEntityEditorSubsystem>();
	check(Mass);
	OnPreMassTickHandle = Mass->GetOnPreTickDelegate().AddUObject(this, &UTypedElementDatabase::OnPreMassTick);
	OnPostMassTickHandle = Mass->GetOnPostTickDelegate().AddUObject(this, &UTypedElementDatabase::OnPostMassTick);

	ActiveEditorEntityManager = Mass->GetMutableEntityManager();
	ActiveEditorPhaseManager = Mass->GetMutablePhaseManager();
	if (ActiveEditorEntityManager && ActiveEditorPhaseManager)
	{
		Environment = MakeUnique<FTypedElementDatabaseEnvironment>(*ActiveEditorEntityManager, *ActiveEditorPhaseManager);

		using PhaseType = std::underlying_type_t<EQueryTickPhase>;
		for (PhaseType PhaseId = 0; PhaseId < static_cast<PhaseType>(EQueryTickPhase::Max); ++PhaseId)
		{
			EQueryTickPhase Phase = static_cast<EQueryTickPhase>(PhaseId);
			EMassProcessingPhase MassPhase = FTypedElementQueryProcessorData::MapToMassProcessingPhase(Phase);

			ActiveEditorPhaseManager->GetOnPhaseStart(MassPhase).AddLambda(
				[this, Phase](float DeltaTime)
				{
					PreparePhase(Phase, DeltaTime);
				});

			ActiveEditorPhaseManager->GetOnPhaseEnd(MassPhase).AddLambda(
				[this, Phase](float DeltaTime)
				{
					FinalizePhase(Phase, DeltaTime);
				});

			// Guarantee that syncing to the data storage always happens before syncing to external.
			RegisterTickGroup(GetQueryTickGroupName(EQueryTickGroups::SyncExternalToDataStorage),
				Phase, GetQueryTickGroupName(EQueryTickGroups::SyncDataStorageToExternal), {}, false);
			// Guarantee that widgets syncs happen after external data has been updated and internal processing 
			// completed to the data storage.
			RegisterTickGroup(GetQueryTickGroupName(EQueryTickGroups::SyncWidgets),
				Phase, {}, GetQueryTickGroupName(EQueryTickGroups::Default), true /* Needs main thread*/);
			RegisterTickGroup(GetQueryTickGroupName(EQueryTickGroups::SyncWidgets),
				Phase, {}, GetQueryTickGroupName(EQueryTickGroups::SyncExternalToDataStorage), true /* Needs main thread*/);
		}
	}
}

void UTypedElementDatabase::SetFactories(TConstArrayView<UClass*> FactoryClasses)
{
	Factories.Reserve(FactoryClasses.Num());

	UClass* BaseFactoryType = UTypedElementDataStorageFactory::StaticClass();

	for (UClass* FactoryClass : FactoryClasses)
	{
		if (FactoryClass->HasAnyClassFlags(CLASS_Abstract))
		{
			continue;
		}
		if (!FactoryClass->IsChildOf(BaseFactoryType))
		{
			continue;
		}
		UTypedElementDataStorageFactory* Factory = NewObject<UTypedElementDataStorageFactory>(this, FactoryClass, NAME_None, EObjectFlags::RF_Transient);
		Factories.Add(FFactoryTypePair
			{
				.Type = FactoryClass,
				.Instance = Factory
			});
	}

	Factories.StableSort(
	[](const FFactoryTypePair& Lhs, const FFactoryTypePair& Rhs)
	{
		return Lhs.Instance->GetOrder() < Rhs.Instance->GetOrder();
	});
	
	for (FFactoryTypePair& Factory : Factories)
	{
		Factory.Instance->PreRegister(*this);
	}
}

void UTypedElementDatabase::ResetFactories()
{
	for (int32 Index = Factories.Num() - 1; Index >= 0; --Index)
	{
		const FFactoryTypePair& Factory = Factories[Index];
		Factory.Instance->PreShutdown(*this);
	}
	Factories.Empty();
}

UTypedElementDatabase::FactoryIterator UTypedElementDatabase::CreateFactoryIterator()
{
	return UTypedElementDatabase::FactoryIterator(this);
}

UTypedElementDatabase::FactoryConstIterator UTypedElementDatabase::CreateFactoryIterator() const
{
	return UTypedElementDatabase::FactoryConstIterator(this);
}

const UTypedElementDataStorageFactory* UTypedElementDatabase::FindFactory(const UClass* FactoryType) const
{
	for (const FFactoryTypePair& Factory : Factories)
	{
		if (Factory.Type == FactoryType)
		{
			return Factory.Instance;
		}
	}
	return nullptr;
}

void UTypedElementDatabase::Deinitialize()
{
	checkf(Factories.IsEmpty(), TEXT("ResetFactories should have been called before deinitialized"));
	
	Reset();
}

void UTypedElementDatabase::OnPreMassTick(float DeltaTime)
{
	checkf(IsAvailable(), TEXT("Typed Element Database was ticked while it's not ready."));
	
	OnUpdateDelegate.Broadcast();
	// Process pending commands after other systems have had a chance to update. Other systems may have executed work needed
	// to complete pending work.
	FTypedElementDatabaseCommandBuffer::ProcessBuffer(DeferredCommands, *Environment);
	DeferredCommands.Reset();
}

void UTypedElementDatabase::OnPostMassTick(float DeltaTime)
{
	checkf(IsAvailable(), TEXT("Typed Element Database was ticked while it's not ready."));
	
	Environment->GetScratchBuffer().BatchDelete();
}

TSharedPtr<FMassEntityManager> UTypedElementDatabase::GetActiveMutableEditorEntityManager()
{
	return ActiveEditorEntityManager;
}

TSharedPtr<const FMassEntityManager> UTypedElementDatabase::GetActiveEditorEntityManager() const
{
	return ActiveEditorEntityManager;
}

TypedElementTableHandle UTypedElementDatabase::RegisterTable(TConstArrayView<const UScriptStruct*> ColumnList, const FName Name)
{
	if (ActiveEditorEntityManager && !TableNameLookup.Contains(Name))
	{
		TypedElementTableHandle Result = Tables.Num();
		FMassArchetypeCreationParams ArchetypeCreationParams;
		ArchetypeCreationParams.DebugName = Name;
		Tables.Add(ActiveEditorEntityManager->CreateArchetype(ColumnList, ArchetypeCreationParams));
		if (Name.IsValid())
		{
			TableNameLookup.Add(Name, Result);
		}
		return Result;
	}
	return TypedElementInvalidTableHandle;
}

TypedElementTableHandle UTypedElementDatabase::RegisterTable(TypedElementTableHandle SourceTable, 
	TConstArrayView<const UScriptStruct*> ColumnList, const FName Name)
{
	if (ActiveEditorEntityManager && SourceTable < Tables.Num() && !TableNameLookup.Contains(Name))
	{
		TypedElementTableHandle Result = Tables.Num();
		FMassArchetypeCreationParams ArchetypeCreationParams;
		ArchetypeCreationParams.DebugName = Name;
		Tables.Add(ActiveEditorEntityManager->CreateArchetype(Tables[SourceTable], ColumnList, ArchetypeCreationParams));
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

TypedElementRowHandle UTypedElementDatabase::ReserveRow()
{
	return ActiveEditorEntityManager ? ActiveEditorEntityManager->ReserveEntity().AsNumber(): TypedElementInvalidRowHandle;
}

TypedElementRowHandle UTypedElementDatabase::AddRow(TypedElementTableHandle Table)
{
	checkf(Table < Tables.Num(), TEXT("Attempting to add a row to a non-existing table."));
	return ActiveEditorEntityManager ? 
		ActiveEditorEntityManager->CreateEntity(Tables[Table]).AsNumber() :
		TypedElementInvalidRowHandle;
}

bool UTypedElementDatabase::AddRow(TypedElementRowHandle ReservedRow, TypedElementTableHandle Table)
{
	checkf(!HasRowBeenAssigned(ReservedRow), TEXT("Attempting to assign a table to row that already has a table assigned."));
	checkf(Table < Tables.Num(), TEXT("Attempting to add a row to a non-existing table."));
	if (ActiveEditorEntityManager)
	{
		ActiveEditorEntityManager->BuildEntity(FMassEntityHandle::FromNumber(ReservedRow), Tables[Table]);
		return true;
	}
	else
	{
		return false;
	}
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

bool UTypedElementDatabase::BatchAddRow(TypedElementTableHandle Table, TConstArrayView<TypedElementRowHandle> ReservedHandles,
	TypedElementDataStorageCreationCallbackRef OnCreated)
{
	OnCreated.CheckCallable();
	checkf(Table < Tables.Num(), TEXT("Attempting to add multiple rows to a non-existing table."));
	if (ActiveEditorEntityManager)
	{
		TArray<FMassEntityHandle> Entities;
		Entities.Reserve(ReservedHandles.Num());
		FMassEntityHandle* CurrentEntityHandle = Entities.GetData();
		for (TypedElementRowHandle RowHandle : ReservedHandles)
		{
			checkf(!HasRowBeenAssigned(RowHandle), TEXT("Attempting to assign a table to row that already has a table assigned."));
			Entities.Add(FMassEntityHandle::FromNumber(RowHandle));
		}
		
		TSharedRef<FMassEntityManager::FEntityCreationContext> Context =
			ActiveEditorEntityManager->BatchCreateReservedEntities(Tables[Table], Entities);

		for (FMassEntityHandle Entity : Entities)
		{
			OnCreated(Entity.AsNumber());
		}

		return true;
	}
	return false;
}


void UTypedElementDatabase::RemoveRow(TypedElementRowHandle Row)
{
	FMassEntityHandle Entity = FMassEntityHandle::FromNumber(Row);
	if (ActiveEditorEntityManager && ActiveEditorEntityManager->IsEntityValid(Entity))
	{
		Environment->GetIndexTable().RemoveRow(Row);
		if (ActiveEditorEntityManager->IsEntityBuilt(FMassEntityHandle::FromNumber(Row)))
		{
			ActiveEditorEntityManager->DestroyEntity(FMassEntityHandle::FromNumber(Row));
		}
		else
		{
			ActiveEditorEntityManager->ReleaseReservedEntity(FMassEntityHandle::FromNumber(Row));
		}
	}
}

bool UTypedElementDatabase::IsRowAvailable(TypedElementRowHandle Row) const
{
	return ActiveEditorEntityManager ? FTypedElementDatabaseCommandBuffer::Execute_IsRowAvailable(*ActiveEditorEntityManager, Row) : false;
}

bool UTypedElementDatabase::HasRowBeenAssigned(TypedElementRowHandle Row) const
{
	return ActiveEditorEntityManager ? FTypedElementDatabaseCommandBuffer::Execute_HasRowBeenAssigned(*ActiveEditorEntityManager, Row) : false;
}

bool UTypedElementDatabase::AddColumn(TypedElementRowHandle Row, const UScriptStruct* ColumnType)
{
	if (ColumnType && ActiveEditorEntityManager && HasRowBeenAssigned(Row))
	{
		FTypedElementDatabaseCommandBuffer::Execute_AddColumnCommand(*ActiveEditorEntityManager, Row, ColumnType);
	}
	else
	{
		FTypedElementDatabaseCommandBuffer::Queue_AddColumnCommand(DeferredCommands, Row, ColumnType);
	}
	return true;
}

bool UTypedElementDatabase::AddColumn(TypedElementRowHandle Row, FTopLevelAssetPath ColumnName)
{
	bool bExactMatch = true;
	UScriptStruct* ColumnStructInfo = Cast<UScriptStruct>(StaticFindObject(UScriptStruct::StaticClass(), ColumnName, bExactMatch));
	return ColumnStructInfo ? AddColumn(Row, ColumnStructInfo) : false;
}

void UTypedElementDatabase::RemoveColumn(TypedElementRowHandle Row, const UScriptStruct* ColumnType)
{
	if (ActiveEditorEntityManager && HasRowBeenAssigned(Row))
	{
		FTypedElementDatabaseCommandBuffer::Execute_RemoveColumnCommand(*ActiveEditorEntityManager, Row, ColumnType);
	}
	else
	{
		FTypedElementDatabaseCommandBuffer::Queue_RemoveColumnCommand(DeferredCommands, Row, ColumnType);
	}
}

void UTypedElementDatabase::RemoveColumn(TypedElementRowHandle Row, FTopLevelAssetPath ColumnName)
{
	bool bExactMatch = true;
	if (UScriptStruct* ColumnStructInfo = Cast<UScriptStruct>(StaticFindObject(UScriptStruct::StaticClass(), ColumnName, bExactMatch)))
	{
		RemoveColumn(Row, ColumnStructInfo);
	}
}

void* UTypedElementDatabase::AddOrGetColumnData(TypedElementRowHandle Row, const UScriptStruct* ColumnType)
{
	FMassEntityHandle Entity = FMassEntityHandle::FromNumber(Row);
	if (ActiveEditorEntityManager && ActiveEditorEntityManager->IsEntityActive(Entity) &&
		ColumnType && ColumnType->IsChildOf(FMassFragment::StaticStruct()))
	{
		FStructView Column = ActiveEditorEntityManager->GetFragmentDataStruct(Entity, ColumnType);
		if (!Column.IsValid())
		{
			ActiveEditorEntityManager->AddFragmentToEntity(Entity, ColumnType);
			Column = ActiveEditorEntityManager->GetFragmentDataStruct(Entity, ColumnType);
			checkf(Column.IsValid(), TEXT("Added a new column to the Typed Element's data store, but it couldn't be retrieved."));

		}
		return Column.GetMemory();
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

const void* UTypedElementDatabase::GetColumnData(TypedElementRowHandle Row, const UScriptStruct* ColumnType) const
{
	FMassEntityHandle Entity = FMassEntityHandle::FromNumber(Row);
	if (ActiveEditorEntityManager && ActiveEditorEntityManager->IsEntityActive(Entity) &&
		ColumnType && ColumnType->IsChildOf(FMassFragment::StaticStruct()))
	{
		FStructView Column = ActiveEditorEntityManager->GetFragmentDataStruct(Entity, ColumnType);
		if (Column.IsValid())
		{
			return Column.GetMemory();
		}
	}
	return nullptr;
}

void* UTypedElementDatabase::GetColumnData(TypedElementRowHandle Row, const UScriptStruct* ColumnType)
{
	return const_cast<void*>(static_cast<const UTypedElementDatabase*>(this)->GetColumnData(Row, ColumnType));
}

ColumnDataResult UTypedElementDatabase::GetColumnData(TypedElementRowHandle Row, FTopLevelAssetPath ColumnName)
{
	FMassEntityHandle Entity = FMassEntityHandle::FromNumber(Row);
	if (ActiveEditorEntityManager && ActiveEditorEntityManager->IsEntityActive(Entity))
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

		if (FragmentType && FragmentType->IsChildOf(FMassFragment::StaticStruct()))
		{
			FStructView Column = ActiveEditorEntityManager->GetFragmentDataStruct(Entity, FragmentType);
			if (Column.IsValid())
			{
				return ColumnDataResult{ FragmentType, Column.GetMemory() };
			}
		}
	}
	return ColumnDataResult{ nullptr, nullptr };
}

bool UTypedElementDatabase::AddColumns(TypedElementRowHandle Row, TConstArrayView<const UScriptStruct*> Columns)
{
	if (ActiveEditorEntityManager)
	{
		FMassEntityHandle Entity = FMassEntityHandle::FromNumber(Row);
		FMassArchetypeHandle Archetype = ActiveEditorEntityManager->GetArchetypeForEntity(Entity);

		FMassFragmentBitSet FragmentsToAdd;
		FMassTagBitSet TagsToAdd;
		if (TypedElementDatabasePrivate::ColumnsToBitSets(Columns, FragmentsToAdd, TagsToAdd).MustUpdate())
		{
			if (ActiveEditorEntityManager->IsEntityActive(Entity))
			{
				FTypedElementDatabaseCommandBuffer::Execute_AddColumnsCommand(*ActiveEditorEntityManager, Row, FragmentsToAdd, TagsToAdd);
			}
			else
			{
				FTypedElementDatabaseCommandBuffer::Queue_AddColumnsCommand(DeferredCommands, Row, FragmentsToAdd, TagsToAdd);
			}
			return true;
		}
	}
	return false;
}

void UTypedElementDatabase::RemoveColumns(TypedElementRowHandle Row, TConstArrayView<const UScriptStruct*> Columns)
{
	FMassEntityHandle Entity = FMassEntityHandle::FromNumber(Row);
	if (ActiveEditorEntityManager)
	{
		FMassArchetypeHandle Archetype = ActiveEditorEntityManager->GetArchetypeForEntity(Entity);

		FMassFragmentBitSet FragmentsToRemove;
		FMassTagBitSet TagsToRemove;
		if (TypedElementDatabasePrivate::ColumnsToBitSets(Columns, FragmentsToRemove, TagsToRemove).MustUpdate())
		{
			if (ActiveEditorEntityManager->IsEntityActive(Entity))
			{
				FTypedElementDatabaseCommandBuffer::Execute_RemoveColumnsCommand(*ActiveEditorEntityManager, Row, FragmentsToRemove, TagsToRemove);
			}
			else
			{
				FTypedElementDatabaseCommandBuffer::Queue_RemoveColumnsCommand(DeferredCommands, Row, FragmentsToRemove, TagsToRemove);
			}
		}
	}
}

bool UTypedElementDatabase::AddRemoveColumns(TypedElementRowHandle Row,
	TConstArrayView<const UScriptStruct*> ColumnsToAdd, TConstArrayView<const UScriptStruct*> ColumnsToRemove)
{
	bool bResult = false;
	FMassEntityHandle Entity = FMassEntityHandle::FromNumber(Row);
	if (ActiveEditorEntityManager && ActiveEditorEntityManager->IsEntityActive(Entity))
	{
		FMassArchetypeHandle Archetype = ActiveEditorEntityManager->GetArchetypeForEntity(Entity);

		FMassFragmentBitSet FragmentsToAdd;
		FMassTagBitSet TagsToAdd;
		if (TypedElementDatabasePrivate::ColumnsToBitSets(ColumnsToAdd, FragmentsToAdd, TagsToAdd).MustUpdate())
		{
			FMassArchetypeCompositionDescriptor AddComposition(
				MoveTemp(FragmentsToAdd), MoveTemp(TagsToAdd), FMassChunkFragmentBitSet(), FMassSharedFragmentBitSet());
			ActiveEditorEntityManager->AddCompositionToEntity_GetDelta(Entity, AddComposition);
			bResult = true;
		}

		FMassTagBitSet TagsToRemove;
		FMassFragmentBitSet FragmentsToRemove;
		if (TypedElementDatabasePrivate::ColumnsToBitSets(ColumnsToRemove, FragmentsToRemove, TagsToRemove).MustUpdate())
		{
			FMassArchetypeCompositionDescriptor RemoveComposition(
				MoveTemp(FragmentsToRemove), MoveTemp(TagsToRemove), FMassChunkFragmentBitSet(), FMassSharedFragmentBitSet());
			ActiveEditorEntityManager->RemoveCompositionFromEntity(Entity, RemoveComposition);
			bResult = true;
		}
	}
	return bResult;
}

bool UTypedElementDatabase::BatchAddRemoveColumns(TConstArrayView<TypedElementRowHandle> Rows, 
	TConstArrayView<const UScriptStruct*> ColumnsToAdd, TConstArrayView<const UScriptStruct*> ColumnsToRemove)
{	
	if (ActiveEditorEntityManager)
	{
		FMassFragmentBitSet FragmentsToAdd;
		FMassFragmentBitSet FragmentsToRemove;

		FMassTagBitSet TagsToAdd;
		FMassTagBitSet TagsToRemove;

		namespace TEDP = TypedElementDatabasePrivate;

		TEDP::ColumnsToBitSetsResult AddResult = TEDP::ColumnsToBitSets(ColumnsToAdd, FragmentsToAdd, TagsToAdd);
		TEDP::ColumnsToBitSetsResult RemoveResult = TEDP::ColumnsToBitSets(ColumnsToRemove, FragmentsToRemove, TagsToRemove);
		
		if (AddResult.MustUpdate() || RemoveResult.MustUpdate())
		{
			using EntityHandleArray = TArray<FMassEntityHandle, TInlineAllocator<32>>;
			using EntityArchetypeLookup = TMap<FMassArchetypeHandle, EntityHandleArray, TInlineSetAllocator<32>>;
			using ArchetypeEntityArray = TArray<FMassArchetypeEntityCollection, TInlineAllocator<32>>;

			// Sort rows (entities) into to matching table (archetype) bucket.
			EntityArchetypeLookup LookupTable;
			for (TypedElementRowHandle EntityId : Rows)
			{
				FMassEntityHandle Entity = FMassEntityHandle::FromNumber(EntityId);
				if (ActiveEditorEntityManager->IsEntityActive(Entity))
				{
					FMassArchetypeHandle Archetype = ActiveEditorEntityManager->GetArchetypeForEntity(Entity);
					EntityHandleArray& EntityCollection = LookupTable.FindOrAdd(Archetype);
					EntityCollection.Add(Entity);
				}
			}
		
			// Construct table (archetype) specific row (entity) collections.
			ArchetypeEntityArray EntityCollections;
			EntityCollections.Reserve(LookupTable.Num());
			for (auto It = LookupTable.CreateConstIterator(); It; ++It)
			{
				EntityCollections.Emplace(It.Key(), It.Value(), FMassArchetypeEntityCollection::EDuplicatesHandling::FoldDuplicates);
			}

			// Batch update using the appropriate fragment/bit sets.
			if (AddResult.bMustUpdateFragments || RemoveResult.bMustUpdateFragments)
			{
				ActiveEditorEntityManager->BatchChangeFragmentCompositionForEntities(EntityCollections, FragmentsToAdd, FragmentsToRemove);
			}
			if (AddResult.bMustUpdateTags || RemoveResult.bMustUpdateTags)
			{
				ActiveEditorEntityManager->BatchChangeTagsForEntities(EntityCollections, TagsToAdd, TagsToRemove);
			}
			return true;
		}
	}
	return false;
}

bool UTypedElementDatabase::HasColumns(TypedElementRowHandle Row, TConstArrayView<const UScriptStruct*> ColumnTypes) const
{
	FMassEntityHandle Entity = FMassEntityHandle::FromNumber(Row);
	if (ActiveEditorEntityManager && ActiveEditorEntityManager->IsEntityActive(Entity))
	{
		FMassArchetypeHandle Archetype = ActiveEditorEntityManager->GetArchetypeForEntity(Entity);
		const FMassArchetypeCompositionDescriptor& Composition = ActiveEditorEntityManager->GetArchetypeComposition(Archetype);

		bool bHasAllColumns = true;
		const UScriptStruct* const* ColumnTypesEnd = ColumnTypes.end();
		for (const UScriptStruct* const* ColumnType = ColumnTypes.begin(); ColumnType != ColumnTypesEnd && bHasAllColumns; ++ColumnType)
		{
			if ((*ColumnType)->IsChildOf(FMassFragment::StaticStruct()))
			{
				bHasAllColumns = Composition.Fragments.Contains(**ColumnType);
			}
			else if ((*ColumnType)->IsChildOf(FMassTag::StaticStruct()))
			{
				bHasAllColumns = Composition.Tags.Contains(**ColumnType);
			}
			else
			{
				return false;
			}
		}

		return bHasAllColumns;
	}
	return false;
}

bool UTypedElementDatabase::HasColumns(TypedElementRowHandle Row, TConstArrayView<TWeakObjectPtr<const UScriptStruct>> ColumnTypes) const
{
	FMassEntityHandle Entity = FMassEntityHandle::FromNumber(Row);
	if (ActiveEditorEntityManager && ActiveEditorEntityManager->IsEntityActive(Entity))
	{
		FMassArchetypeHandle Archetype = ActiveEditorEntityManager->GetArchetypeForEntity(Entity);
		const FMassArchetypeCompositionDescriptor& Composition = ActiveEditorEntityManager->GetArchetypeComposition(Archetype);

		bool bHasAllColumns = true;
		const TWeakObjectPtr<const UScriptStruct>* ColumnTypesEnd = ColumnTypes.end();
		for (const TWeakObjectPtr<const UScriptStruct>* ColumnType = ColumnTypes.begin(); ColumnType != ColumnTypesEnd && bHasAllColumns; ++ColumnType)
		{
			if (ColumnType->IsValid())
			{
				if ((*ColumnType)->IsChildOf(FMassFragment::StaticStruct()))
				{
					bHasAllColumns = Composition.Fragments.Contains(**ColumnType);
					continue;
				}
				else if ((*ColumnType)->IsChildOf(FMassTag::StaticStruct()))
				{
					bHasAllColumns = Composition.Tags.Contains(**ColumnType);
					continue;
				}
			}
			return false;
		}

		return bHasAllColumns;
	}
	return false;
}

bool UTypedElementDatabase::MatchesColumns(TypedElementDataStorage::RowHandle Row, const TypedElementDataStorage::FQueryConditions& Conditions) const
{
	FMassEntityHandle Entity = FMassEntityHandle::FromNumber(Row);
	if (ActiveEditorEntityManager && ActiveEditorEntityManager->IsEntityActive(Entity))
	{
		FMassArchetypeHandle Archetype = ActiveEditorEntityManager->GetArchetypeForEntity(Entity);
		const FMassArchetypeCompositionDescriptor& Composition = ActiveEditorEntityManager->GetArchetypeComposition(Archetype);

		auto Callback = [&Composition](uint8_t ColumnIndex, TWeakObjectPtr<const UScriptStruct> Column)
		{
			if (Column.IsValid())
			{
				if (Column->IsChildOf(FMassFragment::StaticStruct()))
				{
					return Composition.Fragments.Contains(*Column);
				}
				else if (Column->IsChildOf(FMassTag::StaticStruct()))
				{
					return Composition.Tags.Contains(*Column);
				}
			}
			return false;
		};
		return Conditions.Verify(Callback);
	}
	return false;
}

void UTypedElementDatabase::RegisterTickGroup(
	FName GroupName, EQueryTickPhase Phase, FName BeforeGroup, FName AfterGroup, bool bRequiresMainThread)
{
	Queries.RegisterTickGroup(GroupName, Phase, BeforeGroup, AfterGroup, bRequiresMainThread);
}

void UTypedElementDatabase::UnregisterTickGroup(FName GroupName, EQueryTickPhase Phase)
{
	Queries.UnregisterTickGroup(GroupName, Phase);
}

TypedElementQueryHandle UTypedElementDatabase::RegisterQuery(FQueryDescription&& Query)
{
	return (ActiveEditorEntityManager && ActiveEditorPhaseManager)
		? Queries.RegisterQuery(MoveTemp(Query), *Environment, *ActiveEditorEntityManager, *ActiveEditorPhaseManager).Packed()
		: TypedElementInvalidQueryHandle;
}

void UTypedElementDatabase::UnregisterQuery(TypedElementQueryHandle Query)
{
	if (ActiveEditorPhaseManager)
	{
		const FTypedElementExtendedQueryStore::Handle StorageHandle(Query);
		Queries.UnregisterQuery(StorageHandle, *ActiveEditorPhaseManager);
	}
}

const ITypedElementDataStorageInterface::FQueryDescription& UTypedElementDatabase::GetQueryDescription(TypedElementQueryHandle Query) const
{
	const FTypedElementExtendedQueryStore::Handle StorageHandle(Query);
	return Queries.GetQueryDescription(StorageHandle);
}

FName UTypedElementDatabase::GetQueryTickGroupName(EQueryTickGroups Group) const
{
	switch (Group)
	{
		case EQueryTickGroups::Default:
			return NAME_None;
		case EQueryTickGroups::SyncExternalToDataStorage:
			return UE::Mass::ProcessorGroupNames::SyncWorldToMass;
		case EQueryTickGroups::SyncDataStorageToExternal:
			return UE::Mass::ProcessorGroupNames::UpdateWorldFromMass;
		case EQueryTickGroups::SyncWidgets:
			return TickGroupName_SyncWidget;
		default:
			checkf(false, TEXT("EQueryTickGroups value %i can't be translated to a group name by this Data Storage backend."), static_cast<int>(Group));
			return NAME_None;
	}
}

ITypedElementDataStorageInterface::FQueryResult UTypedElementDatabase::RunQuery(TypedElementQueryHandle Query)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEDS.RunQuery);

	if (ActiveEditorEntityManager)
	{
		const FTypedElementExtendedQueryStore::Handle StorageHandle(Query);
		return Queries.RunQuery(*ActiveEditorEntityManager, StorageHandle);
	}
	else
	{
		return FQueryResult();
	}
}

ITypedElementDataStorageInterface::FQueryResult UTypedElementDatabase::RunQuery(
	TypedElementQueryHandle Query, ITypedElementDataStorageInterface::DirectQueryCallbackRef Callback)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEDS.RunQuery);

	if (ActiveEditorEntityManager)
	{
		const FTypedElementExtendedQueryStore::Handle StorageHandle(Query);
		return Queries.RunQuery(*ActiveEditorEntityManager, *Environment, StorageHandle, Callback);
	}
	else
	{
		return FQueryResult();
	}
}

TypedElementDataStorage::RowHandle UTypedElementDatabase::FindIndexedRow(TypedElementDataStorage::IndexHash Index) const
{
	return Environment->GetIndexTable().FindIndexedRow(Index);
}

void UTypedElementDatabase::IndexRow(TypedElementDataStorage::IndexHash Index, TypedElementDataStorage::RowHandle Row)
{
	Environment->GetIndexTable().IndexRow(Index, Row);
}

void UTypedElementDatabase::ReindexRow(TypedElementDataStorage::IndexHash OriginalIndex, TypedElementDataStorage::IndexHash NewIndex, 
	TypedElementDataStorage::RowHandle RowHandle)
{
	Environment->GetIndexTable().ReindexRow(OriginalIndex, NewIndex, RowHandle);
}

void UTypedElementDatabase::RemoveIndex(TypedElementDataStorage::IndexHash Index)
{
	Environment->GetIndexTable().RemoveIndex(Index);
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

void UTypedElementDatabase::PreparePhase(EQueryTickPhase Phase, float DeltaTime)
{
	if (ActiveEditorEntityManager)
	{
		Environment->NextUpdateCycle();
		Queries.RunPhasePreambleQueries(*ActiveEditorEntityManager, *Environment, Phase, DeltaTime);
	}
}

void UTypedElementDatabase::FinalizePhase(EQueryTickPhase Phase, float DeltaTime)
{
	if (ActiveEditorEntityManager)
	{
		Queries.RunPhasePostambleQueries(*ActiveEditorEntityManager, *Environment, Phase, DeltaTime);
	}
}

void UTypedElementDatabase::Reset()
{
	if (UMassEntityEditorSubsystem* Mass = GEditor->GetEditorSubsystem<UMassEntityEditorSubsystem>())
	{
		Mass->GetOnPostTickDelegate().Remove(OnPostMassTickHandle);
		Mass->GetOnPreTickDelegate().Remove(OnPreMassTickHandle);
	}
	OnPostMassTickHandle.Reset();
	OnPreMassTickHandle.Reset();

	if (ActiveEditorPhaseManager)
	{
		Queries.Clear(*ActiveEditorPhaseManager.Get());
	}
	Tables.Reset();
	TableNameLookup.Reset();
	Environment.Reset();
	ActiveEditorPhaseManager.Reset();
	ActiveEditorEntityManager.Reset();
}

void UTypedElementDatabase::DebugPrintQueryCallbacks(FOutputDevice& Output)
{
	Queries.DebugPrintQueryCallbacks(Output);
}

void UTypedElementDatabase::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	UTypedElementDatabase* Database = static_cast<UTypedElementDatabase*>(InThis);

	for (auto& FactoryPair : Database->Factories)
	{
		Collector.AddReferencedObject(FactoryPair.Instance);
		Collector.AddReferencedObject(FactoryPair.Type);
	}
}
