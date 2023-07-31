// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "Math/UnrealMathUtility.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Misc/AutomationTest.h"
#include "Algo/Copy.h"
#include "Algo/Heapify.h"
#include "Algo/HeapSort.h"
#include "Algo/IntroSort.h"
#include "Algo/IsHeap.h"
#include "Algo/IsSorted.h"
#include "Algo/Sort.h"
#include "Algo/Transform.h"
#include "Algo/IndexOf.h"
#include "Algo/LevenshteinDistance.h"
#include "Templates/Greater.h"

namespace UE
{
namespace Impl
{

	struct FFixedTestRangeUnsigned
	{
		using SizeType = uint8;

		FFixedTestRangeUnsigned()
		{
			uint8 Count = 0;
			for (uint8& N : Numbers)
			{
				N = Count++;
			}
		}
		uint8 Num() const
		{
			return UE_ARRAY_COUNT(Numbers);
		}
		const uint8* GetData() const
		{
			return Numbers;
		}
		uint8 Numbers[255];
	};

} // namespace Impl
} // namespace UE

template<> struct TIsContiguousContainer<UE::Impl::FFixedTestRangeUnsigned>{ enum { Value = true }; };

class FAlgosTestBase : public FAutomationTestBase
{
public:
	using FAutomationTestBase::FAutomationTestBase;

	static constexpr int32 NumTestObjects = 32;

	struct FTestData
	{
		FTestData(FString&& InName, int32 InAge, bool bInRetired = false)
			: Name(MoveTemp(InName))
			, Age(InAge)
			, bRetired(bInRetired)
		{
		}

		friend bool operator==(const FTestData& A, const FTestData&B)
		{
			return A.Name == B.Name && A.Age == B.Age && A.bRetired == B.bRetired;
		}
		bool IsTeenager() const
		{
			return Age >= 13 && Age <= 19;
		}

		FString GetName() const
		{
			return Name;
		}

		FString Name;
		int32 Age;
		bool bRetired;
	};

	void Initialize()
	{
		for (int i = 0; i < NumTestObjects; ++i)
		{
			TestData.Add(i);
		}
		for (int i = 0; i < NumTestObjects; ++i)
		{
			TestData2.Add(FMath::Rand());
		}
	}

	void Cleanup()
	{
		TestData2.Empty();
		TestData.Empty();
	}

	void TestCopy()
	{
		TArray<int> TestArray;
		// empty array
		Algo::Copy(TestData, TestArray);
		check(TestArray == TestData);
		// existing data
		Algo::Copy(TestData2, TestArray);
		check(TestArray.Num() == NumTestObjects * 2);
		for (int i = 0; i < NumTestObjects; ++i)
		{
			check(TestArray[i] == TestData[i]);
		}
		for (int i = 0; i < NumTestObjects; ++i)
		{
			check(TestArray[i + NumTestObjects] == TestData2[i]);
		}
	}

	void TestCopyIf()
	{
		TArray<int> TestArray;
		// empty array
		Algo::CopyIf(TestData, TestArray, [](int i) { return (i % 2) == 0; });
		int j = 0;
		for (int i = 0; i < NumTestObjects; ++i)
		{
			if (TestData[i] % 2 == 0)
			{
				check(TestArray[j] == TestData[i]);
				++j;
			}
		}
		// existing data
		Algo::CopyIf(TestData2, TestArray, [](int i) { return (i % 2) == 0; });
		j = 0;
		for (int i = 0; i < NumTestObjects; ++i)
		{
			if (TestData[i] % 2 == 0)
			{
				check(TestArray[j] == TestData[i]);
				++j;
			}
		}
		for (int i = 0; i < NumTestObjects; ++i)
		{
			if (TestData2[i] % 2 == 0)
			{
				check(TestArray[j] == TestData2[i]);
				++j;
			}
		}
		check(j == TestArray.Num())
	}

