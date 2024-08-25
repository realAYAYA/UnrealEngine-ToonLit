// Copyright Epic Games, Inc. All Rights Reserved.

#pragma autortfm

#include "Catch2Includes.h"
#include <AutoRTFM/AutoRTFM.h>

#include <map>
#include <vector>

TEST_CASE("OpenAPI.StartAbortAndStartAgain")
{
	int valueB = 0;
	int valueC = 0;
	AutoRTFM::Transact([&]()
	{
		// Recorded valueB as starting at 0.
		valueB = 20;

		AutoRTFM::Open([&]()
		{
			AutoRTFM::ForTheRuntime::StartTransaction();
			AutoRTFM::ForTheRuntime::RecordOpenWrite(&valueB);
			valueB=10;
			AutoRTFM::AbortTransaction();

			AutoRTFM::ForTheRuntime::ClearTransactionStatus();

			AutoRTFM::ForTheRuntime::StartTransaction();
			AutoRTFM::ForTheRuntime::RecordOpenWrite(&valueC);
			valueC = 30;
			AutoRTFM::AbortTransaction();
		});
	});

	// We rollback the transaction to the value we had when we first recorded the address
	REQUIRE(valueB == 20);
	REQUIRE(valueC == 0);
}

TEST_CASE("OpenAPI.CommitScopedFromOpen_Illegal", "[.]")
{
	AutoRTFM::ETransactionResult transactResult = AutoRTFM::Transact([&]()
	{
		AutoRTFM::Open([&]()
		{
			AutoRTFM::ForTheRuntime::CommitTransaction(); // illegal. Can't Commit from within a scoped transaction
		});
	});
	REQUIRE(transactResult == AutoRTFM::ETransactionResult::AbortedByRequest);
}

TEST_CASE("OpenAPI.RecordDataClosed_Illegal", "[.]")
{
	int value = 0;
	AutoRTFM::ETransactionResult transactResult = AutoRTFM::Transact([&]()
	{
		REQUIRE(AutoRTFM::EContextStatus::OnTrack == AutoRTFM::Close([&]()
		{
			AutoRTFM::ForTheRuntime::RecordOpenWrite(&value); // Illegal. Can't record writes explicitly while closed
			value = 1;
		}));
	});

	REQUIRE(transactResult == AutoRTFM::ETransactionResult::Committed);
	REQUIRE(value == 1);
}

TEST_CASE("OpenAPI.WriteDataInTheOpen")
{
	int value = 0;
	auto transactResult = AutoRTFM::Transact([&]()
	{
		AutoRTFM::Open([&]()
		{
			AutoRTFM::ForTheRuntime::RecordOpenWrite(&value);
			value = 1;
		});
	});

	REQUIRE(transactResult == AutoRTFM::ETransactionResult::Committed);
	REQUIRE(value == 1);
}

TEST_CASE("OpenAPI.AbortTransactionScopedFromOpen")
{
	auto transactResult = AutoRTFM::Transact([&]()
	{
		AutoRTFM::Open([&]()
		{
			AutoRTFM::AbortTransaction();
		});
		FAIL("AutoRTFM::Open failed to throw after an abort");
	});
	REQUIRE(transactResult == AutoRTFM::ETransactionResult::AbortedByRequest);
}

TEST_CASE("OpenAPI.AbortTransactionScopedFromClosed")
{
	auto transactResult = AutoRTFM::Transact([&]()
	{
		REQUIRE(AutoRTFM::EContextStatus::OnTrack == AutoRTFM::Close([&]()
		{
			AutoRTFM::AbortTransaction();
		}));
		FAIL("AutoRTFM::Close should have no-op'ed because it's already closed from the Transact");
	});
	REQUIRE(transactResult == AutoRTFM::ETransactionResult::AbortedByRequest);
}

