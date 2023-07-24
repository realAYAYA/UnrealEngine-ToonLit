// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/ContainersFwd.h"
#include "Delegates/Delegate.h"
#include "Elements/Framework/TypedElementColumnUtils.h"
#include "Math/NumericLimits.h"
#include "UObject/Interface.h"
#include "UObject/NameTypes.h"
#include "UObject/ObjectMacros.h"

#include "TypedElementDataStorageInterface.generated.h"

class UClass;
class UScriptStruct;

struct ColumnDataResult
{
	/** Pointer to the structure that holds the description of the returned data. */
	const UScriptStruct* Description;
	/** Pointer to the column data. The type is guaranteed to match type described in Description. */
	void* Data;
};

using TypedElementTableHandle = uint64;
static constexpr auto TypedElementInvalidTableHandle = TNumericLimits<TypedElementTableHandle>::Max();
using TypedElementRowHandle = uint64;
static constexpr auto TypedElementInvalidRowHandle = TNumericLimits<TypedElementRowHandle>::Max();
using TypedElementQueryHandle = uint64;
static constexpr auto TypedElementInvalidQueryHandle = TNumericLimits<TypedElementQueryHandle>::Max();

using FTypedElementOnDataStorageCreation = FSimpleMulticastDelegate;
using FTypedElementOnDataStorageDestruction = FSimpleMulticastDelegate;
using FTypedElementOnDataStorageUpdate = FSimpleMulticastDelegate;

using TypedElementDataStorageCreationCallbackRef = TFunctionRef<void(TypedElementRowHandle Row)>;

/**
 * Base for the data structures for a column.
 */
USTRUCT()
struct FTypedElementDataStorageColumn
{
	GENERATED_BODY()
};

/**
 * Base for the data structures that act as tags to rows. Tags should not have any data.
 */
USTRUCT()
struct FTypedElementDataStorageTag
{
	GENERATED_BODY()
};

UINTERFACE(MinimalAPI)
class UTypedElementDataStorageInterface : public UInterface
{
	GENERATED_BODY()
};

class TYPEDELEMENTFRAMEWORK_API ITypedElementDataStorageInterface
{
	GENERATED_BODY()

public:
	/**
	 * @section Table management
	 * 
	 * @description
	 * Tables are automatically created by taking an existing table and adding/removing columns. For
	 * performance its however better to create a table before adding objects to the table. This
	 * doesn't prevent those objects from having columns added/removed at a later time.
	 * To make debugging and profiling easier it's also recommended to give tables a name.
	 */

	/** Creates a new table for with the provided columns. Optionally a name can be given which is useful for retrieval later. */
	virtual TypedElementTableHandle RegisterTable(TConstArrayView<const UScriptStruct*> ColumnList) = 0;
	virtual TypedElementTableHandle RegisterTable(TConstArrayView<const UScriptStruct*> ColumnList, const FName Name) = 0;
	/** 
	 * Copies the column information from the provided table and creates a new table for with the provided columns. Optionally a 
	 * name can be given which is useful for retrieval later.
	 */
	virtual TypedElementTableHandle RegisterTable(TypedElementTableHandle SourceTable, 
		TConstArrayView<const UScriptStruct*> ColumnList) = 0;
	virtual TypedElementTableHandle RegisterTable(TypedElementTableHandle SourceTable, 
		TConstArrayView<const UScriptStruct*> ColumnList, const FName Name) = 0;

	/** Returns a previously created table with the provided name or TypedElementInvalidTableHandle if not found. */
	virtual TypedElementTableHandle FindTable(const FName Name) = 0;
	
	/**
	 * @section Row management
	 */

	/** Adds a new row to the provided table. */
	virtual TypedElementRowHandle AddRow(TypedElementTableHandle Table) = 0;
	virtual TypedElementRowHandle AddRow(FName TableName) = 0;
	