	void TestTransform()
	{
		TArray<float> TestArray;

		// empty array
		{
			Algo::Transform(TestData, TestArray, [](int i) { return FMath::DegreesToRadians((float)i); });
			check(TestArray.Num() == NumTestObjects);
			for (int i = 0; i < TestArray.Num(); ++i)
			{
				check(TestArray[i] == FMath::DegreesToRadians((float)TestData[i]));
			}
		}

		// existing data
		{
			Algo::Transform(TestData2, TestArray, [](int i) { return FMath::DegreesToRadians((float)i); });
			check(TestArray.Num() == NumTestObjects * 2);
			for (int i = 0; i < NumTestObjects; ++i)
			{
				check(TestArray[i] == FMath::DegreesToRadians((float)TestData[i]));
			}
			for (int i = 0; i < NumTestObjects; ++i)
			{
				check(TestArray[i + NumTestObjects] == FMath::DegreesToRadians((float)TestData2[i]));
			}
		}

		// projection via member function pointer
		{
			TArray<FString> Strings = {
				TEXT("Hello"),
				TEXT("this"),
				TEXT("is"),
				TEXT("a"),
				TEXT("projection"),
				TEXT("test")
			};

			TArray<int32> Lengths;
			Algo::Transform(Strings, Lengths, &FString::Len);
			check(Lengths == TArray<int32>({ 5, 4, 2, 1, 10, 4 }));
		}

		// projection via data member pointer
		{
			TArray<FTestData> Data = {
				FTestData(TEXT("Alice"), 31),
				FTestData(TEXT("Bob"), 25),
				FTestData(TEXT("Charles"), 19),
				FTestData(TEXT("Donna"), 13)
			};

			TArray<int32> Ages;
			Algo::Transform(Data, Ages, &FTestData::Age);

			check(Ages == TArray<int32>({ 31, 25, 19, 13 }));
		}

		// projection across smart pointers
		{
			TArray<TUniquePtr<FTestData>> Data;
			Data.Add(MakeUnique<FTestData>(TEXT("Elsa"), 61));
			Data.Add(MakeUnique<FTestData>(TEXT("Fred"), 11));
			Data.Add(MakeUnique<FTestData>(TEXT("Georgina"), 34));
			Data.Add(MakeUnique<FTestData>(TEXT("Henry"), 54));
			Data.Add(MakeUnique<FTestData>(TEXT("Ichabod"), 87));

			TArray<FString> Names;
			Algo::Transform(Data, Names, &FTestData::Name);

			TArray<FString> ExpectedNames = { TEXT("Elsa"), TEXT("Fred"), TEXT("Georgina"), TEXT("Henry"), TEXT("Ichabod") };
			check(Names == ExpectedNames);
		}
	}

	void TestTransformIf()
	{
		TArray<float> TestArray;

		// empty array
		{
			Algo::TransformIf(TestData, TestArray, [](int i) { return (i % 2) == 0; }, [](int i) { return FMath::DegreesToRadians((float)i); });
			int j = 0;
			for (int i = 0; i < NumTestObjects; ++i)
			{
				if (TestData[i] % 2 == 0)
				{
					check(TestArray[j] == FMath::DegreesToRadians((float)TestData[i]));
					++j;
				}
			}
		}

		// existing data
		{
			Algo::TransformIf(TestData2, TestArray, [](int i) { return (i % 2) == 0; }, [](int i) { return FMath::DegreesToRadians((float)i); });
			int j = 0;
			for (int i = 0; i < NumTestObjects; ++i)
			{
				if (TestData[i] % 2 == 0)
				{
					check(TestArray[j] == FMath::DegreesToRadians((float)TestData[i]));
					++j;
				}
			}
			for (int i = 0; i < NumTestObjects; ++i)
			{
				if (TestData2[i] % 2 == 0)
				{
					check(TestArray[j] == FMath::DegreesToRadians((float)TestData2[i]));
					++j;
				}
			}
			check(j == TestArray.Num());
		}

		TArray<TUniquePtr<FTestData>> Data;
		Data.Add(MakeUnique<FTestData>(TEXT("Jeff"), 15, false));
		Data.Add(MakeUnique<FTestData>(TEXT("Katrina"), 77, true));
		Data.Add(MakeUnique<FTestData>(TEXT("Lenny"), 29, false));
		Data.Add(MakeUnique<FTestData>(TEXT("Michelle"), 13, false));
		Data.Add(MakeUnique<FTestData>(TEXT("Nico"), 65, true));

		// projection and transform via data member pointer
		{
			TArray<FString> NamesOfRetired;
			Algo::TransformIf(Data, NamesOfRetired, &FTestData::bRetired, &FTestData::Name);
			TArray<FString> ExpectedNamesOfRetired = { TEXT("Katrina"), TEXT("Nico") };
			check(NamesOfRetired == ExpectedNamesOfRetired);
		}

		// projection and transform via member function pointer
		{
			TArray<FString> NamesOfTeenagers;
			Algo::TransformIf(Data, NamesOfTeenagers, &FTestData::IsTeenager, &FTestData::GetName);
			TArray<FString> ExpectedNamesOfTeenagers = { TEXT("Jeff"), TEXT("Michelle") };
			check(NamesOfTeenagers == ExpectedNamesOfTeenagers);
		}
	}

