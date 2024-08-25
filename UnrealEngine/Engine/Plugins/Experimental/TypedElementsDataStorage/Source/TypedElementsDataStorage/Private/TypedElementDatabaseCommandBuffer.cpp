// Copyright Epic Games, Inc. All Rights Reserved.

#include "TypedElementDatabaseCommandBuffer.h"

#include "MassEntityManager.h"
#include "TypedElementDatabaseEnvironment.h"

//
// Queue section
//

void FTypedElementDatabaseCommandBuffer::Queue_AddColumnCommand(
	CommandBuffer& Buffer,
	TypedElementDataStorage::RowHandle Row,
	const UScriptStruct* ColumnType)
{
	AddCommand(Buffer, Row, FAddColumnCommand
		{ 
			.ColumnType = ColumnType 
		});
}

void FTypedElementDatabaseCommandBuffer::Queue_AddColumnsCommand(
	CommandBuffer& Buffer,
	TypedElementDataStorage::RowHandle Row,
	FMassFragmentBitSet FragmentsToAdd,
	FMassTagBitSet TagsToAdd)
{
	AddCommand(Buffer, Row, FAddColumnsCommand
		{ 
			.FragmentsToAdd = MoveTemp(FragmentsToAdd), 
			.TagsToAdd = MoveTemp(TagsToAdd) 
		});
}

void FTypedElementDatabaseCommandBuffer::Queue_RemoveColumnCommand(
	CommandBuffer& Buffer,
	TypedElementDataStorage::RowHandle Row,
	const UScriptStruct* ColumnType)
{
	AddCommand(Buffer, Row, FRemoveColumnCommand
		{ 
			.ColumnType = ColumnType 
		});
}

void FTypedElementDatabaseCommandBuffer::Queue_RemoveColumnsCommand(
	CommandBuffer& Buffer,
	TypedElementDataStorage::RowHandle Row,
	FMassFragmentBitSet FragmentsToRemove,
	FMassTagBitSet TagsToRemove)
{
	AddCommand(Buffer, Row, FRemoveColumnsCommand
		{ 
			.FragmentsToRemove = MoveTemp(FragmentsToRemove), 
			.TagsToRemove = MoveTemp(TagsToRemove) 
		});
}

//
// Execute section
//

bool FTypedElementDatabaseCommandBuffer::Execute_IsRowAvailable(const FMassEntityManager& MassEntityManager, TypedElementDataStorage::RowHandle Row)
{
	return MassEntityManager.IsEntityValid(FMassEntityHandle::FromNumber(Row));
}

bool FTypedElementDatabaseCommandBuffer::Execute_HasRowBeenAssigned(const FMassEntityManager& MassEntityManager, TypedElementDataStorage::RowHandle Row)
{
	return MassEntityManager.IsEntityActive(FMassEntityHandle::FromNumber(Row));
}

void FTypedElementDatabaseCommandBuffer::Execute_AddColumnCommand(
	FMassEntityManager& MassEntityManager,
	TypedElementDataStorage::RowHandle Row,
	const UScriptStruct* ColumnType)
{
	if (ColumnType)
	{
		FMassEntityHandle Entity = FMassEntityHandle::FromNumber(Row);
		if (ColumnType->IsChildOf(FMassTag::StaticStruct()))
		{
			MassEntityManager.AddTagToEntity(Entity, ColumnType);
		}
		else if (ColumnType->IsChildOf(FMassFragment::StaticStruct()))
		{
			FStructView Column = MassEntityManager.GetFragmentDataStruct(Entity, ColumnType);
			// Only add if not already added to avoid asserts from Mass.
			if (!Column.IsValid())
			{
				MassEntityManager.AddFragmentToEntity(Entity, ColumnType);
			}
		}
	}
}

void FTypedElementDatabaseCommandBuffer::Execute_AddColumnsCommand(
	FMassEntityManager& MassEntityManager,
	TypedElementDataStorage::RowHandle Row,
	FMassFragmentBitSet FragmentsToAdd,
	FMassTagBitSet TagsToAdd)
{
	FMassArchetypeCompositionDescriptor AddComposition(
		MoveTemp(FragmentsToAdd), MoveTemp(TagsToAdd), FMassChunkFragmentBitSet(), FMassSharedFragmentBitSet());
	MassEntityManager.AddCompositionToEntity_GetDelta(FMassEntityHandle::FromNumber(Row), AddComposition);
}

void FTypedElementDatabaseCommandBuffer::Execute_RemoveColumnCommand(
	FMassEntityManager& MassEntityManager,
	TypedElementDataStorage::RowHandle Row,
	const UScriptStruct* ColumnType)
{
	if (ColumnType)
	{
		FMassEntityHandle Entity = FMassEntityHandle::FromNumber(Row);
		if (ColumnType->IsChildOf(FMassTag::StaticStruct()))
		{
			MassEntityManager.RemoveTagFromEntity(Entity, ColumnType);
		}
		else if (ColumnType->IsChildOf(FMassFragment::StaticStruct()))
		{
			MassEntityManager.RemoveFragmentFromEntity(Entity, ColumnType);
		}
	}
}

void FTypedElementDatabaseCommandBuffer::Execute_RemoveColumnsCommand(
	FMassEntityManager& MassEntityManager,
	TypedElementDataStorage::RowHandle Row,
	FMassFragmentBitSet FragmentsToRemove,
	FMassTagBitSet TagsToRemove)
{
	FMassArchetypeCompositionDescriptor RemoveComposition(
		MoveTemp(FragmentsToRemove), MoveTemp(TagsToRemove), FMassChunkFragmentBitSet(), FMassSharedFragmentBitSet());
	MassEntityManager.RemoveCompositionFromEntity(FMassEntityHandle::FromNumber(Row), RemoveComposition);
}

void FTypedElementDatabaseCommandBuffer::ProcessBuffer(CommandBuffer& Buffer, FTypedElementDatabaseEnvironment& Environment)
{
	using namespace TypedElementDataStorage;

	Buffer.StableSort(
		[](const FCommand& Lhs, const FCommand& Rhs)
		{
			return Lhs.Row < Rhs.Row;
		});

	struct FProcessor
	{
		FMassEntityManager& EntityManager;
		RowHandle Row;
		void operator()(const FAddColumnCommand& Command) { Execute_AddColumnCommand(EntityManager, Row, Command.ColumnType.Get()); }
		void operator()(const FAddColumnsCommand& Command) { Execute_AddColumnsCommand(EntityManager, Row, Command.FragmentsToAdd, Command.TagsToAdd); }
		void operator()(const FRemoveColumnCommand& Command) { Execute_RemoveColumnCommand(EntityManager, Row, Command.ColumnType.Get()); }
		void operator()(const FRemoveColumnsCommand& Command) { Execute_RemoveColumnsCommand(EntityManager, Row, Command.FragmentsToRemove, Command.TagsToRemove); }
	};
	
	FProcessor Processor{ .EntityManager = Environment.GetMassEntityManager() };
	for (FCommand& Command : Buffer)
	{
		if (Execute_HasRowBeenAssigned(Environment.GetMassEntityManager(), Command.Row))
		{
			Processor.Row = Command.Row;
			Visit(Processor, Command.Data);
		}
	}
}

// 
// misc
//

template<typename T>
void FTypedElementDatabaseCommandBuffer::AddCommand(CommandBuffer& Buffer, TypedElementDataStorage::RowHandle Row, T&& Args)
{
	FCommand Command;
	Command.Row = Row;
	Command.Data.Emplace<T>(Forward<T>(Args));
	Buffer.Add(MoveTemp(Command));
}
