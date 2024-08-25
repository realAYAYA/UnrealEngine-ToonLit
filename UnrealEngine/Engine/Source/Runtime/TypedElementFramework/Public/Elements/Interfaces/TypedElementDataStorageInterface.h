// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/ContainersFwd.h"
#include "Delegates/Delegate.h"
#include "Elements/Common/TypedElementHandles.h"
#include "Elements/Common/TypedElementQueryConditions.h"
#include "Elements/Common/TypedElementQueryDescription.h"
#include "Elements/Common/TypedElementQueryTypes.h"
#include "Elements/Framework/TypedElementColumnUtils.h"
#include "Elements/Interfaces/TypedElementQueryStorageInterfaces.h"
#include "Math/NumericLimits.h"
#include "Misc/EnumClassFlags.h"
#include "Templates/Function.h"
#include "UObject/Interface.h"
#include "UObject/NameTypes.h"
#include "UObject/ObjectMacros.h"

#include "TypedElementDataStorageInterface.generated.h"

class UClass;
class USubsystem;
class UScriptStruct;
class UTypedElementDataStorageFactory;

struct ColumnDataResult
{
	/** Pointer to the structure that holds the description of the returned data. */
	const UScriptStruct* Description;
	/** Pointer to the column data. The type is guaranteed to match type described in Description. */
	void* Data;
};

using TypedElementTableHandle = TypedElementDataStorage::TableHandle;
static constexpr auto TypedElementInvalidTableHandle = TypedElementDataStorage::InvalidTableHandle;
using TypedElementRowHandle = TypedElementDataStorage::RowHandle;
static constexpr auto TypedElementInvalidRowHandle = TypedElementDataStorage::InvalidRowHandle;
using TypedElementQueryHandle = TypedElementDataStorage::QueryHandle;
static constexpr auto TypedElementInvalidQueryHandle = TypedElementDataStorage::InvalidQueryHandle;

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

/**
 * Convenience structure that can be used to pass a list of columns to functions that don't
 * have an dedicate templated version that takes a column list directly, for instance when
 * multiple column lists are used. Note that the returned array view is only available while
 * this object is constructed, so care must be taken with functions that return a const array view.
 */
template<typename... Columns>
struct TTypedElementColumnTypeList
{
	const UScriptStruct* ColumnTypes[sizeof...(Columns)] = { Columns::StaticStruct()... };
	
	operator TConstArrayView<const UScriptStruct*>() const { return ColumnTypes; }
};

class ITypedElementDataStorageInterface
{
	GENERATED_BODY()

public:
	/**
	 * @section Factories
	 *
	 * @description
	 * Factories are an automated way to register tables, queries and other information with TEDS.
	 */

	/** Finds a factory instance registered with TEDS */
	virtual const UTypedElementDataStorageFactory* FindFactory(const UClass* FactoryType) const = 0;

	/** Convenience function for FindFactory */
	template<typename FactoryT>
	const FactoryT* FindFactory() const;
	
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
	virtual TypedElementTableHandle RegisterTable(TConstArrayView<const UScriptStruct*> ColumnList, const FName Name) = 0;
	/** 
	 * Copies the column information from the provided table and creates a new table for with the provided columns. Optionally a 
	 * name can be given which is useful for retrieval later.
	 */
	virtual TypedElementTableHandle RegisterTable(TypedElementTableHandle SourceTable, 
		TConstArrayView<const UScriptStruct*> ColumnList, const FName Name) = 0;

	/** Returns a previously created table with the provided name or TypedElementInvalidTableHandle if not found. */
	virtual TypedElementTableHandle FindTable(const FName Name) = 0;
	
	/**
	 * @section Row management
	 */

	/** 
	 * Reserves a row to be assigned to a table at a later point. If the row is no longer needed before it's been assigned
	 * to a table, it should still be released with RemoveRow.
	 */
	virtual TypedElementRowHandle ReserveRow() = 0;
	/** Adds a new row to the provided table. */
	virtual TypedElementRowHandle AddRow(TypedElementTableHandle Table) = 0;
	/** Adds a new row to the provided table using a previously reserved row.. */
	virtual bool AddRow(TypedElementRowHandle ReservedRow, TypedElementTableHandle Table) = 0;
	