	void TestBinarySearch()
	{
		// Verify static array case
		int StaticArray[] = { 2,4,6,6,6,8 };

		check(Algo::BinarySearch(StaticArray, 6) == 2);
		check(Algo::BinarySearch(StaticArray, 5) == INDEX_NONE);
		check(Algo::BinarySearchBy(StaticArray, 4, FIdentityFunctor()) == 1);

		check(Algo::LowerBound(StaticArray, 6) == 2);
		check(Algo::LowerBound(StaticArray, 5) == 2);
		check(Algo::UpperBound(StaticArray, 6) == 5);
		check(Algo::LowerBound(StaticArray, 7) == 5);
		check(Algo::LowerBound(StaticArray, 9) == 6);
		check(Algo::LowerBoundBy(StaticArray, 6, FIdentityFunctor()) == 2);
		check(Algo::UpperBoundBy(StaticArray, 6, FIdentityFunctor()) == 5);

		// Dynamic array case
		TArray<int32> IntArray = { 2,2,4,4,6,6,6,8,8 };

		check(Algo::BinarySearch(IntArray, 6) == 4);
		check(Algo::BinarySearch(IntArray, 5) == INDEX_NONE);
		check(Algo::BinarySearchBy(IntArray, 4, FIdentityFunctor()) == 2);

		check(Algo::LowerBound(IntArray, 2) == 0);
		check(Algo::UpperBound(IntArray, 2) == 2);
		check(Algo::LowerBound(IntArray, 6) == 4);
		check(Algo::UpperBound(IntArray, 6) == 7);
		check(Algo::LowerBound(IntArray, 5) == 4);
		check(Algo::UpperBound(IntArray, 5) == 4);
		check(Algo::LowerBound(IntArray, 7) == 7);
		check(Algo::LowerBound(IntArray, 9) == 9);
		check(Algo::LowerBoundBy(IntArray, 6, FIdentityFunctor()) == 4);
		check(Algo::UpperBoundBy(IntArray, 6, FIdentityFunctor()) == 7);
	}

