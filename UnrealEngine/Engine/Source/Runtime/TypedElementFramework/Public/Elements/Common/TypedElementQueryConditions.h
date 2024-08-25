// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "UObject/WeakObjectPtrTemplates.h"

class UScriptStruct;

namespace TypedElementDataStorage
{
	struct FColumnBase
	{
		TWeakObjectPtr<const UScriptStruct> TypeInfo = nullptr;

	protected:
		FColumnBase() = default;
		constexpr explicit FColumnBase(TWeakObjectPtr<const UScriptStruct> ColumnTypeInfo) : TypeInfo(ColumnTypeInfo) {}
	};

	template<typename T = void>
	struct FColumn final : public FColumnBase
	{
		template <typename U = T> requires (!std::is_same_v<U, void>)
			constexpr FColumn() : FColumnBase(T::StaticStruct()) {}
		template <typename U = T> requires (std::is_same_v<U, void>)
			constexpr explicit FColumn(TWeakObjectPtr<const UScriptStruct> ColumnTypeInfo) : FColumnBase(ColumnTypeInfo) {};
	};

	/**
	 * Product of boolean combination of multiple columns. This can be used to verify if a collection of columns match
	 * the stored columns.
	 */
	class FQueryConditions final
	{
	public:
		using ContainsCallback = TFunctionRef<bool(uint8_t ColumnIndex, TWeakObjectPtr<const UScriptStruct> Column)>;

		TYPEDELEMENTFRAMEWORK_API friend FQueryConditions operator&&(const FQueryConditions& Lhs, FColumnBase Rhs);
		TYPEDELEMENTFRAMEWORK_API friend FQueryConditions operator&&(const FQueryConditions& Lhs, const FQueryConditions& Rhs);
		TYPEDELEMENTFRAMEWORK_API friend FQueryConditions operator&&(FColumnBase Lhs, FColumnBase Rhs);
		TYPEDELEMENTFRAMEWORK_API friend FQueryConditions operator&&(FColumnBase Lhs, const FQueryConditions& Rhs);

		TYPEDELEMENTFRAMEWORK_API friend FQueryConditions operator||(const FQueryConditions& Lhs, FColumnBase Rhs);
		TYPEDELEMENTFRAMEWORK_API friend FQueryConditions operator||(const FQueryConditions& Lhs, const FQueryConditions& Rhs);
		TYPEDELEMENTFRAMEWORK_API friend FQueryConditions operator||(FColumnBase Lhs, FColumnBase Rhs);
		TYPEDELEMENTFRAMEWORK_API friend FQueryConditions operator||(FColumnBase Lhs, const FQueryConditions& Rhs);

		FQueryConditions() = default;
		// Not marked as "explicit" to allow conversion from a column. This means that conditions with a single
		// argument can be written in the same way as ones that use combinations.
		TYPEDELEMENTFRAMEWORK_API FQueryConditions(FColumnBase Column);

		/** Convert the conditions into a string and append them to the provided string. */
		TYPEDELEMENTFRAMEWORK_API void AppendToString(FString& Output) const;

		/** Runs the provided list of columns through the conditions and returns true if a valid combination of columns is found. */
		TYPEDELEMENTFRAMEWORK_API bool Verify(TConstArrayView<FColumnBase> AvailableColumns) const;
		/**
		 * Runs the provided list of columns through the conditions and returns true if a valid combination of columns is found.
		 * This version returns a list of the columns that were used to match the condition.
		 */
		TYPEDELEMENTFRAMEWORK_API bool Verify(
			TArray<TWeakObjectPtr<const UScriptStruct>>& MatchedColumns,
			TConstArrayView<FColumnBase> AvailableColumns,
			bool AvailableColumnsAreSorted = false) const;
		/** Runs the provided list of columns through the conditions and returns true if a valid combination of columns is found. */
		TYPEDELEMENTFRAMEWORK_API bool Verify(TConstArrayView<TWeakObjectPtr<const UScriptStruct>> AvailableColumns,
			bool AvailableColumnsAreSorted = false) const;
		/**
		 * Runs the provided list of columns through the conditions and returns true if a valid combination of columns is found.
		 * This version returns a list of the columns that were used to match the condition.
		 */
		TYPEDELEMENTFRAMEWORK_API bool Verify(
			TArray<TWeakObjectPtr<const UScriptStruct>>& MatchedColumns,
			TConstArrayView<TWeakObjectPtr<const UScriptStruct>> AvailableColumns,
			bool AvailableColumnsAreSorted = false) const;
		/** Runs the provided list of columns through the conditions and returns true if a valid combination of columns is found. */
		TYPEDELEMENTFRAMEWORK_API bool Verify(TSet<TWeakObjectPtr<const UScriptStruct>> AvailableColumns) const;
		/** Runs through the list of query conditions and uses the callback to verify if a column is available. */
		TYPEDELEMENTFRAMEWORK_API bool Verify(ContainsCallback Callback) const;

