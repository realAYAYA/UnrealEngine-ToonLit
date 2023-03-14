// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_LOW_LEVEL_TESTS

#include "Algo/AllOf.h"
#include "DerivedDataCacheStore.h"
#include "DerivedDataLegacyCacheStore.h"
#include "DerivedDataRequestOwner.h"
#include "TestCacheStore.h"
#include "TestHarness.h"

namespace UE::DerivedData
{

class IMemoryCacheStore;
ILegacyCacheStore* CreateCacheStoreHierarchy(ICacheStoreOwner*& OutOwner, IMemoryCacheStore* MemoryCache);

static FValue CreateHierarchyTestValue(uint32 Value)
{
	return FValue::Compress(FSharedBuffer::MakeView(MakeMemoryView<uint32>({Value})));
}

TEST_CASE("DerivedData::Cache::Hierarchy::PartialRecordPropagation", "[DerivedData]")
{
	ICacheStoreOwner* StoreOwner = nullptr;
	TUniquePtr<ILegacyCacheStore> Hierarchy(CreateCacheStoreHierarchy(StoreOwner, nullptr));

	TUniquePtr<ITestCacheStore> Store0(CreateTestCacheStore(ECacheStoreFlags::Local | ECacheStoreFlags::Query | ECacheStoreFlags::Store, /*bAsync*/ true));
	StoreOwner->Add(Store0.Get(), Store0->GetFlags());
	TUniquePtr<ITestCacheStore> Store1(CreateTestCacheStore(ECacheStoreFlags::Local | ECacheStoreFlags::Query | ECacheStoreFlags::Store, /*bAsync*/ true));
	StoreOwner->Add(Store1.Get(), Store1->GetFlags());
	TUniquePtr<ITestCacheStore> Store2(CreateTestCacheStore(ECacheStoreFlags::Local | ECacheStoreFlags::Query | ECacheStoreFlags::Store, /*bAsync*/ true));
	StoreOwner->Add(Store2.Get(), Store2->GetFlags());

	const FValueId Id0 = FValueId::FromName("0");
	const FValueId Id1 = FValueId::FromName("1");
	const FValueId Id2 = FValueId::FromName("2");
	const FValueWithId Value0(Id0, CreateHierarchyTestValue(0));
	const FValueWithId Value1(Id1, CreateHierarchyTestValue(1));
	const FValueWithId Value2(Id2, CreateHierarchyTestValue(2));
	const FCacheKey Key{FCacheBucket("Test"), FIoHash::HashBuffer(MakeMemoryView<uint8>({0, 1, 2}))};

	Store0->AddRecord(Key, {Value0, Value1.RemoveData(), Value2.RemoveData()});
	Store1->AddRecord(Key, {Value0, Value1, Value2.RemoveData()});
	Store2->AddRecord(Key, {Value0, Value1, Value2});

	FCacheRecordPolicyBuilder PolicyBuilder(ECachePolicy::Default);
	PolicyBuilder.AddValuePolicy(Id0, ECachePolicy::Local);

	FRequestOwner Owner(EPriority::Normal);
	Hierarchy->Get({{{TEXTVIEW("PropagatePartial")}, Key, PolicyBuilder.Build(), 12345}}, Owner, [](FCacheGetResponse&& Response)
	{
		CHECK(Response.UserData == 12345);
		CHECK(Response.Status == EStatus::Ok);
		CHECK(Response.Record.GetValues().Num() == 3);
		CHECK(Algo::AllOf(Response.Record.GetValues(), &FValue::HasData));
	});

	// Check that every value is requested from Store0.
	CHECK(Store0->GetTotalRequestCount() == 1);
	CHECK(Store1->GetTotalRequestCount() == 0);
	CHECK(Store2->GetTotalRequestCount() == 0);
	REQUIRE(Store0->GetGetRequests().Num() == 1);
	const FCacheGetRequest& Request0 = Store0->GetGetRequests()[0];
	CHECK(Request0.Policy.GetRecordPolicy() == (ECachePolicy::Default | ECachePolicy::PartialRecord));
	CHECK(Request0.Policy.GetValuePolicy(Id0) == ECachePolicy::Local);
	CHECK(Request0.Policy.GetValuePolicies().Num() == 1);

	Store0->ExecuteAsync();

	// Check that every value except Value0 is requested from Store1.
	CHECK(Store0->GetTotalRequestCount() == 1);
	CHECK(Store1->GetTotalRequestCount() == 1);
	CHECK(Store2->GetTotalRequestCount() == 0);
	REQUIRE(Store1->GetGetRequests().Num() == 1);
	const FCacheGetRequest& Request1 = Store1->GetGetRequests()[0];
	CHECK(Request1.Policy.GetRecordPolicy() == (ECachePolicy::Default | ECachePolicy::PartialRecord));
	CHECK(Request1.Policy.GetValuePolicy(Id0) == ECachePolicy::None);
	CHECK(Request1.Policy.GetValuePolicies().Num() == 1);

	Store1->ExecuteAsync();

	// Check that every value except Value0 and Value1 is requested from Store2.
	CHECK(Store0->GetTotalRequestCount() == 1);
	CHECK(Store1->GetTotalRequestCount() == 1);
	CHECK(Store2->GetTotalRequestCount() == 1);
	REQUIRE(Store2->GetGetRequests().Num() == 1);
	const FCacheGetRequest& Request2 = Store2->GetGetRequests()[0];
	CHECK(Request2.Policy.GetRecordPolicy() == ECachePolicy::Default);
	CHECK(Request2.Policy.GetValuePolicy(Id0) == ECachePolicy::None);
	CHECK(Request2.Policy.GetValuePolicy(Id1) == ECachePolicy::None);
	CHECK(Request2.Policy.GetValuePolicies().Num() == 2);

	Store2->ExecuteAsync();

	// Check that the complete record is put to Store0.
	CHECK(Store0->GetTotalRequestCount() == 2);
	REQUIRE(Store0->GetPutRequests().Num() == 1);
	const FCachePutRequest& PutRequest0 = Store0->GetPutRequests()[0];
	CHECK(PutRequest0.Policy.GetRecordPolicy() == ECachePolicy::Store);
	CHECK(PutRequest0.Policy.GetValuePolicy(Id0) == ECachePolicy::StoreLocal);
	CHECK(PutRequest0.Policy.GetValuePolicies().Num() == 1);
	CHECK(PutRequest0.Record.GetValues().Num() == 3);
	CHECK(Algo::AllOf(PutRequest0.Record.GetValues(), &FValue::HasData));

	CHECK(!Store0->FindContent(Value1.GetRawHash(), Value1.GetRawSize()));
	CHECK(!Store0->FindContent(Value2.GetRawHash(), Value2.GetRawSize()));
	Store0->ExecuteAsync();
	CHECK(Store0->FindContent(Value1.GetRawHash(), Value1.GetRawSize()));
	CHECK(Store0->FindContent(Value2.GetRawHash(), Value2.GetRawSize()));

	// Check that the complete record is put to Store1.
	CHECK(Store1->GetTotalRequestCount() == 2);
	REQUIRE(Store1->GetPutRequests().Num() == 1);
	const FCachePutRequest& PutRequest1 = Store1->GetPutRequests()[0];
	CHECK(PutRequest1.Policy.GetRecordPolicy() == ECachePolicy::Store);
	CHECK(PutRequest1.Policy.GetValuePolicy(Id0) == ECachePolicy::StoreLocal);
	CHECK(PutRequest1.Policy.GetValuePolicies().Num() == 1);
	CHECK(PutRequest1.Record.GetValues().Num() == 3);
	CHECK(Algo::AllOf(PutRequest1.Record.GetValues(), &FValue::HasData));

	CHECK(!Store1->FindContent(Value2.GetRawHash(), Value2.GetRawSize()));
	Store1->ExecuteAsync();
	CHECK(Store1->FindContent(Value2.GetRawHash(), Value2.GetRawSize()));

	// Wait to ensure that the counts checked below are final.
	Owner.Wait();

	// Check that the stores are not accessed again.
	CHECK(Store0->GetTotalRequestCount() == 2);
	CHECK(Store1->GetTotalRequestCount() == 2);
	CHECK(Store2->GetTotalRequestCount() == 1);

	StoreOwner->RemoveNotSafe(Store2.Get());
	StoreOwner->RemoveNotSafe(Store1.Get());
	StoreOwner->RemoveNotSafe(Store0.Get());
}

TEST_CASE("DerivedData::Cache::Hierarchy::PartialNonDeterministicRecordPropagation", "[DerivedData]")
{
	ICacheStoreOwner* StoreOwner = nullptr;
	TUniquePtr<ILegacyCacheStore> Hierarchy(CreateCacheStoreHierarchy(StoreOwner, nullptr));

	TUniquePtr<ITestCacheStore> Store0(CreateTestCacheStore(ECacheStoreFlags::Local | ECacheStoreFlags::Query | ECacheStoreFlags::Store, /*bAsync*/ true));
	StoreOwner->Add(Store0.Get(), Store0->GetFlags());
	TUniquePtr<ITestCacheStore> Store1(CreateTestCacheStore(ECacheStoreFlags::Local | ECacheStoreFlags::Query | ECacheStoreFlags::Store, /*bAsync*/ true));
	StoreOwner->Add(Store1.Get(), Store1->GetFlags());
	TUniquePtr<ITestCacheStore> Store2(CreateTestCacheStore(ECacheStoreFlags::Local | ECacheStoreFlags::Query | ECacheStoreFlags::Store, /*bAsync*/ true));
	StoreOwner->Add(Store2.Get(), Store2->GetFlags());

	const FValueId Id0 = FValueId::FromName("0");
	const FValueId Id1 = FValueId::FromName("1");
	const FValueId Id2 = FValueId::FromName("2");
	const FValueWithId Value0(Id0, CreateHierarchyTestValue(0));
	const FValueWithId Value1(Id1, CreateHierarchyTestValue(1));
	const FValueWithId Value2(Id2, CreateHierarchyTestValue(2));
	const FValueWithId Value0Diff(Id0, CreateHierarchyTestValue(3));
	const FCacheKey Key{FCacheBucket("Test"), FIoHash::HashBuffer(MakeMemoryView<uint8>({0, 1, 2, 3}))};

	SECTION("Store is requeried if its response contains differing values that were not queried initially")
	{
		Store0->AddRecord(Key, {Value0, Value1.RemoveData(), Value2});
		Store1->AddRecord(Key, {Value0Diff, Value1, Value2.RemoveData()});
		Store2->AddRecord(Key, {Value0Diff, Value1, Value2});

		FCacheRecordPolicyBuilder PolicyBuilder(ECachePolicy::Default);
		PolicyBuilder.AddValuePolicy(Id0, ECachePolicy::Local);

		FRequestOwner Owner(EPriority::Normal);
		Hierarchy->Get({{{TEXTVIEW("PropagatePartial")}, Key, PolicyBuilder.Build(), 12345}}, Owner, [&Id0, &Value0Diff](FCacheGetResponse&& Response)
		{
			CHECK(Response.UserData == 12345);
			CHECK(Response.Status == EStatus::Ok);
			CHECK(Response.Record.GetValues().Num() == 3);
			CHECK(Algo::AllOf(Response.Record.GetValues(), &FValue::HasData));
			CHECK(Response.Record.GetValue(Id0) == Value0Diff);
		});

		CHECK(Store0->GetTotalRequestCount() == 1);
		CHECK(Store1->GetTotalRequestCount() == 0);
		CHECK(Store2->GetTotalRequestCount() == 0);
		Store0->ExecuteAsync();

		// Check that only Value1 is requested from Store1.
		CHECK(Store0->GetTotalRequestCount() == 1);
		CHECK(Store1->GetTotalRequestCount() == 1);
		CHECK(Store2->GetTotalRequestCount() == 0);
		REQUIRE(Store1->GetGetRequests().Num() == 1);
		const FCacheGetRequest& Request1 = Store1->GetGetRequests()[0];
		CHECK(Request1.Policy.GetRecordPolicy() == (ECachePolicy::Default | ECachePolicy::PartialRecord));
		CHECK(Request1.Policy.GetValuePolicy(Id0) == ECachePolicy::None);
		CHECK(Request1.Policy.GetValuePolicy(Id2) == ECachePolicy::None);
		CHECK(Request1.Policy.GetValuePolicies().Num() == 2);
		Store1->ExecuteAsync();

		// Check that only Value0 is requested from Store1.
		CHECK(Store0->GetTotalRequestCount() == 1);
		CHECK(Store1->GetTotalRequestCount() == 2);
		CHECK(Store2->GetTotalRequestCount() == 0);
		REQUIRE(Store1->GetGetRequests().Num() == 2);
		const FCacheGetRequest& Request1Repeat = Store1->GetGetRequests()[1];
		CHECK(Request1Repeat.Policy.GetRecordPolicy() == (ECachePolicy::Default | ECachePolicy::PartialRecord));
		CHECK(Request1Repeat.Policy.GetValuePolicy(Id0) == ECachePolicy::Local);
		CHECK(Request1Repeat.Policy.GetValuePolicy(Id1) == ECachePolicy::None);
		CHECK(Request1Repeat.Policy.GetValuePolicy(Id2) == ECachePolicy::None);
		CHECK(Request1Repeat.Policy.GetValuePolicies().Num() == 3);
		Store1->ExecuteAsync();

		// Execute stores to Store0 and Store2.
		Store0->ExecuteAsync();
		Store2->ExecuteAsync();

		// Wait to ensure that the counts checked below are final.
		Owner.Wait();

		// Check that the complete record was put to Store0 and Store2.
		REQUIRE(Store0->GetPutRequests().Num() == 1);
		REQUIRE(Store2->GetPutRequests().Num() == 1);
		CHECK(Algo::AllOf(Store0->GetPutRequests()[0].Record.GetValues(), &FValue::HasData));
		CHECK(Algo::AllOf(Store2->GetPutRequests()[0].Record.GetValues(), &FValue::HasData));

		// Check that the stores are not accessed again.
		CHECK(Store0->GetTotalRequestCount() == 2);
		CHECK(Store1->GetTotalRequestCount() == 2);
		CHECK(Store2->GetTotalRequestCount() == 1);
	}

	SECTION("Store is not requeried if its response contains differing values that were queried initially")
	{
		Store0->AddRecord(Key, {Value0.RemoveData(), Value1.RemoveData(), Value2});
		Store1->AddRecord(Key, {Value0Diff, Value1, Value2.RemoveData()});
		Store2->AddRecord(Key, {Value0Diff, Value1, Value2});

		FCacheRecordPolicyBuilder PolicyBuilder(ECachePolicy::Default);
		PolicyBuilder.AddValuePolicy(Id0, ECachePolicy::Local);

		FRequestOwner Owner(EPriority::Normal);
		Hierarchy->Get({{{TEXTVIEW("PropagatePartial")}, Key, PolicyBuilder.Build(), 12345}}, Owner, [&Id0, &Value0Diff](FCacheGetResponse&& Response)
		{
			CHECK(Response.UserData == 12345);
			CHECK(Response.Status == EStatus::Ok);
			CHECK(Response.Record.GetValues().Num() == 3);
			CHECK(Algo::AllOf(Response.Record.GetValues(), &FValue::HasData));
			CHECK(Response.Record.GetValue(Id0) == Value0Diff);
		});

		CHECK(Store0->GetTotalRequestCount() == 1);
		CHECK(Store1->GetTotalRequestCount() == 0);
		CHECK(Store2->GetTotalRequestCount() == 0);
		Store0->ExecuteAsync();

		// Check that only Value0 and Value1 are requested from Store1.
		CHECK(Store0->GetTotalRequestCount() == 1);
		CHECK(Store1->GetTotalRequestCount() == 1);
		CHECK(Store2->GetTotalRequestCount() == 0);
		REQUIRE(Store1->GetGetRequests().Num() == 1);
		const FCacheGetRequest& Request1 = Store1->GetGetRequests()[0];
		CHECK(Request1.Policy.GetRecordPolicy() == (ECachePolicy::Default | ECachePolicy::PartialRecord));
		CHECK(Request1.Policy.GetValuePolicy(Id0) == ECachePolicy::Local);
		CHECK(Request1.Policy.GetValuePolicy(Id2) == ECachePolicy::None);
		CHECK(Request1.Policy.GetValuePolicies().Num() == 2);
		Store1->ExecuteAsync();

		// Execute stores to Store0 and Store2.
		Store0->ExecuteAsync();
		Store2->ExecuteAsync();

		// Wait to ensure that the counts checked below are final.
		Owner.Wait();

		// Check that the complete record was put to Store0 and Store2.
		REQUIRE(Store0->GetPutRequests().Num() == 1);
		REQUIRE(Store2->GetPutRequests().Num() == 1);
		CHECK(Algo::AllOf(Store0->GetPutRequests()[0].Record.GetValues(), &FValue::HasData));
		CHECK(Algo::AllOf(Store2->GetPutRequests()[0].Record.GetValues(), &FValue::HasData));

		// Check that the stores are not accessed again.
		CHECK(Store0->GetTotalRequestCount() == 2);
		CHECK(Store1->GetTotalRequestCount() == 1);
		CHECK(Store2->GetTotalRequestCount() == 1);
	}

	StoreOwner->RemoveNotSafe(Store2.Get());
	StoreOwner->RemoveNotSafe(Store1.Get());
	StoreOwner->RemoveNotSafe(Store0.Get());
}

} // UE::DerivedData

#endif // WITH_LOW_LEVEL_TESTS