	void TestIndexOf()
	{
		TArray<FTestData> Data = {
			FTestData(TEXT("Alice"), 31),
			FTestData(TEXT("Bob"), 25),
			FTestData(TEXT("Charles"), 19),
			FTestData(TEXT("Donna"), 13)
		};

		int FixedArray[] = { 2,4,6,6,6,8 };
		check(Algo::IndexOf(FixedArray, 2) == 0);
		check(Algo::IndexOf(FixedArray, 6) == 2);
		check(Algo::IndexOf(FixedArray, 8) == 5);
		check(Algo::IndexOf(FixedArray, 0) == INDEX_NONE);

		check(Algo::IndexOf(Data, FTestData(TEXT("Alice"), 31)) == 0);
		check(Algo::IndexOf(Data, FTestData(TEXT("Alice"), 32)) == INDEX_NONE);

		check(Algo::IndexOfBy(Data, TEXT("Donna"), &FTestData::Name) == 3);
		check(Algo::IndexOfBy(Data, 19, &FTestData::Age) == 2);
		check(Algo::IndexOfBy(Data, 0, &FTestData::Age) == INDEX_NONE);

		auto GetAge = [](const FTestData& In) { return In.Age; };
		check(Algo::IndexOfBy(Data, 19, GetAge) == 2);
		check(Algo::IndexOfBy(Data, 0, GetAge) == INDEX_NONE);

		check(Algo::IndexOfByPredicate(Data, [](const FTestData& In) { return In.Age < 25; }) == 2);
		check(Algo::IndexOfByPredicate(Data, [](const FTestData& In) { return In.Age > 19; }) == 0);
		check(Algo::IndexOfByPredicate(Data, [](const FTestData& In) { return In.Age > 31; }) == INDEX_NONE);

		static const uint8 InvalidIndex = (uint8)-1;
		UE::Impl::FFixedTestRangeUnsigned TestRange;
		check(Algo::IndexOf(TestRange, 25) == 25);
		check(Algo::IndexOf(TestRange, 254) == 254);
		check(Algo::IndexOf(TestRange, 255) == InvalidIndex);
		check(Algo::IndexOf(TestRange, 1024) == InvalidIndex);
	}

	void TestHeapify()
	{
		TArray<int> TestArray = TestData2;
		Algo::Heapify(TestArray);

		check(Algo::IsHeap(TestArray));
	}

	void TestHeapSort()
	{
		TArray<int> TestArray = TestData2;
		Algo::HeapSort(TestArray);

		check(Algo::IsHeap(TestArray));

		check(Algo::IsSorted(TestArray));
	}

	void TestIntroSort()
	{
		TArray<int> TestArray = TestData2;
		Algo::IntroSort(TestArray);

		check(Algo::IsSorted(TestArray));
	}

	void TestSort()
	{
		// regular Sort
		TArray<int> TestArray = TestData2;
		Algo::Sort(TestArray);

		check(Algo::IsSorted(TestArray));

		// Sort with predicate
		TestArray = TestData2;

		TGreater<> Predicate;
		Algo::Sort(TestArray, Predicate);

		check(Algo::IsSorted(TestArray, Predicate));

		// SortBy
		TestArray = TestData2;

		auto Projection = [](int Val) -> int
		{
			return Val % 1000; // will sort using the last 3 digits only
		};

		Algo::SortBy(TestArray, Projection);

		check(Algo::IsSortedBy(TestArray, Projection));

		// SortBy with predicate
		TestArray = TestData2;

		Algo::SortBy(TestArray, Projection, Predicate);

		check(Algo::IsSortedBy(TestArray, Projection, Predicate));
	}