		/** Returns the minimum number of columns needed for a successful match. */
		TYPEDELEMENTFRAMEWORK_API uint8_t MinimumColumnMatchRequired() const;
		/** Returns a list of all columns used. This can include duplicate columns. */
		TYPEDELEMENTFRAMEWORK_API TConstArrayView<TWeakObjectPtr<const UScriptStruct>> GetColumns() const;
		/** Whether or not there are any columns registered for operation.*/
		TYPEDELEMENTFRAMEWORK_API bool IsEmpty() const;

	private:
		void AppendName(FString& Output, TWeakObjectPtr<const UScriptStruct> TypeInfo) const;

		bool EntersScopeNext(uint8_t Index) const;
		bool EntersScope(uint8_t Index) const;
		bool Contains(TWeakObjectPtr<const UScriptStruct> ColumnType, const TArray<FColumnBase>& AvailableColumns) const;

		bool VerifyBootstrap(ContainsCallback Contains) const;
		bool VerifyRange(uint8_t& TokenIndex, uint8_t& ColumnIndex, ContainsCallback Contains) const;

		void ConvertColumnBitToArray(TArray<TWeakObjectPtr<const UScriptStruct>>& MatchedColumns, uint64 ColumnBits) const;

		uint8_t MinimumColumnMatchRequiredRange(uint8_t& Front) const;

		static void AppendQuery(FQueryConditions& Target, const FQueryConditions& Source);

		static constexpr SIZE_T MaxColumnCount = 32;
		static constexpr SIZE_T MaxTokenCount = 64;

		enum class Token : uint8_t
		{
			None,
			And,
			Or,
			ScopeOpen,
			ScopeClose
		};
		using ColumnArray = TWeakObjectPtr<const UScriptStruct>[MaxColumnCount];
		using TokenArray = Token[MaxTokenCount];

		ColumnArray Columns{ nullptr };
		TokenArray Tokens{ Token::None };
		uint8_t ColumnCount = 0;
		uint8_t TokenCount = 0;

	};

	TYPEDELEMENTFRAMEWORK_API FQueryConditions operator&&(const FQueryConditions& Lhs, FColumnBase Rhs);
	TYPEDELEMENTFRAMEWORK_API FQueryConditions operator&&(const FQueryConditions& Lhs, const FQueryConditions& Rhs);
	TYPEDELEMENTFRAMEWORK_API FQueryConditions operator&&(FColumnBase Lhs, FColumnBase Rhs);
	TYPEDELEMENTFRAMEWORK_API FQueryConditions operator&&(FColumnBase Lhs, const FQueryConditions& Rhs);

	TYPEDELEMENTFRAMEWORK_API FQueryConditions operator||(const FQueryConditions& Lhs, FColumnBase Rhs);
	TYPEDELEMENTFRAMEWORK_API FQueryConditions operator||(const FQueryConditions& Lhs, const FQueryConditions& Rhs);
	TYPEDELEMENTFRAMEWORK_API FQueryConditions operator||(FColumnBase Lhs, FColumnBase Rhs);
	TYPEDELEMENTFRAMEWORK_API FQueryConditions operator||(FColumnBase Lhs, const FQueryConditions& Rhs);
} // namespace TypedElementDataStorage