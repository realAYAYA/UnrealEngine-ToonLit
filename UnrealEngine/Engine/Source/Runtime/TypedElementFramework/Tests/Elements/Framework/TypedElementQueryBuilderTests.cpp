// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_TESTS
#include "TypedElementTestColumns.h"

#include "Algo/Sort.h"
#include "CoreTypes.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Elements/Common/TypedElementQueryConditions.h"
#include "Tests/TestHarnessAdapter.h"
#include "UObject/Class.h"

using namespace TypedElementDataStorage;


static void AppendColumnName(FString& Output, TWeakObjectPtr<const UScriptStruct> TypeInfo)
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

static bool TestMatching(const FQueryConditions& TestQuery, const TArray<FColumnBase>& RequestedColumns, bool Expected, bool Sort = false)
{
	if (Sort)
	{
		Algo::SortBy(RequestedColumns,
			[](const FColumnBase& Column) { return Column.TypeInfo.Get(); });
	}

	TArray<TWeakObjectPtr<const UScriptStruct>> Matches;
	bool Result = TestQuery.Verify(Matches, RequestedColumns);

	FString Description = (Result == Expected) ? TEXT("[Pass] ") : TEXT("[Fail] ");
	TestQuery.AppendToString(Description);

#if WITH_EDITORONLY_DATA
	{
		Description += " -> { ";
		auto It = RequestedColumns.begin();
		AppendColumnName(Description, (*It).TypeInfo);
		++It;
		for (; It != RequestedColumns.end(); ++It)
		{
			Description += ", ";
			AppendColumnName(Description, (*It).TypeInfo);
		}
		Description += " } ";
	}
#endif

	if (Expected)
	{
#if WITH_EDITORONLY_DATA
		if (!Matches.IsEmpty())
		{
			Description += " -> { ";
			auto It = Matches.begin();
			AppendColumnName(Description, *It);
			++It;
			for (; It != Matches.end(); ++It)
			{
				Description += ", ";
				AppendColumnName(Description, *It);
			}
			Description += " } ";
		}
#endif

		for (TWeakObjectPtr<const UScriptStruct> Match : Matches)
		{
			bool Found = false;
			for (const FColumnBase& Requested : RequestedColumns)
			{
				if (Match == Requested.TypeInfo)
				{
					Found = true;
					break;
				}
			}

			if (!Found)
			{
				Result = false;
				Description += " [Match failed]";
				break;
			}
		}
	}

	INFO(MoveTemp(Description));
	return (Result == Expected);
}

TEST_CASE_NAMED(FTypedElementQueryConditions_NoColumn, "TypedElementQueryBuilder::FTypedElementQueryConditions_NoColumn", "[ApplicationContextMask][EngineFilter]")
{
	FQueryConditions Example;
	
	CHECK(Example.MinimumColumnMatchRequired() == 0);
	CHECK(TestMatching(Example, { FColumn<FTestColumnA>() }, false));
}

TEST_CASE_NAMED(FTypedElementQueryConditions_OneColumn, "TypedElementQueryBuilder::FTypedElementQueryConditions_OneColumn", "[ApplicationContextMask][EngineFilter]")
{
	FQueryConditions Example{ FColumn<FTestColumnA>() };

	CHECK(Example.MinimumColumnMatchRequired() == 1);
	CHECK(TestMatching(Example, { FColumn<FTestColumnA>() }, true));
}

TEST_CASE_NAMED(FTypedElementQueryConditions1, "TypedElementQueryBuilder::FTypedElementQueryConditions A && B && C", "[ApplicationContextMask][EngineFilter]")
{
	FColumn TestA(FTestColumnA::StaticStruct());
	
	FQueryConditions Example = FColumn<FTestColumnA>() && FColumn<FTestColumnB>() && FColumn<FTestColumnC>();
	
	CHECK(Example.MinimumColumnMatchRequired() == 3);
	CHECK(TestMatching(Example, { FColumn<FTestColumnA>(), FColumn<FTestColumnB>(), FColumn<FTestColumnC>() }, true));
	CHECK(TestMatching(Example, { FColumn<FTestColumnA>(), FColumn<FTestColumnB>(), FColumn<FTestColumnD>() }, false));
}

TEST_CASE_NAMED(FTypedElementQueryConditions2, "TypedElementQueryBuilder::FTypedElementQueryConditions A || B || C", "[ApplicationContextMask][EngineFilter]")
{
	FQueryConditions Example = FColumn<FTestColumnA>() || FColumn<FTestColumnB>() || FColumn<FTestColumnC>();
	
	CHECK(Example.MinimumColumnMatchRequired() == 1);
	CHECK(TestMatching(Example, { FColumn<FTestColumnB>() }, true));
	CHECK(TestMatching(Example, { FColumn<FTestColumnB>(), FColumn<FTestColumnC>() }, true));
	CHECK(TestMatching(Example, { FColumn<FTestColumnD>() }, false));
}

