// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/Common/TypedElementQueryConditions.h"

#include "UObject/Class.h"

namespace TypedElementDataStorage
{
	FQueryConditions::FQueryConditions(FColumnBase Column)
		: ColumnCount(1)
	{
		Columns[0] = Column.TypeInfo;
	}

	void FQueryConditions::AppendToString(FString& Output) const
	{
		if (TokenCount > 0)
		{
			Output += "{ ";
			uint8_t ColumnIndex = 0;
			if (Tokens[0] != Token::ScopeOpen)
			{
				ColumnIndex = 1;
				AppendName(Output, Columns[0]);
			}
			for (uint8_t TokenIndex = 0; TokenIndex < TokenCount; ++TokenIndex)
			{
				switch (Tokens[TokenIndex])
				{
				case Token::And:
					Output += " && ";
					if (!EntersScopeNext(TokenIndex))
					{
						AppendName(Output, Columns[ColumnIndex++]);
					}
					break;
				case Token::Or:
					Output += " || ";
					if (!EntersScopeNext(TokenIndex))
					{
						AppendName(Output, Columns[ColumnIndex++]);
					}
					break;
				case Token::ScopeOpen:
					Output += "( ";
					if (!EntersScopeNext(TokenIndex))
					{
						AppendName(Output, Columns[ColumnIndex++]);
					}
					break;
				case Token::ScopeClose:
					Output += " )";
					break;
				default:
					checkf(false, TEXT("Invalid query token"));
					break;
				}
			}
			Output += " }";
		}
	}

	bool FQueryConditions::Verify(TConstArrayView<FColumnBase> AvailableColumns) const
	{
		return VerifyBootstrap(
			[&AvailableColumns](uint8_t ColumnIndex, TWeakObjectPtr<const UScriptStruct> Column)
			{
				for (const FColumnBase& Target : AvailableColumns)
				{
					if (Target.TypeInfo == Column)
					{
						return true;
					}
				}
		return false;
			});
	}

	bool FQueryConditions::Verify(TArray<TWeakObjectPtr<const UScriptStruct>>& MatchedColumns, TConstArrayView<FColumnBase> AvailableColumns,
		bool AvailableColumnsAreSorted) const
	{
		static_assert(MaxColumnCount < 64, "Query conditions use a bit mask to locate matches. As a result MaxColumnCount can be larger than 64.");
		uint64 Matches = 0;
		bool Result = AvailableColumnsAreSorted
			? VerifyBootstrap(
				[&Matches, &AvailableColumns](uint8_t ColumnIndex, TWeakObjectPtr<const UScriptStruct> Column)
				{
					auto Projection = [](const FColumnBase& ColumnBase)
					{
						return ColumnBase.TypeInfo.Get();
					};
					if (Algo::BinarySearchBy(AvailableColumns, Column.Get(), Projection) != INDEX_NONE)
					{
						Matches |= (uint64)1 << ColumnIndex;
						return true;
					}
					return false;
				})
			: VerifyBootstrap(
				[&Matches, &AvailableColumns](uint8_t ColumnIndex, TWeakObjectPtr<const UScriptStruct> Column)
				{
					for (const FColumnBase& Target : AvailableColumns)
					{
						if (Target.TypeInfo == Column)
						{
							Matches |= (uint64)1 << ColumnIndex;
							return true;
						}
					}
					return false;
				});
				if (Result)
				{
					ConvertColumnBitToArray(MatchedColumns, Matches);
				}
				return Result;
	}

	bool FQueryConditions::Verify(TConstArrayView<TWeakObjectPtr<const UScriptStruct>> AvailableColumns, bool AvailableColumnsAreSorted) const
	{
		return AvailableColumnsAreSorted
			? VerifyBootstrap(
				[&AvailableColumns](uint8_t ColumnIndex, TWeakObjectPtr<const UScriptStruct> Column)
				{
					auto Projection = [](TWeakObjectPtr<const UScriptStruct> ColumnBase)
					{
						return ColumnBase.Get();
					};
					return Algo::BinarySearchBy(AvailableColumns, Column.Get(), Projection) != INDEX_NONE;
				})
			: VerifyBootstrap(
				[&AvailableColumns](uint8_t ColumnIndex, TWeakObjectPtr<const UScriptStruct> Column)
				{
					return AvailableColumns.Find(Column) != INDEX_NONE;
				});
	}