TEST_CASE("OpenAPI.AbortTransactionDoubleScopedFromOpen")
{
	unsigned long value = -42;
	auto transactResult = AutoRTFM::Transact([&]()
	{
		value = 42;

		auto transactResult2 = AutoRTFM::Transact([&]()
		{
			value = 42424242;
			AutoRTFM::Open([&]()
			{
				AutoRTFM::AbortTransaction();
			});
			FAIL("AutoRTFM::Open failed to throw after an abort");
			value = 24242424;
		});

		if (transactResult2 != AutoRTFM::ETransactionResult::AbortedByRequest) FAIL("transactResult2 != AutoRTFM::ETransactionResult::AbortedByRequest");
		if (value != 42) FAIL("value != 42");
		value = 123123123;
	});
	REQUIRE(transactResult == AutoRTFM::ETransactionResult::Committed);
	REQUIRE(value == 123123123);
}

TEST_CASE("OpenAPI.NestedClosedTransactions")
{
	int value = 0x12345678;
	auto transactResult = AutoRTFM::Transact([&]()
	{
		// Read value
		int x = value;
		value = 0x11111111;

		auto transactResult2 = AutoRTFM::Transact([&]()
		{
			if (value != 0x11111111) FAIL("value != 0x11111111");
			// Read value
			int y = value;
			value = 0x22222222;

			auto transactResult3 = AutoRTFM::Transact([&]()
			{
				if (value != 0x22222222) FAIL("value != 0x22222222");
				// Read value
				int z = value;
				value = 0x33333333;

				auto transactResult4 = AutoRTFM::Transact([&]()
				{
					if (value != 0x33333333) FAIL("value != 0x33333333");
					// Read value
					int q = value;
					value = 0x44444444;
					if (value != 0x44444444) FAIL("value != 0x44444444");
					if (q != 0x33333333) FAIL("q != 0x33333333");
				});
				(void)transactResult4;

				if (value != 0x44444444) FAIL("value != 0x44444444");
				if (z != 0x22222222) FAIL("z != 0x22222222");
			});
			(void)transactResult3;

			value = 0x55555555;
			if (value != 0x55555555) FAIL("value != 0x55555555");
			if (y != 0x11111111) FAIL("y != 0x11111111");
		});
		(void)transactResult2;
		if (value != 0x55555555) FAIL("value != 0x55555555");

		value = 0x66666666;
		if (value != 0x66666666) FAIL("value != 0x66666666");
		if (x != 0x12345678) FAIL("x != 0x12345678");
	});

	REQUIRE(transactResult == AutoRTFM::ETransactionResult::Committed);
	REQUIRE(value == 0x66666666);
}

TEST_CASE("OpenAPI.OpenWithCopy")
{
	struct SomeData_t
	{
		int A;
		float B;
		char C;
	};

	SomeData_t SomeData1{ 1,2.0,'3' };

	auto transactResult = AutoRTFM::Transact([&]()
	{
		SomeData_t SomeData2{ 9,8.0,'7' };
		SomeData1.A = 11;
		SomeData2.A = 29;

		AutoRTFM::Open([=]()
		{
			REQUIRE(SomeData1.A == 11);
			REQUIRE(SomeData2.A == 29);
		});
	});

	REQUIRE(transactResult == AutoRTFM::ETransactionResult::Committed);
}

#if defined(_BROKEN_ALLOC_FIXED_)

TEST_CASE("OpenAPI.OpenCloseOpenClose")
{
	// START OPEN
	REQUIRE(!AutoRTFM::IsTransactional());

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

	auto transactResult = AutoRTFM::Transact([&]()
	{
		// A - WE ARE CLOSED 
		if (!AutoRTFM::IsClosed()) FAIL("A - NOT CLOSED AS EXPECTED!");

		// -------------------------------------
		AutoRTFM::Open([&]()
		{
			// B - WE ARE OPEN 
			REQUIRE(!AutoRTFM::IsClosed());

			// -------------------------------------
			REQUIRE(AutoRTFM::EContextStatus::OnTrack == AutoRTFM::Close([&]()
			{
				// C - WE ARE CLOSED AGAIN
				if (!AutoRTFM::IsClosed()) FAIL("C - NOT CLOSED AS EXPECTED!");

				// -------------------------------------
				AutoRTFM::Open([&]()
				{
					// D - WE ARE OPEN AGAIN
					REQUIRE(!AutoRTFM::IsClosed());

					// An abort here will set state on the transactions, but will not LongJump
					// AutoRTFM::Abort();
				});

				// -------------------------------------
				// E - BACK TO CLOSED AFTER AN OPEN

				x = 5;
				for (size_t n = 10; n--;)
					v.push_back(2 * n);
				m.clear();
				m[10].push_back(11);
				m[12].push_back(13);
				m[12].push_back(14);

				// An abort here is closed and will LongJump past F AND G all the way to H
				AutoRTFM::AbortTransaction();

				// -------------------------------------
				AutoRTFM::Open([&]()
				{
					// F - WE ARE OPEN AGAIN //
					REQUIRE(!AutoRTFM::IsClosed());
				});

				// -------------------------------------
				// G - BACK TO CLOSED AGAIN
				if (!AutoRTFM::IsClosed()) FAIL("NOT CLOSED!");

			}));
			// -------------------------------------
			// H - BACK TO OPEN 
			REQUIRE(!AutoRTFM::IsClosed());

		});

		// -------------------------------------
		// I - Finally closed again to finish out the transaction
		if (!AutoRTFM::IsClosed()) FAIL("I - NOT CLOSED AS EXPECTED!");
	});

	REQUIRE(
		AutoRTFM::ETransactionResult::AbortedByRequest ==
		transactResult);
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
	REQUIRE(!AutoRTFM::IsTransactional());
}