	void TestEditDistance()
	{
		struct FEditDistanceTestData
		{
			const TCHAR* A;
			const TCHAR* B;
			ESearchCase::Type SearchCase;
			int32 ExpectedResultDistance;
		};

		const FEditDistanceTestData EditDistanceTests[] =
		{
			//Empty tests
			{ TEXT(""), TEXT("Saturday"), ESearchCase::CaseSensitive, 8 },
			{ TEXT(""), TEXT("Saturday"), ESearchCase::IgnoreCase, 8 },
			{ TEXT("Saturday"), TEXT(""), ESearchCase::CaseSensitive, 8 },
			{ TEXT("Saturday"), TEXT(""), ESearchCase::IgnoreCase, 8 },
			//One letter tests
			{ TEXT("a"), TEXT("a"), ESearchCase::CaseSensitive, 0 },
			{ TEXT("a"), TEXT("b"), ESearchCase::CaseSensitive, 1 },
			//Equal tests
			{ TEXT("Saturday"), TEXT("Saturday"), ESearchCase::CaseSensitive, 0 },
			{ TEXT("Saturday"), TEXT("Saturday"), ESearchCase::IgnoreCase, 0 },
			//Simple casing test
			{ TEXT("Saturday"), TEXT("saturday"), ESearchCase::CaseSensitive, 1 },
			{ TEXT("Saturday"), TEXT("saturday"), ESearchCase::IgnoreCase, 0 },
			{ TEXT("saturday"), TEXT("Saturday"), ESearchCase::CaseSensitive, 1 },
			{ TEXT("saturday"), TEXT("Saturday"), ESearchCase::IgnoreCase, 0 },
			{ TEXT("SaturdaY"), TEXT("saturday"), ESearchCase::CaseSensitive, 2 },
			{ TEXT("SaturdaY"), TEXT("saturday"), ESearchCase::IgnoreCase, 0 },
			{ TEXT("saturdaY"), TEXT("Saturday"), ESearchCase::CaseSensitive, 2 },
			{ TEXT("saturdaY"), TEXT("Saturday"), ESearchCase::IgnoreCase, 0 },
			{ TEXT("SATURDAY"), TEXT("saturday"), ESearchCase::CaseSensitive, 8 },
			{ TEXT("SATURDAY"), TEXT("saturday"), ESearchCase::IgnoreCase, 0 },
			//First char diff
			{ TEXT("Saturday"), TEXT("baturday"), ESearchCase::CaseSensitive, 1 },
			{ TEXT("Saturday"), TEXT("baturday"), ESearchCase::IgnoreCase, 1 },
			//Last char diff
			{ TEXT("Saturday"), TEXT("Saturdai"), ESearchCase::CaseSensitive, 1 },
			{ TEXT("Saturday"), TEXT("Saturdai"), ESearchCase::IgnoreCase, 1 },
			//Middle char diff
			{ TEXT("Satyrday"), TEXT("Saturday"), ESearchCase::CaseSensitive, 1 },
			{ TEXT("Satyrday"), TEXT("Saturday"), ESearchCase::IgnoreCase, 1 },
			//Real cases
			{ TEXT("Copy_Body"), TEXT("Body"), ESearchCase::CaseSensitive, 5 },
			{ TEXT("Copy_Body"), TEXT("Body"), ESearchCase::IgnoreCase, 5 },
			{ TEXT("copy_Body"), TEXT("Paste_Body"), ESearchCase::CaseSensitive, 5 },
			{ TEXT("copy_Body"), TEXT("Paste_Body"), ESearchCase::IgnoreCase, 5 },
			{ TEXT("legs"), TEXT("Legs_1"), ESearchCase::CaseSensitive, 3 },
			{ TEXT("legs"), TEXT("Legs_1"), ESearchCase::IgnoreCase, 2 },
			{ TEXT("arms"), TEXT("Arms"), ESearchCase::CaseSensitive, 1 },
			{ TEXT("arms"), TEXT("Arms"), ESearchCase::IgnoreCase, 0 },
			{ TEXT("Saturday"), TEXT("Sunday"), ESearchCase::CaseSensitive, 3 },
			{ TEXT("Saturday"), TEXT("Sunday"), ESearchCase::IgnoreCase, 3 },
			{ TEXT("Saturday"), TEXT("suNday"), ESearchCase::CaseSensitive, 4 },
			{ TEXT("Saturday"), TEXT("suNday"), ESearchCase::IgnoreCase, 3 },
			{ TEXT("Saturday"), TEXT("sUnday"), ESearchCase::CaseSensitive, 5 },
			{ TEXT("Saturday"), TEXT("sUnday"), ESearchCase::IgnoreCase, 3 },
		};

		for (const FEditDistanceTestData& Test : EditDistanceTests)
		{
			RunEditDistanceTest(Test.A, Test.B, Test.SearchCase, Test.ExpectedResultDistance);
		}
	}

