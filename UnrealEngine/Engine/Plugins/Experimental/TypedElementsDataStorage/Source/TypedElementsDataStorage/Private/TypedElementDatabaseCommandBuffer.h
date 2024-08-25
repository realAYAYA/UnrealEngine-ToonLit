// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Common/TypedElementHandles.h"
#include "MassArchetypeTypes.h"
#include "Misc/TVariant.h"
#include "UObject/Class.h"
#include "UObject/WeakObjectPtrTemplates.h"

class FTypedElementDatabaseEnvironment;
struct FMassEntityManager;
struct FMassProcessingPhaseManager;

class FTypedElementDatabaseCommandBuffer final
{
private:
	struct FAddColumnCommand
	{
		TWeakObjectPtr<const UScriptStruct> ColumnType;
	};
	struct FAddColumnsCommand
	{
		FMassFragmentBitSet FragmentsToAdd;
		FMassTagBitSet TagsToAdd;
	};
	struct FRemoveColumnCommand
	{
		TWeakObjectPtr<const UScriptStruct> ColumnType;
	};
	struct FRemoveColumnsCommand
	{
		FMassFragmentBitSet FragmentsToRemove;
		FMassTagBitSet TagsToRemove;
	};
	using CommandData = TVariant<FAddColumnCommand, FAddColumnsCommand, FRemoveColumnCommand, FRemoveColumnsCommand>;

	struct FCommand
	{
		TypedElementDataStorage::RowHandle Row;
		CommandData Data;
	};

public:
	using CommandBuffer = TArray<FCommand>;

	/**
	 * @section Queue_* functions create a command that can be stored for later execution in the provided command buffer.
	 * Typically the Execute_* counter part will be called for execution.
	 */
	static void Queue_AddColumnCommand(
		CommandBuffer& Buffer,
		TypedElementDataStorage::RowHandle Row,
		const UScriptStruct* ColumnType);
	static void Queue_AddColumnsCommand(
		CommandBuffer& Buffer,
		TypedElementDataStorage::RowHandle Row,
		FMassFragmentBitSet FragmentsToAdd,
		FMassTagBitSet TagsToAdd);
	static void Queue_RemoveColumnCommand(
		CommandBuffer& Buffer,
		TypedElementDataStorage::RowHandle Row,
		const UScriptStruct* ColumnType);
	static void Queue_RemoveColumnsCommand(
		CommandBuffer& Buffer,
		TypedElementDataStorage::RowHandle Row,
		FMassFragmentBitSet FragmentsToRemove,
		FMassTagBitSet TagsToRemove);

	/**
	 * @section Execute_* functions directly execute a command with limited validation checks.
	 */
	
	static bool Execute_IsRowAvailable(const FMassEntityManager& MassEntityManager, TypedElementDataStorage::RowHandle Row);
	static bool Execute_HasRowBeenAssigned(const FMassEntityManager& MassEntityManager, TypedElementDataStorage::RowHandle Row);

	static void Execute_AddColumnCommand(
		FMassEntityManager& MassEntityManager, 
		TypedElementDataStorage::RowHandle Row,
		const UScriptStruct* ColumnType);
	static void Execute_AddColumnsCommand(
		FMassEntityManager& MassEntityManager, 
		TypedElementDataStorage::RowHandle Row,
		FMassFragmentBitSet FragmentsToAdd, 
		FMassTagBitSet TagsToAdd);
	static void Execute_RemoveColumnCommand(
		FMassEntityManager& MassEntityManager, 
		TypedElementDataStorage::RowHandle Row,
		const UScriptStruct* ColumnType);
	static void Execute_RemoveColumnsCommand(
		FMassEntityManager& MassEntityManager, 
		TypedElementDataStorage::RowHandle Row,
		FMassFragmentBitSet FragmentsToRemove, 
		FMassTagBitSet TagsToRemove);

	static void ProcessBuffer(CommandBuffer& Buffer, FTypedElementDatabaseEnvironment& Environment);

private:
	template<typename T>
	static void AddCommand(CommandBuffer& Buffer, TypedElementDataStorage::RowHandle Row, T&& Args);
};