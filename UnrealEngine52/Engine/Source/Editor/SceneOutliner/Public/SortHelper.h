// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/Views/SHeaderRow.h"
#include "ISceneOutlinerTreeItem.h"

namespace SceneOutliner
{
	/** Wrapper type that will sort FString's using a more natural comparison (correctly sorts numbers and ignores underscores) */
	struct FNumericStringWrapper
	{
		FString String;

		FNumericStringWrapper()
		{}
		FNumericStringWrapper(FString&& InString)
			: String(InString)
		{}
		FNumericStringWrapper(FNumericStringWrapper&& Other)
			: String(MoveTemp(Other.String))
		{}
		FNumericStringWrapper& operator=(FNumericStringWrapper&& Other)
		{
			String = MoveTemp(Other.String);
			return *this;
		}
		FORCEINLINE bool operator<(const FNumericStringWrapper& Other) const
		{
			return CompareNumeric(String, Other.String) < 0;
		}
		FORCEINLINE bool operator>(const FNumericStringWrapper& Other) const
		{
			return CompareNumeric(String, Other.String) > 0;
		}
		FORCEINLINE bool operator==(const FNumericStringWrapper& Other) const
		{
			return String == Other.String;
		}

	private:
		static bool BothAscii(TCHAR C1, TCHAR C2)
		{
			return (((uint32)C1 | (uint32)C2) & 0xffffff80) == 0;
		}

		static bool BothNumbers(TCHAR C1, TCHAR C2)
		{
			return C1 >= '0' && C1 <= '9' && C2 >= '0' && C2 <= '9';
		}

		static int32 CompareNumeric(const FString& A, const FString& B)
		{
			const TCHAR* String1 = *A;
			const TCHAR* String2 = *B;

			while (true)
			{
				TCHAR C1 = *String1;
				TCHAR C2 = *String2;

				// Ignore underscores
				if (TChar<TCHAR>::IsUnderscore(C1))
				{
					String1++;
					continue;
				}

				// Ignore underscores
				if (TChar<TCHAR>::IsUnderscore(C2))
				{
					String2++;
					continue;
				}

				// Sort numerically when numbers are found 
				if (BothNumbers(C1, C2))
				{
					int64 IntA = 0;
					do
					{
						IntA *= 10;
						IntA += TChar<TCHAR>::ConvertCharDigitToInt(C1);

						String1++;
						C1 = *String1;
					} while (C1 >= '0' && C1 <= '9');

					int64 IntB = 0;
					do
					{
						IntB *= 10;
						IntB += TChar<TCHAR>::ConvertCharDigitToInt(C2);

						String2++;
						C2 = *String2;
					} while (C2 >= '0' && C2 <= '9');

					if (IntA != IntB)
					{
						return IntA < IntB ? -1 : 1;
					}

					continue;
				}
				else if (C1 == C2)
				{
					// Reached the end of the string
					if (C1 == 0)
					{
						// Strings compared equal, return shortest first
						return A.Len() == B.Len() ? 0 : A.Len() < B.Len() ? -1 : 1;
					}
				}
				else if (BothAscii(C1, C2))
				{
					if (int32 Diff = TChar<TCHAR>::ToUnsigned(TChar<TCHAR>::ToLower(C1)) - TChar<TCHAR>::ToUnsigned(TChar<TCHAR>::ToLower(C2)))
					{
						return Diff;
					}
				}
				else
				{
					return TChar<TCHAR>::ToUnsigned(C1) - TChar<TCHAR>::ToUnsigned(C2);
				}

				String1++;
				String2++;
			}
		}

		FNumericStringWrapper(const FNumericStringWrapper&);
		FNumericStringWrapper& operator=(const FNumericStringWrapper&);
	};
}


/**
	* Templated helper to alleviate performance problems with sorting based on complex predicates.
	* Example Usage:
	* 		FSceneOutlinerSortHelper<FString>().Primary([](const ISceneOutlinerTreeItem& Item){ return Item->GetString(); }).SortItems(Array);
	*
	* Or:
	* 		FSceneOutlinerSortHelper<FString, FString>()
	*			.Primary(FGetPrimaryString())
	*			.Secondary(FGetSecondaryString())
	*			.SortItems(Array);
	*/