	/**
	 * Add multiple rows at once. For each new row the OnCreated callback is called. Callers are expected to use the callback to
	 * initialize the row if needed.
	 */
	virtual bool BatchAddRow(TypedElementTableHandle Table, int32 Count, TypedElementDataStorageCreationCallbackRef OnCreated) = 0;
	virtual bool BatchAddRow(FName TableName, int32 Count, TypedElementDataStorageCreationCallbackRef OnCreated) = 0;
	
	/** Removes a previously added row. */
	virtual void RemoveRow(TypedElementRowHandle Row) = 0;

	
	/**
	 * @section Column management
	 */

	/** Adds a tag to a row or does nothing if already added. */
	virtual void AddTag(TypedElementRowHandle Row, const UScriptStruct* TagType) = 0;
	virtual void AddTag(TypedElementRowHandle Row, FTopLevelAssetPath TagName) = 0;
	
	/** Adds a new column to a row. If the column already exists it will be returned instead. */
	virtual void* AddOrGetColumnData(TypedElementRowHandle Row, const UScriptStruct* ColumnType) = 0;
	virtual ColumnDataResult AddOrGetColumnData(TypedElementRowHandle Row, FTopLevelAssetPath ColumnName) = 0;
	
	/**
	 * Sets the data of a column using the provided argument bag. This is only meant for simple initialization for 
	 * fragments that use UPROPERTY to expose properties. For complex initialization or when the fragment type is known
	 * it's recommende to use call that work directly on the type for better performance and a wider range of configuration options.
	 */
	virtual ColumnDataResult AddOrGetColumnData(TypedElementRowHandle Row, FTopLevelAssetPath ColumnName,
		TConstArrayView<TypedElement::ColumnUtils::Argument> Arguments) = 0;
	
	/** Retrieves a pointer to the column of the given row or a nullptr if not found. */
	virtual void* GetColumnData(TypedElementRowHandle Row, const UScriptStruct* ColumnType) = 0;
	virtual ColumnDataResult GetColumnData(TypedElementRowHandle Row, FTopLevelAssetPath ColumnName) = 0;
	

	/**
	 * @section Query
	 * @description
	 * Queries can be constructed using the Query Builder. Note that the Query Builder allows for the creation of queries that
	 * are more complex than the backend may support. The backend is allowed to simplify the query, in which case the query
	 * can be used directly in the processor to do additional filtering. This will however impact performance and it's 
	 * therefore recommended to try to simplify the query first before relying on extended query filtering in a processor.
	 */
	struct FQueryDescription final
	{
		enum class EActionType : uint8
		{
			None,	/** Do nothing. */
			Select,	/** Selects a set of columns for further processing. */
			Count	/** Counts the number of entries that match the filter condition. */
		};

		using OperatorIndex = int32;
		enum class EOperatorType
		{
			SimpleAll,			// Unary: Type
			SimpleAny,			// Unary: Type
			SimpleNone,			// Unary: Type
			SimpleOptional,		// Unary: Type
			And,				// Binary: left operator index, right operator index
			Or,					// Binary: left operator index, right operator index
			Not,				// Unary: condition index
			Type				// Unary: Type
		};

		struct FBinaryOperator final
		{
			OperatorIndex Left;
			OperatorIndex Right;
		};
		
		union FOperator
		{
			FBinaryOperator Binary;
			OperatorIndex Unary;
			const UScriptStruct* Type;
		};

		enum class EAccessType { ReadOnly, ReadWrite };

		struct FAccessControlledStruct
		{
			FAccessControlledStruct() = default;
			inline FAccessControlledStruct(const UScriptStruct* Type, EAccessType Access) : Type(Type), Access(Access) {}

			const UScriptStruct* Type;
			EAccessType Access;
		};

		struct FAccessControlledClass
		{
			FAccessControlledClass() = default;
			inline FAccessControlledClass(const UClass* Type, EAccessType Access) : Type(Type), Access(Access) {}

			const UClass* Type;
			EAccessType Access;
		};

