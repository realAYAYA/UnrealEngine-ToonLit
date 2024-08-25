// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/ContainerAllocationPolicies.h"
#include "Elements/Common/TypedElementHandles.h"
#include "Elements/Common/TypedElementQueryTypes.h"
#include "Elements/Interfaces/TypedElementQueryStorageInterfaces.h"
#include "Elements/Framework/TypedElementMetaData.h"
#include "Templates/Function.h"
#include "UObject/NameTypes.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"

class UScriptStruct;

namespace TypedElementDataStorage
{
	struct FQueryDescription;
	
	using QueryCallback = TFunction<void(const FQueryDescription&, IQueryContext&)>;
	using QueryCallbackRef = TFunctionRef<void(const FQueryDescription&, IQueryContext&)>;
	using DirectQueryCallback = TFunction<void(const FQueryDescription&, IDirectQueryContext&)>;
	using DirectQueryCallbackRef = TFunctionRef<void(const FQueryDescription&, IDirectQueryContext&)>;
	
	struct FQueryDescription final
	{
		static constexpr int32 NumInlineSelections = 8;
		static constexpr int32 NumInlineConditions = 8;
		static constexpr int32 NumInlineDependencies = 2;
		static constexpr int32 NumInlineGroups = 2;

		enum class EActionType : uint8
		{
			None,	//< Do nothing.
			Select,	//< Selects a set of columns for further processing.
			Count,	//< Counts the number of entries that match the filter condition.

			Max //< Value indicating the maximum value in this enum. Not to be used as an enum value.
		};

		using OperatorIndex = int32;
		enum class EOperatorType : uint16
		{
			SimpleAll,			//< Unary: Type
			SimpleAny,			//< Unary: Type
			SimpleNone,			//< Unary: Type
			SimpleOptional,		//< Unary: Type
			And,				//< Binary: left operator index, right operator index
			Or,					//< Binary: left operator index, right operator index
			Not,				//< Unary: condition index
			Type,				//< Unary: Type

			Max //< Value indicating the maximum value in this enum. Not to be used as an enum value.
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
			TWeakObjectPtr<const UScriptStruct> Type;
		};

		struct FCallbackData
		{
			TArray<FName, TInlineAllocator<NumInlineGroups>> BeforeGroups;
			TArray<FName, TInlineAllocator<NumInlineGroups>> AfterGroups;
			QueryCallback Function;
			FName Name;
			FName Group;
			const UScriptStruct* MonitoredType{ nullptr };
			EQueryCallbackType Type{ EQueryCallbackType::None };
			EQueryTickPhase Phase;
			bool bForceToGameThread{ false };
		};

		FCallbackData Callback;

		// The list of arrays below are required to remain in the same order as they're added as the function binding expects certain entries
		// to be in a specific location.

		TArray<TWeakObjectPtr<const UScriptStruct>, TInlineAllocator<NumInlineSelections>> SelectionTypes;
		TArray<EQueryAccessType, TInlineAllocator<NumInlineSelections>> SelectionAccessTypes;
		TArray<FColumnMetaData, TInlineAllocator<NumInlineSelections>> SelectionMetaData;

		TArray<EOperatorType, TInlineAllocator<NumInlineConditions>> ConditionTypes;
		TArray<FOperator, TInlineAllocator<NumInlineConditions>> ConditionOperators;

		TArray<TWeakObjectPtr<const UClass>, TInlineAllocator<NumInlineDependencies>> DependencyTypes;
		TArray<EQueryDependencyFlags, TInlineAllocator<NumInlineDependencies>> DependencyFlags;
		/** Cached instances of the dependencies. This will always match the count of the other Dependency*Types, but may contain null pointers. */
		TArray<TWeakObjectPtr<UObject>, TInlineAllocator<NumInlineDependencies>> CachedDependencies;
		TArray<QueryHandle> Subqueries;
		FMetaData MetaData;

		EActionType Action;
		/** If true, this query only has simple operations and is guaranteed to be executed fully and at optimal performance. */
		bool bSimpleQuery{ false };
	};
} // namespace TypedElementDataStorage