TEST_CASE_NAMED(FTypedElementQueryConditions3, "TypedElementQueryBuilder::FTypedElementQueryConditions A && (B || C)", "[ApplicationContextMask][EngineFilter]")
{
	FQueryConditions Example = FColumn<FTestColumnA>() && (FColumn<FTestColumnB>() || FColumn<FTestColumnC>());
	
	CHECK(Example.MinimumColumnMatchRequired() == 2);
	CHECK(TestMatching(Example, { FColumn<FTestColumnA>(), FColumn<FTestColumnB>() }, true));
	CHECK(TestMatching(Example, { FColumn<FTestColumnA>(), FColumn<FTestColumnC>() }, true));
	CHECK(TestMatching(Example, { FColumn<FTestColumnA>(), FColumn<FTestColumnD>() }, false));
	CHECK(TestMatching(Example, { FColumn<FTestColumnD>(), FColumn<FTestColumnB>() }, false));
}

TEST_CASE_NAMED(FTypedElementQueryConditions4, "TypedElementQueryBuilder::FTypedElementQueryConditions A && (B || C) && (D || E)", "[ApplicationContextMask][EngineFilter]")
{
	FQueryConditions Example = 
		FColumn<FTestColumnA>() && 
		(FColumn<FTestColumnB>() || FColumn<FTestColumnC>()) &&
		(FColumn<FTestColumnD>() || FColumn<FTestColumnE>());
	
	CHECK(Example.MinimumColumnMatchRequired() == 3);
	
	CHECK(TestMatching(Example, { FColumn<FTestColumnA>() }, false));
	CHECK(TestMatching(Example, { FColumn<FTestColumnA>(), FColumn<FTestColumnB>() }, false));

	CHECK(TestMatching(Example, { FColumn<FTestColumnA>(), FColumn<FTestColumnB>(), FColumn<FTestColumnD>() }, true));
	CHECK(TestMatching(Example, { FColumn<FTestColumnA>(), FColumn<FTestColumnB>(), FColumn<FTestColumnE>() }, true));
	CHECK(TestMatching(Example, { FColumn<FTestColumnA>(), FColumn<FTestColumnC>(), FColumn<FTestColumnD>() }, true));

	CHECK(TestMatching(Example, { FColumn<FTestColumnA>(), FColumn<FTestColumnC>(), FColumn<FTestColumnF>() }, false));
	CHECK(TestMatching(Example, { FColumn<FTestColumnA>(), FColumn<FTestColumnF>(), FColumn<FTestColumnD>() }, false));
	CHECK(TestMatching(Example, { FColumn<FTestColumnB>(), FColumn<FTestColumnC>(), FColumn<FTestColumnD>() }, false));
}

TEST_CASE_NAMED(FTypedElementQueryConditions5, "TypedElementQueryBuilder::FTypedElementQueryConditions (A || B) && (C || D) && (E || F)", "[ApplicationContextMask][EngineFilter]")
{
	FQueryConditions Example =
		(FColumn<FTestColumnA>() || FColumn<FTestColumnB>()) &&
		(FColumn<FTestColumnC>() || FColumn<FTestColumnD>()) &&
		(FColumn<FTestColumnE>() || FColumn<FTestColumnF>());
	
	CHECK(Example.MinimumColumnMatchRequired() == 3); 
	
	CHECK(TestMatching(Example, { FColumn<FTestColumnA>() }, false));
	CHECK(TestMatching(Example, { FColumn<FTestColumnA>(), FColumn<FTestColumnC>() }, false));
	
	CHECK(TestMatching(Example, { FColumn<FTestColumnA>(), FColumn<FTestColumnC>(), FColumn<FTestColumnE>() }, true));
	CHECK(TestMatching(Example, { FColumn<FTestColumnB>(), FColumn<FTestColumnC>(), FColumn<FTestColumnE>() }, true));
	
	CHECK(TestMatching(Example, { FColumn<FTestColumnA>(), FColumn<FTestColumnC>(), FColumn<FTestColumnG>() }, false));
	CHECK(TestMatching(Example, { FColumn<FTestColumnA>(), FColumn<FTestColumnG>(), FColumn<FTestColumnD>() }, false));
	CHECK(TestMatching(Example, { FColumn<FTestColumnG>(), FColumn<FTestColumnC>(), FColumn<FTestColumnD>() }, false));
}

