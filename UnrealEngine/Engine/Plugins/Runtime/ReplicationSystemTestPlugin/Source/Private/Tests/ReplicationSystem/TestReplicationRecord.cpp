// Copyright Epic Games, Inc. All Rights Reserved.

#include "NetworkAutomationTest.h"
#include "NetworkAutomationTestMacros.h"
#include "Iris/ReplicationSystem/ReplicationRecord.h"

namespace UE::Net::Private
{

static constexpr FReplicationRecord::ReplicationRecordIndex InvalidReplicationRecordIndex = FReplicationRecord::InvalidReplicationRecordIndex;
static constexpr FReplicationRecord::ReplicationRecordIndex ReplicationRecordIndexMask = FReplicationRecord::ReplicationRecordIndexMask;

UE_NET_TEST(ReplicationRecord, ResetRecordList)
{
	FReplicationRecord ReplicationRecord;
	FReplicationRecord::FRecordInfoList List;

	ReplicationRecord.ResetList(List);

	UE_NET_ASSERT_EQ(List.LastRecordIndex, InvalidReplicationRecordIndex);
	UE_NET_ASSERT_EQ(List.FirstRecordIndex, InvalidReplicationRecordIndex);
}

UE_NET_TEST(ReplicationRecord, PushInfoAndAddToList)
{
	FReplicationRecord ReplicationRecord;
	FReplicationRecord::FRecordInfoList List;

	ReplicationRecord.ResetList(List);

	// Insert entries in list
	const uint32 InfoCount = 10;
	for (uint32 It = 0; It < InfoCount; ++It)
	{
		FReplicationRecord::FRecordInfo Info;
		ReplicationRecord.PushInfoAndAddToList(List, Info);

		UE_NET_ASSERT_EQ(It, (uint32)List.LastRecordIndex);
	}
	UE_NET_ASSERT_EQ(0U, (uint32)List.FirstRecordIndex);

	// Iterate over entries from start to end
	uint32 CurrentIndex = List.FirstRecordIndex;
	uint32 CurrentExpectedIndex = 0;
	
	while (CurrentIndex != InvalidReplicationRecordIndex)
	{
		UE_NET_ASSERT_EQ((uint32)CurrentExpectedIndex, CurrentIndex);

		const FReplicationRecord::FRecordInfo* CurrentRecordInfo = ReplicationRecord.GetInfoForIndex(CurrentIndex);
		UE_NET_ASSERT_TRUE(CurrentRecordInfo != nullptr);

		CurrentIndex = CurrentRecordInfo->NextIndex;
		++CurrentExpectedIndex;
	};
	UE_NET_ASSERT_EQ(InfoCount, CurrentExpectedIndex);
}

UE_NET_TEST(ReplicationRecord, PopInfoAndRemoveFromList)
{
	FReplicationRecord ReplicationRecord;
	FReplicationRecord::FRecordInfoList List;

	ReplicationRecord.ResetList(List);

	// Insert entries in list
	const uint32 InfoCount = 10;
	for (uint32 It = 0; It < InfoCount; ++It)
	{
		FReplicationRecord::FRecordInfo Info;
		ReplicationRecord.PushInfoAndAddToList(List, Info);
	}

	for (uint32 It = 0; It < InfoCount; ++It)
	{
		// We expect the order to be the same when we pushed data
		UE_NET_ASSERT_EQ(It, (uint32)ReplicationRecord.GetFrontIndex());
		ReplicationRecord.PopInfoAndRemoveFromList(List);
		
		if (It < (InfoCount - 1))
		{
			// First record index should now point to It +1 or be invalid
			UE_NET_ASSERT_EQ(It + 1 , (uint32)List.FirstRecordIndex);
			UE_NET_ASSERT_EQ(InfoCount - 1, (uint32)List.LastRecordIndex);
		}
		else
		{
			// List should now be empty
			UE_NET_ASSERT_EQ(InvalidReplicationRecordIndex, List.LastRecordIndex);
			UE_NET_ASSERT_EQ(InvalidReplicationRecordIndex, List.FirstRecordIndex);
		}
	}
}

UE_NET_TEST(ReplicationRecord, TestMaxCapacityAndWraparound)
{
	FReplicationRecord ReplicationRecord;
	FReplicationRecord::FRecordInfoList List;

	ReplicationRecord.ResetList(List);

	// Insert entries in list to max capacity
	const uint32 InfoCount = ReplicationRecordIndexMask + 1;
	for (uint32 It = 0; It < InfoCount; ++It)
	{
		FReplicationRecord::FRecordInfo Info;
		Info.Index = It;
		ReplicationRecord.PushInfoAndAddToList(List, Info);
	}

	UE_NET_ASSERT_EQ(ReplicationRecordIndexMask, List.LastRecordIndex);
	UE_NET_ASSERT_EQ(0U, (uint32)List.FirstRecordIndex);

	// Pop one so we can test wraparound
	ReplicationRecord.PopInfoAndRemoveFromList(List);
	
	UE_NET_ASSERT_EQ(1U, (uint32)ReplicationRecord.GetFrontIndex());

	// Push one more
	FReplicationRecord::FRecordInfo Info;
	ReplicationRecord.PushInfoAndAddToList(List, Info);

	// should wraparound
	UE_NET_ASSERT_EQ(0U, (uint32)List.LastRecordIndex);

	// Access Last pushed entry and verify that it points to what we expect
	FReplicationRecord::FRecordInfo* LastInfo = ReplicationRecord.GetInfoForIndex(List.LastRecordIndex);
	UE_NET_ASSERT_TRUE(LastInfo != nullptr);
	UE_NET_ASSERT_EQ(InvalidReplicationRecordIndex, LastInfo->NextIndex);
}

}
