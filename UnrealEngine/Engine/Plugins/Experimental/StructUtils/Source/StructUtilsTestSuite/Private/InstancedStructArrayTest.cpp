// Copyright Epic Games, Inc. All Rights Reserved.

#include "AITestsCommon.h"
#include "InstancedStructArray.h"
#include "StructUtilsTestTypes.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"

#define LOCTEXT_NAMESPACE "StructUtilsTests"

PRAGMA_DISABLE_OPTIMIZATION

namespace FInstancedStructArrayTest
{

struct FTest_BasicInstancedStructArray : FAITestBase
{
	virtual bool InstantTest() override
	{
		FInstancedStructArray Array;

		TArray<FConstStructView> TestStructs;
		TestStructs.Add(TBaseStructure<FTestStructSimple>::Get());
		TestStructs.Add(TBaseStructure<FVector>::Get());
		TestStructs.Add(TBaseStructure<FTestStructComplex>::Get());
		
		Array.Append(TestStructs);

		Array[1].GetMutable<FVector>().X = 42.0;

		AITEST_TRUE(TEXT("Should have 3 items"), Array.Num() == 3);
		AITEST_TRUE(TEXT("Item 0 should be FTestStructSimple"), Array[0].GetScriptStruct() == TBaseStructure<FTestStructSimple>::Get());
		AITEST_TRUE(TEXT("Item 1 should be FVector"), Array[1].GetScriptStruct() == TBaseStructure<FVector>::Get());
		AITEST_TRUE(TEXT("Item 2 should be FTestStructComplex"), Array[2].GetScriptStruct() == TBaseStructure<FTestStructComplex>::Get());
		AITEST_TRUE(TEXT("Item 1 should have X == 42.0"), FMath::IsNearlyEqual(Array[1].GetMutable<FVector>().X, 42.0));

		Array[2].GetMutable<FTestStructComplex>().StringArray.Add(TEXT("Foo"));
		
		TArray<FInstancedStruct> TestInstanced;
		TestInstanced.AddDefaulted_GetRef().InitializeAs<FTransform>();
		TestInstanced.AddDefaulted_GetRef().InitializeAs<FVector>(1,2,3);
		Array.Append(TestInstanced);

		AITEST_TRUE(TEXT("Should have 5 items"), Array.Num() == 5);
		AITEST_TRUE(TEXT("Item 4 should have Z == 3.0"), FMath::IsNearlyEqual(Array[4].GetMutable<FVector>().Z, 3.0));
		AITEST_TRUE(TEXT("Item 2 should have text Foo"), Array[2].GetMutable<FTestStructComplex>().StringArray[0] == TEXT("Foo"));

		Array.RemoveAt(2, 1);
		AITEST_TRUE(TEXT("Should have 4 items"), Array.Num() == 4);
		AITEST_TRUE(TEXT("Item 2 should be FTransform"), Array[2].GetScriptStruct() == TBaseStructure<FTransform>::Get());
		AITEST_TRUE(TEXT("Item 3 should be FVector"), Array[3].GetScriptStruct() == TBaseStructure<FVector>::Get());
		AITEST_TRUE(TEXT("Item 3 should have Z == 3.0"), FMath::IsNearlyEqual(Array[3].GetMutable<FVector>().Z, 3.0));

		Array.SetNum(2);
		AITEST_TRUE(TEXT("Should have 2 items"), Array.Num() == 2);

		FInstancedStructArray Array2;
		Array2.Append({ TBaseStructure<FQuat>::Get(), TBaseStructure<FTestStructSimple1>::Get() });
		Array.InsertAt(1, Array2);
		AITEST_TRUE(TEXT("Should have 4 items"), Array.Num() == 4);
		AITEST_TRUE(TEXT("Item 0 should be FTestStructSimple"), Array[0].GetScriptStruct() == TBaseStructure<FTestStructSimple>::Get());
		AITEST_TRUE(TEXT("Item 1 should be FQuat"), Array[1].GetScriptStruct() == TBaseStructure<FQuat>::Get());
		AITEST_TRUE(TEXT("Item 2 should be FTestStructSimple1"), Array[2].GetScriptStruct() == TBaseStructure<FTestStructSimple1>::Get());
		AITEST_TRUE(TEXT("Item 3 should be FVector"), Array[3].GetScriptStruct() == TBaseStructure<FVector>::Get());
		AITEST_TRUE(TEXT("Item 3 should have X == 42.0"), FMath::IsNearlyEqual(Array[3].GetMutable<FVector>().X, 42.0));

		Array.RemoveAt(0, 3);
		AITEST_TRUE(TEXT("Should have 1 item"), Array.Num() == 1);
		AITEST_TRUE(TEXT("Item 0 should be FVector"), Array[0].GetScriptStruct() == TBaseStructure<FVector>::Get());
		AITEST_TRUE(TEXT("Item 0 should have X == 42.0"), FMath::IsNearlyEqual(Array[0].GetMutable<FVector>().X, 42.0));

		Array.InsertAt(0, Array2);
		AITEST_TRUE(TEXT("Should have 1 item"), Array.Num() == 3);
		AITEST_TRUE(TEXT("Item 2 should have X == 42.0"), FMath::IsNearlyEqual(Array[2].GetMutable<FVector>().X, 42.0));

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FTest_BasicInstancedStructArray, "System.StructUtils.InstancedStructArray.Basic");

struct FTest_SerializeInstancedStructArray : FAITestBase
{
	virtual bool InstantTest() override
	{
		FInstancedStructArray Array;

		TArray<FInstancedStruct> TestInstanced;
		TestInstanced.AddDefaulted_GetRef().InitializeAs<FTransform>();
		TestInstanced.AddDefaulted_GetRef().InitializeAs<FVector>(1,2,3);
		TestInstanced.AddDefaulted_GetRef().InitializeAs<FTestStructComplex>();
		Array.Append(TestInstanced);

		Array[2].GetMutable<FTestStructComplex>().StringArray.Add(TEXT("Foo"));

		AITEST_TRUE(TEXT("Should have 3 items"), Array.Num() == 3);
		AITEST_TRUE(TEXT("Item 2 should have text Foo"), Array[2].GetMutable<FTestStructComplex>().StringArray[0] == TEXT("Foo"));

		TArray<uint8> Memory;
		
		FMemoryWriter Writer(Memory);
		FObjectAndNameAsStringProxyArchive WriterProxy(Writer, /*bInLoadIfFindFails*/false);
		const bool bSaveResult = Array.Serialize(WriterProxy);
		AITEST_TRUE(TEXT("Saving should succeed"), bSaveResult);

		FMemoryReader Reader(Memory);
		FObjectAndNameAsStringProxyArchive ReaderProxy(Reader, /*bInLoadIfFindFails*/true);
		const bool bLoadResult = Array.Serialize(ReaderProxy);
		AITEST_TRUE(TEXT("Loading to same array should succeed"), bLoadResult);

		FInstancedStructArray Array2;
		FMemoryReader Reader2(Memory);
		FObjectAndNameAsStringProxyArchive ReaderProxy2(Reader2, /*bInLoadIfFindFails*/true);
		const bool bLoadResult2 = Array2.Serialize(ReaderProxy2);
		AITEST_TRUE(TEXT("Loading to Array2 should succeed"), bLoadResult2);
		AITEST_TRUE(TEXT("Array2 should have 3 items"), Array2.Num() == 3);
		AITEST_TRUE(TEXT("Array2 item 2 should have text Foo"), Array2[2].GetMutable<FTestStructComplex>().StringArray[0] == TEXT("Foo"));

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FTest_SerializeInstancedStructArray, "System.StructUtils.InstancedStructArray.Serialize");

} // FInstancedStructArrayTest

PRAGMA_ENABLE_OPTIMIZATION

#undef LOCTEXT_NAMESPACE
