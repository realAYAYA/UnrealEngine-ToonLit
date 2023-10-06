// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreTypes.h"
#include "MovieSceneFwd.h"
#include "Misc/AutomationTest.h"
#include "Containers/SparseBitSet.h"
#include "Math/RandomStream.h"
#include "Misc/GeneratedTypeName.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UE::MovieScene::Tests
{

	template<typename SparseBitSet>
	void TestBitSet(FAutomationTestBase* Test, SparseBitSet& InBitSet)
	{
		// Set random bits in the bitset
		const int32 MaxBitIndex = static_cast<int32>(InBitSet.GetMaxNumBits());

		// Static int for re-running tests using the same seed as a previous run
		static int32 SeedOverride = 0;
		const int32 InitialSeed = SeedOverride == 0 ? FMath::Rand() : SeedOverride;
		FRandomStream Random(InitialSeed);

		UE_LOG(LogMovieScene, Display, TEXT("Running TSparseBitSet<> tests for %s with a random seed %i..."), GetGeneratedTypeName<SparseBitSet>(), InitialSeed);

		TSet<uint32> SetIndices;
		const int32 Num = Random.RandHelper(MaxBitIndex);

		for (int32 Index = 0; Index < Num; ++Index)
		{
			const uint32 Bit = static_cast<uint32>(Random.RandHelper(MaxBitIndex));
			InBitSet.SetBit(Bit);
			SetIndices.Add(Bit);
		}

		Test->TestEqual(TEXT("Num Set Bits"), InBitSet.CountSetBits(), static_cast<uint32>(SetIndices.Num()));

		// Test all bits are the correct state
		for (uint32 BitIndex = 0; BitIndex < static_cast<uint32>(MaxBitIndex); ++BitIndex)
		{
			Test->TestEqual(FString::Printf(TEXT("Bit index %d"), BitIndex), InBitSet.IsBitSet(BitIndex), SetIndices.Contains(BitIndex));
		}

		// Test the iterator
		int32 NumIterated = 0;
		for (int32 BitIndex : InBitSet)
		{
			++NumIterated;
			if (!SetIndices.Contains(BitIndex))
			{
				Test->AddError(FString::Printf(TEXT("Bit %i was iterated but it shouldn't be set!"), BitIndex));
			}
		}

		Test->TestEqual(TEXT("Number of iterated bits"), NumIterated, SetIndices.Num());
	}

} // namespace UE::MovieScene::Tests

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMovieSceneSparseBitSetTest, 
		"System.Engine.Sequencer.SparseBitSet", 
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMovieSceneSparseBitSetTest::RunTest(const FString& Parameters)
{
	using namespace UE::MovieScene;
	using namespace UE::MovieScene::Tests;

	{
		// 8 bit buckets
		TSparseBitSet<uint8, TDynamicSparseBitSetBucketStorage<uint8, 4>> BitSet8_8;
		TestEqual(TEXT("TSparseBitSet<uint8, ...<uint8>>::GetMaxNumBits"), BitSet8_8.GetMaxNumBits(), 8u*8u);
		TestBitSet(this, BitSet8_8);

		TSparseBitSet<uint16, TDynamicSparseBitSetBucketStorage<uint8, 4>> BitSet16_8;
		TestEqual(TEXT("TSparseBitSet<uint16, ...<uint8>>::GetMaxNumBits"), BitSet16_8.GetMaxNumBits(), 16u*8u);
		TestBitSet(this, BitSet16_8);

		TSparseBitSet<uint32, TDynamicSparseBitSetBucketStorage<uint8, 4>> BitSet32_8;
		TestEqual(TEXT("TSparseBitSet<uint32, ...<uint8>>::GetMaxNumBits"), BitSet32_8.GetMaxNumBits(), 32u*8u);
		TestBitSet(this, BitSet32_8);

		TSparseBitSet<uint64, TDynamicSparseBitSetBucketStorage<uint8, 4>> BitSet64_8;
		TestEqual(TEXT("TSparseBitSet<uint64, ...<uint8>>::GetMaxNumBits"), BitSet64_8.GetMaxNumBits(), 64u*8u);
		TestBitSet(this, BitSet64_8);

		// 16 bit buckets
		TSparseBitSet<uint8, TDynamicSparseBitSetBucketStorage<uint16, 4>> BitSet8_16;
		TestEqual(TEXT("TSparseBitSet<uint8, ...<uint16>>::GetMaxNumBits"), BitSet8_16.GetMaxNumBits(), 8u*16u);
		TestBitSet(this, BitSet8_16);

		TSparseBitSet<uint16, TDynamicSparseBitSetBucketStorage<uint16, 4>> BitSet16_16;
		TestEqual(TEXT("TSparseBitSet<uint16, ...<uint16>>::GetMaxNumBits"), BitSet16_16.GetMaxNumBits(), 16u*16u);
		TestBitSet(this, BitSet16_16);

		TSparseBitSet<uint32, TDynamicSparseBitSetBucketStorage<uint16, 4>> BitSet32_16;
		TestEqual(TEXT("TSparseBitSet<uint32, ...<uint16>>::GetMaxNumBits"), BitSet32_16.GetMaxNumBits(), 32u*16u);
		TestBitSet(this, BitSet32_16);

		TSparseBitSet<uint64, TDynamicSparseBitSetBucketStorage<uint16, 4>> BitSet64_16;
		TestEqual(TEXT("TSparseBitSet<uint64, ...<uint16>>::GetMaxNumBits"), BitSet64_16.GetMaxNumBits(), 64u*16u);
		TestBitSet(this, BitSet64_16);

		// 32 bit buckets
		TSparseBitSet<uint8, TDynamicSparseBitSetBucketStorage<uint32, 4>> BitSet8_32;
		TestEqual(TEXT("TSparseBitSet<uint8, ...<uint32>>::GetMaxNumBits"), BitSet8_32.GetMaxNumBits(), 8u*32u);
		TestBitSet(this, BitSet8_32);

		TSparseBitSet<uint16, TDynamicSparseBitSetBucketStorage<uint32, 4>> BitSet16_32;
		TestEqual(TEXT("TSparseBitSet<uint16, ...<uint32>>::GetMaxNumBits"), BitSet16_32.GetMaxNumBits(), 16u*32u);
		TestBitSet(this, BitSet16_32);

		TSparseBitSet<uint32, TDynamicSparseBitSetBucketStorage<uint32, 4>> BitSet32_32;
		TestEqual(TEXT("TSparseBitSet<uint32, ...<uint32>>::GetMaxNumBits"), BitSet32_32.GetMaxNumBits(), 32u*32u);
		TestBitSet(this, BitSet32_32);

		TSparseBitSet<uint64, TDynamicSparseBitSetBucketStorage<uint32, 4>> BitSet64_32;
		TestEqual(TEXT("TSparseBitSet<uint64, ...<uint32>>::GetMaxNumBits"), BitSet64_32.GetMaxNumBits(), 64u*32u);
		TestBitSet(this, BitSet64_32);

		// 64 bit buckets
		TSparseBitSet<uint8, TDynamicSparseBitSetBucketStorage<uint64, 4>> BitSet8_64;
		TestEqual(TEXT("TSparseBitSet<uint8, ...<uint64>>::GetMaxNumBits"), BitSet8_64.GetMaxNumBits(), 8u*64u);
		TestBitSet(this, BitSet8_64);

		TSparseBitSet<uint16, TDynamicSparseBitSetBucketStorage<uint64, 4>> BitSet16_64;
		TestEqual(TEXT("TSparseBitSet<uint16, ...<uint64>>::GetMaxNumBits"), BitSet16_64.GetMaxNumBits(), 16u*64u);
		TestBitSet(this, BitSet16_64);

		TSparseBitSet<uint32, TDynamicSparseBitSetBucketStorage<uint64, 4>> BitSet32_64;
		TestEqual(TEXT("TSparseBitSet<uint32, ...<uint64>>::GetMaxNumBits"), BitSet32_64.GetMaxNumBits(), 32u*64u);
		TestBitSet(this, BitSet32_64);

		TSparseBitSet<uint64, TDynamicSparseBitSetBucketStorage<uint64, 4>> BitSet64_64;
		TestEqual(TEXT("TSparseBitSet<uint64, ...<uint64>>::GetMaxNumBits"), BitSet64_64.GetMaxNumBits(), 64u*64u);
		TestBitSet(this, BitSet64_64);


	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