#endif

TEST_CASE("OpenAPI.Commit_TransactOpenCloseCommit")
{
	REQUIRE(!AutoRTFM::IsTransactional());

	// We're open
	REQUIRE(!AutoRTFM::IsClosed());

	int value = 10;
	value++;

	// Close and start the top-level transaction
	AutoRTFM::Transact([&]()
	{
		if (!AutoRTFM::IsClosed()) FAIL("Not Closed");

		AutoRTFM::Open([&]()
		{
			AutoRTFM::ForTheRuntime::StartTransaction();

			REQUIRE(AutoRTFM::EContextStatus::OnTrack == AutoRTFM::Close([&]()
			{
				value = 42;
			}));

			REQUIRE(value == 42); // RTFM writes through immediately, so we can see this value in the open
			AutoRTFM::ForTheRuntime::CommitTransaction();
		});

		if (value != 42) FAIL("Value != 42!");

		value = 420;
	});

	REQUIRE(value == 420);

	REQUIRE(!AutoRTFM::IsTransactional());
}


TEST_CASE("OpenAPI.Commit_TransactOpenCloseAbort")
{
	REQUIRE(!AutoRTFM::IsTransactional());

	// We're open
	REQUIRE(!AutoRTFM::IsClosed());

	int value = 10;
	value++;

	// Close and start the top-level transaction
	AutoRTFM::Transact([&]()
	{
		if (!AutoRTFM::IsClosed()) FAIL("Not Closed");

		AutoRTFM::Open([&]()
		{
			double valueLocal = 1.0;
			AutoRTFM::ForTheRuntime::StartTransaction();

			// Closing from the open doesn't work
			REQUIRE(AutoRTFM::EContextStatus::OnTrack == AutoRTFM::Close([&]()
			{
				value = 42;
				valueLocal = 10.0;
			}));

			AutoRTFM::AbortTransaction(); // undoes value = 42 in the open
			REQUIRE(valueLocal == 1.0);
		});

		FAIL("Should not reach here!");
	});

	REQUIRE(value == 11);
	REQUIRE(!AutoRTFM::IsTransactional());
}

TEST_CASE("OpenAPI.DoubleTransact")
{
	double value = 1.0;

	AutoRTFM::Transact([&]()
	{
		AutoRTFM::Transact([&]()
		{
			value *= 2.5;
			AutoRTFM::AbortTransaction();
		});

		value *= 10.0;
	});

	REQUIRE(value == 10.0);
}

TEST_CASE("OpenAPI.DoubleTransact2")
{
	double value = 1.0;

	AutoRTFM::Transact([&]()
	{
		value = value + 2.0;
		AutoRTFM::Transact([&]()
		{
			if (value == 3.0)
			{
				value *= 2.5;
			}

			if (value == 7.5)
			AutoRTFM::AbortTransaction();
		});

		value *= 10.0;
	});

	REQUIRE(value == 30.0);
}