	bool FQueryConditions::Verify(TArray<TWeakObjectPtr<const UScriptStruct>>& MatchedColumns,
		TConstArrayView<TWeakObjectPtr<const UScriptStruct>> AvailableColumns, bool AvailableColumnsAreSorted) const
	{
		static_assert(MaxColumnCount < 64, "Query conditions use a bit mask to locate matches. As a result MaxColumnCount can be larger than 64.");
		uint64 Matches = 0;
		bool Result = AvailableColumnsAreSorted
			? VerifyBootstrap(
				[&Matches, &AvailableColumns](uint8_t ColumnIndex, TWeakObjectPtr<const UScriptStruct> Column)
				{
					auto Projection = [](TWeakObjectPtr<const UScriptStruct> ColumnBase)
					{
						return ColumnBase.Get();
					};
					if (Algo::BinarySearchBy(AvailableColumns, Column.Get(), Projection) != INDEX_NONE)
					{
						Matches |= (uint64)1 << ColumnIndex;
						return true;
					}
					return false;
				})
			: VerifyBootstrap(
				[&Matches, &AvailableColumns](uint8_t ColumnIndex, TWeakObjectPtr<const UScriptStruct> Column)
				{
					return AvailableColumns.Find(Column) != INDEX_NONE;
				});
				if (Result)
				{
					ConvertColumnBitToArray(MatchedColumns, Matches);
				}
				return Result;
	}

	bool FQueryConditions::Verify(TSet<TWeakObjectPtr<const UScriptStruct>> AvailableColumns) const
	{
		return VerifyBootstrap(
			[&AvailableColumns](uint8_t ColumnIndex, TWeakObjectPtr<const UScriptStruct> Column)
			{
				return AvailableColumns.Find(Column) != nullptr;
			});
	}

	bool FQueryConditions::Verify(ContainsCallback Callback) const
	{
		return VerifyBootstrap(Callback);
	}

	uint8_t FQueryConditions::MinimumColumnMatchRequired() const
	{
		if (TokenCount > 0)
		{
			uint8_t Front = 0;
			return MinimumColumnMatchRequiredRange(Front);
		}
		else if (ColumnCount == 1)
		{
			return 1;
		}
		else
		{
			return 0;
		}
	}

	TConstArrayView<TWeakObjectPtr<const UScriptStruct>> FQueryConditions::GetColumns() const
	{
		return TConstArrayView<TWeakObjectPtr<const UScriptStruct>>(Columns, ColumnCount);
	}

	bool FQueryConditions::IsEmpty() const
	{
		return ColumnCount == 0;
	}

	void FQueryConditions::AppendName(FString& Output, TWeakObjectPtr<const UScriptStruct> TypeInfo) const
	{
#if WITH_EDITORONLY_DATA
		static FName DisplayNameName(TEXT("DisplayName"));
		if (const FString* Name = TypeInfo->FindMetaData(DisplayNameName))
		{
			Output += *Name;
		}
#else
		Output += TEXT("<Unavailable>");
#endif
	}

	bool FQueryConditions::EntersScopeNext(uint8_t Index) const
	{
		return Index < (TokenCount - 1) && Tokens[Index + 1] == Token::ScopeOpen;
	}

	bool FQueryConditions::EntersScope(uint8_t Index) const
	{
		return Tokens[Index] == Token::ScopeOpen;
	}

	bool FQueryConditions::Contains(TWeakObjectPtr<const UScriptStruct> ColumnType, const TArray<FColumnBase>& AvailableColumns) const
	{
		for (const FColumnBase& Column : AvailableColumns)
		{
			if (ColumnType == Column.TypeInfo)
			{
				return true;
			}
		}
		return false;
	}