	/**
	 * Add multiple rows at once. For each new row the OnCreated callback is called. Callers are expected to use the callback to
	 * initialize the row if needed.
	 */
	virtual bool BatchAddRow(TypedElementTableHandle Table, int32 Count, TypedElementDataStorageCreationCallbackRef OnCreated) = 0;
	/**
	 * Add multiple rows at once. For each new row the OnCreated callback is called. Callers are expected to use the callback to
	 * initialize the row if needed. This version uses a set of previously reserved rows. Any row that can't be used will be 
	 * released.
	 */
	virtual bool BatchAddRow(TypedElementTableHandle Table, TConstArrayView<TypedElementRowHandle> ReservedHandles,
		TypedElementDataStorageCreationCallbackRef OnCreated) = 0;

	/** Removes a previously reserved or added row. If the row handle is invalid or already removed, nothing happens */
	virtual void RemoveRow(TypedElementRowHandle Row) = 0;

	/** Checks whether or not a row is in use. This is true even if the row has only been reserved. */
	virtual bool IsRowAvailable(TypedElementRowHandle Row) const = 0;
	/** Checks whether or not a row has been reserved but not yet assigned to a table. */
	virtual bool HasRowBeenAssigned(TypedElementRowHandle Row) const = 0;

	
	/**
	 * @section Column management
	 */

	/** Adds a column to a row or does nothing if already added. */
	virtual bool AddColumn(TypedElementRowHandle Row, const UScriptStruct* ColumnType) = 0;
	virtual bool AddColumn(TypedElementRowHandle Row, FTopLevelAssetPath ColumnName) = 0;
	/**
	 * Adds multiple columns from a row. This is typically more efficient than adding columns one 
	 * at a time.
	 */
	virtual bool AddColumns(TypedElementRowHandle Row, TConstArrayView<const UScriptStruct*> Columns) = 0;

	/** Removes a column from a row or does nothing if already removed. */
	virtual void RemoveColumn(TypedElementRowHandle Row, const UScriptStruct* ColumnType) = 0;
	virtual void RemoveColumn(TypedElementRowHandle Row, FTopLevelAssetPath ColumnName) = 0;
	/**
	 * Removes multiple columns from a row. This is typically more efficient than adding columns one
	 * at a time.
	 */
	virtual void RemoveColumns(TypedElementRowHandle Row, TConstArrayView<const UScriptStruct*> Columns) = 0;
	/** 
	 * Adds and removes the provided column types from the provided row. This is typically more efficient 
	 * than individually adding and removing columns as well as being faster than adding and removing
	 * columns separately.
	 */
	virtual bool AddRemoveColumns(TypedElementRowHandle Row, TConstArrayView<const UScriptStruct*> ColumnsToAdd,
		TConstArrayView<const UScriptStruct*> ColumnsToRemove) = 0;

	/** Adds and removes the provided column types from the provided list of rows. */
	virtual bool BatchAddRemoveColumns(
		TConstArrayView<TypedElementRowHandle> Rows,
		TConstArrayView<const UScriptStruct*> ColumnsToAdd,
		TConstArrayView<const UScriptStruct*> ColumnsToRemove) = 0;

	/**
	 * Adds a new column to a row. If the column already exists it will be returned instead. If the colum couldn't
	 * be added or the column type points to a tag an nullptr will be returned.
	 */
	virtual void* AddOrGetColumnData(TypedElementRowHandle Row, const UScriptStruct* ColumnType) = 0;
	virtual ColumnDataResult AddOrGetColumnData(TypedElementRowHandle Row, FTopLevelAssetPath ColumnName) = 0;
	/**
	 * Sets the data of a column using the provided argument bag. This is only meant for simple initialization for 
	 * fragments that use UPROPERTY to expose properties. For complex initialization or when the fragment type is known
	 * it's recommended to use calls that work directly on the type for better performance and a wider range of configuration options.
	 * If the column couldn't be created or the column name points to a tag, then the result will contain only nullptrs.
	 */
	virtual ColumnDataResult AddOrGetColumnData(TypedElementRowHandle Row, FTopLevelAssetPath ColumnName,
		TConstArrayView<TypedElement::ColumnUtils::Argument> Arguments) = 0;
	