TEST_CASE("OpenAPI.DoubleTransact3")
{
	double result = 0.0;
	AutoRTFM::Transact([&]()
	{
		double value = 1.0;
		value = value + 2.0;
		AutoRTFM::Transact([&]()
		{
			if (value == 3.0)
			{
				value *= 2.5;
			}

			if (value == 7.5)
			AutoRTFM::AbortTransaction();
		});

		value *= 10.0;
		result = value;
	});

	REQUIRE(result == 30.0);
}

TEST_CASE("OpenAPI.StackWriteCommitInTheOpen1")
{
	int value = 0;
	AutoRTFM::Transact([&]()
	{
		AutoRTFM::Open([&]()
		{
			AutoRTFM::ForTheRuntime::StartTransaction();
			AutoRTFM::ForTheRuntime::RecordOpenWrite(&value);
			value = 10;
			AutoRTFM::ForTheRuntime::CommitTransaction();
			REQUIRE(value == 10);
		});
	});
}

TEST_CASE("OpenAPI.StackWriteCommitInTheOpen2")
{
	AutoRTFM::Transact([&]()
	{
		int value = 0;
		AutoRTFM::Open([&]()
		{
			AutoRTFM::ForTheRuntime::StartTransaction();
			REQUIRE(AutoRTFM::EContextStatus::OnTrack == AutoRTFM::Close([&]()
			{
				AutoRTFM::Open([&]()
				{
					AutoRTFM::ForTheRuntime::RecordOpenWrite(&value);
					value = 10;
				});
			}));

			AutoRTFM::ForTheRuntime::CommitTransaction();
			REQUIRE(value == 10);
		});
	});
}

TEST_CASE("OpenAPI.StackWriteAbortInTheOpen1")
{
	int value = 0;
	AutoRTFM::Transact([&]()
	{
		AutoRTFM::Open([&]()
		{
			AutoRTFM::ForTheRuntime::StartTransaction();
			AutoRTFM::ForTheRuntime::RecordOpenWrite(&value);
			value = 10;
			AutoRTFM::AbortTransaction();
			REQUIRE(value == 0);
		});
	});
}

#if defined(OPENAPI_ILLEGAL_TESTS)
TEST_CASE("OpenAPI.StackWriteCommitInTheOpen3_Illegal")
{
	AutoRTFM::Transact([&]()
	{
		int value = 0;
		AutoRTFM::Open([&]()
		{
			AutoRTFM::ForTheRuntime::StartTransaction();
			AutoRTFM::ForTheRuntime::WriteMemory(&value, 10);
			AutoRTFM::ForTheRuntime::CommitTransaction();
			REQUIRE(value == 10);
		});
	});
}
#endif

int value1 = 0;

TEST_CASE("OpenAPI.WriteMemory1")
{
	const int sourceValue = 10;
	AutoRTFM::Transact([&]()
	{
		AutoRTFM::Open([&]()
		{
			AutoRTFM::ForTheRuntime::StartTransaction();

			AutoRTFM::ForTheRuntime::WriteMemory(&value1, &sourceValue);
			AutoRTFM::ForTheRuntime::CommitTransaction();
			REQUIRE(value1 == 10);
		});
	});
}

TEST_CASE("OpenAPI.StackWriteAbortInTheOpen2")
{
	int value = 0;
	int bGotToA = false;
	AutoRTFM::Transact([&]()
	{
		AutoRTFM::Open([&]()
		{
			AutoRTFM::ForTheRuntime::StartTransaction();
			AutoRTFM::ForTheRuntime::WriteMemory(&value, 10); // Illegal to write to value because it's in the inner-most closed-nest

			AutoRTFM::AbortTransaction();
		});

		// Never gets here
		bGotToA = true;
		REQUIRE(value == 0);
	});

	REQUIRE(bGotToA == false);
	REQUIRE(value == 0);
}