template<typename PrimaryKeyType, typename SecondaryKeyType = int32>
struct FSceneOutlinerSortHelper
{
	typedef TFunction<PrimaryKeyType(const ISceneOutlinerTreeItem&)> FPrimaryFunction;
	typedef TFunction<SecondaryKeyType(const ISceneOutlinerTreeItem&)> FSecondaryFunction;

	FSceneOutlinerSortHelper()
		: PrimarySortMode(EColumnSortMode::None), SecondarySortMode(EColumnSortMode::None)
	{}

	/** Sort primarily by the specified function and mode. Beware the function is a reference, so must be valid for the lifetime of this instance. */
	FSceneOutlinerSortHelper& Primary(FPrimaryFunction&& Function, EColumnSortMode::Type SortMode)
	{
		PrimarySortMode = SortMode;
		PrimaryFunction = MoveTemp(Function);
		return *this;
	}

	/** Sort secondarily by the specified function and mode. Beware the function is a reference, so must be valid for the lifetime of this instance. */
	FSceneOutlinerSortHelper& Secondary(FSecondaryFunction&& Function, EColumnSortMode::Type SortMode)
	{
		SecondarySortMode = SortMode;
		SecondaryFunction = MoveTemp(Function);
		return *this;
	}

	/** Sort the specified array using the current sort settings */
	void Sort(TArray<FSceneOutlinerTreeItemPtr>& Array)
	{
		TArray<FSortPayload> SortData;
		const auto End = Array.Num();
		for (int32 Index = 0; Index < End; ++Index)
		{
			const auto& Element = Array[Index];

			PrimaryKeyType PrimaryKey = PrimaryFunction(*Element);

			SecondaryKeyType SecondaryKey;
			if (SecondarySortMode != EColumnSortMode::None)
			{
				SecondaryKey = SecondaryFunction(*Element);
			}

			SortData.Emplace(Index, MoveTemp(PrimaryKey), MoveTemp(SecondaryKey));
		}

		SortData.Sort([&](const FSortPayload& One, const FSortPayload& Two){
			if (PrimarySortMode == EColumnSortMode::Ascending && One.PrimaryKey != Two.PrimaryKey)
				return One.PrimaryKey < Two.PrimaryKey;
			else if (PrimarySortMode == EColumnSortMode::Descending && One.PrimaryKey != Two.PrimaryKey)
				return One.PrimaryKey > Two.PrimaryKey;

			if (SecondarySortMode == EColumnSortMode::Ascending)
				return One.SecondaryKey < Two.SecondaryKey;
			else if (SecondarySortMode == EColumnSortMode::Descending)
				return One.SecondaryKey > Two.SecondaryKey;

			return false;
		});

		TArray<FSceneOutlinerTreeItemPtr> NewArray;
		NewArray.Reserve(Array.Num());

		for (const auto& Element : SortData)
		{
			NewArray.Add(Array[Element.OriginalIndex]);
		}
		Array = MoveTemp(NewArray);
	}

private:

	EColumnSortMode::Type 	PrimarySortMode;
	EColumnSortMode::Type 	SecondarySortMode;

	FPrimaryFunction	PrimaryFunction;
	FSecondaryFunction	SecondaryFunction;

	/** Aggregated data from the sort methods. We extract the sort data from all elements first, then sort based on the extracted data. */
	struct FSortPayload
	{
		int32 OriginalIndex;
			
		PrimaryKeyType PrimaryKey;
		SecondaryKeyType SecondaryKey;

		FSortPayload(int32 InOriginalIndex, PrimaryKeyType&& InPrimaryKey, SecondaryKeyType&& InSecondaryKey)
			: OriginalIndex(InOriginalIndex), PrimaryKey(MoveTemp(InPrimaryKey)), SecondaryKey(MoveTemp(InSecondaryKey))
		{}

		FSortPayload(FSortPayload&& Other)
		{
			(*this) = MoveTemp(Other);
		}
		FSortPayload& operator=(FSortPayload&& rhs)
		{
			OriginalIndex = rhs.OriginalIndex;
			PrimaryKey = MoveTemp(rhs.PrimaryKey);
			SecondaryKey = MoveTemp(rhs.SecondaryKey);
			return *this;
		}

	private:
		FSortPayload(const FSortPayload&);
		FSortPayload& operator=(const FSortPayload&);
	};
};
