// Copyright Epic Games, Inc. All Rights Reserved.

#pragma autortfm

#include "Catch2Includes.h"
#include <AutoRTFM/AutoRTFM.h>
#include <thread>
#include <vector>

struct MyStruct final {
	uint8_t u8;
	uint64_t u64;

	void DoSomething() {
		u8++;
		u64++;
	}
};

TEST_CASE("benchmarks.transaction_cost")
{
#if defined(NDEBUG)
	auto Sizes = {1, 100, 1000, 10000};
#else
	auto Sizes = {1, 100};
#endif

	for (const unsigned Size : Sizes)
	{
		std::vector<MyStruct> Datas(Size);

		BENCHMARK("open " + std::to_string(Size))
		{
			for (unsigned i = 0; i < Size; i++)
			{
				Datas[i].DoSomething();
			}
		};

		BENCHMARK("one transaction " + std::to_string(Size))
		{
			AutoRTFM::Transact([&] ()
			{
				for (unsigned i = 0; i < Size; i++)
				{
					Datas[i].DoSomething();
				}
			});
		};

		BENCHMARK("many transactions " + std::to_string(Size))
		{
			for (unsigned i = 0; i < Size; i++)
			{
				AutoRTFM::Transact([&] ()
				{
					Datas[i].DoSomething();
				});
			}
		};
	}
}