	/** Retrieves a pointer to the column of the given row or a nullptr if not found or if the column type is a tag. */
	virtual void* GetColumnData(TypedElementRowHandle Row, const UScriptStruct* ColumnType) = 0;
	virtual const void* GetColumnData(TypedElementRowHandle Row, const UScriptStruct* ColumnType) const = 0;
	virtual ColumnDataResult GetColumnData(TypedElementRowHandle Row, FTopLevelAssetPath ColumnName) = 0;

	/** Determines if the provided row contains the collection of columns and tags. */
	virtual bool HasColumns(TypedElementRowHandle Row, TConstArrayView<const UScriptStruct*> ColumnTypes) const = 0;
	virtual bool HasColumns(TypedElementRowHandle Row, TConstArrayView<TWeakObjectPtr<const UScriptStruct>> ColumnTypes) const = 0;
	/** Determines if the columns in the row match the query conditions. */
	virtual bool MatchesColumns(TypedElementDataStorage::RowHandle Row, const TypedElementDataStorage::FQueryConditions& Conditions) const = 0;
	

	/**
	 * @section Query
	 * @description
	 * Queries can be constructed using the Query Builder. Note that the Query Builder allows for the creation of queries that
	 * are more complex than the back-end may support. The back-end is allowed to simplify the query, in which case the query
	 * can be used directly in the processor to do additional filtering. This will however impact performance and it's 
	 * therefore recommended to try to simplify the query first before relying on extended query filtering in a processor.
	 */

	using EQueryTickPhase = TypedElementDataStorage::EQueryTickPhase;
	using EQueryTickGroups = TypedElementDataStorage::EQueryTickGroups;
	using EQueryCallbackType = TypedElementDataStorage::EQueryCallbackType;
	using EQueryAccessType = TypedElementDataStorage::EQueryAccessType;
	using EQueryDependencyFlags = TypedElementDataStorage::EQueryDependencyFlags;
	using FQueryResult = TypedElementDataStorage::FQueryResult;

	using IQueryContext = TypedElementDataStorage::IQueryContext;
	using IDirectQueryContext = TypedElementDataStorage::IDirectQueryContext;
	using ISubqueryContext = TypedElementDataStorage::ISubqueryContext;

	using FQueryDescription = TypedElementDataStorage::FQueryDescription;
	using QueryCallback = TypedElementDataStorage::QueryCallback;
	using QueryCallbackRef = TypedElementDataStorage::QueryCallbackRef;
	using DirectQueryCallback = TypedElementDataStorage::DirectQueryCallback;
	using DirectQueryCallbackRef = TypedElementDataStorage::DirectQueryCallbackRef;
	using SubqueryCallback = TypedElementDataStorage::SubqueryCallback;
	using SubqueryCallbackRef = TypedElementDataStorage::SubqueryCallbackRef;

	/** 
	 * Registers a query with the data storage. The description is processed into an internal format and may be changed. If no valid
	 * could be created an invalid query handle will be returned. It's recommended to use the Query Builder for a more convenient
	 * and safer construction of a query.
	 */
	virtual TypedElementQueryHandle RegisterQuery(FQueryDescription&& Query) = 0;
	/** Removes a previous registered. If the query handle is invalid or the query has already been deleted nothing will happen. */
	virtual void UnregisterQuery(TypedElementQueryHandle Query) = 0;
	/** Returns the description of a previously registered query. If the query no longer exists an empty description will be returned. */
	virtual const FQueryDescription& GetQueryDescription(TypedElementQueryHandle Query) const = 0;
	/**
	 * Tick groups for queries can be given any name and the Data Storage will figure out the order of execution based on found
	 * dependencies. However keeping processors within the same query group can help promote better performance through parallelization.
	 * Therefore a collection of common tick group names is provided to help create consistent tick group names.
	 */
	virtual FName GetQueryTickGroupName(EQueryTickGroups Group) const = 0;
	/** Directly runs a query. If the query handle is invalid or has been deleted nothing will happen. */
	virtual FQueryResult RunQuery(TypedElementQueryHandle Query) = 0;
	/**
	 * Directly runs a query. The callback will be called for batches of matching rows. During a single call to RunQuery the callback
	 * may be called multiple times. If the query handle is invalid or has been deleted nothing happens and the callback won't be called
	 */
	virtual FQueryResult RunQuery(TypedElementQueryHandle Query, DirectQueryCallbackRef Callback) = 0;
	