TEST_CASE("OpenAPI.WriteTrivialStructure")
{
	struct SomeData
	{
		int A;
		double B;
		float C;
		char D;
		long E[5];
	};

	SomeData data = { 1,2.0, 3.0f, 'q', {123,234,345,456,567} };
	SomeData data2 = { 9,8.0, 7.0f, '^', {999,888,777,666,555} };

	AutoRTFM::Transact([&]()
	{
		AutoRTFM::Open([&]()
		{
			AutoRTFM::ForTheRuntime::StartTransaction();

			AutoRTFM::ForTheRuntime::WriteMemory(&data, &data2);
			REQUIRE(data.A == 9);
			REQUIRE(data.B == 8.0);
			REQUIRE(data.C == 7.0f);
			REQUIRE(data.D == '^');
			REQUIRE(data.E[0] == 999);
			REQUIRE(data.E[1] == 888);
			REQUIRE(data.E[2] == 777);
			REQUIRE(data.E[3] == 666);
			REQUIRE(data.E[4] == 555);
			
			AutoRTFM::AbortTransaction();
			REQUIRE(data.A == 1);
			REQUIRE(data.B == 2.0);
			REQUIRE(data.C == 3.0f);
			REQUIRE(data.D == 'q');
			REQUIRE(data.E[0] == 123);
			REQUIRE(data.E[1] == 234);
			REQUIRE(data.E[2] == 345);
			REQUIRE(data.E[3] == 456);
			REQUIRE(data.E[4] == 567);
		});
	});
}

TEST_CASE("OpenAPI.WriteTrivialStructure2")
{
	struct SomeData
	{
		int A;
		double B;
		float C;
		char D;
		long E[5];
	};

	SomeData data = { 1,2.0, 3.0f, 'q', {123,234,345,456,567} };
	SomeData data2 = { 9,8.0, 7.0f, '^', {999,888,777,666,555} };
	SomeData data3 = { 19,28.0, 37.0f, '@', {4999,5888,6777,7666,8555} };

	AutoRTFM::Transact([&]()
	{
		AutoRTFM::Open([&]()
			{
				AutoRTFM::ForTheRuntime::StartTransaction();

				AutoRTFM::ForTheRuntime::WriteMemory(&data, &data2);
				REQUIRE(data.A == 9);
				REQUIRE(data.B == 8.0);
				REQUIRE(data.C == 7.0f);
				REQUIRE(data.D == '^');
				REQUIRE(data.E[0] == 999);
				REQUIRE(data.E[1] == 888);
				REQUIRE(data.E[2] == 777);
				REQUIRE(data.E[3] == 666);
				REQUIRE(data.E[4] == 555);
					
				AutoRTFM::ForTheRuntime::WriteMemory(&data, &data3);
				REQUIRE(data.A == 19);
				REQUIRE(data.B == 28.0);
				REQUIRE(data.C == 37.0f);
				REQUIRE(data.D == '@');
				REQUIRE(data.E[0] == 4999);
				REQUIRE(data.E[1] == 5888);
				REQUIRE(data.E[2] == 6777);
				REQUIRE(data.E[3] == 7666);
				REQUIRE(data.E[4] == 8555);

				AutoRTFM::AbortTransaction();
				REQUIRE(data.A == 1);
				REQUIRE(data.B == 2.0);
				REQUIRE(data.C == 3.0f);
				REQUIRE(data.D == 'q');
				REQUIRE(data.E[0] == 123);
				REQUIRE(data.E[1] == 234);
				REQUIRE(data.E[2] == 345);
				REQUIRE(data.E[3] == 456);
				REQUIRE(data.E[4] == 567);
			});
	});
}

TEST_CASE("OpenAPI.Footgun1")
{
	int valueA = 0;
	int valueB = 0;

	AutoRTFM::Transact([&]()
	{
		// Does nothing - already closed
		REQUIRE(AutoRTFM::EContextStatus::OnTrack == AutoRTFM::Close([&]()
		{
			// Recorded valueB as starting at 0.
			valueB = 123;

			AutoRTFM::Open([&]()
			{
				// Unrecorded assignments in the open
				valueA = 10;
				valueB = 10;
			});
				
			// valueA is now recorded as starting at 10
			valueA = 20;
			AutoRTFM::AbortTransaction();
		}));
	});

	// We rollback the transaction to the value we had when we first recorded the address
	REQUIRE(valueA == 10);
	REQUIRE(valueB == 0);
}