		TArray<FAccessControlledStruct, TInlineAllocator<16>> Selection;
		TArray<EOperatorType, TInlineAllocator<32>> ConditionTypes;
		TArray<FOperator, TInlineAllocator<32>> ConditionOperators;
		TArray<FAccessControlledClass, TInlineAllocator<4>> Dependencies;
		EActionType Action;
		/** If true, this query only has simple operations and is guaranteed to be executed fully and at optimal performance. */
		bool bSimpleQuery{ false };
	};
	virtual TypedElementQueryHandle RegisterQuery(const FQueryDescription& Query) = 0;
	virtual void UnregisterQuery(TypedElementQueryHandle Query) = 0;
	struct FQueryResult
	{
		enum class ECompletion
		{
			/** Query could be fully executed. */
			Fully,
			/** Only portions of the query were executed. This is caused by a problem that was encountered partway through processing. */
			Partially,
			/** 
			 * The backend doesn't support the particular query. This may be a limitation in how/where the query is run or because
			 * the query contains actions and/or operations that are not supported.
			 */
			Unsupported,
			/** The provided query is no longer available. */
			Unavailable
		};
		
		uint32 Count{ 0 }; /** The number of rows were processed. */
		ECompletion Completed{ ECompletion::Unavailable };
	};
	virtual FQueryResult RunQuery(TypedElementQueryHandle Query) = 0;
	
	/**
	 * @section Misc
	 */
	
	/**
	 * Called periodically when the storage is available. This provides an opportunity to do any repeated processing
	 * for the data storage.
	 */
	virtual FTypedElementOnDataStorageUpdate& OnUpdate() = 0;

	/**
	 * Whether or not the data storage is available. The data storage is available most of the time, but can be
	 * unavailable for a brief time between being destroyed and a new one created.
	 */
	virtual bool IsAvailable() const = 0;

	/** Returns a pointer to the registered external system if found, otherwise null. */
	virtual void* GetExternalSystemAddress(UClass* Target) = 0;

	

	/**
	 *
	 * The following are utility functions that are not part of the interface but are provided in order to make using the
	 * interface easier.
	 *
	 */
	
	
	/** Adds a tag to a row or does nothing if already added. */
	template<typename TagType>
	void AddTag(TypedElementRowHandle Row);
	
	/**
	 * Returns a pointer to the column of the given row or creates a new one if not found. Optionally arguments can be provided
	 * to update or initialize the column's data.
	 */
	template<typename ColumnType, typename... Args>
	ColumnType* AddOrGetColumn(TypedElementRowHandle Row, Args... Arguments);
	
	/** Returns a pointer to the column of the given row or a nullptr if the type couldn't be found or the row doesn't exist. */
	template<typename ColumnType>
	ColumnType* GetColumn(TypedElementRowHandle Row);

	/** Returns a pointer to the registered external system if found, otherwise null. */
	template<typename SystemType>
	SystemType* GetExternalSystem();
};

// Implementations
template<typename TagType>
void ITypedElementDataStorageInterface::AddTag(TypedElementRowHandle Row)
{
	AddTag(Row, TagType::StaticStruct());
}

template<typename ColumnType, typename... Args>
ColumnType* ITypedElementDataStorageInterface::AddOrGetColumn(TypedElementRowHandle Row, Args... Arguments)
{
	auto* Result = reinterpret_cast<ColumnType*>(AddOrGetColumnData(Row, ColumnType::StaticStruct()));
	if constexpr (sizeof...(Arguments) > 0)
	{
		if (Result)
		{
			new(Result) ColumnType{ Forward<Arguments>... };
		}
	}
	return Result;
}

template<typename ColumnType>
ColumnType* ITypedElementDataStorageInterface::GetColumn(TypedElementRowHandle Row)
{
	return reinterpret_cast<ColumnType*>(GetColumnData(Row, ColumnType::StaticStruct()));
}

template<typename SystemType>
SystemType* ITypedElementDataStorageInterface::GetExternalSystem()
{
	return reinterpret_cast<SystemType*>(GetExternalSystemAddress(SystemType::StaticClass()));
}