	/**
	 * @section Indexing
	 * @description
	 * In order for rows to reference each other it's often needed to find a row based on the content of one of its columns. This can be
	 * done by linearly searching through columns, though this comes at a performance cost. As an alternative the data storage allows
	 * one or more indexes to be created for a row. An index is a 64-bit value and typically uses a hash value of an identifying value.
	 */

	/** Retrieves the row for an indexed object. Returns an invalid row handle if the hash wasn't found. */
	virtual TypedElementDataStorage::RowHandle FindIndexedRow(TypedElementDataStorage::IndexHash Index) const = 0;
	/** 
	 * Registers a row under the index hash. The same row can be registered multiple, but an index hash can only be associated 
	 * with a single row.
	 */
	virtual void IndexRow(TypedElementDataStorage::IndexHash Index, TypedElementDataStorage::RowHandle Row) = 0;
	/** Updates the index of a row to a new value. Effectively this is the same as removing an index and adding a new one. */
	virtual void ReindexRow(TypedElementDataStorage::IndexHash OriginalIndex, TypedElementDataStorage::IndexHash NewIndex, TypedElementDataStorage::RowHandle Row) = 0;
	/** Removes a previously registered index hash from the index lookup table or does nothing if the hash no longer exists. */
	virtual void RemoveIndex(TypedElementDataStorage::IndexHash Index) = 0;

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
	
	
	/** Adds a column to a row or does nothing if already added. */
	template<typename Column>
	bool AddColumn(TypedElementRowHandle Row);

	/** Removes a tag from a row or does nothing if already removed. */
	template<typename Column>
	void RemoveColumn(TypedElementRowHandle Row);
	
	/**
	 * Adds multiple columns from a row. This is typically more efficient than adding columns one
	 * at a time.
	 */
	template<typename... Columns>
	void AddColumns(TypedElementRowHandle Row);

	/**
	 * Removes multiple columns from a row. This is typically more efficient than adding columns one
	 * at a time.
	 */
	template<typename... Columns>
	void RemoveColumns(TypedElementRowHandle Row);

	/**
	 * Returns a pointer to the column of the given row or creates a new one if not found. Optionally arguments can be provided
	 * to update or initialize the column's data.
	 */
	template<typename ColumnType, typename... Args>
	ColumnType* AddOrGetColumn(TypedElementRowHandle Row, Args... Arguments);

	/**
	 * Returns a pointer to the column of the given row or creates a new one if not found.
	 * Enables type deduction of ColumnType from Column argument.
	 * 
	 * For example, FTransformColumn added and deduced from second argument:
	 * StorageInterface->AddOrGetColumn(Row, FTransformColumn{.Transform = Transform});
	 */
	template<typename ColumnType>
	ColumnType* AddOrGetColumn(TypedElementRowHandle Row, ColumnType&& Column);
	
	/** Returns a pointer to the column of the given row or a nullptr if the type couldn't be found or the row doesn't exist. */
	template<typename ColumnType>
	ColumnType* GetColumn(TypedElementRowHandle Row);

	/** Returns a pointer to the column of the given row or a nullptr if the type couldn't be found or the row doesn't exist. */
	template<typename ColumnType>
	const ColumnType* GetColumn(TypedElementRowHandle Row) const;

	template<typename... ColumnTypes>
	bool HasColumns(TypedElementRowHandle Row) const;