TEST_CASE("OpenAPI.Footgun2")
{
	int valueB = 0;
	int valueC = 0;
	AutoRTFM::Transact([&]()
	{
		// Does nothing - already closed
		REQUIRE(AutoRTFM::EContextStatus::OnTrack == AutoRTFM::Close([&]()
		{
			// Recorded valueB as starting at 0.
			valueB = 20;

			AutoRTFM::Open([&]()
			{
				// Unrecorded assignments in the open
				valueB = 10;
				valueC = 10;
				AutoRTFM::ForTheRuntime::RecordOpenWrite(&valueC);
				// valueC was recorded in the open after the change - too late
			});

			// valueA is now recorded as starting at 10
			valueC = 40;
			AutoRTFM::AbortTransaction();
		}));
	});

	// We rollback the transaction to the value we had when we first recorded the address
	REQUIRE(valueB == 0);
	REQUIRE(valueC == 10);
}

#if 0
TEST_CASE("OpenAPI.StartCloseOpenCommit")
{
	REQUIRE(!AutoRTFM::IsTransactional());

	int value = 10;
	value++;
	(void)value;

	AutoRTFM::ForTheRuntime::StartTransaction();

	// Can't close outside of a transaction
	REQUIRE(AutoRTFM::EContextStatus::OnTrack == AutoRTFM::Close([&]()
	{
		value = 420;
	}));

	// assignment within the close should be visible to us
	REQUIRE(value == 420);

	// Setting a value in the open requires us to register the memory address with the transaction
	value = 42;
	AutoRTFM::ForTheRuntime::RecordOpenWrite(&value);

	AutoRTFM::ForTheRuntime::CommitTransaction();

	// Finally, 42 is committed to value
	REQUIRE(value == 42);

	REQUIRE(!AutoRTFM::IsTransactional());
}
#endif

TEST_CASE("OpenAPI.TransOpenStartCloseAbortAbort")
{
	bool bGetsToA = false;
	bool bGetsToB = false;
	bool bGetsToC = false;
	bool bGetsToD = false;

	REQUIRE(!AutoRTFM::IsTransactional());

	AutoRTFM::Transact([&]() 
	{
		AutoRTFM::Open([&]()
		{
			AutoRTFM::ForTheRuntime::StartTransaction();

			int value = 10;
			value++;

			value = 42;
			AutoRTFM::EContextStatus CloseStatus = AutoRTFM::Close([&]()
			{
				value = 420;
				AutoRTFM::AbortTransaction();
				bGetsToA = true;
			});

			REQUIRE(CloseStatus == AutoRTFM::EContextStatus::AbortedByRequest);

			AutoRTFM::ForTheRuntime::ClearTransactionStatus();

			REQUIRE(bGetsToA == false);

			bGetsToB = true;
			REQUIRE(value == 42);
			AutoRTFM::AbortTransaction();
			bGetsToC = true;
			REQUIRE(value == 42);
		});

		bGetsToD = true;
	});

	REQUIRE(bGetsToA == false);
	REQUIRE(bGetsToB == true);
	REQUIRE(bGetsToC == true);
	REQUIRE(bGetsToD == false);
	REQUIRE(!AutoRTFM::IsTransactional());
}

TEST_CASE("OpenAPI.TransOpenTransCloseAbortAbort")
{
	bool bGetsToA = false;
	bool bGetsToB = false;
	bool bGetsToC = false;
	bool bGetsToD = false;

	REQUIRE(!AutoRTFM::IsTransactional());

	AutoRTFM::Transact([&]() 
	{
		AutoRTFM::Open([&]()
		{
			AutoRTFM::Transact([&]()
			{
				int value = 10;
				value++;

				value = 42;
				// Can't close outside of a Transact
				REQUIRE(AutoRTFM::EContextStatus::OnTrack == AutoRTFM::Close([&]()
				{
					value = 420;
					AutoRTFM::AbortTransaction();
					bGetsToA = true;
				}));

				REQUIRE(bGetsToA == false);

				bGetsToB = true;
				REQUIRE(value == 42);
				AutoRTFM::AbortTransaction();
				bGetsToC = true;
				REQUIRE(value == 42);
			});
		});

		bGetsToD = true;
	});

	REQUIRE(bGetsToA == false);
	REQUIRE(bGetsToB == false);
	REQUIRE(bGetsToC == false);
	REQUIRE(bGetsToD == true);
	REQUIRE(!AutoRTFM::IsTransactional());
}