	bool FQueryConditions::VerifyBootstrap(ContainsCallback Contains) const
	{
		if (TokenCount > 0)
		{
			uint8_t TokenIndex = 0;
			uint8_t ColumnIndex = 0;
			return VerifyRange(TokenIndex, ColumnIndex, Forward<ContainsCallback>(Contains));
		}
		else if (ColumnCount == 1)
		{
			return Contains(0, Columns[0]);
		}
		else
		{
			return false;
		}
	}

	bool FQueryConditions::VerifyRange(uint8_t& TokenIndex, uint8_t& ColumnIndex, ContainsCallback Contains) const
	{
		auto Init = [&]()
		{
			if (EntersScope(TokenIndex))
			{
				return VerifyRange(++TokenIndex, ColumnIndex, Contains);
			}
			else
			{
				bool Result = Contains(ColumnIndex, Columns[ColumnIndex]);
				ColumnIndex++;
				return Result;
			}
		};
		bool Result = Init();

		while (TokenIndex < TokenCount)
		{
			Token T = Tokens[TokenIndex];
			++TokenIndex;
			switch (T)
			{
			case Token::And:
				if (EntersScope(TokenIndex))
				{
					Result = VerifyRange(++TokenIndex, ColumnIndex, Contains) && Result;
				}
				else
				{
					Result = Contains(ColumnIndex, Columns[ColumnIndex]) && Result;
					ColumnIndex++;
				}
				break;
			case Token::Or:
				if (EntersScope(TokenIndex))
				{
					Result = VerifyRange(++TokenIndex, ColumnIndex, Contains) || Result;
				}
				else
				{
					Result = Contains(ColumnIndex, Columns[ColumnIndex]) || Result;
					ColumnIndex++;
				}
				break;
			case Token::ScopeOpen:
				checkf(false, TEXT("The scope open in a query should be called during processing as it should be captured by an earlier statement."));
				break;
			case Token::ScopeClose:
				return Result;
			default:
				checkf(false, TEXT("Encountered an unknown query token."));
				break;
			}
		}
		return Result;
	}

	void FQueryConditions::ConvertColumnBitToArray(TArray<TWeakObjectPtr<const UScriptStruct>>& MatchedColumns, uint64 ColumnBits) const
	{
		uint64 Index = 0;
		while (ColumnBits)
		{
			if (ColumnBits & 1)
			{
				MatchedColumns.Add(Columns[Index]);
			}
			++Index;
			ColumnBits >>= 1;
		}
	}

	uint8_t FQueryConditions::MinimumColumnMatchRequiredRange(uint8_t& Front) const
	{
		uint8_t Result = EntersScope(Front) ? MinimumColumnMatchRequiredRange(++Front) : 1;
		for (; Front < TokenCount; ++Front)
		{
			Token T = Tokens[Front];
			switch (T)
			{
			case Token::And:
				if (EntersScopeNext(Front))
				{
					Front += 2;
					Result += MinimumColumnMatchRequiredRange(Front);
				}
				else
				{
					++Result;
				}
				break;
			case Token::Or:
				if (EntersScopeNext(Front))
				{
					Front += 2;
					uint8_t Lhs = MinimumColumnMatchRequiredRange(Front);
					Result = FMath::Min(Result, Lhs);
				}
				break;
			case Token::ScopeOpen:
				checkf(false, TEXT("The scope open in a query should be called during processing as it should be captured by an earlier statement."));
				break;
			case Token::ScopeClose:
				++Front;
				return Result;
			default:
				checkf(false, TEXT("Encountered an unknown query token."));
				break;
			}
		}
		return Result;
	}

	void FQueryConditions::AppendQuery(FQueryConditions& Target, const FQueryConditions& Source)
	{
		checkf(Target.ColumnCount + Source.ColumnCount < MaxColumnCount, TEXT("Too many columns in the query."));
		for (uint8_t Index = 0; Index < Source.ColumnCount; ++Index)
		{
			Target.Columns[Target.ColumnCount + Index] = Source.Columns[Index];
		}

		checkf(Target.TokenCount + Source.TokenCount < MaxTokenCount, TEXT("Too many operations in the query. Try simplifying your query."));
		for (uint8_t Index = 0; Index < Source.TokenCount; ++Index)
		{
			Target.Tokens[Target.TokenCount + Index] = Source.Tokens[Index];
		}

		Target.ColumnCount += Source.ColumnCount;
		Target.TokenCount += Source.TokenCount;
	}