	void RunEditDistanceTest(const FString& A, const FString& B, const ESearchCase::Type SearchCase, const int32 ExpectedResultDistance)
	{
		// Run test
		int32 ResultDistance = MAX_int32;
		if (SearchCase == ESearchCase::IgnoreCase)
		{
			ResultDistance = Algo::LevenshteinDistance(A.ToLower(), B.ToLower());
		}
		else
		{
			ResultDistance = Algo::LevenshteinDistance(A, B);
		}

		if (ResultDistance != ExpectedResultDistance)
		{
			FString SearchCaseStr = SearchCase == ESearchCase::CaseSensitive ? TEXT("CaseSensitive") : TEXT("IgnoreCase");
			AddError(FString::Printf(TEXT("Algo::EditDistance return the wrong distance between 2 string (A '%s', B '%s', case '%s', result '%d', expected '%d')."), *A, *B, *SearchCaseStr, ResultDistance, ExpectedResultDistance));
		}
	}

	void TestEditDistanceArray()
	{
		struct FEditDistanceArrayTestData
		{
			const TCHAR* ArrayDescriptionA;
			const TCHAR* ArrayDescriptionB;
			TArray<int32> A;
			TArray<int32> B;
			int32 ExpectedResultDistance;
		};

		const FEditDistanceArrayTestData EditDistanceArrayTests[] =
		{
			//Identical array
			{ TEXT("{1, 2, 3, 4}"), TEXT("{1, 2, 3, 4}"), {1, 2, 3, 4}, {1, 2, 3, 4}, 0 },
			//1 difference
			{ TEXT("{1, 2, 3, 4}"), TEXT("{1, 2, 3, 10}"), {1, 2, 3, 4}, {1, 2, 3, 10}, 1 },
			//1 character less
			{ TEXT("{1, 2, 3, 4}"), TEXT("{1, 2, 3}"), {1, 2, 3, 4}, {1, 2, 3}, 1 },
			//1 character more
			{ TEXT("{1, 2, 3, 4}"), TEXT("{1, 2, 3, 4, 5}"), {1, 2, 3, 4}, {1, 2, 3, 4, 5}, 1 },
			//2 character more
			{ TEXT("{1, 2, 3, 4}"), TEXT("{1, 2, 3, 4, 5, 6}"), {1, 2, 3, 4}, {1, 2, 3, 4, 5, 6}, 2 },
			//B string empty
			{ TEXT("{1, 2, 3, 4}"), TEXT("{}"), {1, 2, 3, 4}, {}, 4 },
		};

		for (const FEditDistanceArrayTestData& Test : EditDistanceArrayTests)
		{
			RunEditDistanceTestArray(Test.ArrayDescriptionA, Test.ArrayDescriptionB, Test.A, Test.B, Test.ExpectedResultDistance);
		}
	}

	void RunEditDistanceTestArray(const FString& ArrayDescriptionA, const FString& ArrayDescriptionB, const TArray<int32>& A, const TArray<int32>& B, const int32 ExpectedResultDistance)
	{
		// Run test
		int32 ResultDistance = Algo::LevenshteinDistance(A, B);

		if (ResultDistance != ExpectedResultDistance)
		{
			AddError(FString::Printf(TEXT("Algo::EditDistance return the wrong distance between 2 array (A '%s', B '%s', result '%d', expected '%d')."), *ArrayDescriptionA, *ArrayDescriptionB, ResultDistance, ExpectedResultDistance));
		}
	}

private:
	TArray<int> TestData;
	TArray<int> TestData2;
};

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FAlgosTest, FAlgosTestBase, "System.Core.Misc.Algos", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FAlgosTest::RunTest(const FString& Parameters)
{
	Initialize();
	TestCopy();
	TestCopyIf();
	TestTransform();
	TestTransformIf();
	TestBinarySearch();
	TestIndexOf();
	TestHeapify();
	TestHeapSort();
	TestIntroSort();
	TestSort();
	TestEditDistance();
	TestEditDistanceArray();
	Cleanup();

	return true;
}
