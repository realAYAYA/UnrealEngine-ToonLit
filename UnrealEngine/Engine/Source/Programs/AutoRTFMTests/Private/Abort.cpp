// Copyright Epic Games, Inc. All Rights Reserved.

#pragma autortfm

#include "Catch2Includes.h"
#include <AutoRTFM/AutoRTFM.h>
#include <map>
#include <vector>

TEST_CASE("Abort")
{
    int x = 42;
    std::vector<int> v;
    std::map<int, std::vector<int>> m;
    v.push_back(100);
    m[1].push_back(2);
    m[1].push_back(3);
    m[4].push_back(5);
    m[6].push_back(7);
    m[6].push_back(8);
    m[6].push_back(9);

	auto transaction = AutoRTFM::Transact([&]()
    {
		x = 5;
    	for (size_t n = 10; n--;)
    		v.push_back(2 * n);
    	m.clear();
    	m[10].push_back(11);
    	m[12].push_back(13);
    	m[12].push_back(14);
    	AutoRTFM::AbortTransaction();
	});

    REQUIRE(
        AutoRTFM::ETransactionResult::AbortedByRequest ==
		transaction);
    REQUIRE(x == 42);
    REQUIRE(v.size() == 1);
    REQUIRE(v[0] == 100);
    REQUIRE(m.size() == 3);
    REQUIRE(m[1].size() == 2);
    REQUIRE(m[1][0] == 2);
    REQUIRE(m[1][1] == 3);
    REQUIRE(m[4].size() == 1);
    REQUIRE(m[4][0] == 5);
    REQUIRE(m[6].size() == 3);
    REQUIRE(m[6][0] == 7);
    REQUIRE(m[6][1] == 8);
    REQUIRE(m[6][2] == 9);
}

TEST_CASE("Abort.NestedAbortOrder")
{
	AutoRTFM::ETransactionResult InnerResult;
	unsigned Orderer = 0;

	AutoRTFM::Commit([&]
	{
		InnerResult = AutoRTFM::Transact([&]
			{
				AutoRTFM::OnAbort([&]
					{
						REQUIRE(1 == Orderer);
						Orderer += 1;
					});

				AutoRTFM::OnAbort([&]
					{
						REQUIRE(0 == Orderer);
						Orderer += 1;
					});

				AutoRTFM::AbortTransaction();
			});
	});

	REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == InnerResult);
	REQUIRE(2 == Orderer);
}

TEST_CASE("Abort.TransactionInOpenCommit")
{
	AutoRTFM::ETransactionResult InnerResult;

	AutoRTFM::Commit([&]
	{
		AutoRTFM::OnCommit([&]
		{
			bool bDidSomething = false;

			InnerResult = AutoRTFM::Transact([&]
			{
				bDidSomething = true;
			});

			REQUIRE(false == bDidSomething);
		});
	});

	REQUIRE(AutoRTFM::ETransactionResult::AbortedByTransactInOnCommit == InnerResult);
}

TEST_CASE("Abort.TransactionInOpenAbort")
{
	AutoRTFM::ETransactionResult Result;
	AutoRTFM::ETransactionResult InnerResult;

	Result = AutoRTFM::Transact([&]
	{
		AutoRTFM::OnAbort([&]
		{
			bool bDidSomething = false;

			InnerResult = AutoRTFM::Transact([&]
			{
				bDidSomething = true;
			});

			REQUIRE(false == bDidSomething);
		});

		AutoRTFM::AbortTransaction();
	});

	REQUIRE(AutoRTFM::ETransactionResult::AbortedByTransactInOnAbort == InnerResult);
	REQUIRE(AutoRTFM::ETransactionResult::AbortedByRequest == Result);
}

TEST_CASE("Abort.Cascade")
{
	bool bTouched = false;

	const AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
	{
		bTouched = true;
		AutoRTFM::Transact([&]
		{
			AutoRTFM::CascadingAbortTransaction();
		});
	});

	REQUIRE(AutoRTFM::ETransactionResult::AbortedByCascade == Result);
	REQUIRE(false == bTouched);
}

TEST_CASE("Abort.CascadeThroughOpen")
{
	bool bTouched = false;

	const AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
	{
		bTouched = true;

		AutoRTFM::Open([&]
		{
			const AutoRTFM::EContextStatus Status = AutoRTFM::Close([&]
			{
				AutoRTFM::Transact([&]
				{
					AutoRTFM::CascadingAbortTransaction();
				});
			});

			REQUIRE(AutoRTFM::EContextStatus::AbortedByCascade == Status);
		});
	});

	REQUIRE(AutoRTFM::ETransactionResult::AbortedByCascade == Result);
	REQUIRE(false == bTouched);
}

TEST_CASE("Abort.CascadeThroughManualTransaction")
{
	bool bTouched = false;

	const AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
	{
		bTouched = true;

		AutoRTFM::Open([&]
		{
			REQUIRE(true == AutoRTFM::ForTheRuntime::StartTransaction());

			const AutoRTFM::EContextStatus Status = AutoRTFM::Close([&]
			{
				AutoRTFM::CascadingAbortTransaction();
			});

			REQUIRE(AutoRTFM::EContextStatus::AbortedByCascade == Status);

			// We need to clear the status ourselves.
			AutoRTFM::ForTheRuntime::ClearTransactionStatus();

			// Before manually starting the cascade again.
			AutoRTFM::CascadingAbortTransaction();
		});
	});

	REQUIRE(AutoRTFM::ETransactionResult::AbortedByCascade == Result);
	REQUIRE(false == bTouched);
}