	FQueryConditions operator&&(const FQueryConditions& Lhs, FColumnBase Rhs)
	{
		FQueryConditions Result = Lhs;
		Result.Columns[Result.ColumnCount++] = Rhs.TypeInfo;
		Result.Tokens[Result.TokenCount++] = FQueryConditions::Token::And;
		return Result;
	}

	FQueryConditions operator&&(const FQueryConditions& Lhs, const FQueryConditions& Rhs)
	{
		FQueryConditions Result;
		Result.Tokens[Result.TokenCount++] = FQueryConditions::Token::ScopeOpen;
		FQueryConditions::AppendQuery(Result, Lhs);
		Result.Tokens[Result.TokenCount++] = FQueryConditions::Token::ScopeClose;

		Result.Tokens[Result.TokenCount++] = FQueryConditions::Token::And;

		Result.Tokens[Result.TokenCount++] = FQueryConditions::Token::ScopeOpen;
		FQueryConditions::AppendQuery(Result, Rhs);
		Result.Tokens[Result.TokenCount++] = FQueryConditions::Token::ScopeClose;

		return Result;
	}

	FQueryConditions operator&&(FColumnBase Lhs, FColumnBase Rhs)
	{
		FQueryConditions Result(Lhs);
		Result.Columns[Result.ColumnCount++] = Rhs.TypeInfo;
		Result.Tokens[Result.TokenCount++] = FQueryConditions::Token::And;
		return Result;
	}

	FQueryConditions operator&&(FColumnBase Lhs, const FQueryConditions& Rhs)
	{
		FQueryConditions Result(Lhs);
		Result.Tokens[Result.TokenCount++] = FQueryConditions::Token::And;
		Result.Tokens[Result.TokenCount++] = FQueryConditions::Token::ScopeOpen;
		FQueryConditions::AppendQuery(Result, Rhs);
		Result.Tokens[Result.TokenCount++] = FQueryConditions::Token::ScopeClose;
		return Result;
	}

	FQueryConditions operator||(const FQueryConditions& Lhs, FColumnBase Rhs)
	{
		FQueryConditions Result = Lhs;
		Result.Columns[Result.ColumnCount++] = Rhs.TypeInfo;
		Result.Tokens[Result.TokenCount++] = FQueryConditions::Token::Or;
		return Result;
	}

	FQueryConditions operator||(const FQueryConditions& Lhs, const FQueryConditions& Rhs)
	{
		FQueryConditions Result;
		Result.Tokens[Result.TokenCount++] = FQueryConditions::Token::ScopeOpen;
		FQueryConditions::AppendQuery(Result, Lhs);
		Result.Tokens[Result.TokenCount++] = FQueryConditions::Token::ScopeClose;

		Result.Tokens[Result.TokenCount++] = FQueryConditions::Token::Or;

		Result.Tokens[Result.TokenCount++] = FQueryConditions::Token::ScopeOpen;
		FQueryConditions::AppendQuery(Result, Rhs);
		Result.Tokens[Result.TokenCount++] = FQueryConditions::Token::ScopeClose;

		return Result;
	}

	FQueryConditions operator||(FColumnBase Lhs, FColumnBase Rhs)
	{
		FQueryConditions Result(Lhs);
		Result.Columns[Result.ColumnCount++] = Rhs.TypeInfo;
		Result.Tokens[Result.TokenCount++] = FQueryConditions::Token::Or;
		return Result;
	}

	FQueryConditions operator||(FColumnBase Lhs, const FQueryConditions& Rhs)
	{
		FQueryConditions Result(Lhs);
		Result.Tokens[Result.TokenCount++] = FQueryConditions::Token::Or;
		Result.Tokens[Result.TokenCount++] = FQueryConditions::Token::ScopeOpen;
		FQueryConditions::AppendQuery(Result, Rhs);
		Result.Tokens[Result.TokenCount++] = FQueryConditions::Token::ScopeClose;
		return Result;
	}
} // namespace TypedElementDataStorage