	/** Returns a pointer to the registered external system if found, otherwise null. */
	template<typename SystemType>
	SystemType* GetExternalSystem();
};



// Implementations

template <typename FactoryT>
const FactoryT* ITypedElementDataStorageInterface::FindFactory() const
{
	return static_cast<const FactoryT*>(FindFactory(FactoryT::StaticClass()));
}

template<typename Column>
bool ITypedElementDataStorageInterface::AddColumn(TypedElementRowHandle Row)
{
	return AddColumn(Row, Column::StaticStruct());
}

template<typename Column>
void ITypedElementDataStorageInterface::RemoveColumn(TypedElementRowHandle Row)
{
	RemoveColumn(Row, Column::StaticStruct());
}

template<typename... Columns>
void ITypedElementDataStorageInterface::AddColumns(TypedElementRowHandle Row)
{
	AddColumns(Row, { Columns::StaticStruct()...});
}

template<typename... Columns>
void ITypedElementDataStorageInterface::RemoveColumns(TypedElementRowHandle Row)
{
	RemoveColumns(Row, { Columns::StaticStruct()...});
}

template<typename ColumnType, typename FirstArg, typename... NextArgs>
struct TConstructFromSlicedObjectDisabler
{
	// Fail assertion if the only argument is derived from FTypedElementDataStorageColumn and not the same as ColumnType
	// This gives a good indication that object slicing is probably happening.
	static_assert(!(
		sizeof...(NextArgs) == 0 &&                                    // There is only one argument to the callback and ...
		std::is_base_of_v<FTypedElementDataStorageColumn, FirstArg> && // ... the type of the argument derives from FTypedElementDataStorageColumn
		!std::is_same_v<ColumnType, FirstArg>),                        // ... but it isn't the same type as the column we are trying to add
		"Probable object slicing detected. The invoked constructor of ColumnType uses a different column type as it's first argument. "
		"This is detected as a likely object slice and disabled. "
		"If this is what you intended, then explicitly slice the object using the constructor before passing it as an argument");
};

template<typename ColumnType, typename... Args>
ColumnType* ITypedElementDataStorageInterface::AddOrGetColumn(TypedElementRowHandle Row, Args... Arguments)
{
	ColumnType* Result = static_cast<ColumnType*>(AddOrGetColumnData(Row, ColumnType::StaticStruct()));
	if constexpr (sizeof...(Arguments) > 0)
	{
		[[maybe_unused]] TConstructFromSlicedObjectDisabler<ColumnType, Args...> SliceDisabler;
		
		if (Result)
		{
			new(Result) ColumnType{ std::forward<Args>(Arguments)... };
		}
	}
	return Result;
}

template<typename ColumnType>
ColumnType* ITypedElementDataStorageInterface::AddOrGetColumn(TypedElementRowHandle Row, ColumnType&& Column)
{
	ColumnType* Result = static_cast<ColumnType*>(AddOrGetColumnData(Row, ColumnType::StaticStruct()));

	if (Result)
	{
		new(Result) ColumnType(MoveTemp(Column));
	}
	return Result;
}

template<typename ColumnType>
ColumnType* ITypedElementDataStorageInterface::GetColumn(TypedElementRowHandle Row)
{
	return reinterpret_cast<ColumnType*>(GetColumnData(Row, ColumnType::StaticStruct()));
}

template<typename ColumnType>
const ColumnType* ITypedElementDataStorageInterface::GetColumn(TypedElementRowHandle Row) const
{
	return reinterpret_cast<const ColumnType*>(GetColumnData(Row, ColumnType::StaticStruct()));
}

template<typename... ColumnType>
bool ITypedElementDataStorageInterface::HasColumns(TypedElementRowHandle Row) const
{
	return HasColumns(Row, TConstArrayView<const UScriptStruct*>({ ColumnType::StaticStruct()... }));
}

template<typename SystemType>
SystemType* ITypedElementDataStorageInterface::GetExternalSystem()
{
	return reinterpret_cast<SystemType*>(GetExternalSystemAddress(SystemType::StaticClass()));
}