TEST_CASE_NAMED(FTypedElementQueryConditions6, "TypedElementQueryBuilder::FTypedElementQueryConditions ((A || B) && (C || D)) || (E && F)", "[ApplicationContextMask][EngineFilter]")
{
	FQueryConditions Example =
		(
			(FColumn<FTestColumnA>() || FColumn<FTestColumnB>()) &&
			(FColumn<FTestColumnC>() || FColumn<FTestColumnD>())
		) ||
		(FColumn<FTestColumnE>() && FColumn<FTestColumnF>());
	
	CHECK(Example.MinimumColumnMatchRequired() == 2);

	CHECK(TestMatching(Example, { FColumn<FTestColumnA>() }, false));
	CHECK(TestMatching(Example, { FColumn<FTestColumnA>(), FColumn<FTestColumnC>() }, true));

	CHECK(TestMatching(Example, { FColumn<FTestColumnE>(), FColumn<FTestColumnF>() }, true));
	CHECK(TestMatching(Example, { FColumn<FTestColumnG>() }, false));
}

TEST_CASE_NAMED(FTypedElementQueryConditions7, "TypedElementQueryBuilder::FTypedElementQueryConditions (A && B) || (C && D) || (E && F)", "[ApplicationContextMask][EngineFilter]")
{
	FQueryConditions Example =
		(FColumn<FTestColumnA>() && FColumn<FTestColumnB>()) ||
		(FColumn<FTestColumnC>() && FColumn<FTestColumnD>()) ||
		(FColumn<FTestColumnE>() && FColumn<FTestColumnF>());

	CHECK(Example.MinimumColumnMatchRequired() == 2);

	CHECK(TestMatching(Example, { FColumn<FTestColumnA>() }, false));
	CHECK(TestMatching(Example, { FColumn<FTestColumnA>(), FColumn<FTestColumnB>() }, true));
	CHECK(TestMatching(Example, { FColumn<FTestColumnC>(), FColumn<FTestColumnD>() }, true));
	CHECK(TestMatching(Example, { FColumn<FTestColumnE>(), FColumn<FTestColumnF>() }, true));

	CHECK(TestMatching(Example, { FColumn<FTestColumnC>(), FColumn<FTestColumnD>(), FColumn<FTestColumnE>(), FColumn<FTestColumnF>() }, true));


	CHECK(TestMatching(Example, { FColumn<FTestColumnE>() }, false));
	CHECK(TestMatching(Example, { FColumn<FTestColumnG>() }, false));
}

TEST_CASE_NAMED(FTypedElementQueryConditions_MultiMatch, "TypedElementQueryBuilder::FTypedElementQueryConditions_MultiMatch", "[ApplicationContextMask][EngineFilter]")
{
	FQueryConditions Example =
		(FColumn<FTestColumnA>() || FColumn<FTestColumnB>()) &&
		(FColumn<FTestColumnC>() || FColumn<FTestColumnD>()) &&
		(FColumn<FTestColumnE>() || FColumn<FTestColumnF>());

	CHECK(TestMatching(Example, 
		{ 
			FColumn<FTestColumnA>(), 
			FColumn<FTestColumnB>(), 
			FColumn<FTestColumnC>(), 
			FColumn<FTestColumnD>(),
			FColumn<FTestColumnE>(),
			FColumn<FTestColumnF>()
		}, true));

	CHECK(TestMatching(Example,
		{
			FColumn<FTestColumnA>(),
			FColumn<FTestColumnC>(),
			FColumn<FTestColumnE>(),
			FColumn<FTestColumnG>()
		}, true));
}

TEST_CASE_NAMED(FTypedElementQueryConditions_Sorted, "TypedElementQueryBuilder::FTypedElementQueryConditions_Sorted", "[ApplicationContextMask][EngineFilter]")
{
	FQueryConditions Example =
		(FColumn<FTestColumnA>() && FColumn<FTestColumnB>()) ||
		(FColumn<FTestColumnC>() && FColumn<FTestColumnD>()) ||
		(FColumn<FTestColumnE>() && FColumn<FTestColumnF>());

	CHECK(Example.MinimumColumnMatchRequired() == 2);

	CHECK(TestMatching(Example, { FColumn<FTestColumnA>(), FColumn<FTestColumnB>() }, true, true));
	CHECK(TestMatching(Example, { FColumn<FTestColumnC>(), FColumn<FTestColumnD>() }, true, true));
	CHECK(TestMatching(Example, { FColumn<FTestColumnE>(), FColumn<FTestColumnF>() }, true, true));

	CHECK(TestMatching(Example, { FColumn<FTestColumnC>(), FColumn<FTestColumnD>(), FColumn<FTestColumnE>(), FColumn<FTestColumnF>() }, true, true));
}

#endif // WITH_TESTS
