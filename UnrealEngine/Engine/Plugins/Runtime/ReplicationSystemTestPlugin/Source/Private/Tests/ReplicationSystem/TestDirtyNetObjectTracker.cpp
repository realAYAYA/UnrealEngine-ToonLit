// Copyright Epic Games, Inc. All Rights Reserved.

#include "NetworkAutomationTest.h"
#include "NetworkAutomationTestMacros.h"
#include "Iris/ReplicationSystem/DirtyNetObjectTracker.h"
#include "Iris/ReplicationSystem/NetHandle.h"
#include "Iris/ReplicationSystem/ReplicationSystem.h"

namespace UE::Net::Private
{

class FDirtyNetObjectTrackerTestFixture : public FNetworkAutomationTestSuiteFixture
{
public:
	FDirtyNetObjectTrackerTestFixture()
	: FNetworkAutomationTestSuiteFixture()
	, DirtyNetObjectTracker(nullptr)
	{
	}

	virtual void SetUp() override
	{
		DirtyNetObjectTracker = new FDirtyNetObjectTracker();
	}

	virtual void TearDown() override
	{
		delete DirtyNetObjectTracker;
	}

protected:
	static constexpr uint32 ReplicationSystemId = 0;
	static constexpr uint32 NetObjectIndexRangeStart = 1;
	static constexpr uint32 NetObjectIndexRangeEnd = 1000;

	FDirtyNetObjectTracker* DirtyNetObjectTracker;

};

class FDirtyNetObjectTrackerWithInitTestFixture : public FDirtyNetObjectTrackerTestFixture
{
protected:
	virtual void SetUp() override
	{
		FDirtyNetObjectTrackerTestFixture::SetUp();
		Init();
	}

	void Init();
};

UE_NET_TEST_FIXTURE(FDirtyNetObjectTrackerTestFixture, TestInit)
{
	const FDirtyNetObjectTrackerInitParams InitParams 
	{
		ReplicationSystemId,
		NetObjectIndexRangeStart,
		NetObjectIndexRangeEnd,
	};

	DirtyNetObjectTracker->Init(InitParams);
	const FNetBitArrayView& DirtyObjects = DirtyNetObjectTracker->GetDirtyNetObjects();
	UE_NET_ASSERT_EQ(DirtyObjects.GetNumBits(), NetObjectIndexRangeEnd + 1U);

	UE_NET_ASSERT_FALSE(DirtyObjects.IsAnyBitSet()) << "Objects are marked as dirty before marking any object state as dirty";
}

UE_NET_TEST_FIXTURE(FDirtyNetObjectTrackerWithInitTestFixture, CannotMarkInvalidObjectAsDirty)
{
	MarkNetObjectStateDirty(ReplicationSystemId, FNetHandle().GetId());
	MarkNetObjectStateDirty(ReplicationSystemId, NetObjectIndexRangeEnd + 1);

	const FNetBitArrayView& DirtyObjects = DirtyNetObjectTracker->GetDirtyNetObjects();
	UE_NET_ASSERT_FALSE(DirtyObjects.IsAnyBitSet());
}

UE_NET_TEST_FIXTURE(FDirtyNetObjectTrackerWithInitTestFixture, CanMarkValidObjectAsDirty)
{
	constexpr uint32 IndexInRange = NetObjectIndexRangeStart + (NetObjectIndexRangeEnd - NetObjectIndexRangeStart)/2;
	MarkNetObjectStateDirty(ReplicationSystemId, IndexInRange);
	MarkNetObjectStateDirty(ReplicationSystemId, NetObjectIndexRangeStart);
	MarkNetObjectStateDirty(ReplicationSystemId, NetObjectIndexRangeEnd);

	const FNetBitArrayView& DirtyObjects = DirtyNetObjectTracker->GetDirtyNetObjects();
	UE_NET_ASSERT_TRUE(DirtyObjects.GetBit(IndexInRange));
	UE_NET_ASSERT_TRUE(DirtyObjects.GetBit(NetObjectIndexRangeStart));
	UE_NET_ASSERT_TRUE(DirtyObjects.GetBit(NetObjectIndexRangeEnd));
}

UE_NET_TEST_FIXTURE(FDirtyNetObjectTrackerWithInitTestFixture, CanClearDirtyObjects)
{
	MarkNetObjectStateDirty(ReplicationSystemId, NetObjectIndexRangeStart);
	MarkNetObjectStateDirty(ReplicationSystemId, NetObjectIndexRangeEnd);

	DirtyNetObjectTracker->ClearDirtyNetObjects();
	const FNetBitArrayView& DirtyObjects = DirtyNetObjectTracker->GetDirtyNetObjects();
	UE_NET_ASSERT_FALSE(DirtyObjects.IsAnyBitSet());
}

UE_NET_TEST(DirtyNetObjectTracker, MarkingObjectAsDirtyInNonExistingSystemDoesNotCrash)
{
	constexpr uint32 NonExistingReplicationSystemId = 4711;
	constexpr uint32 ArbitraryNetObjectIndex = 1;
	MarkNetObjectStateDirty(NonExistingReplicationSystemId, ArbitraryNetObjectIndex);
}

//
void FDirtyNetObjectTrackerWithInitTestFixture::Init()
{
	const FDirtyNetObjectTrackerInitParams InitParams 
	{
		ReplicationSystemId,
		NetObjectIndexRangeStart,
		NetObjectIndexRangeEnd,
	};

	DirtyNetObjectTracker->Init(InitParams);
}

}
