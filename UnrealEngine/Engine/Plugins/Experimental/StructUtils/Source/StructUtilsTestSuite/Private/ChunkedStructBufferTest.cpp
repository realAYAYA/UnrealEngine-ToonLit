// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "AITestsCommon.h"
#include "ChunkedStructBuffer.h"
#include "StructUtilsTestTypes.h"

#define LOCTEXT_NAMESPACE "StructUtilsTests"

PRAGMA_DISABLE_OPTIMIZATION

struct FStructUtilsTest_ChunkedStructBufferBasic : FAITestBase
{
	virtual bool InstantTest() override
	{
		FChunkedStructBuffer Buffer(1234);

		AITEST_TRUE("Chunk size should be 1234", Buffer.GetChunkSize() == 1234);

		Buffer.Emplace<FTestStructSimple>(42.0f);
		AITEST_TRUE("Should have 1 item", Buffer.Num() == 1);

		FTestStructComplex Item;
		Item.String = TEXT("Foo");
		Item.StringArray.Add(FString(TEXT("Bar")));
		Buffer.Add(Item);
		AITEST_TRUE("Should have 2 items", Buffer.Num() == 2);

		int Index = 0;
		bool bFoundSimple = false;
		bool bFoundComplex = false;
		Buffer.ForEach([&Index, &bFoundSimple, &bFoundComplex](FStructView Item)
		{
			if (Index == 0)
			{
				bFoundSimple = Item.GetScriptStruct() == FTestStructSimple::StaticStruct();
			}
			else if (Index == 1)
			{
				bFoundComplex = Item.GetScriptStruct() == FTestStructComplex::StaticStruct();
			}

			Index++;
		});

		AITEST_TRUE("Should have FTestStructSimple at index 0", bFoundSimple);
		AITEST_TRUE("Should have FTestStructComplex at index 1", bFoundComplex);

		Buffer.Clear();
		AITEST_TRUE("Should have 0 items", Buffer.Num() == 0);
		AITEST_TRUE("Should have 0 chunks", Buffer.GetNumUsedChunks() == 0);
		AITEST_TRUE("Should have 0 free chunk", Buffer.GetNumFreeChunks() == 0);

		// This should allocate second chunk,as there are more than 8 types.
		Buffer.Emplace<FTestStructSimple>();
		Buffer.Emplace<FTestStructSimple1>();
		Buffer.Emplace<FTestStructSimple2>();
		Buffer.Emplace<FTestStructSimple3>();
		Buffer.Emplace<FTestStructSimple4>();
		Buffer.Emplace<FTestStructSimple5>();
		Buffer.Emplace<FTestStructSimple6>();
		Buffer.Emplace<FTestStructSimple7>();
		Buffer.Emplace<FTestStructComplex>();
		AITEST_TRUE("Should have 9 items", Buffer.Num() == 9);
		AITEST_TRUE("Should have 2 chunks", Buffer.GetNumUsedChunks() == 2);
		
		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FStructUtilsTest_ChunkedStructBufferBasic, "System.StructUtils.ChunkedStructBuffer.Basic");

struct FStructUtilsTest_ChunkedStructBufferAppend : FAITestBase
{
	virtual bool InstantTest() override
	{
		FChunkedStructBuffer Buffer1(64);
		FChunkedStructBuffer Buffer2(64);
		FChunkedStructBuffer Buffer3(64);

		for (int32 i = 0; i < 30; i++)
		{
			FTestStructSimple& Item = Buffer1.Emplace_GetRef<FTestStructSimple>();
			Item.Float = (float)i;
		}
		AITEST_TRUE("Buffer1 should have 30 item", Buffer1.Num() == 30);
		
		for (int32 i = 0; i < 5; i++)
		{
			FTestStructComplex Item;
			FTestStructComplex& ItemRef = Buffer2.Add_GetRef(Item);
			ItemRef.String = FString::FormatAsNumber(i);
		}
		AITEST_TRUE("Buffer2 should have 5 item", Buffer2.Num() == 5);

		Buffer3.Append(Buffer1);
		AITEST_TRUE("Buffer3 should have 30 items", Buffer3.Num() == 30);

		Buffer3.Append(MoveTemp(Buffer2));
		AITEST_TRUE("Buffer3 should have 35 items", Buffer3.Num() == 35);
		AITEST_TRUE("Buffer2 should have 0 items", Buffer2.Num() == 0);
		AITEST_TRUE("Buffer2 should have 0 used chunks", Buffer2.GetNumUsedChunks() == 0);
		AITEST_TRUE("Buffer2 should have 0 free chunks", Buffer2.GetNumFreeChunks() == 0);

		Buffer2.Emplace<FTestStructSimple>(1);
		AITEST_TRUE("Buffer2 should have 1 item", Buffer2.Num() == 1);

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FStructUtilsTest_ChunkedStructBufferAppend, "System.StructUtils.ChunkedStructBuffer.Append");


struct FStructUtilsTest_ChunkedStructBufferFreelist : FAITestBase
{
	virtual bool InstantTest() override
	{
		FChunkedStructBuffer Buffer(64);

		for (int32 i = 0; i < 30; i++)
		{
			FTestStructSimple& Item = Buffer.Emplace_GetRef<FTestStructSimple>();
			Item.Float = (float)i;
		}
		AITEST_TRUE("Buffer should have 30 item", Buffer.Num() == 30);

		Buffer.Reset();
		AITEST_TRUE("Buffer should have 0 item", Buffer.Num() == 0);
		AITEST_TRUE("Should have 0 chunks", Buffer.GetNumUsedChunks() == 0);
		AITEST_TRUE("Should have >0 free chunks", Buffer.GetNumFreeChunks() > 0);

		for (int32 i = 0; i < 45; i++)
		{
			Buffer.Emplace<FTestStructSimple>((float)-i);
		}
		AITEST_TRUE("Buffer should have 45 items", Buffer.Num() == 45);
		AITEST_TRUE("Should have 0 free chunks", Buffer.GetNumFreeChunks() == 0);

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FStructUtilsTest_ChunkedStructBufferFreelist, "System.StructUtils.ChunkedStructBuffer.Freelist");


struct FStructUtilsTest_ChunkedStructBufferForEach : FAITestBase
{
	virtual bool InstantTest() override
	{
		FChunkedStructBuffer Buffer;

		Buffer.Emplace<FTestStructSimple>(1.0f);
		Buffer.Emplace<FTestStructComplex>();
		Buffer.Emplace<FTestStructSimple>(2.0f);
		Buffer.Emplace<FTestStructSimple>(3.0f);
		Buffer.Emplace<FTestStructSimple1>();
		Buffer.Emplace<FTestStructComplex>();

		int32 SimpleCount = 0;
		int32 ComplexCount = 0;
		Buffer.ForEach([&SimpleCount, &ComplexCount](FStructView Item)
		{
			if (Item.GetScriptStruct() == FTestStructSimple::StaticStruct())
			{
				SimpleCount++;
			}
			else if (Item.GetScriptStruct() == FTestStructComplex::StaticStruct())
			{
				ComplexCount++;
			}
		});
		AITEST_TRUE("Buffer should have 3 simple items", SimpleCount == 3);
		AITEST_TRUE("Buffer should have 2 complex items", ComplexCount == 2);

	
		int32 Filter1Count = 0;
		float Sum = 0.0f;
		Buffer.ForEach<FTestStructSimple>([&Filter1Count, &Sum](FTestStructSimple& Item)
		{
			Sum += Item.Float;
			Filter1Count++;
		});
		AITEST_TRUE("Should have 3 items with Filter1", Filter1Count == 3);
		AITEST_TRUE("Should have sum of 6", FMath::IsNearlyEqual(Sum, 6.0f));

		TArray<const UScriptStruct*> Filter2 { FTestStructSimple::StaticStruct(), FTestStructSimple1::StaticStruct() };
		int32 Filter2Count = 0;
		Buffer.ForEachFiltered(Filter2, [&Filter2Count](FStructView Item)
		{
			Filter2Count++;
		});
		AITEST_TRUE("Should have 4 items with Filter2", Filter2Count == 4);

		
		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FStructUtilsTest_ChunkedStructBufferForEach, "System.StructUtils.ChunkedStructBuffer.ForEach");



PRAGMA_ENABLE_OPTIMIZATION

#undef LOCTEXT_NAMESPACE
